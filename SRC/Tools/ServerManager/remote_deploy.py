"""Stopped-server installation of four POSIX build artifacts. Standard library only."""
import importlib.util
import json
import os
from pathlib import Path
import platform
import shutil
import signal
import socket
import subprocess
import sys
import threading
import tempfile

PROGRAMS = {'GameServer': 'GameServer', 'Database': 'Database',
            'LoginServer': 'LoginServer', 'ServerPackageCheck': 'GameServer'}


def load_manager(root):
    spec = importlib.util.spec_from_file_location('_auriga_deploy_manager', root / 'manage.py')
    manager = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(manager)
    if getattr(manager, 'PACKAGE_CONTROL_VERSION', 0) < 2:
        raise ValueError('A szervermappa manage.py fájlját frissíteni kell a telepítéshez.')
    return manager


def journal(path, value):
    temporary = path.with_suffix('.new')
    with temporary.open('w', encoding='utf-8') as output:
        json.dump(value, output, ensure_ascii=True, indent=2)
        output.flush();os.fsync(output.fileno())
    os.replace(temporary, path)
    if os.name != 'nt':
        descriptor = os.open(path.parent, os.O_RDONLY)
        try: os.fsync(descriptor)
        finally: os.close(descriptor)


def pending_deployment(root, helpers):
    directory = root / 'run/deployments';helpers.no_links(directory)
    for path in directory.glob('*/result.json'):
        helpers.no_links(path)
        if json.loads(path.read_text(encoding='utf-8'))['status'] in ('installing', 'rollback-failed'):
            raise ValueError('Korábbi telepítés helyreállítást igényel; ellenőrizd a mentést és a naplót: ' + str(path))


def artifacts(build, configuration, helpers):
    result = {}
    cache = build / 'CMakeCache.txt';helpers.no_links(cache)
    if cache.is_file():
        values = dict(line.split('=', 1) for line in cache.read_text(encoding='utf-8', errors='replace').splitlines()
                      if line.startswith(('CMAKE_BUILD_TYPE:STRING=', 'CMAKE_CONFIGURATION_TYPES:STRING=')))
        selected = values.get('CMAKE_BUILD_TYPE:STRING')
        if not values.get('CMAKE_CONFIGURATION_TYPES:STRING') and selected != configuration:
            raise ValueError('A buildmappa konfigurációja eltér a kiválasztottól: %s / %s.' % (selected or 'nincs', configuration))
    machine = {'amd64': 62, 'x86_64': 62, 'aarch64': 183, 'arm64': 183}.get(platform.machine().lower())
    for name, component in PROGRAMS.items():
        directory = build / 'SRC/Server' / component
        candidates = [directory / configuration / name, directory / name, build / name]
        found = [path for path in candidates if path.is_file()]
        if len(found) != 1:
            raise ValueError('Hiányzó vagy többértelmű buildkimenet: %s. Fordítsd le mind a négy buildcélt.' % name)
        path = found[0];helpers.no_links(path)
        with path.open('rb') as stream: header = stream.read(20)
        endian = 'little' if len(header) > 5 and header[5] == 1 else 'big'
        if (len(header) < 20 or header[:5] != b'\x7fELF\x02' or header[5] not in (1, 2)
                or int.from_bytes(header[16:18], endian) not in (2, 3)
                or machine is None or int.from_bytes(header[18:20], endian) != machine
                or header[7] not in ((0, 9) if sys.platform.startswith('freebsd') else (0, 3))):
            raise ValueError('Nem ehhez a géphez való 64 bites ELF-program: ' + str(path))
        result[name] = path
    return result


def process_images():
    if sys.platform.startswith('freebsd'):
        result = subprocess.run(['/usr/bin/procstat', '-b', '-a'], capture_output=True, text=True,
                                timeout=10, env=dict(os.environ, LC_ALL='C'))
        if result.returncode or result.stderr.strip():
            raise ValueError('A futó programok nem ellenőrizhetők procstat segítségével.')
        rows = []
        for line in result.stdout.splitlines():
            parts = line.split(None, 3)
            if len(parts) != 4 or not parts[0].isdigit(): continue
            if parts[3].startswith('/'):
                rows.append((int(parts[0]), Path(parts[3])))
            elif any(program.startswith(parts[1]) for program in PROGRAMS):
                raise ValueError('Egy szerverfolyamat programútvonala nem ellenőrizhető: PID ' + parts[0])
        if not rows: raise ValueError('A procstat nem adott ellenőrizhető folyamatlistát.')
        return rows
    if not sys.platform.startswith('linux') or not Path('/proc/self/exe').exists():
        raise ValueError('A telepítés FreeBSD/Linux célgépen támogatott, ellenőrizhető folyamatlistával.')
    rows = []
    for folder in Path('/proc').iterdir():
        if not folder.name.isdigit(): continue
        try:
            rows.append((int(folder.name), Path(os.readlink(folder / 'exe').removesuffix(' (deleted)'))))
        except FileNotFoundError: pass
        except PermissionError:
            try: name = (folder / 'comm').read_text().strip()
            except FileNotFoundError: continue
            if any(program.startswith(name) for program in PROGRAMS):
                raise ValueError('Egy szerverfolyamat nem ellenőrizhető: PID ' + folder.name)
    return rows


def stopped(root, manager, config):
    state_file = root / 'run/supervisor.json'
    if state_file.is_file():
        state = manager.read_json(state_file)
        records = [state.get('supervisor'), *(child.get('identity') for child in state.get('children', []))]
        if any(manager.owned(record) for record in records):
            raise ValueError('Telepítés előtt állítsd le a kiválasztott szervert az Áttekintés oldalon.')
    targets = [root / 'bin' / name for name in PROGRAMS]
    for pid, image in process_images():
        for target in targets:
            try: same = image == target or image.samefile(target)
            except (FileNotFoundError, PermissionError): same = image == target
            if same: raise ValueError('Még fut a szervermappa egyik programja (PID %s): %s' % (pid, target.name))
    # A manually started or still-exiting instance must not be replaced either.
    ports = [config['database']['network']['bind_port'], config['login']['port']]
    for section in config['game_servers'].values():
        network = dict(config.get('game_defaults', {}).get('network', {}), **section.get('network', {}))
        ports.extend(network[key] for key in ('port', 'p2p_port') if key in network)
    for port in set(ports):
        if type(port) is not int or not 1 <= port <= 65535: raise ValueError('Érvénytelen szerverport a konfigurációban.')
        with socket.socket() as probe:
            try: probe.bind(('0.0.0.0', port))
            except OSError: raise ValueError('Telepítés előtt szabaddá kell válnia a szerverportnak: %s' % port) from None


def redacted(text, config):
    secrets = []
    def visit(value):
        if isinstance(value, dict):
            for key, child in value.items():
                if any(part in key.lower() for part in ('password', 'interserver_key', 'token', 'secret')):
                    if isinstance(child, str) and child: secrets.append(child)
                else: visit(child)
        elif isinstance(value, list):
            for child in value: visit(child)
    visit(config)
    for value in sorted(set(secrets), key=len, reverse=True): text = text.replace(value, '••••••')
    return text


def validate_programs(directory, root, config):
    calls = [('Database', 'database'), ('LoginServer', 'login')]
    calls += [('GameServer', name) for name in config['game_servers']]
    for program, instance in calls:
        result = subprocess.run([str(directory / program), '--config', str(root / 'server.json'),
                                 '--instance', instance, '--check-config'], cwd=root,
                                capture_output=True, text=True, errors='replace', timeout=90)
        if result.returncode:
            raise ValueError('%s konfigurációellenőrzése sikertelen: %s' %
                             (instance, redacted((result.stdout + result.stderr)[-6000:], config)))
    # Check that the fourth executable and its Lua runtime can actually load.
    with tempfile.TemporaryDirectory(prefix='auriga-deploy-check-') as temporary:
        script = Path(temporary) / 'check.lua';script.write_text('return true\n', encoding='utf-8')
        result = subprocess.run([str(directory / 'ServerPackageCheck'), '--check-lua', str(script)],
                                cwd=root, capture_output=True, text=True, errors='replace', timeout=90)
        if result.returncode:
            raise ValueError('A ServerPackageCheck ellenőrzése sikertelen: ' + redacted((result.stdout + result.stderr)[-6000:], config))


def deploy(request, helpers):
    root, build = helpers.root_path(request['root']), helpers.root_path(request['build'])
    if root.is_relative_to(build) or build.is_relative_to(root):
        raise ValueError('A buildmappa és a szervermappa különálló mappák legyenek.')
    if request['configuration'] not in ('Release', 'Debug', 'RelWithDebInfo', 'MinSizeRel'):
        raise ValueError('Ismeretlen buildkonfiguráció.')
    for name in ('.auriga-package.json', 'manage.py', 'server.json', 'bin', 'run'):
        helpers.no_links(root / name)
    if not (root / '.auriga-package.json').is_file(): raise ValueError('A cél nem Auriga szervercsomag.')
    config = json.loads((root / 'server.json').read_text(encoding='utf-8-sig'))
    if config.get('schema_version') != 1 or not isinstance(config.get('game_servers'), dict):
        raise ValueError('Érvénytelen server.json.')
    manager = load_manager(root)
    stop = threading.Event();handlers = {}
    if threading.current_thread() is threading.main_thread():
        for name in ('SIGTERM', 'SIGINT', 'SIGHUP'):
            sig = getattr(signal, name, None)
            if sig is not None: handlers[sig] = signal.signal(sig, lambda *_: stop.set())
    def check_stop():
        if stop.is_set(): raise InterruptedError('A telepítés megszakadt; visszaállítás szükséges.')
    try:
        with helpers.locked(build), helpers.locked(root), manager.supervisor_lock():
            pending_deployment(root, helpers)
            stopped(root, manager, config)
            configuration_hash = helpers.digest(root / 'server.json')
            programs = artifacts(build, request['configuration'], helpers)
            target = root / 'bin'
            for name in PROGRAMS:
                helpers.no_links(target / name)
                if (target / name).exists() and not (target / name).is_file():
                    raise ValueError('Program helyén nem fájl található: ' + name)
            required = sum(path.stat().st_size for path in programs.values())
            required += sum((target / name).stat().st_size for name in PROGRAMS if (target / name).is_file())
            if shutil.disk_usage(root).free < required + 8 * 1024 * 1024:
                raise ValueError('Nincs elég szabad hely a telepítéshez és a biztonsági mentéshez.')
            folder = root / 'run/deployments' / request['id'];helpers.no_links(folder)
            folder.mkdir(mode=0o700, parents=True, exist_ok=False)
            helpers.emit('Telepítési napló és mentés: ' + str(folder))
            staged, backup = folder / 'staging', folder / 'backup'
            staged.mkdir(mode=0o700);backup.mkdir(mode=0o700)
            record = dict(status='preparing', build=str(build), configuration=request['configuration'], files={})
            installed = []
            try:
                for name, original in programs.items():
                    helpers.emit('Másolás és SHA-256 ellenőrzés: ' + name)
                    check_stop();expected = helpers.digest(original)
                    shutil.copy2(original, staged / name);os.chmod(staged / name, 0o755)
                    if helpers.digest(staged / name) != expected: raise ValueError('A buildfájl másolás közben megváltozott: ' + name)
                    record['files'][name] = dict(sha256=expected, source=str(original),
                        previous_sha256=helpers.digest(target / name) if (target / name).is_file() else None)
                helpers.emit('Az előkészített programok konfiguráció- és Lua-ellenőrzése…')
                validate_programs(staged, root, config)
                check_stop();stopped(root, manager, config)
                for name in PROGRAMS:
                    if (target / name).is_file():
                        shutil.copy2(target / name, backup / name)
                        if helpers.digest(backup / name) != record['files'][name]['previous_sha256']:
                            raise ValueError('A mentés közben megváltozott a célfájl: ' + name)
                if helpers.digest(root / 'server.json') != configuration_hash:
                    raise ValueError('A szerver konfigurációja közben megváltozott; próbáld újra.')
                if getattr(helpers,'backup_before_deploy',None):
                    record['automatic_backup']=helpers.backup_before_deploy(root)
                record['status'] = 'installing';journal(folder / 'result.json', record)
                target.mkdir(exist_ok=True)
                for name in PROGRAMS:
                    check_stop()
                    current = helpers.digest(target / name) if (target / name).is_file() else None
                    if current != record['files'][name]['previous_sha256']:
                        raise ValueError('A célfájl közben megváltozott: ' + name)
                    os.replace(staged / name, target / name);installed.append(name)
                helpers.emit('A telepített programok ellenőrzése…')
                validate_programs(target, root, config)
                if helpers.digest(root / 'server.json') != configuration_hash:
                    raise ValueError('A szerver konfigurációja telepítés közben megváltozott.')
                for name in PROGRAMS:
                    if helpers.digest(target / name) != record['files'][name]['sha256']:
                        raise ValueError('A telepített program ellenőrzőösszege eltér: ' + name)
                check_stop();record['status'] = 'success';journal(folder / 'result.json', record)
            except BaseException as error:
                helpers.emit('A telepítés sikertelen; az elvégzett fájlcserék visszaállítása…')
                errors = []
                for name in reversed(installed):
                    try:
                        if (backup / name).is_file():
                            temporary = target / (name + '.rollback-' + request['id'])
                            shutil.copy2(backup / name, temporary);os.replace(temporary, target / name)
                        else: (target / name).unlink(missing_ok=True)
                    except OSError: errors.append(name)
                record['status'] = 'rollback-failed' if errors else 'rolled-back' if installed else 'failed'
                record['error'] = redacted(str(error), config)
                record['rollback_errors'] = errors
                journal(folder / 'result.json', record)
                if errors: raise RuntimeError('A visszaállítás nem teljes: %s. Biztonsági mentés: %s' % (', '.join(errors), backup)) from None
                raise
            helpers.emit('AURIGA_INSTALL_SUCCESS files=4 destination=%s backup=%s' % (target, backup))
    finally:
        for sig, handler in handlers.items(): signal.signal(sig, handler)
