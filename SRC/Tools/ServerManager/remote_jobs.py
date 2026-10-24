"""Remote job plans and immutable upload snapshots; no network access on import."""
import base64
import fnmatch
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import secrets
import shlex
import tempfile

from backend import EditorError


def defaults():
    return dict(source_dir='', build_dir='', configuration='Release', generator='Ninja',
                parallel=2, targets='GameServer Database LoginServer ServerPackageCheck', cmake='cmake',
                configure_args='-DAURIGA_USE_VCPKG=OFF', upload_dir='',
                excludes='.git;.plastic;build;Library;Temp;__pycache__;.venv')


def remote_path(value, label):
    if (not isinstance(value, str) or not value.startswith('/') or value.startswith('//')
            or len(value) > 2048 or any(ord(c) < 32 or ord(c) == 127 for c in value)
            or '..' in PurePosixPath(value).parts or str(PurePosixPath(value)) == '/'):
        raise EditorError(label + ': teljes Unix-mappaútvonal szükséges (nem a / gyökér).')
    return str(PurePosixPath(value))


def validate_settings(value):
    template = defaults()
    if not isinstance(value, dict) or set(value) != set(template):
        raise EditorError('Ismeretlen távoli munkabeállítás.')
    value = dict(value)
    for key in template:
        if key == 'parallel':
            if type(value[key]) is not int or not 1 <= value[key] <= 128:
                raise EditorError('A párhuzamos buildfeladatok száma 1–128 legyen.')
        elif (not isinstance(value[key], str) or len(value[key]) > 4096
              or any(ord(c) < 32 or ord(c) == 127 for c in value[key])):
            raise EditorError('Érvénytelen távoli munkamező: ' + key)
    for key in ('source_dir', 'build_dir', 'upload_dir'):
        if value[key]:
            value[key] = remote_path(value[key], key)
    if value['configuration'] not in ('Release', 'Debug', 'RelWithDebInfo', 'MinSizeRel'):
        raise EditorError('Ismeretlen buildkonfiguráció.')
    if value['generator'] not in ('Ninja', 'Unix Makefiles'):
        raise EditorError('Ninja vagy Unix Makefiles generátor választható.')
    if not re.fullmatch(r'(?:/[A-Za-z0-9_./+-]+|[A-Za-z0-9_.+-]+)', value['cmake']):
        raise EditorError('A CMake mező egy programnév vagy teljes Unix-programútvonal legyen.')
    return value


def build_request(settings, action='build'):
    settings = validate_settings(settings)
    source = remote_path(settings['source_dir'], 'Forrásmappa')
    build = remote_path(settings['build_dir'], 'Buildmappa')
    if source == build or PurePosixPath(source).is_relative_to(PurePosixPath(build)):
        raise EditorError('A build külön mappába kerüljön; a forrás nem lehet a buildmappában.')
    targets = settings['targets'].split()
    if not targets or any(not re.fullmatch(r'[A-Za-z0-9_][A-Za-z0-9_.+-]*', t) for t in targets):
        raise EditorError('Adj meg buildcélokat szóközzel elválasztva.')
    try:
        extra = shlex.split(settings['configure_args'])
    except ValueError as error:
        raise EditorError('Hibás CMake-paraméterezés: ' + str(error)) from None
    # Keep -S/-B/-G, presets, install and script modes under the form's control.
    if any(not re.fullmatch(r'-D[A-Za-z_][A-Za-z0-9_]*(?::[A-Za-z]+)?=.*', arg) for arg in extra):
        raise EditorError('A további CMake-paraméterek -DNÉV=ÉRTÉK alakúak legyenek; szóköznél idézőjellel.')
    configure = [settings['cmake'], '-S', source, '-B', build, '-G', settings['generator'],
                 '-DCMAKE_BUILD_TYPE=' + settings['configuration'], *extra]
    compile_argv = [settings['cmake'], '--build', build, '--config', settings['configuration'],
                    '--parallel', str(settings['parallel']), '--target', *targets]
    return dict(action=action, id=secrets.token_hex(12), source=source, build=build,
                configure=configure, compile=compile_argv)


def worker_input(request):
    request = dict(request)
    if request['action'] == 'install':
        request['install_support'] = Path(__file__).with_name('remote_deploy.py').read_text(encoding='utf-8')
        base=Path(__file__).resolve().parent
        request['backup_support'] = {name:next(path for path in
            (base/'remote_support'/(name+'.py'),base/(name+'.py')) if path.is_file()).read_text(encoding='utf-8')
            for name in ('crash_tools','backup_tools','backend')}
    if request['action'] == 'packages':
        request['package_support'] = Path(__file__).with_name('remote_packages.py').read_text(encoding='utf-8')
    source = Path(__file__).with_name('remote_worker.py').read_text(encoding='utf-8')
    data = base64.b64encode(json.dumps(request, ensure_ascii=True).encode()).decode('ascii')
    return (source + '\nif __name__ == "__main__":\n    main(' + repr(data) + ')\n').encode('utf-8')


def install_request(settings, root):
    settings = validate_settings(settings)
    build = remote_path(settings['build_dir'], 'Buildmappa')
    root = remote_path(root, 'Szervermappa')
    if PurePosixPath(build).is_relative_to(PurePosixPath(root)) or PurePosixPath(root).is_relative_to(PurePosixPath(build)):
        raise EditorError('A buildmappa és a szervermappa különálló mappák legyenek.')
    return dict(action='install', id=secrets.token_hex(12), build=build, root=root,
                configuration=settings['configuration'])


class UploadSnapshot:
    """Copy/hash off the UI thread so subsequent edits cannot change reviewed bytes."""
    def __init__(self, sources, destination, excludes='', contents=False, cancelled=lambda: False, progress=lambda _: None):
        self.directory = tempfile.TemporaryDirectory(prefix='auriga-upload-')
        self.root = Path(self.directory.name)
        self.request = dict(action='prepare', id=secrets.token_hex(12),
                            destination=remote_path(destination, 'Feltöltési cél'), files=[], directories=[])
        self.skipped = 0
        patterns = [p.strip() for p in excludes.split(';') if p.strip()]
        seen, dirs = set(), set()

        def add(path, relative):
            if cancelled():
                raise EditorError('A feltöltés előkészítését megszakítottad.')
            if path.is_symlink() or (hasattr(path, 'is_junction') and path.is_junction()):
                raise EditorError('Szimbolikus link/junction nem tölthető fel: ' + str(path))
            rel = PurePosixPath(relative)
            if any(fnmatch.fnmatchcase(part, pattern) for part in rel.parts for pattern in patterns):
                self.skipped += 1
                return
            if (not rel.parts or rel.is_absolute() or '..' in rel.parts or '\\' in relative
                    or any(ord(c) < 32 or ord(c) == 127 for c in relative)
                    or rel.parts[0] == '.auriga-manager'):
                raise EditorError('Nem feltölthető fájlnév: ' + relative)
            if relative.casefold() in seen:
                raise EditorError('Ütköző feltöltési útvonal: ' + relative)
            seen.add(relative.casefold())
            if path.is_dir():
                dirs.add(relative)
                for child in sorted(path.iterdir()):
                    add(child, relative + '/' + child.name)
            elif path.is_file():
                before = path.stat()
                key = '%08d' % len(self.request['files'])
                digest = hashlib.sha256()
                size = 0
                with path.open('rb') as original, (self.root / key).open('xb') as copy:
                    while chunk := original.read(1024 * 1024):
                        if cancelled():
                            raise EditorError('A feltöltés előkészítését megszakítottad.')
                        copy.write(chunk)
                        digest.update(chunk)
                        size += len(chunk)
                after = path.stat()
                if (before.st_size, before.st_mtime_ns, before.st_ino) != (after.st_size, after.st_mtime_ns, after.st_ino):
                    raise EditorError('Előkészítés közben módosult a fájl: ' + str(path))
                self.request['files'].append(dict(path=relative, key=key, size=size, sha256=digest.hexdigest()))
                dirs.update(str(parent) for parent in rel.parents if str(parent) != '.')
                progress(f"{len(self.request['files'])} fájl előkészítve: {relative}")
            else:
                raise EditorError('Nem szabályos fájl vagy mappa: ' + str(path))

        try:
            if not sources or (contents and (len(sources) != 1 or not Path(sources[0]).is_dir())):
                raise EditorError('Válassz fájlokat vagy mappát. A tartalom módhoz pontosan egy mappa kell.')
            for original in sources:
                path = Path(original).absolute()
                if contents:
                    if path.is_symlink() or (hasattr(path, 'is_junction') and path.is_junction()):
                        raise EditorError('Linkelt forrásmappa nem tölthető fel.')
                    for child in sorted(path.iterdir()):
                        add(child, child.name)
                else:
                    add(path, path.name)
            if not self.request['files'] and not dirs:
                raise EditorError('A kiválasztás és a kizárások után nincs feltölthető tartalom.')
            self.request['directories'] = sorted(dirs, key=lambda name: (name.count('/'), name))
        except BaseException:
            self.close()
            raise

    @property
    def stage(self):
        return self.request['destination'] + '/.auriga-manager/uploads/' + self.request['id']

    def batch(self):
        # Only numeric snapshots are passed to SFTP: selected filenames (including
        # glob characters and quotes) travel as JSON, never as SFTP commands.
        quote = lambda p: '"' + str(p).replace('\\', '/').replace('"', '\\"') + '"'
        rows = ['put ' + quote((self.root / row['key']).as_posix()) + ' ' +
                quote(self.stage + '/files/' + row['key']) for row in self.request['files']]
        path = self.root / 'batch'
        path.write_text('\n'.join([*rows, 'bye', '']), encoding='utf-8', newline='\n')
        return path

    def close(self):
        self.directory.cleanup()
