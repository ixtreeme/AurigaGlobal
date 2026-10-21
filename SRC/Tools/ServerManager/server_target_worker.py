"""Private package RPC, delivered through verified SSH stdin. Standard library only."""
import base64
import contextlib
import io
import json
import os
from pathlib import Path, PurePosixPath
import re
import secrets
import subprocess
import sys
import time
import types

MAX_FILE = 8 * 1024 * 1024
ACTIONS = {'prepare', 'check', 'start', 'stop', 'reload-quests', 'apply-quests', 'database-check', 'setup-db', 'stop-db'}


def relative_path(value):
    path = PurePosixPath(value)
    if (not isinstance(value, str) or not path.parts or path.is_absolute() or '..' in path.parts or
            '\\' in value or any(ord(c) < 32 for c in value)):
        raise ValueError('Érvénytelen relatív fájlútvonal.')
    return path.as_posix()


def no_links(path):
    for part in (path, *path.parents):
        if part.is_symlink() or (hasattr(part, 'is_junction') and part.is_junction()):
            raise ValueError('Linkelt csomag vagy fájl nem kezelhető távolról.')


def load_support(request):
    for name in ('backend', 'editor_checks'):
        module = types.ModuleType(name)
        module.__file__ = '<auriga-' + name + '>'
        sys.modules[name] = module
        exec(compile(request['support'][name], module.__file__, 'exec'), module.__dict__)
    return sys.modules['backend']


def bytes_packet(api, root, relative):
    relative = relative_path(relative)
    no_links(root / relative)
    document = api.Document(root, relative, allow_invalid_json=True)
    return dict(relative=document.relative, data=base64.b64encode(document.original).decode('ascii'), sha256=document.digest)


def inventory(api, root):
    return dict(data=api.file_list(root), quests=api.file_list(root, True), logs=api.log_files(root))


def status(api, root):
    state = api.describe(root)
    jobs = root / 'run/manager-jobs'
    manager = api.load_manager(root)
    # The detached start process may still be checking files/SQL before it
    # creates supervisor.json. Report that phase without guessing from SSH.
    for path in sorted(jobs.glob('remote-start-*.json'), reverse=True)[:8]:
        try:
            record = api.strict_json(path.read_text(encoding='utf-8'))
            if manager.owned(record['identity']):
                state['starting'] = not state['running']
                if state['starting']: state['phase'] = 'Indítás…'
                break
        except (OSError, ValueError, KeyError, TypeError):
            pass
    return state


def start_detached(api, root, arguments):
    manager = api.load_manager(root)
    state = status(api, root)
    if state['running'] or state.get('starting'):
        raise ValueError('A kiválasztott távoli szerver már fut vagy indul.')
    directory = root / 'run/manager-jobs'
    no_links(directory)
    directory.mkdir(parents=True, exist_ok=True, mode=0o700)
    identity = str(time.time_ns()) + '-' + secrets.token_hex(8)
    logfile = directory / ('remote-start-' + identity + '.log')
    command = [sys.executable, '-B', str(root / 'manage.py'), 'start', *arguments]
    kwargs = dict(cwd=root, stdin=subprocess.DEVNULL, stderr=subprocess.STDOUT, close_fds=True)
    if os.name == 'nt': kwargs['creationflags'] = subprocess.CREATE_NO_WINDOW | subprocess.CREATE_NEW_PROCESS_GROUP
    else: kwargs['start_new_session'] = True
    with logfile.open('xb') as log:
        process = subprocess.Popen(command, stdout=log, **kwargs)
    time.sleep(.1)
    record = manager.identity(process.pid)
    if not record:
        raise ValueError(api.redact(api.tail(logfile), root)[-8000:] or 'A távoli felügyelő nem indult el.')
    manager.save_json(directory / ('remote-start-' + identity + '.json'), dict(identity=record, log=logfile.relative_to(root).as_posix()))
    return dict(output='A távoli indítás elindult. A felügyelő az SSH bontása után is fut.\nNapló: ' + logfile.relative_to(root).as_posix(), detached=True)


def dispatch(api, root, request):
    operation, args = request['operation'], request.get('arguments', {})
    if operation == 'connect':
        return dict(state=status(api, root), inventory=inventory(api, root), document=bytes_packet(api, root, 'server.json'))
    if operation == 'status': return dict(state=status(api, root))
    if operation == 'inventory': return inventory(api, root)
    if operation == 'read': return bytes_packet(api, root, args['relative'])
    if operation == 'save':
        relative = relative_path(args['relative'])
        no_links(root / relative)
        raw = base64.b64decode(args['data'], validate=True)
        if len(raw) > MAX_FILE: raise ValueError('Túl nagy mentendő fájl.')
        message = api.save_document(root, relative, raw, args['expected_hash'])
        return dict(output=api.redact(message, root), document=bytes_packet(api, root, relative))
    if operation == 'backups':
        relative = relative_path(args['relative'])
        return [p.relative_to(root).as_posix() for p in api.backup_files(root, relative)][:2000]
    if operation == 'restore':
        relative = relative_path(args['relative'])
        backup = root / relative_path(args['backup'])
        no_links(backup)
        raw = api.restore_payload(root, relative, backup)
        if len(raw) > MAX_FILE: raise ValueError('Túl nagy biztonsági másolat.')
        return dict(data=base64.b64encode(raw).decode('ascii'))
    if operation == 'log':
        relative = relative_path(args['relative'])
        if relative not in api.log_files(root): raise ValueError('Ez a fájl nem megnyitható naplóként.')
        no_links(root / relative)
        return dict(text=api.redact(api.tail(root / relative), root))
    if operation == 'action':
        argv = args['argv']
        if not isinstance(argv, list) or not argv or argv[0] not in ACTIONS:
            raise ValueError('Ismeretlen távoli szerverművelet.')
        if argv[0] == 'start':
            rest = argv[1:]
            if len(rest) % 2 or any(rest[i] != '--channel' or not str(rest[i+1]).isdigit() for i in range(0, len(rest), 2)):
                raise ValueError('Hibás csatornakiválasztás.')
            return start_detached(api, root, rest)
        if len(argv) != 1: raise ValueError('Nem megengedett műveleti paraméter.')
        manager = api.load_manager(root)
        if argv[0] == 'setup-db' and not status(api, root).get('can_setup'):
            raise ValueError('Ezen a célgépen nincs saját Windowsos adatbázis-előkészítés. Az SQL-kapcsolatok a Beállítások oldalon szerkeszthetők.')
        stream = io.StringIO()
        old = sys.argv
        try:
            sys.argv = [str(root / 'manage.py'), *argv]
            with contextlib.redirect_stdout(stream), contextlib.redirect_stderr(stream):
                code = manager.main()
        finally: sys.argv = old
        output = api.redact(stream.getvalue(), root)[-160000:]
        if code: raise ValueError(output or 'A távoli művelet sikertelen.')
        return dict(output=output)
    raise ValueError('Ismeretlen csomagművelet.')


def main(encoded):
    sys.dont_write_bytecode = True
    os.umask(0o077)
    request = json.loads(base64.b64decode(encoded, validate=True))
    identity = request.get('id', '')
    response = dict(schema_version=1, id=identity, root=request.get('root'), target=request.get('target'))
    api = root = None
    try:
        if request.get('schema_version') != 1 or not re.fullmatch('[a-f0-9]{24}', identity):
            raise ValueError('Ismeretlen kérésformátum.')
        root = Path(request['root'])
        if not root.is_absolute() or '..' in root.parts or root == Path(root.anchor):
            raise ValueError('Teljes távoli szervercsomag-útvonal szükséges.')
        no_links(root)
        api = load_support(request)
        root = api.package_root(root)
        if getattr(api.load_manager(root), 'PACKAGE_CONTROL_VERSION', 0) < 2:
            raise ValueError('A távoli manage.py frissítést igényel. A friss szervercsomag manage.py fájlját másold a profilban megadott szervermappába.')
        response.update(ok=True, result=dispatch(api, root, request))
    except Exception as error:
        message = str(error)
        if api and root:
            try: message = api.redact(message, root)
            except Exception: message = type(error).__name__
        response.update(ok=False, error=message[:160000])
    print('AURIGA_TARGET_RESULT ' + json.dumps(response, ensure_ascii=True), flush=True)
