"""Portable package backups, verified restore journals and policy (stdlib only)."""
import contextlib
import datetime
import hashlib
import io
import json
import os
from pathlib import Path, PurePosixPath
import re
import secrets
import shutil
import signal
import subprocess
import tarfile
import tempfile
import threading
import time

SCOPES = ('config', 'data', 'tls', 'sql')
IDENTIFIER = r'[A-Za-z0-9_]{1,64}'
BACKUP_ID = r'[0-9]{8}T[0-9]{6}-[a-f0-9]{12}'
MAX_FILES = 100000


def windows():return os.name=='nt'


def digest(path):
    with Path(path).open('rb') as stream: return hashlib.file_digest(stream, 'sha256').hexdigest()


def no_links(path):
    path = Path(path)
    for part in (path, *path.parents):
        if part.is_symlink() or (hasattr(part,'is_junction') and part.is_junction()):
            raise ValueError('Linkelt mentési útvonal nem használható: '+str(path))
    return path


def store(root):
    path = no_links(Path(root)/'run/backups')
    path.mkdir(mode=0o700, parents=True, exist_ok=True)
    return path


def atomic_json(path, value):
    path = no_links(path)
    path.parent.mkdir(mode=0o700, parents=True, exist_ok=True)
    pending = path.with_name(path.name+'.new-'+secrets.token_hex(6))
    try:
        descriptor = os.open(pending, os.O_WRONLY|os.O_CREAT|os.O_EXCL, 0o600)
        with os.fdopen(descriptor,'w',encoding='utf-8') as output:
            json.dump(value,output,ensure_ascii=True,indent=2);output.flush();os.fsync(output.fileno())
        os.replace(pending,path)
    finally: pending.unlink(missing_ok=True)


def stamp(): return datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S-')+secrets.token_hex(6)


def summary(meta):
    return {key:meta[key] for key in ('id','created','reason','comment','scopes','archive_size','archive_sha256')}


def validate_meta(meta):
    if not isinstance(meta,dict) or meta.get('version')!=1 or not re.fullmatch(BACKUP_ID,str(meta.get('id',''))):
        raise ValueError('Hibás mentési leíró.')
    selected=scopes(meta.get('scopes'))
    datetime.datetime.fromisoformat(meta['created'])
    if meta.get('reason') not in ('manual','scheduled','before-restore','before-deploy') or not isinstance(meta.get('comment'),str):
        raise ValueError('Hibás mentéstípus vagy megjegyzés.')
    if not isinstance(meta.get('files'),dict) or len(meta['files'])>MAX_FILES or not isinstance(meta.get('sql'),list):
        raise ValueError('Hibás mentési fájllista.')
    for name,row in meta['files'].items():
        valid_relative(name)
        if (not isinstance(row,dict) or type(row.get('size')) is not int or row['size']<0 or
            not re.fullmatch('[a-f0-9]{64}',str(row.get('sha256',''))) or type(row.get('mode')) is not int or not 0<=row['mode']<=0o777):
            raise ValueError('Hibás mentési fájlmetaadat.')
        if name.startswith('files/'):
            if file_scope(name[6:]) not in selected:raise ValueError('Az archívum tartalma eltér a kijelölt összetevőktől.')
        elif not re.fullmatch(r'sql/[0-9]{3}\.sql',name) or 'sql' not in selected:raise ValueError('Hibás SQL-archívumtag.')
    sql_names=set()
    for group in meta['sql']:
        if (not isinstance(group,dict) or not isinstance(group.get('host'),str) or type(group.get('port')) is not int or
            not 1<=group['port']<=65535 or not isinstance(group.get('databases'),list) or not group['databases']):
            raise ValueError('Hibás SQL-mentési cél.')
        for name in group['databases']:
            if not isinstance(name,str) or not re.fullmatch(IDENTIFIER,name) or name.lower() in ('mysql','information_schema','performance_schema','sys'):
                raise ValueError('Tiltott SQL-adatbázis a mentésben.')
        if group.get('file') not in meta['files'] or group['file'] in sql_names:raise ValueError('Hibás SQL-mentési hivatkozás.')
        sql_names.add(group['file'])
    if sql_names!={name for name in meta['files'] if name.startswith('sql/')} or ('sql' in selected and not sql_names):
        raise ValueError('Hiányos SQL-mentés.')
    if 'config' in selected and 'files/server.json' not in meta['files']:raise ValueError('Hiányzó szerverkonfiguráció.')
    return meta


def scopes(value):
    if not isinstance(value,list) or not value or len(value)!=len(set(value)) or any(name not in SCOPES for name in value):
        raise ValueError('Válassz mentendő összetevőket.')
    return [name for name in SCOPES if name in value]


def defaults():
    return dict(version=1, enabled=False, hour=3, minute=0, keep=7,
                scopes=list(SCOPES), auto_before_deploy=False)


def validate_policy(value):
    if not isinstance(value,dict) or set(value)!=set(defaults()) or value.get('version')!=1:
        raise ValueError('Ismeretlen mentési beállítás.')
    if any(type(value[key]) is not bool for key in ('enabled','auto_before_deploy')):
        raise ValueError('Hibás mentési kapcsoló.')
    for key,limit in (('hour',23),('minute',59),('keep',365)):
        if type(value[key]) is not int or not 0<=value[key]<=limit:raise ValueError('Hibás mentési idő vagy megőrzési szám.')
    value=dict(value);value['scopes']=scopes(value['scopes']);return value


def policy(root):
    path=no_links(Path(root)/'run/backups/policy.json')
    return validate_policy(json.loads(path.read_text(encoding='utf-8'))) if path.is_file() else defaults()


def pending(root):
    for path in sorted((Path(root)/'run/backup-restores').glob('*/result.json')):
        no_links(path)
        try:
            value=json.loads(path.read_text(encoding='utf-8'))
            if value.get('status') in ('restoring','rollback-failed'):return dict(path=str(path),**value)
        except (OSError,ValueError):return dict(path=str(path),status='unknown')
    return None


def assert_ready(root):
    if pending(root):raise ValueError('Befejezetlen visszaállítás: előbb a Mentések oldalon állíts helyre egy ellenőrzött mentést.')


@contextlib.contextmanager
def durable():
    previous={}
    if threading.current_thread() is threading.main_thread():
        for name in ('SIGHUP','SIGTERM','SIGINT'):
            sig=getattr(signal,name,None)
            if sig is not None:previous[sig]=signal.signal(sig,signal.SIG_IGN)
    try:yield
    finally:
        for sig,handler in previous.items():signal.signal(sig,handler)


@contextlib.contextmanager
def job_lock(root,api,operation):
    folder=store(root);path=folder/'operation.lock'
    with path.open('a+b') as handle:
        handle.seek(0,2)
        if not handle.tell():handle.write(b'0');handle.flush()
        handle.seek(0)
        try:
            if os.name=='nt':
                import msvcrt
                msvcrt.locking(handle.fileno(),msvcrt.LK_NBLCK,1)
            else:
                import fcntl
                fcntl.flock(handle,fcntl.LOCK_EX|fcntl.LOCK_NB)
        except OSError:raise ValueError('Már fut mentési vagy visszaállítási művelet ezen a szerveren.') from None
        try:
            manager=api.load_manager(root)
            atomic_json(folder/'active.json',dict(operation=operation,identity=manager.identity(os.getpid()),started=time.time()))
            yield
        finally:
            (folder/'active.json').unlink(missing_ok=True)
            handle.seek(0)
            if os.name=='nt':msvcrt.locking(handle.fileno(),msvcrt.LK_UNLCK,1)
            else:fcntl.flock(handle,fcntl.LOCK_UN)


def valid_relative(name):
    if not isinstance(name,str) or '\\' in name or ':' in name or any(ord(c)<32 for c in name):raise ValueError('Hibás mentési fájlnév.')
    path=PurePosixPath(name)
    if not path.parts or path.is_absolute() or '..' in path.parts or str(path)!=name:
        raise ValueError('Érvénytelen relatív mentési útvonal.')
    return path


def file_scope(name):
    path=valid_relative(name)
    if name=='server.json':return 'config'
    if path.parts[0] in ('data','runtime_defaults') and len(path.parts)>1:return 'data'
    if path.parts[0]=='tls' and len(path.parts)>1:return 'tls'
    if (path.parts[0]=='run' and len(path.parts)>2 and
        not set(path.parts)&{'backups','backup-restores','crashes','mariadb','manager-updates','deployments'} and
        ('mark' in path.parts[2:-1] or path.name=='guild_symbol.tga')):return 'data'
    raise ValueError('Nem visszaállítható csomagfájl: '+name)


def package_files(root,selected,api,config=None):
    root=Path(root);result={};bases=[]
    if 'config' in selected:result['server.json']=no_links(root/'server.json')
    if 'data' in selected:
        bases.extend(root/name for name in ('data','runtime_defaults'))
        manager=api.load_manager(root);cfg=config if config is not None else manager.config()
        for name in cfg['game_servers']:
            directory=manager.instance_directory(cfg,name)
            bases.append(directory/'mark')
            path=directory/'guild_symbol.tga'
            if path.is_file():result[path.relative_to(root).as_posix()]=no_links(path)
    if 'tls' in selected:bases.append(root/'tls')
    for base in bases:
        no_links(base)
        if not base.exists():continue
        for directory,folders,files in os.walk(base,followlinks=False):
            for name in [*folders,*files]:no_links(Path(directory)/name)
            for name in files:
                path=Path(directory)/name
                if not path.is_file():raise ValueError('Nem szabályos mentendő fájl: '+str(path))
                relative=path.relative_to(root).as_posix();file_scope(relative);result[relative]=path
                if len(result)>MAX_FILES:raise ValueError('Túl sok mentendő fájl.')
    return dict(sorted(result.items()))


def validate_configuration(path):
    cfg=json.loads(Path(path).read_text(encoding='utf-8-sig'))
    if (not isinstance(cfg,dict) or cfg.get('schema_version')!=1 or
        any(not isinstance(cfg.get(key),dict) for key in ('common','database','login','game_servers'))):
        raise ValueError('A mentett server.json szerkezete hibás; a visszaállítás nem indítható.')
    sections=[cfg['common'],cfg['database'],cfg['login'],cfg.get('game_defaults',{}),*cfg['game_servers'].values()]
    for section in sections:
        if not isinstance(section,dict) or any(key in section and not isinstance(section[key],dict) for key in ('network','server','sql','account_sql')):
            raise ValueError('A mentett server.json valamelyik szekciója hibás.')
        directory=section.get('working_directory')
        if directory is not None and (not isinstance(directory,str) or valid_relative(directory).parts[0]!='run'):
            raise ValueError('A mentett futási könyvtárnak a csomag run mappájában kell maradnia.')


def sql_groups(config,api):
    groups={};seen={}
    for entry in api.sql_connections(config):
        conn=entry['connection'];name=conn['database']
        if not re.fullmatch(IDENTIFIER,name) or name.lower() in ('mysql','information_schema','performance_schema','sys'):
            raise ValueError('Nem menthető adatbázisnév: '+name)
        endpoint=(conn['host'],conn['port'],name)
        credentials=(conn['user'],conn['password'])
        if endpoint in seen and seen[endpoint]!=credentials:
            raise ValueError('Egy adatbázishoz eltérő SQL-felhasználók tartoznak. A mentéshez egységes mentési hozzáférés szükséges.')
        seen[endpoint]=credentials
        key=(conn['host'],conn['port'],*credentials)
        group=groups.setdefault(key,dict(connection=dict(conn),databases=[]))
        if name not in group['databases']:group['databases'].append(name)
    # A single global read lock covers all game schemas on each SQL endpoint.
    endpoints={}
    for key,group in groups.items():
        endpoint=key[:2]
        if endpoint in endpoints:raise ValueError('Az egy SQL-szerveren levő adatbázisok mentéséhez közös SQL-felhasználó szükséges.')
        endpoints[endpoint]=group
        group['databases'].sort()
    return sorted(groups.values(),key=lambda group:(group['connection']['host'],group['connection']['port'],group['databases']))


def find_sql(root,api):
    folder=Path(root)/'tools/mariadb/bin';suffix='.exe' if os.name=='nt' else ''
    client=folder/('mariadb'+suffix)
    if not client.is_file():client=Path(api.sql_client() or '')
    if not client.is_file():raise ValueError('A célgépen nincs mariadb kliensprogram.')
    dump=client.with_name('mariadb-dump'+suffix)
    if not dump.is_file():dump=Path(shutil.which('mariadb-dump') or '/usr/local/bin/mariadb-dump')
    if not dump.is_file():raise ValueError('A célgépen nincs mariadb-dump. Telepítsd a MariaDB klienseszközöket.')
    return client,dump


@contextlib.contextmanager
def sql_access(root,group,api):
    client,dump=find_sql(root,api);conn=group['connection']
    manager=api.load_manager(root)
    private=store(root)/'.private';no_links(private);private.mkdir(mode=0o700,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='sql-',dir=private) as directory:
        auth=None
        if os.name=='nt' and (Path(root)/'run/mariadb/owner.json').is_file():
            owner=manager.read_json(Path(root)/'run/mariadb/owner.json')
            if conn['host']=='127.0.0.1' and conn['port']==owner['port']:
                manager.verify_datadir()
                auth=['--defaults-file='+str(no_links(Path(root)/'run/mariadb/admin.cnf')),
                      '--protocol=tcp','--host=127.0.0.1','--port='+str(conn['port'])]
        if auth is None and os.name!='nt' and os.geteuid()==0 and conn['host'] in ('127.0.0.1','localhost','::1'):
            probe=subprocess.run([str(client),'--no-defaults','--protocol=socket','--user=root','--batch','--skip-column-names',
                '--connect-timeout=3','--execute','SELECT @@port;'],capture_output=True,text=True,timeout=8)
            if not probe.returncode and probe.stdout.strip()==str(conn['port']):auth=['--no-defaults','--protocol=socket','--user=root']
        if auth is None:
            quote=lambda text:'"'+str(text).replace('\\','\\\\').replace('"','\\"').replace('\n','\\n').replace('\r','\\r').replace('\t','\\t')+'"'
            options=Path(directory)/'client.cnf'
            descriptor=os.open(options,os.O_CREAT|os.O_EXCL|os.O_WRONLY,0o600)
            with os.fdopen(descriptor,'w',encoding='utf-8') as stream:
                stream.write('[client]\n'+'\n'.join(key+'='+quote(conn[key]) for key in ('host','port','user','password'))+'\n')
            auth=['--defaults-file='+str(options),'--protocol=tcp']
        yield client,dump,auth


def sql_run(argv,root,api,*,source=None,target=None,timeout=1800):
    with tempfile.TemporaryFile() as error:
        result=subprocess.run([str(arg) for arg in argv],stdin=source or subprocess.DEVNULL,
            stdout=target or subprocess.DEVNULL,stderr=error,timeout=timeout,
            start_new_session=os.name!='nt',creationflags=subprocess.CREATE_NO_WINDOW if os.name=='nt' else 0)
        if result.returncode:
            error.seek(0);message=api.redact(error.read(12000).decode('utf-8',errors='replace'),root)
            raise ValueError('SQL-mentési/helyreállítási hiba (%d): %s'%(result.returncode,message))


def _create(root,api,selected,reason,comment=''):
    selected=scopes(selected);folder=store(root);identity=stamp();work=folder/('.pending-'+identity)
    work.mkdir(mode=0o700)
    try:
        files=package_files(root,selected,api);required=sum(path.stat().st_size for path in files.values())
        if shutil.disk_usage(folder).free<required*2+32*1024*1024:raise ValueError('Nincs elég szabad hely a mentéshez.')
        meta=dict(version=1,id=identity,created=datetime.datetime.now(datetime.timezone.utc).isoformat(),
            origin=str(root),reason=reason,comment=str(comment)[:500],scopes=selected,files={},sql=[],sql_consistency='global-read-lock-per-endpoint')
        for name,path in files.items():
            before=path.stat();target=work/'payload/files'/name;target.parent.mkdir(parents=True,exist_ok=True)
            shutil.copy2(path,target);after=path.stat()
            if (before.st_size,before.st_mtime_ns)!=(after.st_size,after.st_mtime_ns) or digest(path)!=digest(target):
                raise ValueError('Mentés közben változott a fájl: '+name)
            meta['files']['files/'+name]=dict(size=before.st_size,sha256=digest(target),mode=before.st_mode&0o777)
        if 'sql' in selected:
            config=api.strict_json((Path(root)/'server.json').read_text(encoding='utf-8-sig'))
            for index,group in enumerate(sql_groups(config,api)):
                name='sql/%03d.sql'%index;target=work/'payload'/name;target.parent.mkdir(parents=True,exist_ok=True)
                with sql_access(root,group,api) as (client,dump,auth),target.open('xb') as output:
                    sql_run([dump,*auth,'--lock-all-tables','--quick','--hex-blob','--routines','--events','--triggers',
                        '--skip-add-locks','--default-character-set=utf8mb4','--databases',*group['databases']],root,api,target=output)
                conn=group['connection']
                meta['sql'].append(dict(file=name,host=conn['host'],port=conn['port'],databases=group['databases']))
                meta['files'][name]=dict(size=target.stat().st_size,sha256=digest(target),mode=0o600)
        # Detect concurrent additions/deletions and edits, including hot quest changes.
        current=package_files(root,selected,api)
        if set(current)!=set(files) or any(digest(path)!=meta['files']['files/'+name]['sha256'] for name,path in current.items()):
            raise ValueError('A csomag tartalma mentés közben változott. Próbáld újra.')
        archive=work/'backup.tar.gz'
        with tarfile.open(archive,'w:gz') as tar:
            raw=json.dumps(meta,ensure_ascii=True,sort_keys=True).encode();info=tarfile.TarInfo('manifest.json');info.size=len(raw);info.mode=0o600
            tar.addfile(info,io.BytesIO(raw))
            for name,row in meta['files'].items():tar.add(work/'payload'/name,arcname=name,recursive=False)
        archive.chmod(0o600)
        meta.update(archive_sha256=digest(archive),archive_size=archive.stat().st_size)
        atomic_json(work/'meta.json',meta)
        atomic_json(work/'summary.json',summary(meta))
        shutil.rmtree(work/'payload',ignore_errors=False) if (work/'payload').exists() else None
        os.replace(work,folder/identity)
        return meta
    except BaseException:
        if work.is_dir():shutil.rmtree(work)
        raise


def create(root,api,selected,reason='manual',comment=''):
    root=api.package_root(root);manager=api.load_manager(root)
    stopped=not api.describe(root)['running']
    with durable(),(manager.supervisor_lock() if stopped else contextlib.nullcontext()),job_lock(root,api,'create'),manager.named_lock('quest-publication'):
        assert_ready(root)
        return _create(root,api,selected,reason,comment)


def backup(root,identity):
    if not isinstance(identity,str) or not re.fullmatch(BACKUP_ID,identity):raise ValueError('Hibás mentésazonosító.')
    folder=no_links(store(root)/identity);path=folder/'meta.json';no_links(path)
    if not path.is_file() or path.stat().st_size>32*1024*1024:raise ValueError('A mentés nem található vagy túl nagy a leírója.')
    meta=json.loads(path.read_text(encoding='utf-8'))
    validate_meta(meta)
    if meta['id']!=identity:raise ValueError('Hibás mentési azonosító.')
    return folder,meta


def archive_info(root,identity):
    folder,meta=backup(root,identity);archive=no_links(folder/'backup.tar.gz')
    if archive.stat().st_size!=meta['archive_size'] or digest(archive)!=meta['archive_sha256']:
        raise ValueError('A mentés archívuma sérült vagy megváltozott.')
    return dict(path=str(archive),sha256=meta['archive_sha256'],size=meta['archive_size'],id=identity)


@contextlib.contextmanager
def extracted(root,identity):
    folder,meta=backup(root,identity);archive_info(root,identity)
    with tempfile.TemporaryDirectory(prefix='.verify-',dir=store(root)) as temporary:
        stage=Path(temporary);seen=set();total=0
        with tarfile.open(folder/'backup.tar.gz','r|gz') as tar:
            first=tar.next()
            if not first or first.name!='manifest.json' or not first.isfile() or first.size>32*1024*1024:raise ValueError('Hibás mentési archívum.')
            manifest=json.loads(tar.extractfile(first).read())
            expected={key:value for key,value in meta.items() if key not in ('archive_sha256','archive_size')}
            if manifest!=expected:raise ValueError('Az archívum és a mentési leíró eltér.')
            if len(manifest['files'])>MAX_FILES:raise ValueError('Túl sok archívumtag.')
            member=tar.next()
            while member is not None:
                name=member.name;valid_relative(name)
                row=manifest['files'].get(name)
                if not row or name in seen or not member.isfile() or member.size!=row['size']:
                    raise ValueError('Nem megengedett archívumtag: '+name)
                if name.startswith('files/'):file_scope(name.removeprefix('files/'))
                elif not re.fullmatch(r'sql/[0-9]{3}\.sql',name):raise ValueError('Hibás SQL-archívumtag.')
                total+=member.size
                if total>1024**4 or shutil.disk_usage(stage).free<member.size+16*1024*1024:raise ValueError('Nincs elég hely a mentés ellenőrzéséhez.')
                target=stage/name;target.parent.mkdir(parents=True,exist_ok=True)
                with tar.extractfile(member) as source,target.open('xb') as output:shutil.copyfileobj(source,output,1024*1024)
                if digest(target)!=row['sha256']:raise ValueError('Hibás ellenőrzőösszeg: '+name)
                seen.add(name)
                member=tar.next()
            if seen!=set(manifest['files']):raise ValueError('Hiányos mentési archívum.')
        if 'files/server.json' in manifest['files']:validate_configuration(stage/'files/server.json')
        yield meta,stage


def inventory(root,api):
    root=api.package_root(root);items=[]
    for directory in sorted(store(root).iterdir(),reverse=True):
        if not re.fullmatch(BACKUP_ID,directory.name) or directory.is_symlink():continue
        try:
            path=no_links(directory/'summary.json')
            meta=json.loads(path.read_text(encoding='utf-8')) if path.is_file() else backup(root,directory.name)[1]
            if meta['id']!=directory.name:continue
            datetime.datetime.fromisoformat(meta['created'])
            items.append(summary(meta))
        except (OSError,ValueError,KeyError):continue
        if len(items)>=500:break
    items.sort(key=lambda row:row['created'],reverse=True)
    activity=None
    try:
        value=json.loads((store(root)/'active.json').read_text())
        if api.load_manager(root).owned(value.get('identity')):activity=value['operation']
    except (OSError,ValueError,KeyError,TypeError):pass
    last=None
    try:
        path=no_links(store(root)/'schedule-last.json')
        if path.is_file() and path.stat().st_size<64000:last=json.loads(path.read_text(encoding='utf-8'))
    except (OSError,ValueError):pass
    return dict(backups=items,policy=policy(root),pending=pending(root),active=activity,schedule_last=last,free_bytes=shutil.disk_usage(root).free)


def plan_state(root,api,identity,selected):
    folder,meta=backup(root,identity);selected=scopes(selected)
    if not set(selected).issubset(meta['scopes']):raise ValueError('A mentés nem tartalmaz minden kiválasztott összetevőt.')
    if api.describe(root)['running']:raise ValueError('Visszaállítás előtt állítsd le a játékszervert. Az SQL-szolgáltatás maradjon elindítva.')
    files=package_files(root,selected,api)
    config=api.strict_json((Path(root)/'server.json').read_text(encoding='utf-8-sig'))
    groups=sql_groups(config,api) if 'sql' in selected else []
    public=[dict(host=row['connection']['host'],port=row['connection']['port'],databases=row['databases']) for row in groups]
    saved=[{key:row[key] for key in ('host','port','databases')} for row in meta['sql']]
    # Databases may be recovered on a different endpoint, but never renamed implicitly.
    if 'sql' in selected and sorted(row['databases'] for row in public)!=sorted(row['databases'] for row in saved):
        raise ValueError('A jelenlegi SQL-adatbázisnevek eltérnek a mentéstől. Automatikus átnevezés nem végezhető.')
    if 'sql' in selected:
        for row in saved:
            choices=[group for group in public if group['databases']==row['databases']]
            if len(choices)>1 and sum((group['host'],group['port'])==(row['host'],row['port']) for group in choices)!=1:
                raise ValueError('Az azonos nevű adatbázisokat tartalmazó SQL-célgépek nem párosíthatók egyértelműen.')
    paths={name:digest(path) for name,path in files.items()}
    source={name.removeprefix('files/'):row for name,row in meta['files'].items() if name.startswith('files/') and file_scope(name[6:]) in selected}
    value=dict(backup=identity,archive=meta['archive_sha256'],scopes=selected,config=digest(Path(root)/'server.json'),files=paths,sql=public)
    fingerprint=hashlib.sha256(json.dumps(value,sort_keys=True).encode()).hexdigest()
    return dict(id=identity,fingerprint=fingerprint,scopes=selected,files=len(source),
                remove=sorted(set(files)-set(source)),sql=public,archive_size=meta['archive_size'])


def restore_plan(root,api,identity,selected):
    with extracted(root,identity):return plan_state(root,api,identity,selected)


def apply_snapshot(root,api,meta,stage,selected,preserve_connections,sql_target_config=None):
    root=Path(root)
    if 'sql' in selected:
        config=sql_target_config or api.strict_json((root/'server.json').read_text(encoding='utf-8-sig'));groups=sql_groups(config,api)
        for row in meta['sql']:
            choices=[group for group in groups if group['databases']==row['databases']]
            if len(choices)>1:choices=[group for group in choices if
                (group['connection']['host'],group['connection']['port'])==(row['host'],row['port'])]
            if len(choices)!=1:raise ValueError('Nem egyértelmű SQL-visszaállítási cél.')
            group=choices[0]
            with sql_access(root,group,api) as (client,dump,auth),(stage/row['file']).open('rb') as source:
                # Recreate only the reviewed game schemas, including removal of
                # tables/routines created after the saved snapshot.
                drops='\n'.join('DROP DATABASE IF EXISTS `'+name+'`;' for name in group['databases']).encode()
                with tempfile.TemporaryFile() as commands:
                    commands.write(drops);commands.seek(0)
                    sql_run([client,*auth,'--connect-timeout=5','--binary-mode','--batch'],root,api,source=commands)
                sql_run([client,*auth,'--connect-timeout=5','--binary-mode','--batch','--default-character-set=utf8mb4','--max-allowed-packet=1G'],root,api,source=source)
    source={name[6:]:row for name,row in meta['files'].items() if name.startswith('files/') and file_scope(name[6:]) in selected}
    old=package_files(root,selected,api,config=sql_target_config)
    for name,row in source.items():
        target=no_links(root/name);target.parent.mkdir(parents=True,exist_ok=True)
        original=stage/'files'/name
        raw=None
        if name=='server.json' and preserve_connections:
            current=api.strict_json(target.read_text(encoding='utf-8-sig'));restored=api.strict_json(original.read_text(encoding='utf-8-sig'))
            for key in ('common','database','game_defaults'):
                if key in current and 'sql' in current[key]:restored.setdefault(key,{})['sql']=current[key]['sql']
            if 'account_sql' in current.get('login',{}):restored.setdefault('login',{})['account_sql']=current['login']['account_sql']
            for instance,section in current['game_servers'].items():
                if 'sql' in section and instance in restored['game_servers']:restored['game_servers'][instance]['sql']=section['sql']
            raw=(json.dumps(restored,ensure_ascii=True,indent=2)+'\n').encode()
        temporary=target.with_name(target.name+'.restore-'+secrets.token_hex(6))
        try:
            if raw is None:shutil.copy2(original,temporary)
            else:temporary.write_bytes(raw)
            if os.name!='nt':temporary.chmod(row['mode']&0o777)
            os.replace(temporary,target)
        finally:temporary.unlink(missing_ok=True)
    for name in set(old)-set(source):no_links(root/name).unlink()


def restore(root,api,identity,selected,fingerprint,preserve_connections=True):
    root=api.package_root(root);manager=api.load_manager(root);selected=scopes(selected)
    if type(preserve_connections) is not bool:raise ValueError('Hibás SQL-kapcsolatmegőrzési beállítás.')
    with durable(),manager.supervisor_lock(),job_lock(root,api,'restore'),manager.named_lock('quest-publication'):
        with extracted(root,identity) as (meta,stage):
            current=plan_state(root,api,identity,selected)
            if current['fingerprint']!=fingerprint:raise ValueError('A visszaállítási terv megváltozott. Készíts új tervet.')
            previous_pending=pending(root)
            sql_target_config=api.strict_json((Path(root)/'server.json').read_text(encoding='utf-8-sig'))
            safety=_create(root,api,selected,'before-restore','Védőmentés a visszaállítás előtt')
            job=Path(root)/'run/backup-restores'/stamp();job.mkdir(mode=0o700,parents=True)
            record=dict(status='restoring',backup=identity,safety_backup=safety['id'],scopes=selected,started=time.time())
            atomic_json(job/'result.json',record)
            try:
                apply_snapshot(root,api,meta,stage,selected,preserve_connections,sql_target_config)
                record['status']='success';atomic_json(job/'result.json',record)
                if previous_pending:
                    previous_pending['status']='recovered';atomic_json(Path(previous_pending.pop('path')),previous_pending)
            except BaseException as error:
                record['error']=api.redact(str(error),root)
                try:
                    with extracted(root,safety['id']) as (old,old_stage):apply_snapshot(root,api,old,old_stage,selected,False,sql_target_config)
                    record['status']='rolled-back'
                except BaseException as rollback:
                    record.update(status='rollback-failed',rollback_error=api.redact(str(rollback),root))
                atomic_json(job/'result.json',record)
                raise ValueError('A visszaállítás sikertelen (%s). Védőmentés: %s. Napló: %s'%(record['status'],safety['id'],job)) from error
            return dict(status='success',safety_backup=safety['id'],journal=str(job/'result.json'))


def before_deploy(root,api,report=lambda _:None):
    assert_ready(root);settings=policy(root)
    if not settings['auto_before_deploy']:return None
    report('Automatikus mentés a build telepítése előtt…')
    # The deployer already owns the supervisor lock.
    manager=api.load_manager(root)
    with durable(),job_lock(root,api,'before-deploy'),manager.named_lock('quest-publication'):
        value=_create(root,api,settings['scopes'],'before-deploy')
    report('Build előtti mentés: '+value['id']);return value['id']


def import_archive(root,api,path,expected_hash):
    """Register a downloaded/offsite archive; no SQL or package files are restored."""
    path=no_links(Path(path))
    if not path.is_file() or digest(path)!=expected_hash:raise ValueError('A feltöltött mentés ellenőrzőösszege eltér.')
    with job_lock(root,api,'import'):
        with tarfile.open(path,'r:gz') as tar:
            first=tar.next()
            if not first or first.name!='manifest.json' or not first.isfile() or first.size>32*1024*1024:
                raise ValueError('Nem Auriga-mentési archívum.')
            meta=json.loads(tar.extractfile(first).read())
        validate_meta(meta)
        meta.update(archive_sha256=expected_hash,archive_size=path.stat().st_size)
        folder=store(root)/meta['id']
        if folder.exists():
            existing=archive_info(root,meta['id'])
            if existing['sha256']!=expected_hash:raise ValueError('Azonos mentésazonosítóhoz eltérő archívum tartozik.')
            return dict(id=meta['id'],already_present=True)
        folder.mkdir(mode=0o700)
        try:
            shutil.copy2(path,folder/'backup.tar.gz');(folder/'backup.tar.gz').chmod(0o600)
            atomic_json(folder/'meta.json',meta)
            with extracted(root,meta['id']):pass
            atomic_json(folder/'summary.json',summary(meta))
        except BaseException:
            if folder.resolve().parent!=store(root).resolve():raise ValueError('Hibás importtakarítási cél.')
            shutil.rmtree(folder);raise
        return dict(id=meta['id'],already_present=False)


def run_schedule(root,api):
    settings=policy(root)
    if not settings['enabled']:return dict(skipped='disabled')
    root=api.package_root(root);manager=api.load_manager(root)
    today=datetime.datetime.now().astimezone().date().isoformat()
    with durable(),(manager.supervisor_lock() if not api.describe(root)['running'] else contextlib.nullcontext()),job_lock(root,api,'scheduled'),manager.named_lock('quest-publication'):
        path=store(root)/'schedule-last.json'
        if path.is_file():
            last=json.loads(path.read_text())
            if last.get('date')==today and last.get('status')=='success':return dict(skipped='already-completed')
        assert_ready(root)
        try:
            value=_create(root,api,settings['scopes'],'scheduled')
            atomic_json(path,dict(date=today,status='success',backup=value['id'],finished=time.time()))
            if settings['keep']:
                keep={value['id']};pinned=set()
                for journal in (Path(root)/'run/backup-restores').glob('*/result.json'):
                    try:
                        record=json.loads(journal.read_text());pinned.update((record.get('backup'),record.get('safety_backup')))
                    except (OSError,ValueError):pass
                items=inventory(root,api)['backups']
                automatic=[row for row in items if row['reason']=='scheduled']
                keep.update(row['id'] for row in automatic[:settings['keep']]);keep.update(pinned)
                for row in automatic:
                    if row['id'] not in keep:
                        target=no_links(store(root)/row['id'])
                        if target.resolve().parent!=store(root).resolve():raise ValueError('Hibás megőrzési cél.')
                        shutil.rmtree(target)
            return dict(backup=value['id'],status='success')
        except BaseException as error:
            atomic_json(path,dict(date=today,status='failed',error=api.redact(str(error),root),finished=time.time()));raise


def scheduled_command(root):
    if getattr(__import__('sys'),'frozen',False):return [__import__('sys').executable,'--backup-scheduled',str(root)]
    return [__import__('sys').executable,str(Path(__file__).with_name('app.py')),'--backup-scheduled',str(root)]


def save_policy(root,api,value,runner_source=''):
    value=validate_policy(value);root=api.package_root(root)
    with job_lock(root,api,'policy'):
        folder=store(root);old=policy(root)
        marker='AurigaBackup-'+hashlib.sha256(str(root).encode()).hexdigest()[:24]
        owned=folder/'scheduler.json'
        if windows():
            command=scheduled_command(root)
            if value['enabled']:
                probe=subprocess.run(['schtasks','/Query','/TN',marker],capture_output=True)
                existing_ours=owned.is_file() and json.loads(owned.read_text()).get('enabled') is True
                if not probe.returncode and not existing_ours:raise ValueError('Az ütemezett feladat neve már foglalt.')
                result=subprocess.run(['schtasks','/Create','/TN',marker,'/SC','DAILY','/ST','%02d:%02d'%(value['hour'],value['minute']),
                    '/TR',subprocess.list2cmdline(command),'/IT','/RL','LIMITED','/F'],capture_output=True,text=True,timeout=30,
                    creationflags=subprocess.CREATE_NO_WINDOW)
            elif owned.is_file():
                result=subprocess.run(['schtasks','/Delete','/TN',marker,'/F'],capture_output=True,text=True,timeout=30,creationflags=subprocess.CREATE_NO_WINDOW)
            else:result=None
            if result and result.returncode:raise ValueError('A Windows Feladatütemező nem fogadta el a módosítást: '+result.stderr[-2000:]+result.stdout[-2000:])
        else:
            import shlex,sys
            cron=shutil.which('crontab') or '/usr/bin/crontab'
            result=subprocess.run([cron,'-l'],capture_output=True,text=True,timeout=10)
            if result.returncode and 'no crontab' not in result.stderr.lower():raise ValueError('A crontab nem olvasható: '+result.stderr[-2000:])
            current=result.stdout if not result.returncode else ''
            lines=[line for line in current.splitlines() if not line.endswith('# '+marker)]
            if value['enabled']:
                if not isinstance(runner_source,str) or not runner_source or len(runner_source)>2*1024*1024:
                    raise ValueError('Hiányzó ütemezett mentési futtatókód.')
                compile(runner_source,'backup-scheduled-worker.py','exec')
                runner=folder/'scheduled-worker.py';pending_file=folder/'scheduled-worker.py.new'
                pending_file.write_text(runner_source,encoding='utf-8');pending_file.chmod(0o600);os.replace(pending_file,runner)
                invocation=shlex.join([sys.executable,'-B',str(runner)]).replace('%',r'\%')
                logfile=shlex.quote(str(folder/'schedule.log')).replace('%',r'\%')
                lines.append('%d %d * * * %s >> %s 2>&1 # %s'%(value['minute'],value['hour'],invocation,logfile,marker))
            if value['enabled'] or owned.is_file():
                result=subprocess.run([cron,'-'],input='\n'.join(lines)+'\n',capture_output=True,text=True,timeout=10)
                if result.returncode:raise ValueError('A crontab módosítása sikertelen: '+result.stderr[-2000:])
        atomic_json(folder/'policy.json',value)
        atomic_json(owned,dict(version=1,name=marker,enabled=value['enabled'],changed=time.time()))
    return dict(policy=value)
