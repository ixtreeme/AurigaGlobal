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
ACTIONS = {'prepare', 'check', 'start', 'stop', 'reload-quests', 'apply-quests', 'database-check', 'setup-db', 'stop-db',
           'start-cores', 'stop-cores'}


def core_selection(rest):
    # start-cores / stop-cores: --instance <game core> or --channel <number>, in pairs.
    return bool(rest) and len(rest) % 2 == 0 and all(
        (rest[i] == '--channel' and str(rest[i + 1]).isdigit()) or
        (rest[i] == '--instance' and isinstance(rest[i + 1], str) and re.fullmatch('[A-Za-z0-9][A-Za-z0-9_-]{0,63}', rest[i + 1]))
        for i in range(0, len(rest), 2))


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
    for name in ('crash_tools', 'backup_tools', 'backend', 'editor_checks'):
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


def latest_start(api, manager, root, state):
    jobs = root / 'run/manager-jobs'
    no_links(jobs)
    for path in sorted(jobs.glob('remote-start-*.json'), reverse=True)[:8]:
        try:
            if not re.fullmatch(r'remote-start-[0-9]+-[a-f0-9]{16}\.json', path.name): continue
            no_links(path)
            record = api.strict_json(path.read_text(encoding='utf-8'))
            logfile = path.with_suffix('.log')
            if record['log'] != logfile.relative_to(root).as_posix(): continue
            no_links(logfile)
            live = manager.owned(record['identity'])
            output = api.redact(api.tail(logfile), root) if logfile.is_file() else ''
            if live:
                phase = 'running' if state['phase'] == 'Fut' else 'stopping' if state['phase'] == 'Leállás' else 'starting'
            else:
                failed = 'SERVER_MANAGER_ERROR:' in output or 'Traceback (most recent call last):' in output
                stopped = 'STACK_LISTENING;' in output or 'Graceful shutdown requested' in output
                phase = 'stopped' if stopped and not failed else 'failed'
            return dict(id=path.stem, log=record['log'], status=phase,
                        output=output[-8000:] if phase == 'failed' else '')
        except (OSError, ValueError, KeyError, TypeError):
            continue
    return None


def status(api, root, force_sql=False):
    state = api.describe(root)
    if state['database_kind'] == 'external':
        state['database_status'] = api.external_sql_status(root, state['config'], force=force_sql)
        state['database_running'] = state['database_status']['status'] == 'available'
        state['database_initialized'] = state['database_running']
        state['can_start'] = state['can_start'] and state['database_running']
    manager = api.load_manager(root)
    # The detached start process may still be checking files/SQL before it
    # creates supervisor.json. Report that phase without guessing from SSH.
    job = state['start_job'] = latest_start(api, manager, root, state)
    state['starting'] = bool(job and job['status'] == 'starting')
    if state['starting']: state['phase'] = 'Indítás…'
    if (job and job['status'] == 'failed' and not state['running'] and
            not state.get('deployment_pending') and not state.get('backup_restore_pending')):
        state['phase'] = 'Indítás sikertelen'
    return state


def start_detached(api, root, arguments):
    api.backup_tools.assert_ready(root)
    manager = api.load_manager(root)
    pending = api.pending_deployment(root)
    if pending:
        raise ValueError('Befejezetlen buildtelepítés; indítás előtt ellenőrizd a mentést és a naplót: ' + str(pending))
    state = status(api, root)
    if state['running'] or state.get('starting'):
        raise ValueError('A kiválasztott távoli szerver már fut vagy indul.')
    directory = root / 'run/manager-jobs'
    no_links(directory)
    directory.mkdir(parents=True, exist_ok=True, mode=0o700)
    identity = str(time.time_ns()) + '-' + secrets.token_hex(8)
    logfile = directory / ('remote-start-' + identity + '.log')
    command = [sys.executable, '-B', '-u', str(root / 'manage.py'), 'start', *arguments]
    kwargs = dict(cwd=root, stdin=subprocess.DEVNULL, stderr=subprocess.STDOUT, close_fds=True)
    if os.name == 'nt': kwargs['creationflags'] = subprocess.CREATE_NO_WINDOW | subprocess.CREATE_NEW_PROCESS_GROUP
    else: kwargs['start_new_session'] = True
    with logfile.open('xb') as log:
        process = subprocess.Popen(command, stdout=log, **kwargs)
    time.sleep(.1)
    record = manager.identity(process.pid)
    manager.save_json(logfile.with_suffix('.json'), dict(identity=record, log=logfile.relative_to(root).as_posix()))
    if not record:
        raise ValueError(api.redact(api.tail(logfile), root)[-8000:] or 'A távoli felügyelő nem indult el.')
    return dict(output='A távoli indítás folyamatban. A fájlok és az SQL ellenőrzése után indulnak a core-ok.\nNapló: ' + logfile.relative_to(root).as_posix(), detached=True)


def dispatch(api, root, request):
    operation, args = request['operation'], request.get('arguments', {})
    if operation == 'connect':
        return dict(state=status(api, root, force_sql=True), inventory=inventory(api, root), document=bytes_packet(api, root, 'server.json'))
    if operation == 'status': return dict(state=status(api, root, force_sql=args.get('force_sql') is True))
    if operation == 'crashes': return api.crash_tools.inventory(root)
    if operation == 'crash-analyze':
        result = api.crash_tools.analyze(root, args['core'], args['binary'], args.get('expected'))
        result['report'] = api.redact(result['report'], root)
        return result
    if operation == 'backup-list': return api.backup_tools.inventory(root,api)
    if operation == 'backup-create': return api.backup_tools.summary(api.backup_tools.create(root,api,args['scopes'],comment=args.get('comment','')))
    if operation == 'backup-info': return api.backup_tools.archive_info(root,args['id'])
    if operation == 'backup-plan': return api.backup_tools.restore_plan(root,api,args['id'],args['scopes'])
    if operation == 'backup-restore': return api.backup_tools.restore(root,api,args['id'],args['scopes'],args['fingerprint'],args.get('preserve_connections',True))
    if operation == 'backup-policy': return api.backup_tools.save_policy(root,api,args['policy'],args.get('runner',''))
    if operation == 'backup-scheduled': return api.backup_tools.run_schedule(root,api)
    if operation == 'backup-import':
        if not re.fullmatch('[a-f0-9]{24}',args['transfer']):raise ValueError('Hibás feltöltésazonosító.')
        staged=root/'run/backups/incoming'/args['transfer']/'backup.tar.gz'
        no_links(staged)
        try:return api.backup_tools.import_archive(root,api,staged,args['sha256'])
        finally:
            staged.unlink(missing_ok=True)
            if staged.parent.is_dir():staged.parent.rmdir()
    if operation == 'backup-upload-prepare':
        identity=args['transfer']
        if not re.fullmatch('[a-f0-9]{24}',identity):raise ValueError('Hibás feltöltésazonosító.')
        path=root/'run/backups/incoming'/identity;no_links(path);path.mkdir(mode=0o700,parents=True,exist_ok=False)
        return dict(path=str(path/'backup.tar.gz'))
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
        if argv[0] in ('start-cores', 'stop-cores'):
            if not core_selection(argv[1:]): raise ValueError('Hibás core- vagy csatornakiválasztás.')
        elif len(argv) != 1: raise ValueError('Nem megengedett műveleti paraméter.')
        manager = api.load_manager(root)
        if argv[0] in ('start-cores', 'stop-cores') and not hasattr(manager, 'control_cores'):
            raise ValueError('A távoli manage.py nem ismeri a core-onkénti indítást. Másold fel a friss manage.py fájlt a szervermappába.')
        if argv[0] in ('start-cores', 'stop-cores') and not api.describe(root).get('core_control'):
            raise ValueError('A futó felügyelő nem támogatja a core-onkénti vezérlést. Állítsd le, majd indítsd újra a szervert a friss manage.py használatával.')
        if argv[0] == 'setup-db' and not status(api, root).get('can_setup'):
            raise ValueError('Ezen a célgépen nincs saját Windowsos adatbázis-előkészítés. Az SQL-kapcsolatok a Beállítások oldalon szerkeszthetők.')
        if argv[0] == 'stop-db' and api.describe(root)['database_kind'] == 'external':
            raise ValueError('A külső SQL-szolgáltatást az alkalmazás nem állítja le.')
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
