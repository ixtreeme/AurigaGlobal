"""Self-contained localhost server/owned MariaDB supervisor. See README.md."""
import argparse
import contextlib
import ctypes
import datetime
import hashlib
import importlib.util
import json
import os
import re
from pathlib import Path
import secrets
import shutil
import signal
import socket
import ssl
import subprocess
import sys
import tempfile
import time
from types import SimpleNamespace

ROOT = Path(__file__).resolve().parent
PACKAGE_CONTROL_VERSION = 2
CRASH_MONITOR_VERSION = 1


class StopRequested(Exception):
    pass


def read_json(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))


def replace_file(source, target):
    # Windows readers may briefly open a state/request file without DELETE
    # sharing. Preserve atomic publication and retry only that transient error.
    deadline = time.monotonic() + 2
    while True:
        try:
            os.replace(source, target)
            return
        except PermissionError:
            if os.name != 'nt' or time.monotonic() >= deadline: raise
            time.sleep(0.01)


def save_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + '.new-' + secrets.token_hex(4))
    descriptor = os.open(temporary, os.O_CREAT | os.O_EXCL | os.O_WRONLY, 0o600)
    with os.fdopen(descriptor, 'w', encoding='utf-8') as handle:
        json.dump(value, handle, indent=2)
        handle.flush()
        os.fsync(handle.fileno())
    replace_file(temporary, path)
    if os.name != 'nt':
        descriptor = os.open(path.parent, os.O_RDONLY)
        try:
            os.fsync(descriptor)
        finally:
            os.close(descriptor)


def identity(pid):
    """PID alone is never enough to identify a process after a restart."""
    if os.name == 'nt':
        from ctypes import wintypes
        kernel = ctypes.WinDLL('kernel32', use_last_error=True)
        kernel.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
        kernel.OpenProcess.restype = wintypes.HANDLE
        kernel.CloseHandle.argtypes = [wintypes.HANDLE]
        kernel.GetProcessTimes.argtypes = [wintypes.HANDLE] + [ctypes.POINTER(wintypes.FILETIME)] * 4
        kernel.GetExitCodeProcess.argtypes = [wintypes.HANDLE, ctypes.POINTER(wintypes.DWORD)]
        kernel.QueryFullProcessImageNameW.argtypes = [wintypes.HANDLE, wintypes.DWORD, wintypes.LPWSTR, ctypes.POINTER(wintypes.DWORD)]
        handle = kernel.OpenProcess(0x1000, False, pid)
        if not handle: return None
        try:
            exit_code = wintypes.DWORD()
            if not kernel.GetExitCodeProcess(handle, ctypes.byref(exit_code)) or exit_code.value != 259: return None
            times = [wintypes.FILETIME() for _ in range(4)]
            size, name = wintypes.DWORD(32768), ctypes.create_unicode_buffer(32768)
            if not kernel.GetProcessTimes(handle, *(ctypes.byref(t) for t in times)): return None
            if not kernel.QueryFullProcessImageNameW(handle, 0, name, ctypes.byref(size)): return None
            return dict(pid=pid, created=(times[0].dwHighDateTime << 32) | times[0].dwLowDateTime,
                        image=os.path.normcase(name.value))
        finally:
            kernel.CloseHandle(handle)
    try:
        stat = Path('/proc/%d/stat' % pid).read_text().rsplit(')', 1)[1].split()
        if stat[0] == 'Z': return None
        return dict(pid=pid, created=stat[19], image=os.readlink('/proc/%d/exe' % pid))
    except OSError:
        if not sys.platform.startswith('freebsd'):
            return None
    # FreeBSD does not require procfs. Keep the PID, start time and executable
    # together, using ps(1)'s stable, header-free fields in the C locale.
    try:
        result = subprocess.run(['ps', '-ww', '-p', str(int(pid)), '-o', 'lstart=', '-o', 'state=', '-o', 'comm='],
            capture_output=True, text=True, timeout=3, env=dict(os.environ, LC_ALL='C'))
        fields = result.stdout.strip().split(None, 6)
        if result.returncode or len(fields) != 7 or fields[5].startswith('Z'):
            return None
        return dict(pid=pid, created=' '.join(fields[:5]), image=fields[6])
    except (OSError, ValueError, subprocess.TimeoutExpired):
        return None


def owned(record):
    return bool(record and identity(record['pid']) == record)


def config():
    if not (ROOT / '.auriga-package.json').is_file(): raise RuntimeError('Not an owned Auriga package directory')
    return read_json(ROOT / 'server.json')


def executable(name):
    return ROOT / 'bin' / (name + ('.exe' if os.name == 'nt' else ''))


def native(argv, input=None, timeout=90):
    result = subprocess.run([str(v) for v in argv], input=input, capture_output=True, text=True, encoding='utf-8', timeout=timeout,
                            creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)
    return result


def file_check():
    cfg = config()
    result = native([sys.executable, '-B', ROOT / 'run_server.py', '--config', ROOT / 'server.json', '--bin-dir', ROOT / 'bin', '--check-config'])
    print(result.stdout, end=''); print(result.stderr, end='', file=sys.stderr)
    if result.returncode: raise RuntimeError('Native configuration check failed')
    committed = quest_module().current(SimpleNamespace(**globals()))
    runtime = Path(committed['snapshot']) if committed else ROOT / 'data'
    extra = ['--quest-runtime', runtime] if committed else []
    result = native([executable('ServerPackageCheck'), ROOT / 'data', *extra])
    print(result.stdout, end=''); print(result.stderr, end='', file=sys.stderr)
    if result.returncode: raise RuntimeError('Native data/Lua check failed')
    result = native([executable('ServerPackageCheck'), '--check-libraries', runtime])
    print(result.stdout, end=''); print(result.stderr, end='', file=sys.stderr)
    if result.returncode: raise RuntimeError('Quest library bootstrap/dialog check failed')
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    cert, key = cfg['common']['tls']['cert_file'], cfg['common']['tls']['key_file']
    context.load_cert_chain(str(ROOT / cert), str(ROOT / key))
    print('TLS_KEY_PAIR_OK')
    return cfg


def free_port(port):
    with socket.socket() as probe:
        if os.name == 'nt': probe.setsockopt(socket.SOL_SOCKET, socket.SO_EXCLUSIVEADDRUSE, 1)
        probe.bind(('127.0.0.1', port))


@contextlib.contextmanager
def named_lock(name):
    if name not in ('supervisor', 'quest-publication'):
        raise RuntimeError('Unknown runtime lock')
    path = ROOT / 'run' / (name + '.lock')
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open('a+b') as handle:
        handle.seek(0, 2)
        if handle.tell() == 0: handle.write(b'0'); handle.flush()
        handle.seek(0)
        try:
            if os.name == 'nt':
                import msvcrt
                msvcrt.locking(handle.fileno(), msvcrt.LK_NBLCK, 1)
            else:
                import fcntl
                fcntl.flock(handle, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except OSError:
            raise RuntimeError('Supervisor startup is already in progress or running' if name == 'supervisor' else 'Quest publication/edit already in progress') from None
        try:
            yield
        finally:
            handle.seek(0)
            if os.name == 'nt':
                msvcrt.locking(handle.fileno(), msvcrt.LK_UNLCK, 1)
            else:
                fcntl.flock(handle, fcntl.LOCK_UN)


@contextlib.contextmanager
def supervisor_lock():
    with named_lock('supervisor'):
        yield


def quest_module():
    spec = importlib.util.spec_from_file_location('_auriga_quest_reload', ROOT / 'quest_reload.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def quest_reload_status():
    if not (ROOT / 'quest_reload.py').is_file():
        return dict(available=False, cores=[])
    return quest_module().status(SimpleNamespace(**globals()))


def reload_quests():
    return quest_module().reload(SimpleNamespace(**globals()))


def apply_quests():
    return quest_module().apply_offline(SimpleNamespace(**globals()))


def mariadb_bin(explicit=None):
    if explicit:
        folder = Path(explicit).resolve()
    else:
        installed = list(Path(os.environ.get('ProgramFiles', 'C:/Program Files')).glob('MariaDB */bin'))
        installed.sort(reverse=True)
        candidates = [ROOT / 'tools/mariadb/bin', *installed]
        folder = next((p for p in candidates if (p / 'mariadbd.exe').is_file()), None)
        if folder is None: raise RuntimeError('MariaDB binaries not found; supply --mariadb-bin')
    suffix = '.exe' if os.name == 'nt' else ''
    if any(not (folder / (name + suffix)).is_file() for name in ('mariadbd','mariadb','mariadb-admin')):
        raise RuntimeError('MariaDB executables are missing')
    return folder


def db_folder():
    config()
    return ROOT / 'run/mariadb'


def credentials(password):
    path = db_folder() / 'admin.cnf'
    # Passwords generated by this tool contain no option-file escape characters.
    descriptor = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600)
    with os.fdopen(descriptor, 'w', encoding='utf-8') as handle:
        handle.write('[client]\nuser=root\npassword=' + password + '\n')


def sql(text, admin=True):
    cfg, folder = config(), db_folder()
    local = read_json(folder / 'owner.json')
    binary = Path(local['binary_dir']) / ('mariadb.exe' if os.name == 'nt' else 'mariadb')
    argv = [binary]
    if admin: argv.append('--defaults-extra-file=' + str(folder / 'admin.cnf'))
    argv += ['--protocol=tcp', '--host=127.0.0.1', '--port=' + str(local['port']), '--connect-timeout=2', '--default-character-set=utf8mb4', '--batch', '--skip-column-names']
    result = native(argv, input=text, timeout=120)
    if result.returncode:
        (folder / 'client-error.log').write_text(result.stderr, encoding='utf-8')
        raise RuntimeError('Local SQL command failed; see run/mariadb/client-error.log')
    return result.stdout


def verify_datadir():
    owner = read_json(db_folder() / 'owner.json')
    if any(c['port'] != owner['port'] or c['host'] != '127.0.0.1' for c in config()['common']['sql'].values()):
        raise RuntimeError('server.json SQL endpoint differs from the owned local MariaDB')
    value = sql('SELECT @@datadir;').strip()
    if os.path.normcase(str(Path(value).resolve())) != os.path.normcase(str((db_folder() / 'data').resolve())):
        raise RuntimeError('SQL endpoint belongs to a different data directory; operation refused')


def start_db():
    folder = db_folder()
    owner = read_json(folder / 'owner.json')
    record = read_json(folder / 'process.json') if (folder / 'process.json').is_file() else None
    if owned(record):
        verify_datadir(); return False
    free_port(owner['port'])
    executable = Path(owner['binary_dir']) / ('mariadbd.exe' if os.name == 'nt' else 'mariadbd')
    with (folder / 'server.log').open('ab') as log:
        process = subprocess.Popen([str(executable), '--defaults-file=' + str(folder / 'data/my.ini'), '--bind-address=127.0.0.1', '--console'],
                                   stdout=log, stderr=subprocess.STDOUT, creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)
    record = identity(process.pid)
    if not record: raise RuntimeError('Cannot identify newly created MariaDB process')
    save_json(folder / 'process.json', record)
    deadline = time.monotonic() + 45
    while time.monotonic() < deadline:
        if process.poll() is not None: raise RuntimeError('Local MariaDB exited; see run/mariadb/server.log')
        try:
            verify_datadir(); print('LOCAL_MARIADB_READY port=' + str(owner['port'])); return True
        except RuntimeError:
            time.sleep(0.2)
    # The stored identity is retained for a safe retry/explicit stop-db.
    raise RuntimeError('Local MariaDB readiness deadline exceeded; see run/mariadb/server.log')


def stop_db():
    folder = db_folder()
    stack = ROOT / 'run/supervisor.json'
    if stack.is_file() and any(owned(child['identity']) for child in read_json(stack)['children']):
        raise RuntimeError('Stop the owned server stack before stopping its local MariaDB')
    if not (folder / 'process.json').exists(): return
    record = read_json(folder / 'process.json')
    if not owned(record):
        print('Local MariaDB is already stopped'); return
    verify_datadir()
    owner = read_json(folder / 'owner.json')
    binary = Path(owner['binary_dir']) / ('mariadb-admin.exe' if os.name == 'nt' else 'mariadb-admin')
    result = native([binary, '--defaults-extra-file=' + str(folder / 'admin.cnf'), '--protocol=tcp', '--host=127.0.0.1', '--port=' + str(owner['port']), 'shutdown'])
    if result.returncode: raise RuntimeError('Local MariaDB did not accept shutdown')
    deadline = time.monotonic() + 20
    while owned(record) and time.monotonic() < deadline: time.sleep(0.1)
    if owned(record): raise RuntimeError('Local MariaDB has not exited; its identity was retained')
    print('LOCAL_MARIADB_STOPPED')


def init_db(explicit=None):
    cfg, folder = config(), db_folder()
    if (folder / 'data').exists(): raise RuntimeError('Database data directory already exists; initialization refused')
    ports = {connection['port'] for connection in cfg['common']['sql'].values()}
    if len(ports) != 1 or any(c['host'] != '127.0.0.1' for c in cfg['common']['sql'].values()):
        raise RuntimeError('Local bootstrap requires one loopback SQL endpoint')
    port = ports.pop(); free_port(port)
    binaries = mariadb_bin(explicit)
    installer = binaries / ('mariadb-install-db.exe' if os.name == 'nt' else 'mariadb-install-db')
    if os.name != 'nt': raise RuntimeError('This local database bootstrap currently targets Windows')
    folder.mkdir(parents=True, exist_ok=True)
    save_json(folder / 'owner.json', dict(package=str(ROOT), binary_dir=str(binaries), port=port))
    credentials('')
    result = native([installer, '--datadir=' + str(folder / 'data'), '--port=' + str(port)], timeout=120)
    (folder / 'initialize.log').write_text(result.stdout + result.stderr, encoding='utf-8')
    if result.returncode: raise RuntimeError('Local MariaDB initialization failed; see initialize.log')
    start_db()
    try:
        for name in ('schema.sql','seed_core.sql','live_locale.sql','motion.sql'):
            path = ROOT / 'db' / name
            if path.is_file(): sql(path.read_text(encoding='utf-8'))
        grants = []
        for name, connection in cfg['common']['sql'].items():
            user, password, database = connection['user'], connection['password'], connection['database']
            if not all(c.isalnum() or c == '_' for c in user + database): raise RuntimeError('Unsupported SQL identifier')
            if not all(c.isalnum() or c in '_-' for c in password): raise RuntimeError('Unsupported generated SQL password')
            grants += ["CREATE USER IF NOT EXISTS '%s'@'127.0.0.1' IDENTIFIED BY '%s';" % (user,password),
                       "GRANT ALL ON `%s`.* TO '%s'@'127.0.0.1';" % (database,user)]
        sql('\n'.join(grants))
        password = secrets.token_urlsafe(40)
        sql("ALTER USER 'root'@'localhost' IDENTIFIED BY '%s';" % password)
        credentials(password)
        save_json(folder / 'initialized.json', dict(version=1, schema_sha256=hashlib.sha256((ROOT / 'db/schema.sql').read_bytes()).hexdigest()))
        print('LOCAL_DATABASE_INITIALIZED; reference/player data import is required')
    finally:
        stop_db()


def database_check():
    folder = db_folder()
    if os.name != 'nt':
        return external_database_check()
    if (folder / 'import-in-progress.json').exists(): raise RuntimeError('A database import is incomplete; inspect its private import journal')
    if not (folder / 'initialized.json').is_file(): raise RuntimeError('Run init-db before database-check')
    start_db()
    verify_datadir()
    rows = sql((ROOT / 'db/check_ready.sql').read_text(encoding='utf-8'))
    print(rows, end='')
    if any(line and not line.endswith('\tREADY') for line in rows.splitlines()):
        raise RuntimeError('Database reference data is missing or invalid; import the adapted SQL backup first')


def external_database_check():
    """Read-only checks for a FreeBSD/system MariaDB, using server.json roles.

    Never starts, stops, initializes or imports the external SQL service.
    Credentials go into private temporary option files, never process arguments.
    """
    cfg = config()
    binary = shutil.which('mariadb') or shutil.which('mysql') or next(
        (str(Path(directory) / name) for name in ('mariadb','mysql') for directory in ('/usr/local/bin','/usr/bin')
         if (Path(directory) / name).is_file() and os.access(Path(directory) / name, os.X_OK)), None)
    if not binary:
        raise RuntimeError('Install a MariaDB command-line client on the server to check external SQL connections')
    postfix = cfg.get('common', {}).get('table_postfix', '')
    if not isinstance(postfix, str) or not re.fullmatch('[A-Za-z0-9_]*', postfix):
        raise RuntimeError('Unsupported SQL table postfix')
    queries = {name: [] for name in cfg['common']['sql']}
    readiness = ROOT / 'db/check_ready.sql'
    if not readiness.is_file():
        raise RuntimeError('The package is missing db/check_ready.sql')
    for statement in re.split(r'\bUNION\s+ALL\b', readiness.read_text(encoding='utf-8'), flags=re.I):
        statement = re.sub(r'(?m)^\s*--[^\n]*$', '', statement).strip().rstrip(';')
        if (not statement.upper().startswith('SELECT ') or ';' in statement or
                re.search(r'\b(INTO|OUTFILE|DUMPFILE|UPDATE|INSERT|DELETE|DROP|ALTER|CREATE|CALL|LOAD|REPLACE|TRUNCATE|SLEEP|BENCHMARK)\b', statement, re.I)):
            raise RuntimeError('Database readiness checks must contain only read-only SELECT queries')
        match = re.search(r'\bFROM\s+(common|player)\.([A-Za-z0-9_]+)', statement, re.I)
        if not match: raise RuntimeError('Unsupported database readiness query')
        role, table = match.group(1).lower(), match.group(2)
        suffix = postfix if role == 'player' and table not in ('shop', 'shop_item') else ''
        query = statement[:match.start()] + 'FROM `' + table + suffix + '`' + statement[match.end():]
        queries[role].append(query.strip().rstrip(';') + ';')
    def quoted(value):
        text = str(value)
        if '\x00' in text: raise RuntimeError('Invalid SQL connection setting')
        return '"' + text.replace('\\', '\\\\').replace('"', '\\"').replace('\n', '\\n').replace('\r', '\\r').replace('\t', '\\t') + '"'
    with tempfile.TemporaryDirectory(prefix='auriga-sql-check-') as directory:
        for name, connection in cfg['common']['sql'].items():
            options = Path(directory) / 'client.cnf'
            descriptor = os.open(options, os.O_CREAT | os.O_TRUNC | os.O_WRONLY, 0o600)
            with os.fdopen(descriptor, 'w', encoding='utf-8') as handle:
                handle.write('[client]\n' + '\n'.join(key + '=' + quoted(connection[key]) for key in ('host', 'port', 'user', 'password', 'database')) + '\n')
            argv = [binary, '--defaults-file=' + str(options), '--protocol=tcp', '--connect-timeout=5',
                    '--default-character-set=utf8mb4', '--batch', '--skip-column-names']
            result = native(argv, input='SELECT 1;\n' + '\n'.join(queries.get(name, [])), timeout=30)
            if result.returncode:
                log = ROOT / 'run/sql-check-error.log'
                log.parent.mkdir(parents=True, exist_ok=True)
                descriptor = os.open(log, os.O_CREAT | os.O_TRUNC | os.O_WRONLY, 0o600)
                with os.fdopen(descriptor, 'w', encoding='utf-8') as handle: handle.write(result.stderr)
                raise RuntimeError('External SQL check failed for role ' + name + '; see run/sql-check-error.log')
            lines = result.stdout.splitlines()
            if not lines or lines[0] != '1' or any(not line.endswith('\tREADY') for line in lines[1:] if line):
                raise RuntimeError('External SQL reference data is missing or invalid for role ' + name)
            print('EXTERNAL_SQL_READY ' + name)


def load_backup_import():
    spec = importlib.util.spec_from_file_location('_auriga_backup_import', ROOT / 'db/backup_import.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def setup_database(backup=None, explicit=None):
    backup = backup or ROOT / 'db/backup'
    if (db_folder() / 'data').exists():
        raise RuntimeError('The local database already exists; setup will not overwrite it')
    module = load_backup_import()
    module.validate_backup(backup, ROOT / 'db/schema.sql')
    init_db(explicit)
    module.import_backup(SimpleNamespace(**globals()), backup)
    try:
        database_check()
    finally:
        stop_db()
    print('LOCAL_DATABASE_SETUP_COMPLETE')


def request_child_stop(process, directory, token):
    if process.poll() is not None: return
    request = directory / '.auriga-stop'
    temporary = directory / ('.auriga-stop-' + secrets.token_hex(4))
    temporary.write_text(str(process.pid) + ' ' + token, encoding='ascii')
    replace_file(temporary, request)


def select_instances(cfg, channels, all_channels=False):
    selected = set(channels or [1])
    games = [name for name, core in cfg['game_servers'].items()
             if all_channels or core['server']['channel'] in selected or core['server']['channel'] == 99]
    available = {core['server']['channel'] for core in cfg['game_servers'].values()}
    if selected - available: raise RuntimeError('Unknown selected channel')
    return ['database'] + games + ['login']


def instance_ports(cfg, name):
    if name == 'database': return [cfg['database']['network']['bind_port']]
    if name == 'login': return [cfg['login']['port']]
    network = cfg['game_servers'][name]['network']
    return [network['port'], network['p2p_port']]


def reachable_host(bind_ip):
    """The address to reach a listener bound to bind_ip: itself, or loopback for all interfaces."""
    if not bind_ip or bind_ip == '0.0.0.0': return '127.0.0.1'
    return '::1' if bind_ip == '::' else bind_ip


def instance_endpoints(cfg, name):
    """(host, port) pairs each server listens on, as server.json binds them.

    A core bound to the VPS address does not answer on 127.0.0.1, so readiness and the
    free-port check use the configured bind_ip. LoginListener binds every IPv4 interface."""
    if name == 'login': return [('127.0.0.1', port) for port in instance_ports(cfg, name)]
    if name == 'database':
        bind_ip = cfg['database'].get('network', {}).get('bind_ip', '0.0.0.0')
    else:
        bind_ip = cfg['game_servers'][name]['network'].get(
            'bind_ip', cfg.get('game_defaults', {}).get('network', {}).get('bind_ip', ''))
    host = reachable_host(bind_ip)
    return [(host, port) for port in instance_ports(cfg, name)]


def free_endpoint(name, host, port):
    with socket.socket(socket.AF_INET6 if ':' in host else socket.AF_INET) as probe:
        if os.name == 'nt': probe.setsockopt(socket.SOL_SOCKET, socket.SO_EXCLUSIVEADDRUSE, 1)
        try: probe.bind((host, port))
        except OSError as error:
            raise RuntimeError('%s cannot listen on %s:%d: the port is in use or the address is not on this machine (%s)'
                               % (name, host, port, error)) from None


def instance_directory(cfg, name):
    section = cfg[name] if name in ('database', 'login') else cfg['game_servers'][name]
    path = (ROOT / section.get('working_directory', 'run/' + name)).resolve()
    if not path.is_relative_to((ROOT / 'run').resolve()):
        raise RuntimeError('Managed working_directory must stay under this package/run')
    return path


def prepare_runtime(cfg, names=None):
    names = list(names) if names is not None else ['database'] + list(cfg['game_servers']) + ['login']
    created = 0
    def seed(original, target):
        nonlocal created
        if not target.resolve().is_relative_to((ROOT / 'run').resolve()):
            raise RuntimeError('Runtime seed target must stay under this package/run')
        if original.is_file() and not target.exists():
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(original, target)
            created += 1
    for name in names:
        directory = instance_directory(cfg, name)
        directory.mkdir(parents=True, exist_ok=True)
        for original in (ROOT / 'runtime_defaults').glob('*'):
            if original.is_file(): seed(original, directory / original.name)
        if name not in ('database', 'login'):
            mark = directory / 'mark'
            if not mark.resolve().is_relative_to((ROOT / 'run').resolve()):
                raise RuntimeError('Runtime mark directory must stay under this package/run')
            mark.mkdir(exist_ok=True)
            for original in (ROOT / 'runtime_defaults/mark').glob('*'):
                if original.is_file(): seed(original, mark / original.name)
            symbol = ROOT / 'runtime_defaults/instances' / name / 'guild_symbol.tga'
            seed(symbol, directory / symbol.name)
    return dict(instances=names, seeded_files=created)


def prepare_package():
    # This workflow completes the server files without starting SQL or game services.
    with supervisor_lock():
        cfg = file_check()
        state_path = ROOT / 'run/supervisor.json'
        if state_path.is_file() and any(owned(c['identity']) for c in read_json(state_path)['children']):
            raise RuntimeError('Stop the owned server stack before preparing its runtime files')
        runtime = prepare_runtime(cfg)
        report = dict(status='SERVER_FILES_READY', checked_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),
                      database_stage='DEFERRED', services_started=False, **runtime)
        save_json(ROOT / 'server-files-ready.json', report)
    print('SERVER_FILES_READY instances=%d seeded_files=%d' % (len(runtime['instances']), runtime['seeded_files']))
    return report


def enable_child_cores():
    """Only change this supervisor's inherited limit, never global sysctls."""
    if os.name == 'nt': return
    try:
        import resource
        soft, hard = resource.getrlimit(resource.RLIMIT_CORE)
        if soft != hard: resource.setrlimit(resource.RLIMIT_CORE, (hard, hard))
        if hard == 0: print('CRASH_MONITOR: core dump disabled by hard resource limit', flush=True)
    except (OSError, ValueError) as error:
        print('CRASH_MONITOR: core limit could not be enabled: ' + str(error), flush=True)


def crash_launch(program, directory, fingerprints):
    stat = program.stat()
    key = (str(program), stat.st_size, stat.st_mtime_ns)
    if key not in fingerprints:
        with program.open('rb') as stream:
            fingerprints[key] = hashlib.file_digest(stream, 'sha256').hexdigest()
    return dict(program=str(program), sha256=fingerprints[key], started_ns=time.time_ns(),
                before={p.name: (p.stat().st_size, p.stat().st_mtime_ns) for p in directory.iterdir()
                        if p.is_file() and not p.is_symlink() and
                        (p.name == 'core' or '.core' in p.name or p.suffix in ('.dmp', '.mdmp'))})


def record_crash(name, process, directory, launch, phase):
    """Persist an unexpected exit before the normal sibling shutdown removes state."""
    code = process.poll()
    if code is None: return None
    event_id = datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S-') + secrets.token_hex(6)
    folder = ROOT / 'run/crashes' / event_id
    folder.mkdir(mode=0o700, parents=True)
    reason = ('Windows exception 0x%08X' % (code & 0xffffffff) if os.name == 'nt' and code & 0x80000000 else
              signal.Signals(-code).name if os.name != 'nt' and code < 0 and -code in signal.valid_signals() else
              'Unexpected exit (%d)' % code)
    event = dict(schema_version=1, id=event_id, timestamp=datetime.datetime.now(datetime.timezone.utc).isoformat(),
                 instance=name, pid=process.pid, exit_code=code, reason=reason, phase=phase,
                 program=launch['program'], binary_sha256=launch['sha256'], binary=None, cores=[], errors=[])
    save_json(folder / 'event.json', event)
    try:
        for original in list(directory.iterdir()):
            if not original.is_file() or original.is_symlink(): continue
            stat = original.stat()
            if not (original.name == 'core' or '.core' in original.name or original.suffix in ('.dmp', '.mdmp')): continue
            if (stat.st_size, stat.st_mtime_ns) == tuple(launch['before'].get(original.name, ())): continue
            if stat.st_mtime_ns < launch['started_ns']: continue
            with original.open('rb') as source: header = source.read(18)
            elf_core = (header[:4] == b'\x7fELF' and len(header) >= 18 and
                        int.from_bytes(header[16:18], 'little' if header[5] == 1 else 'big') == 4)
            if not elf_core and header[:4] != b'MDMP': continue
            target = folder / original.name
            os.replace(original, target)
            if os.name != 'nt': target.chmod(0o600)
            event['cores'].append(target.relative_to(ROOT).as_posix())
        for filename in ('console.log', 'syserr', 'syslog'):
            original = directory / filename
            if original.is_file() and not original.is_symlink():
                with original.open('rb') as stream:
                    stream.seek(max(0, original.stat().st_size - 160000))
                    (folder / (filename + '.tail.log')).write_bytes(stream.read(160000))
        program = Path(launch['program'])
        with program.open('rb') as stream: current = hashlib.file_digest(stream, 'sha256').hexdigest()
        if current != launch['sha256']:
            event['errors'].append('Executable changed after launch; current binary was not archived.')
        else:
            binaries = ROOT / 'run/crashes/binaries' / current
            binaries.mkdir(mode=0o700, parents=True, exist_ok=True)
            target = binaries / program.name
            if not target.exists():
                temporary = target.with_name(target.name + '.new-' + event_id)
                try:
                    shutil.copy2(program, temporary)
                    with temporary.open('rb') as stream:
                        if hashlib.file_digest(stream, 'sha256').hexdigest() != current:
                            raise RuntimeError('Executable changed while archiving crash binary')
                    replace_file(temporary, target)
                finally: temporary.unlink(missing_ok=True)
            event['binary'] = target.relative_to(ROOT).as_posix()
            pdb = program.with_suffix('.pdb')
            if pdb.is_file() and not (binaries / pdb.name).exists(): shutil.copy2(pdb, binaries / pdb.name)
    except (OSError, ValueError, RuntimeError) as error:
        event['errors'].append(str(error))
    finally:
        save_json(folder / 'event.json', event)
    print('CRASH_RECORDED ' + event_id + ' ' + name + ' ' + reason, flush=True)
    return event


def supervise_owned(channels, all_channels=False):
    cfg = file_check()
    database_check()
    names = select_instances(cfg, channels, all_channels)
    state_file, stop_file = ROOT / 'run/supervisor.json', ROOT / 'run/stop.request'
    if state_file.exists():
        previous = read_json(state_file)
        if owned(previous['supervisor']) or any(owned(c['identity']) for c in previous['children']):
            raise RuntimeError('An owned stack is still running; use status/stop')
    for name in names:
        for host, port in instance_endpoints(cfg, name): free_endpoint(name, host, port)
    state_file.parent.mkdir(parents=True, exist_ok=True)
    token = secrets.token_hex(24)
    state = dict(supervisor=identity(os.getpid()), token=token, children=[], phase='starting', crash_monitor=CRASH_MONITOR_VERSION)
    children, shutdown_errors = [], []
    launches, fingerprints = {}, {}
    def detect_exit():
        exited = [(name, process, directory) for name, process, directory, _ in children if process.poll() is not None]
        for name, process, directory in exited:
            try: record_crash(name, process, directory, launches[name], state['phase'])
            except Exception as error: print('CRASH_MONITOR_ERROR ' + str(error), file=sys.stderr, flush=True)
        return bool(exited)
    try:
        save_json(state_file, state)
        prepare_runtime(cfg, names)
        enable_child_cores()
        for name in names:
            if stop_file.is_file() and stop_file.read_text(encoding='ascii').strip() == token:
                stop_file.unlink(); raise StopRequested()
            directory = instance_directory(cfg, name)
            child_token = secrets.token_hex(24)
            env = dict(os.environ, AURIGA_SHUTDOWN_TOKEN=child_token)
            program = executable('Database' if name == 'database' else 'LoginServer' if name == 'login' else 'GameServer')
            launches[name] = crash_launch(program, directory, fingerprints)
            with (directory / 'console.log').open('ab') as log:
                process = subprocess.Popen([str(program), '--config', str(ROOT / 'server.json'), '--instance', name],
                    cwd=directory, env=env, stdout=log, stderr=subprocess.STDOUT,
                    creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)
            children.append((name, process, directory, child_token))
            state['children'].append(dict(name=name, identity=identity(process.pid), token=child_token,
                                          directory=str(directory)))
            save_json(state_file, state)
            deadline = time.monotonic() + 90
            pending = set(instance_endpoints(cfg, name))
            while pending and time.monotonic() < deadline:
                if stop_file.is_file() and stop_file.read_text(encoding='ascii').strip() == token:
                    stop_file.unlink(); raise StopRequested()
                if detect_exit(): raise RuntimeError('A server exited during startup; see run/crashes and run/*/console.log')
                for endpoint in list(pending):
                    try:
                        with socket.create_connection(endpoint, timeout=0.2): pending.remove(endpoint)
                    except OSError: pass
                time.sleep(0.2)
            if pending:
                raise RuntimeError('Readiness deadline exceeded for %s: nothing listens on %s'
                                   % (name, ', '.join('%s:%d' % endpoint for endpoint in sorted(pending))))
            print('LISTENING ' + name, flush=True)
        state['phase'] = 'listening'; save_json(state_file, state)
        print('STACK_LISTENING; Ctrl+C or server.ps1 stop requests graceful shutdown', flush=True)
        while True:
            if stop_file.is_file() and stop_file.read_text(encoding='ascii').strip() == token:
                stop_file.unlink(); break
            if detect_exit(): raise RuntimeError('A server exited; see run/crashes; shutting down the remaining owned servers')
            time.sleep(0.25)
    except (KeyboardInterrupt, StopRequested):
        print('Graceful shutdown requested')
    finally:
        state['phase'] = 'stopping'; save_json(state_file, state)
        for name, process, directory, child_token in reversed(children):
            if name == 'database' and shutdown_errors:
                shutdown_errors.append(name)
                continue # Keep the cache alive while an unresponsive game can still write.
            request_child_stop(process, directory, child_token)
            try:
                process.wait(timeout=60)
                print('STOPPED ' + name, flush=True)
            except subprocess.TimeoutExpired:
                shutdown_errors.append(name)
        if not shutdown_errors:
            state_file.unlink(missing_ok=True)
        else:
            print('Shutdown not complete; process identities retained: ' + ', '.join(shutdown_errors), file=sys.stderr)


def supervise(channels, all_channels=False):
    with supervisor_lock():
        supervise_owned(channels, all_channels)


def stop():
    path = ROOT / 'run/supervisor.json'
    if not path.is_file(): print('No managed stack'); return
    state = read_json(path)
    if not owned(state['supervisor']):
        # The supervisor may have crashed. Retained PID+creation-time+image and
        # the per-child token still allow graceful recovery without killing by PID.
        cfg = config()
        for child in reversed(state['children']):
            if not owned(child['identity']): continue
            directory = instance_directory(cfg, child['name'])
            if directory != Path(child['directory']).resolve(): raise RuntimeError('Retained child directory differs from current config')
            request = directory / '.auriga-stop'
            request.write_text(str(child['identity']['pid']) + ' ' + child['token'], encoding='ascii')
            deadline = time.monotonic() + 60
            while owned(child['identity']) and time.monotonic() < deadline: time.sleep(0.2)
            if owned(child['identity']): raise RuntimeError('Owned child did not finish graceful recovery: ' + child['name'])
        path.unlink()
        print('Owned orphaned stack stopped gracefully'); return
    (ROOT / 'run/stop.request').write_text(state['token'], encoding='ascii')
    print('Graceful stop requested from the owning supervisor')


def status():
    config()
    path = ROOT / 'run/supervisor.json'
    if path.is_file():
        state = read_json(path)
        print('Supervisor:', 'running' if owned(state['supervisor']) else 'stale', state['phase'])
        for child in state['children']: print(child['name'] + ': ' + ('running' if owned(child['identity']) else 'stopped'))
    else: print('Server stack: stopped')
    path = db_folder() / 'process.json'
    print('Local MariaDB:', 'running' if path.exists() and owned(read_json(path)) else 'stopped/not initialized')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=['prepare','check','setup-db','init-db','import-db','start-db','stop-db','database-check','start','stop','status','reload-quests','quest-status','apply-quests'])
    parser.add_argument('--channel', type=int, action='append')
    parser.add_argument('--all-channels', action='store_true')
    parser.add_argument('--mariadb-bin')
    parser.add_argument('--backup', type=Path)
    args = parser.parse_args()
    try:
        if args.action == 'prepare': prepare_package()
        elif args.action == 'check': file_check()
        elif args.action == 'init-db': init_db(args.mariadb_bin)
        elif args.action == 'setup-db': setup_database(args.backup, args.mariadb_bin)
        elif args.action == 'import-db':
            if not args.backup: raise RuntimeError('import-db requires --backup <adapted backup directory>')
            load_backup_import().import_backup(SimpleNamespace(**globals()), args.backup)
        elif args.action == 'reload-quests': reload_quests()
        elif args.action == 'apply-quests': apply_quests()
        elif args.action == 'quest-status': print(json.dumps(quest_reload_status(), ensure_ascii=False))
        elif args.action == 'start-db': start_db()
        elif args.action == 'stop-db': stop_db()
        elif args.action == 'database-check': database_check()
        elif args.action == 'start': supervise(args.channel, args.all_channels)
        elif args.action == 'stop': stop()
        else: status()
        return 0
    except (RuntimeError, OSError, ValueError, KeyError, subprocess.TimeoutExpired) as error:
        # SQL values are never included in errors; subprocess SQL diagnostics stay in local files.
        print('SERVER_MANAGER_ERROR:', str(error) if isinstance(error, RuntimeError) else type(error).__name__, file=sys.stderr)
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
