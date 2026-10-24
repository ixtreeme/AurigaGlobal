"""File transactions and process inspection for the Qt6 localhost manager.

This module has no Qt dependency. SQL is controlled only by explicit manage.py actions.
"""
import contextlib
from concurrent.futures import ThreadPoolExecutor
import copy
import datetime
import hashlib
import importlib.util
import json
import math
import os
from pathlib import Path
import secrets
import shutil
import subprocess
import sys
import tempfile
import re
import time
import crash_tools
import backup_tools


class EditorError(RuntimeError):
    pass


def strict_json(text):
    def pairs(values):
        result = {}
        for key, value in values:
            if key in result:
                raise EditorError('Ismétlődő JSON-kulcs: ' + key)
            result[key] = value
        return result
    def invalid(_):
        raise EditorError('A JSON nem tartalmazhat NaN vagy Infinity értéket.')
    def number(text):
        value = float(text)
        if not math.isfinite(value):
            raise EditorError('A JSON-szám túl nagy; végtelen értéket eredményez.')
        return value
    try:
        return json.loads(text, object_pairs_hook=pairs, parse_constant=invalid, parse_float=number)
    except json.JSONDecodeError as error:
        raise EditorError('Hibás JSON: %d. sor, %d. oszlop.' % (error.lineno, error.colno)) from None
    except (ValueError, RecursionError) as error:
        raise EditorError('A JSON nem dolgozható fel: túl nagy szám vagy túl mély szerkezet.') from error


def sha(data):
    return hashlib.sha256(data).hexdigest()


def secret_field(key):
    key = str(key).lower()
    return any(part in key for part in ('password', 'interserver_key', 'token', 'secret'))


def merge(base, overlay):
    result = copy.deepcopy(base)
    for key, value in overlay.items():
        result[key] = merge(result[key], value) if isinstance(value, dict) and isinstance(result.get(key), dict) else copy.deepcopy(value)
    return result


def value_at(document, path):
    for key in path:
        document = document[key]
    return document


def load_manager(root):
    spec = importlib.util.spec_from_file_location('_auriga_package_manager', root / 'manage.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def package_root(path):
    root = Path(path).resolve()
    if not (root / '.auriga-package.json').is_file() or not (root / 'manage.py').is_file():
        raise EditorError('Válaszd az összeállított Auriga szervercsomag mappáját.')
    document = strict_json((root / 'server.json').read_text(encoding='utf-8-sig'))
    if not isinstance(document, dict) or document.get('schema_version') != 1 or not isinstance(document.get('game_servers'), dict):
        raise EditorError('Nem megfelelő server.json formátum.')
    return root


def editable_path(root, relative):
    relative = Path(relative)
    path = (root / relative).resolve()
    if relative.is_absolute() or not path.is_relative_to(root) or path == root:
        raise EditorError('A fájlnak a szervercsomagban kell maradnia.')
    rel = path.relative_to(root)
    if rel.as_posix() == 'server.json':
        return path
    if rel.parts[0] != 'data':
        raise EditorError('Itt csak a server.json és a data szöveges állományai szerkeszthetők.')
    suffix = path.suffix.lower()
    lua_state = rel.as_posix().startswith('data/quest/object/state/')
    if suffix not in ('.json', '.lua', '.quest', '.txt', '.start', '.script', '.when') and not lua_state and rel.as_posix() != 'data/quest/quest_list':
        raise EditorError('Ez az állomány nem szerkeszthető szövegként.')
    return path


class Document:
    def __init__(self, root, relative, allow_invalid_json=False):
        self.root = package_root(root)
        self.path = editable_path(self.root, relative)
        self.relative = self.path.relative_to(self.root).as_posix()
        self.load_bytes(self.path.read_bytes(), allow_invalid_json)

    @classmethod
    def from_bytes(cls, relative, raw, allow_invalid_json=False):
        """A remote document has no local filesystem path or local package."""
        value = cls.__new__(cls)
        value.root = None
        value.relative = relative
        value.path = Path(relative)
        value.load_bytes(raw, allow_invalid_json)
        return value

    def load_bytes(self, raw, allow_invalid_json):
        self.original = raw
        if len(self.original) > 8 * 1024 * 1024 or b'\x00' in self.original:
            raise EditorError('Ez a fájl túl nagy vagy bináris állomány.')
        self.digest = sha(self.original)
        if self.original.startswith(b'\xef\xbb\xbf'):
            self.encoding = 'utf-8-sig'
        else:
            try:
                self.original.decode('utf-8')
                self.encoding = 'utf-8'
            except UnicodeDecodeError:
                self.encoding = 'latin1'
        self.newline = '\r\n' if b'\r\n' in self.original else '\n'
        self.text = self.original.decode(self.encoding).replace('\r\n', '\n')
        self.is_json = self.path.suffix.lower() == '.json'
        self.value = None
        self.parse_error = None
        if self.is_json:
            try:
                self.value = strict_json(self.text)
            except EditorError as error:
                if not allow_invalid_json:
                    raise
                self.parse_error = str(error)

    def encoded(self, text):
        if self.is_json:
            strict_json(text)
        try:
            return text.replace('\r\n', '\n').replace('\n', self.newline).encode(self.encoding)
        except UnicodeEncodeError:
            raise EditorError('A szöveg nem írható vissza az eredeti %s kódolással.' % self.encoding) from None


def pending_deployment(root):
    for path in sorted((root / 'run/deployments').glob('*/result.json')):
        try:
            if strict_json(path.read_text(encoding='utf-8'))['status'] not in ('installing', 'rollback-failed'):
                continue
        except (OSError, ValueError, KeyError, TypeError):
            pass
        return path
    return None


def sql_client():
    for name in ('mariadb', 'mysql'):
        found = shutil.which(name)
        if found: return found
        for directory in ('/usr/local/bin', '/usr/bin'):
            path = Path(directory) / name
            if path.is_file() and os.access(path, os.X_OK): return str(path)
    return None


def sql_connections(config):
    """Mirror native common/default/instance overrides; deduplicate credentials privately."""
    common = config.get('common', {}).get('sql', {})
    sections = [('database', merge({'sql': {key:common[key] for key in ('player','account','common') if key in common}}, config['database']))]
    sections.append(('login', {'sql': {'account': merge(common.get('account', {}), config['login'].get('account_sql', {}))}}))
    for name, core in config['game_servers'].items():
        base = {'sql': {key:common[key] for key in ('player','common','log') if key in common}}
        sections.append((name, merge(merge(base, config.get('game_defaults', {})), core)))
    connections = {}
    for instance, section in sections:
        for role, connection in section.get('sql', {}).items():
            value = {key:connection.get(key, default) for key, default in
                     (('host',''),('port',3306),('user',''),('password',''),('database',''))}
            if value['port'] == 0: value['port'] = 3306
            if (any(not isinstance(value[key], str) or '\x00' in value[key] for key in ('host','user','password','database'))
                    or not value['host'] or not value['database'] or type(value['port']) is not int or not 1 <= value['port'] <= 65535):
                raise EditorError('Érvénytelen SQL-kapcsolat: ' + instance + '/' + role)
            identity = tuple(value[key] for key in ('host','port','user','password','database'))
            connections.setdefault(identity, dict(connection=value, roles=[]))['roles'].append(instance + '/' + role)
    if not connections or len(connections) > 64: raise EditorError('Az SQL-kapcsolatok száma 1–64 legyen.')
    return list(connections.values())


def probe_sql(binary, entry):
    connection = entry['connection']
    row = dict(roles=entry['roles'], host=connection['host'], port=connection['port'], database=connection['database'])
    def quoted(value):
        return '"' + str(value).replace('\\','\\\\').replace('"','\\"').replace('\n','\\n').replace('\r','\\r').replace('\t','\\t') + '"'
    try:
        with tempfile.TemporaryDirectory(prefix='auriga-sql-status-') as directory:
            options = Path(directory) / 'client.cnf'
            descriptor = os.open(options, os.O_CREAT | os.O_EXCL | os.O_WRONLY, 0o600)
            with os.fdopen(descriptor, 'w', encoding='utf-8') as handle:
                handle.write('[client]\n' + '\n'.join(key+'='+quoted(value) for key,value in connection.items())+'\n')
            result = subprocess.run([binary, '--defaults-file='+str(options), '--protocol=tcp', '--connect-timeout=2',
                                     '--default-character-set=utf8mb4', '--batch', '--skip-column-names'],
                                    input='SELECT 1;\n', capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=3,
                                    creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)
        if result.returncode == 0 and result.stdout.strip() == '1': return dict(row, status='available', message='Kapcsolat és adatbázis-hozzáférés rendben.')
        match = re.search(r'\bERROR\s+(\d+)\b', result.stderr)
        code = int(match[1]) if match else None
        messages = {1044:'Nincs jogosultság az adatbázishoz.',1045:'Hibás SQL-felhasználó, jelszó vagy hozzáférési szabály.',
                    1049:'A beállított adatbázis nem létezik.',2002:'Az SQL-végpont nem érhető el.',
                    2003:'Az SQL-végpont nem érhető el.',2005:'Az SQL-kiszolgáló neve nem oldható fel.',
                    2013:'Az SQL-kapcsolat megszakadt vagy időtúllépés történt.'}
        return dict(row, status='unavailable' if code in (2002,2003,2005,2013) else 'error', code=code,
                    message=messages.get(code,'Az SQL-ellenőrzés sikertelen.'))
    except subprocess.TimeoutExpired:
        return dict(row, status='unavailable', message='Az SQL-kapcsolat ellenőrzésének időkorlátja lejárt.')
    except (OSError, UnicodeError):
        return dict(row, status='unknown', message='Az SQL-kliens nem indítható.')


def external_sql_status(root, config, force=False):
    """Read-only SELECT 1 checks. The private, secret-free cache expires after 30 seconds."""
    key = sha(json.dumps(config, sort_keys=True, ensure_ascii=True).encode())
    cache = root / 'run/sql-status.json'
    safe_cache = not any(path.is_symlink() or (hasattr(path,'is_junction') and path.is_junction()) for path in (cache,*cache.parents))
    if safe_cache and not force and cache.is_file() and cache.stat().st_size <= 128*1024:
        try:
            value = strict_json(cache.read_text(encoding='utf-8'))
            if (value['version'] == 1 and value['key'] == key and 0 <= time.time()-value['result']['checked_at'] < 30
                    and value['result']['status'] in ('available','partial','unavailable','error','unknown')):
                return value['result']
        except (EditorError, OSError, ValueError, KeyError, TypeError): pass
    result = dict(status='unknown', checked_at=time.time(), connections=[], message='')
    binary = sql_client()
    if not binary:
        result['message'] = 'Az ellenőrzéshez MariaDB parancssori kliens szükséges a célgépen.'
    else:
        try: entries = sql_connections(config)
        except EditorError as error: result.update(status='error', message=str(error))
        except (AttributeError, KeyError, TypeError, ValueError):
            result.update(status='error', message='Az SQL-kapcsolatok konfigurációja hibás.')
        else:
            with ThreadPoolExecutor(max_workers=min(8,len(entries))) as pool:
                rows = list(pool.map(lambda entry: probe_sql(binary,entry), entries))
            statuses = {row['status'] for row in rows}
            result.update(connections=rows, status='available' if statuses == {'available'} else
                          'partial' if 'available' in statuses else 'error' if 'error' in statuses else
                          'unknown' if 'unknown' in statuses else 'unavailable')
    if safe_cache:
        try: load_manager(root).save_json(cache, dict(version=1,key=key,result=result))
        except OSError: pass
    return result


def describe(root):
    root = package_root(root)
    manager = load_manager(root)
    config = strict_json((root / 'server.json').read_text(encoding='utf-8-sig'))
    states, running, state = {}, False, {}
    retained = root / 'run/supervisor.json'
    phase = 'Leállítva'
    if retained.is_file():
        try:
            state = manager.read_json(retained)
        except FileNotFoundError:
            state = {}
        supervisor_live = manager.owned(state.get('supervisor'))
        phase = {'starting': 'Indulás', 'listening': 'Fut', 'stopping': 'Leállás'}.get(state.get('phase'), 'Ismeretlen')
        for child in state.get('children', []):
            states[child['name']] = manager.owned(child.get('identity'))
        running = supervisor_live or any(states.values())
        if not running:
            phase = 'Leállítva (korábbi állapotfájl)'
        elif not supervisor_live:
            phase = 'Felügyelet nélkül; leállítható'
    rows = []
    for name in ['database', *config['game_servers'], 'login']:
        if name == 'database':
            section, channel = config['database'], '—'
            port = section.get('network', {}).get('bind_port', 5300)
        elif name == 'login':
            section, channel = config['login'], '—'
            port = section.get('port', 0)
        else:
            section = merge(config.get('game_defaults', {}), config['game_servers'][name])
            channel = section.get('server', {}).get('channel', 0)
            port = section.get('network', {}).get('port', 0)
        rows.append(dict(name=name, channel=channel, port=port, running=states.get(name, False)))
    db_record = root / 'run/mariadb/process.json'
    external = os.name != 'nt'
    db_live = not external and db_record.is_file() and manager.owned(manager.read_json(db_record))
    pending = pending_deployment(root)
    backup_pending = backup_tools.pending(root)
    if pending and not running:
        phase = 'Befejezetlen buildtelepítés: ' + pending.relative_to(root).as_posix()
    if backup_pending and not running:
        phase = 'Befejezetlen visszaállítás — Mentések oldal'
    return dict(config=config, rows=rows, running=running, phase=phase, database_running=db_live,
                crashes=crash_tools.summary(root), crash_monitor_available=bool(getattr(manager, 'CRASH_MONITOR_VERSION', 0)),
                crash_monitor_active=bool(state.get('crash_monitor')) if retained.is_file() else False,
                database_initialized=(root / 'run/mariadb/data').is_dir(),
                database_kind='external' if external else 'managed',
                can_start=not pending and not backup_pending and ((external and hasattr(manager, 'external_database_check')) or (root / 'run/mariadb/data').is_dir()),
                backup_restore_pending=backup_pending,
                deployment_pending=pending.relative_to(root).as_posix() if pending else None,
                can_setup=os.name == 'nt' and (root / 'db/backup/manifest.json').is_file(),
                reload_supported=(root / 'quest_reload.py').is_file() and hasattr(manager, 'apply_quests'),
                core_control=hasattr(manager, 'control_cores'),
                hotreload=bool(running and hasattr(manager, 'quest_reload_status') and manager.quest_reload_status()['available']))


def file_list(root, quests=False):
    root = package_root(root)
    base = root / ('data/quest' if quests else 'data')
    result = []
    for path in sorted(base.rglob('*')):
        if not path.is_file() or path.is_symlink():
            continue
        if not quests and ('quest' in path.relative_to(base).parts or path.suffix != '.json'):
            continue
        relative = path.relative_to(root).as_posix()
        try:
            editable_path(root, relative)
        except EditorError:
            continue
        result.append(relative)
        if len(result) > 20000:
            raise EditorError('Túl sok szerkeszthető fájl a csomagban.')
    return result


def log_files(root):
    root = package_root(root)
    candidates = [root / 'preflight.log']
    candidates += sorted(p for p in (root / 'run').rglob('*') if p.is_file() and
                         (p.suffix == '.log' or p.name in ('syserr', 'syslog')))
    return [p.relative_to(root).as_posix() for p in candidates if p.is_file() and not p.is_symlink() and
            p.resolve().is_relative_to(root)][:2000]


def hotreload_file(relative):
    """Only resources captured by a quest generation may change while online."""
    path = Path(relative).as_posix()
    if not path.startswith('data/') or path in ('data/settings.lua', 'data/restart_city_pos.lua'):
        return False
    if any(part in ('object', 'pre_qc', '.git', '.plastic') for part in Path(path).parts):
        return False
    return (path.endswith(('.lua', '.quest')) or path in
            ('data/quest/quest_list', 'data/quest/questnpc.json', 'data/quest/questcategory.json'))


def redact(text, root):
    values = []
    def visit(value):
        if isinstance(value, dict):
            for key, child in value.items():
                if secret_field(key) and isinstance(child, str) and child:
                    values.append(child)
                else:
                    visit(child)
        elif isinstance(value, list):
            for child in value:
                visit(child)
    for file in ('server.json', 'run/supervisor.json', 'run/mariadb/admin.json'):
        try:
            visit(strict_json((root / file).read_text(encoding='utf-8-sig')))
        except (OSError, EditorError):
            pass
    for value in sorted(set(values), key=len, reverse=True):
        text = text.replace(value, '••••••')
    return text


def native(root, argv):
    result = subprocess.run([str(v) for v in argv], cwd=root, capture_output=True, text=True,
                            encoding='utf-8', errors='replace', timeout=90,
                            creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)
    output = redact(result.stdout + result.stderr, root)
    if result.returncode:
        raise EditorError(output.strip() or 'A natív ellenőrző elutasította a módosítást.')
    return output


def validate_candidate(root, relative, candidate):
    original = editable_path(root, relative)
    if original.name == 'server.json' and original.parent == root:
        config = strict_json(candidate.read_text(encoding='utf-8-sig'))
        if not isinstance(config, dict) or not isinstance(config.get('game_servers'), dict):
            raise EditorError('A game_servers objektum kötelező.')
        output = ''
        for instance in ['database', *config['game_servers'], 'login']:
            program = 'Database' if instance == 'database' else 'LoginServer' if instance == 'login' else 'GameServer'
            exe = root / 'bin' / (program + ('.exe' if os.name == 'nt' else ''))
            output += native(root, [exe, '--config', candidate, '--instance', instance, '--check-config'])
        # Keep the packaged supervisor's working-directory ownership contract too.
        for instance in ['database', *config['game_servers'], 'login']:
            section = config[instance] if instance in ('database', 'login') else config['game_servers'][instance]
            directory = (root / section.get('working_directory', 'run/' + instance)).resolve()
            if not directory.is_relative_to((root / 'run').resolve()):
                raise EditorError('A futási mappáknak a csomag run mappájában kell maradniuk.')
        return output
    relative = Path(relative).as_posix()
    from editor_checks import language_for
    if language_for(relative) == 'quest':
        checker = root / 'bin' / ('ServerPackageCheck.exe' if os.name == 'nt' else 'ServerPackageCheck')
        return native(root, [checker, '--check-quest', candidate])
    runtime_lua = original.suffix in ('.start', '.script', '.when') or relative.startswith('data/quest/object/state/')
    runtime_lua = runtime_lua or (original.suffix == '.lua' and not relative.startswith(('data/quest/Quest/', 'data/quest/pre_qc/')))
    if original.suffix != '.json' and not runtime_lua:
        return 'Szöveg ellenőrizve.\n'
    if runtime_lua:
        checker = root / 'bin' / ('ServerPackageCheck.exe' if os.name == 'nt' else 'ServerPackageCheck')
        return native(root, [checker, '--check-lua', candidate])
    with tempfile.TemporaryDirectory(prefix='auriga-manager-check-') as directory:
        stage = Path(directory)
        (stage / 'quest').mkdir()
        (stage / 'quest/quest_list').write_text('', encoding='utf-8')
        for name in ('questnpc.json', 'questcategory.json'):
            shutil.copy2(root / 'data/quest' / name, stage / 'quest' / name)
        target = stage / Path(relative).relative_to('data')
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(candidate, target)
        checker = root / 'bin' / ('ServerPackageCheck.exe' if os.name == 'nt' else 'ServerPackageCheck')
        return native(root, [checker, stage])


def save_document(root, relative, payload, expected_hash):
    root = package_root(root)
    document = Document(root, relative, allow_invalid_json=True)
    if document.digest != expected_hash:
        raise EditorError('A fájlt közben más módosította. Töltsd újra a mentés előtt.')
    if document.is_json:
        strict_json(payload.decode(document.encoding))
    if len(payload) > 8 * 1024 * 1024 or b'\x00' in payload:
        raise EditorError('A mentendő szöveg túl nagy vagy bináris.')
    manager = load_manager(root)
    initial = describe(root)
    live = initial['running'] and initial.get('hotreload') and hotreload_file(document.relative)
    lock = manager.named_lock('quest-publication') if live else manager.supervisor_lock()
    with lock:
        state = describe(root)
        if state['running'] and not (live and state.get('hotreload') and hotreload_file(document.relative)):
            raise EditorError('Ez a fájl csak leállított szervernél menthető. Futás közben a hotreloadot támogató core-ok questforrásai szerkeszthetők.')
        if document.relative == 'server.json' and state.get('database_kind', 'managed') == 'managed' and state['database_running']:
            raise EditorError('A konfiguráció mentése előtt a helyi MariaDB-t is állítsd le.')
        candidate = document.path.with_name('.' + document.path.name + '.edit-' + secrets.token_hex(6))
        descriptor = os.open(candidate, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
        try:
            with os.fdopen(descriptor, 'wb') as handle:
                handle.write(payload)
            output = validate_candidate(root, document.relative, candidate)
            if sha(document.path.read_bytes()) != expected_hash:
                raise EditorError('A fájlt az ellenőrzés alatt más módosította. Töltsd újra.')
            stamp = datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S.%fZ-') + secrets.token_hex(3)
            backup = root / 'run/editor-backups' / stamp / document.relative
            if not backup.resolve().is_relative_to((root / 'run').resolve()):
                raise EditorError('Érvénytelen mentési útvonal.')
            backup.parent.mkdir(parents=True, exist_ok=False)
            descriptor = os.open(backup, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
            with os.fdopen(descriptor, 'wb') as handle:
                handle.write(document.original)
            manager.replace_file(candidate, document.path)
            return output + 'MENTVE: ' + document.relative + '\nBiztonsági másolat: ' + backup.relative_to(root).as_posix() + '\n'
        finally:
            candidate.unlink(missing_ok=True)


def tail(path, limit=160000):
    with path.open('rb') as handle:
        handle.seek(0, os.SEEK_END)
        size = handle.tell()
        handle.seek(max(0, size - limit))
        return handle.read().decode('utf-8', errors='replace')


def backup_files(root, relative):
    editable_path(root, relative)
    base = root / 'run/editor-backups'
    return sorted((p for p in base.glob('*/' + Path(relative).as_posix())
                   if p.is_file() and p.resolve().is_relative_to(base.resolve())), reverse=True)


def restore_payload(root, relative, backup):
    backup = Path(backup).resolve()
    if backup not in [p.resolve() for p in backup_files(root, relative)]:
        raise EditorError('Ehhez a fájlhoz nem tartozik ilyen biztonsági másolat.')
    return backup.read_bytes()


def worker(root, logfile, arguments):
    root = package_root(root)
    logfile = Path(logfile).resolve()
    if not logfile.is_relative_to((root / 'run').resolve()):
        raise EditorError('A kezelő naplója csak a run mappába írható.')
    logfile.parent.mkdir(parents=True, exist_ok=True)
    with logfile.open('w', encoding='utf-8', buffering=1) as output:
        with contextlib.redirect_stdout(output), contextlib.redirect_stderr(output):
            try:
                if arguments[0] == 'save':
                    _, relative, payload, expected = arguments
                    payload = Path(payload).resolve()
                    if not payload.is_relative_to((root / 'run/editor-pending').resolve()):
                        raise EditorError('Érvénytelen szerkesztői munkafájl.')
                    print(save_document(root, relative, payload.read_bytes(), expected), end='')
                else:
                    manager = load_manager(root)
                    sys.argv = [str(root / 'manage.py'), *arguments]
                    code = manager.main()
                    return code
                return 0
            except (EditorError, OSError, ValueError, KeyError, TypeError, subprocess.TimeoutExpired) as error:
                print('HIBA:', redact(str(error), root))
                return 1
