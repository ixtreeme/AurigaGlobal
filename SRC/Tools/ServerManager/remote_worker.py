"""Standard-library worker sent over verified SSH stdin; also exercised locally."""
import base64
import contextlib
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import queue
import re
import shutil
import signal
import stat
import subprocess
import sys
import threading
import time
import types


def emit(message):
    print(message, flush=True)


def path_at(root, relative):
    parts = PurePosixPath(relative).parts
    if (not parts or PurePosixPath(relative).is_absolute() or '..' in parts
            or '\\' in relative or parts[0] == '.auriga-manager'):
        raise ValueError('Érvénytelen relatív útvonal: ' + relative)
    path = root.joinpath(*parts)
    no_links(path)
    return path


def no_links(path):
    for part in (path, *path.parents):
        if part.is_symlink() or (hasattr(part, 'is_junction') and part.is_junction()):
            raise ValueError('Linkelt célútvonal nem használható: ' + str(part))


def root_path(value):
    path = Path(value)
    if not path.is_absolute() or '..' in path.parts or path == Path(path.anchor):
        raise ValueError('Teljes munkamappa szükséges; gyökérmappa nem használható.')
    no_links(path)
    return path


def metadata(root):
    directory = root / '.auriga-manager'
    no_links(directory)
    directory.mkdir(mode=0o700, parents=True, exist_ok=True)
    return directory


def digest(path):
    h = hashlib.sha256()
    with path.open('rb') as stream:
        while chunk := stream.read(1024 * 1024):
            h.update(chunk)
    return h.hexdigest()


@contextlib.contextmanager
def locked(root):
    lock = metadata(root) / 'operation.lock'
    try:
        lock.mkdir(mode=0o700)
    except FileExistsError:
        raise ValueError('Másik build/feltöltés zárolja a mappát: ' + str(lock)) from None
    try:
        (lock / 'owner.json').write_text(json.dumps(dict(pid=os.getpid(), time=time.time())), encoding='utf-8')
        yield
    finally:
        (lock / 'owner.json').unlink(missing_ok=True)
        lock.rmdir()


def upload_stage(request):
    root = root_path(request['destination'])
    stage = metadata(root) / 'uploads' / request['id']
    no_links(stage)
    return root, stage


def prepare(request):
    root, stage = upload_stage(request)
    stage.mkdir(mode=0o700, parents=True, exist_ok=False)
    (stage / 'files').mkdir(mode=0o700)
    (stage / 'request.json').write_text(json.dumps(request), encoding='utf-8')
    emit('AURIGA_UPLOAD_PREPARED ' + str(stage))


def commit(request):
    root, stage = upload_stage(request)
    manifest = json.loads((stage / 'request.json').read_text(encoding='utf-8'))
    backup = stage / 'backup'
    backup.mkdir(mode=0o700)
    installed, created_dirs = [], []
    with locked(root):
        # Verify EVERY byte and target before touching any original.
        for row in manifest['files']:
            target = path_at(root, row['path'])
            staged = stage / 'files' / row['key']
            no_links(staged)
            if staged.stat().st_size != row['size'] or digest(staged) != row['sha256']:
                raise ValueError('Feltöltési ellenőrzőösszeg eltér: ' + row['path'])
            if target.exists() and (not target.is_file() or not request.get('overwrite', False)):
                raise ValueError('A cél létezik; a felülírás nincs engedélyezve: ' + row['path'])
        for relative in manifest['directories']:
            target = path_at(root, relative)
            if target.exists() and not target.is_dir():
                raise ValueError('Mappa helyén fájl található: ' + relative)
        try:
            for relative in manifest['directories']:
                target = path_at(root, relative)
                if not target.exists():
                    target.mkdir()
                    created_dirs.append(target)
            for row in manifest['files']:
                target = path_at(root, row['path'])
                old = backup / row['key']
                existed = target.exists()
                if existed:
                    shutil.copy2(target, old)
                    # Preserve existing Unix executable bits; new scripts can
                    # explicitly be chmod'ed after upload by their operator.
                    os.chmod(stage / 'files' / row['key'], stat.S_IMODE(target.stat().st_mode))
                else:
                    os.chmod(stage / 'files' / row['key'], 0o644)
                os.replace(stage / 'files' / row['key'], target)
                installed.append((target, old, existed))
                emit('Feltöltve: ' + row['path'])
        except BaseException:
            for target, old, existed in reversed(installed):
                if existed:
                    os.replace(old, target)
                else:
                    target.unlink()
            for directory in reversed(created_dirs):
                directory.rmdir()
            raise
    (stage / 'result.json').write_text(json.dumps(dict(status='success', files=len(installed))), encoding='utf-8')
    emit('AURIGA_UPLOAD_SUCCESS files=%d backup=%s' % (len(installed), backup))


def cancel_build(request):
    build = root_path(request['build'])
    folder = metadata(build) / 'jobs' / request['id']
    no_links(folder)
    if not (folder / 'active').is_file():
        raise ValueError('A build nem aktív ezen a feladatazonosítón.')
    (folder / 'cancel').touch()
    emit('AURIGA_BUILD_CANCEL_REQUESTED')


def build_environment():
    environment = dict(os.environ)
    if sys.platform.startswith('freebsd'):
        # Non-interactive SSH may omit the package tools and base compilers.
        paths = environment.get('PATH', '').split(os.pathsep)
        for directory in ('/usr/local/bin', '/usr/bin', '/bin'):
            if directory not in paths: paths.append(directory)
        environment['PATH'] = os.pathsep.join(path for path in paths if path)
    return environment


def cmake_definition(argv, name):
    value = None
    for argument in argv:
        if argument.startswith('-D') and '=' in argument:
            key, candidate = argument[2:].split('=', 1)
            if key.split(':', 1)[0] == name: value = candidate
    return value


def build_commands(request, build, environment, report):
    configure, compile_argv = list(request['configure']), list(request['compile'])
    # The worker tests also support small custom subprocess fixtures. Actual
    # manager build requests always contain this explicit CMake generator.
    if '-G' not in configure: return configure, compile_argv
    generator = configure[configure.index('-G') + 1]
    cache = {}
    cache_file = build / 'CMakeCache.txt'
    no_links(cache_file)
    if cache_file.is_file():
        if cache_file.stat().st_size > 4 * 1024 * 1024:
            raise ValueError('Túl nagy CMakeCache.txt; válassz új buildmappát.')
        for line in cache_file.read_text(encoding='utf-8', errors='replace').splitlines():
            if not line or line.startswith(('#', '//')) or '=' not in line: continue
            key, value = line.split('=', 1)
            cache[key.split(':', 1)[0]] = value
    if cache.get('CMAKE_GENERATOR') and cache['CMAKE_GENERATOR'] != generator:
        raise ValueError('Ebben a buildmappában már %s generátor van; a(z) %s generátorhoz válassz új buildmappát.' %
                         (cache['CMAKE_GENERATOR'], generator))
    cmake = shutil.which(configure[0], path=environment.get('PATH', ''))
    if not cmake:
        hint = ' FreeBSD-n telepítés rootként: pkg install cmake.' if sys.platform.startswith('freebsd') else ''
        raise ValueError('A CMake program nem található: ' + configure[0] + '.' + hint)
    configure[0] = cmake
    if compile_argv[0] == request['configure'][0]: compile_argv[0] = cmake
    explicit = cmake_definition(configure, 'CMAKE_MAKE_PROGRAM')
    candidate = explicit if explicit is not None else cache.get('CMAKE_MAKE_PROGRAM')
    tool = shutil.which(candidate, path=environment.get('PATH', '')) if candidate else None
    if explicit is not None and not tool:
        raise ValueError('A megadott CMAKE_MAKE_PROGRAM nem futtatható: ' + repr(explicit))
    names = {'Ninja': ('ninja', 'ninja-build'), 'Unix Makefiles': ('gmake', 'make')}
    if not tool:
        tool = next((found for name in names.get(generator, ())
                     if (found := shutil.which(name, path=environment.get('PATH', '')))), None)
    if not tool:
        hint = ''
        if sys.platform.startswith('freebsd'):
            hint = ' FreeBSD-n telepítés rootként: pkg install %s.' % ('ninja' if generator == 'Ninja' else 'gmake')
        raise ValueError('A(z) %s buildeszköze nem található a távoli gépen vagy a PATH-ban.%s '
                         'A CMake-konfigurálás és a fordítás nem indult el.' % (generator, hint))
    if explicit is None:
        # Also repairs a previous CMAKE_MAKE_PROGRAM-NOTFOUND cache entry.
        configure.append('-DCMAKE_MAKE_PROGRAM:FILEPATH=' + tool)
    report('Buildkörnyezet: CMake=' + cmake + '; ' + generator + '=' + tool)
    return configure, compile_argv


def run_command(argv, folder, log, stop, cwd, environment=None):
    emit('> ' + ' '.join(repr(arg) for arg in argv))
    process = subprocess.Popen(argv, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                               stdin=subprocess.DEVNULL, cwd=cwd, env=environment, start_new_session=os.name != 'nt')
    lines = queue.Queue()

    def read():
        try:
            for line in iter(process.stdout.readline, b''):
                lines.put(line)
        finally:
            lines.put(None)

    reader = threading.Thread(target=read, daemon=True)
    reader.start()
    ended, cancelled = False, False
    try:
        while not ended or process.poll() is None:
            if stop.is_set() or (folder / 'cancel').exists():
                cancelled = True
                break
            try:
                line = lines.get(timeout=.15)
            except queue.Empty:
                continue
            if line is None:
                ended = True
            else:
                text = line.decode('utf-8', errors='replace')
                log.write(text)
                log.flush()
                print(text, end='', flush=True)
        if cancelled:
            raise InterruptedError('A build megszakítva.')
        code = process.wait()
        if code:
            raise RuntimeError('A buildparancs hibakódja: ' + str(code))
    finally:
        if process.poll() is None or cancelled:
            if os.name == 'nt':
                if process.poll() is None:
                    process.terminate()
            else:
                with contextlib.suppress(ProcessLookupError):
                    os.killpg(process.pid, signal.SIGTERM)
            try:
                process.wait(timeout=3)
            except subprocess.TimeoutExpired:
                if os.name == 'nt':
                    process.kill()
                else:
                    with contextlib.suppress(ProcessLookupError):
                        os.killpg(process.pid, signal.SIGKILL)
                process.wait()
        process.stdout.close()


def build_job(request):
    source, build = root_path(request['source']), root_path(request['build'])
    if not (source / 'CMakeLists.txt').is_file():
        raise ValueError('A forrásmappában nincs CMakeLists.txt.')
    build.mkdir(parents=True, exist_ok=True)
    folder = metadata(build) / 'jobs' / request['id']
    folder.mkdir(mode=0o700, parents=True)
    (folder / 'active').touch()
    stop = threading.Event()
    handlers = {}
    if threading.current_thread() is threading.main_thread():
        for name in ('SIGTERM', 'SIGHUP', 'SIGINT'):
            sig = getattr(signal, name, None)
            if sig is not None:
                handlers[sig] = signal.signal(sig, lambda *_: stop.set())
    result = dict(status='failed')
    try:
        with locked(source), locked(build), (folder / 'build.log').open('w', encoding='utf-8') as log:
            environment = build_environment()
            def report(message):
                emit(message)
                log.write(message + '\n');log.flush()
            configure, compile_argv = build_commands(request, build, environment, report)
            run_command(configure, folder, log, stop, source, environment)
            if request['action'] != 'configure':
                run_command(compile_argv, folder, log, stop, source, environment)
        result = dict(status='success')
        emit('AURIGA_BUILD_SUCCESS log=' + str(folder / 'build.log'))
    except InterruptedError:
        result = dict(status='cancelled')
        raise
    finally:
        (folder / 'active').unlink(missing_ok=True)
        (folder / 'result.json').write_text(json.dumps(result), encoding='utf-8')
        for sig, handler in handlers.items():
            signal.signal(sig, handler)


def install_job(request):
    module = types.ModuleType('_auriga_remote_deploy')
    exec(compile(request['install_support'], '<auriga-remote-deploy>', 'exec'), module.__dict__)
    module.deploy(request, types.SimpleNamespace(root_path=root_path, no_links=no_links,
                  locked=locked, digest=digest, emit=emit))


def package_job(request):
    module = types.ModuleType('_auriga_remote_packages')
    exec(compile(request['package_support'], '<auriga-remote-packages>', 'exec'), module.__dict__)
    module.execute(request, emit)


def main(encoded):
    if hasattr(sys.stdout, 'reconfigure'):
        sys.stdout.reconfigure(encoding='utf-8', errors='replace')
    os.umask(0o077)
    try:
        request = json.loads(base64.b64decode(encoded))
        if not re.fullmatch(r'[0-9a-f]{24}', request['id']):
            raise ValueError('Hibás feladatazonosító.')
        actions = dict(prepare=prepare, commit=commit, build=build_job, configure=build_job,
                       cancel=cancel_build, install=install_job, packages=package_job)
        actions[request['action']](request)
    except Exception as error:
        emit('AURIGA_JOB_FAILED ' + str(error))
        raise SystemExit(1)
