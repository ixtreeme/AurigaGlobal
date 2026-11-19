"""Drop publication acknowledgements from owned, running game cores.

SQL commits one immutable release/head. Each core validates that release on
its item worker and atomically swaps only its drop view; item pins stay fixed.
"""
import hashlib
import json
from pathlib import Path
import re
import time


def status(api):
    try:
        cfg=api.config();state=api.read_json(api.ROOT/'run/supervisor.json')
        if state.get('phase')!='listening' or not api.owned(state.get('supervisor')):
            return dict(available=False,running=False,cores=[])
        cores=[]
        for child in state['children']:
            if child['name'] not in cfg['game_servers']: continue
            if not api.owned(child['identity']): raise RuntimeError('A game core is unavailable: '+child['name'])
            directory=api.instance_directory(cfg,child['name'])
            if directory!=Path(child['directory']).resolve(): raise RuntimeError('Game core directory changed')
            path=directory/'.auriga-drops-status.json'
            if path.is_symlink() or path.stat().st_size>32768: raise RuntimeError('Invalid drop status file')
            row=api.read_json(path);token=child.get('token','')
            if (not re.fullmatch('[a-f0-9]{48}',token) or row.get('protocol')!=1 or
                row.get('pid')!=child['identity']['pid'] or row.get('instance')!=child['name'] or
                row.get('token_hash')!=hashlib.sha256(token.encode()).hexdigest() or
                abs(time.time()-row.get('updated_at',0))>15):
                raise RuntimeError('Game core needs the live-drop build: '+child['name'])
            cores.append(dict(name=child['name'],hash=row['hash'],release=row['release'],base_hash=row['base_hash'],
                              supported=row['supported'],containers_supported=row.get('containers_supported',False),error=row.get('error',''),pending=row.get('pending',False)))
        return dict(available=bool(cores) and all(c['supported'] for c in cores),
                    containers_available=bool(cores) and all(c['supported'] and c['containers_supported'] for c in cores),running=True,cores=cores)
    except (OSError,ValueError,KeyError,TypeError,RuntimeError) as error:
        return dict(available=False,running=True,cores=[],error=str(error))


def wait(api,expected_hash,release,timeout=30):
    if not re.fullmatch('[a-f0-9]{64}',expected_hash) or release and not re.fullmatch('[a-f0-9]{32}',release):
        raise RuntimeError('Invalid drop release identity')
    end=time.monotonic()+timeout
    while True:
        result=status(api)
        result.update(hash=expected_hash,release=release,activated=False)
        if not result.get('running'): return result
        if result['available'] and all(c['hash']==expected_hash and c['release']==release and not c['error'] for c in result['cores']):
            result['activated']=True;return result
        if time.monotonic()>=end: return result
        time.sleep(.2)


def reload(api):
    cfg=api.config()['common']['sql']['player']
    helper=api.ROOT/'manager/_internal/validators'/('CatalogEditorTool.exe' if api.os.name=='nt' else 'CatalogEditorTool')
    if not helper.is_file(): raise RuntimeError('Missing CatalogEditorTool in the local package')
    result=api.native([helper],input=json.dumps(dict(action='read',connection=dict(port=cfg['port'],user=cfg['user'],password=cfg['password']))))
    rows=[json.loads(s.removeprefix('AURIGA_CATALOG_RESULT ')) for s in result.stdout.splitlines() if s.startswith('AURIGA_CATALOG_RESULT ')]
    if result.returncode or len(rows)!=1 or not rows[0].get('ok'): raise RuntimeError('Cannot read the saved drop release')
    live=rows[0]['result']['live_drops']
    if not live['supported']: raise RuntimeError('Apply migration 043_live_mob_drops.sql')
    outcome=wait(api,live['hash'],live['release'])
    print(json.dumps(outcome,ensure_ascii=True),flush=True)
    if not outcome['activated'] and outcome.get('running'): raise RuntimeError('Drop activation is still pending; saved release retained, repeat reload-drops')
    return outcome
