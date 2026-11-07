"""Cold-restart the owned localhost stack and MariaDB, then verify pet state in Unity.

Accepts completed private pet fixtures, never arbitrary accounts. Each source
must have a saved first pet and its five isolated codexpcombat identities.
Generated credentials and SQL snapshots remain in the ignored work directory.
"""
from pathlib import Path
from datetime import datetime,timezone
import argparse,hashlib,json,os,subprocess,sys,time

REPO=Path(__file__).resolve().parents[2]
sys.path[:0]=[str(REPO/'SRC/Tools/ServerManager'),str(REPO/'db/tools')]
import backend,item_bench
from mariadb_client import Client

def read(path):return json.loads(path.read_text(encoding='utf-8-sig'))
def write(path,value):path.write_text(json.dumps(value,indent=2)+'\n',encoding='utf-8')
def digest(value):return hashlib.sha256(json.dumps(value,sort_keys=True).encode()).hexdigest()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--package',type=Path,required=True)
    parser.add_argument('--unity-project',type=Path,required=True)
    parser.add_argument('--fixture',type=Path,action='append',required=True)
    parser.add_argument('--work-dir',type=Path,required=True)
    args=parser.parse_args();root=args.package.resolve();project=args.unity_project.resolve();work=args.work_dir.resolve()
    assert work.is_relative_to(REPO/'build') and not work.exists();work.mkdir(parents=True)
    root,manager,ownership,auth=item_bench.connection(root)
    fixtures=[]
    for source in args.fixture:
        source=source.resolve();assert source.is_relative_to(REPO/'build')
        config=read(source)
        assert config['Host']=='127.0.0.1' and len(config['Accounts'])==5 and len(config['Cases'])==18
        assert all(a['Login']=='codexpcombat'+config['Token']+str(i) for i,a in enumerate(config['Accounts']))
        progress=read(Path(str(source)+'.progress.json'));assert progress['Token']==config['Token']
        seal=next(p['Seal'] for p in progress['Pets'] if p['Effect']==1)
        config.update(RestartMode=1,RestartSeal=seal,RestartSkinLocal=config['SkinLocals'][0] if config.get('Skins') else 0,
            RestartProof=str(work/(source.stem+'.proof.private.json')))
        path=work/(source.stem+'.fixture.private.json');write(path,config);fixtures.append((path,config))
    owners=[int(c['Accounts'][0]['Pid']) for _,c in fixtures];assert len(owners)==len(set(owners)) and len(owners)>=2
    def connection():return Client(**auth,library=str(root/'bin/libmariadb.dll'))
    def snapshot(db,selected):
        where='owner_kind=1 AND owner_id IN ('+','.join(map(str,selected))+')'
        result={name:db.query('SELECT '+columns+' FROM player.'+table+' WHERE '+where+' ORDER BY '+order) for name,table,columns,order in [
            ('items','item_instance','owner_id,instance_id,state_revision,HEX(state_sha256),HEX(last_operation),HEX(growth_pet_blob),HEX(pet_supply_blob),destroyed','owner_id,instance_id'),
            ('wallets','item_wallet','owner_id,currency,state_revision,balance','owner_id,currency'),
            ('learning','item_book_learning','owner_id,state_revision,HEX(state_sha256),HEX(state_blob),HEX(last_operation)','owner_id'),
            ('cooldowns','item_pet_study_cooldown','owner_id,skill_id,ready_at_ms','owner_id,skill_id'),
            ('fences','item_owner_fence','owner_id,session_generation,state_revision','owner_id')]}
        result['character']=db.query('SELECT id,exp,skill_point,sub_skill_point,horse_skill_point,typed_book_revision,HEX(skill_level) FROM player.player WHERE id IN ('+','.join(map(str,selected))+') ORDER BY id')
        return result
    def protected(db):
        where=' NOT (owner_kind=1 AND owner_id IN ('+','.join(map(str,owners))+'))'
        return {name:db.query(query+' WHERE '+where+' ORDER BY '+order) for name,query,order in [
            ('items','SELECT owner_kind,owner_id,instance_id,state_revision,HEX(state_sha256),HEX(last_operation) FROM player.item_instance','instance_id'),
            ('wallets','SELECT owner_kind,owner_id,currency,state_revision,balance FROM player.item_wallet','owner_kind,owner_id,currency'),
            ('learning','SELECT owner_kind,owner_id,state_revision,HEX(state_sha256),HEX(state_blob),HEX(last_operation) FROM player.item_book_learning','owner_kind,owner_id')]}
    def command(action):
        result=subprocess.run([sys.executable,str(root/'manage.py'),action],cwd=root,capture_output=True,text=True,creationflags=item_bench.NO_WINDOW,timeout=60)
        (work/(action+'.log')).write_text(result.stdout+result.stderr,encoding='utf-8')
        assert result.returncode==0,'Owned command failed: '+action
    def start_stack():
        with (work/'start.log').open('ab') as output:
            subprocess.Popen([sys.executable,str(root/'manage.py'),'start','--channel','1','--channel','99'],cwd=root,
                stdout=output,stderr=subprocess.STDOUT,creationflags=item_bench.NO_WINDOW)
    def unity(path,label):
        log=work/(label+'.log')
        result=subprocess.run([sys.executable,str(REPO/'tests/server/PetPassiveUnityLiveSmoke.py'),'--package',str(root),
            '--unity-project',str(project),'--fixture',str(path),'--log',str(log),'--timeout','1200'],cwd=REPO,
            capture_output=True,text=True,creationflags=item_bench.NO_WINDOW,timeout=1250)
        (work/(label+'.launcher.log')).write_text(result.stdout+result.stderr,encoding='utf-8')
        assert result.returncode==0,'Unity probe failed: '+label
        marker='METIN2_TYPED_PET_RESTART_ARMED=SUCCESS' if label.startswith('arm-') else 'METIN2_TYPED_PET_RESTART_LIVE=SUCCESS'
        assert marker in log.read_text(encoding='utf-8',errors='replace')
        print('PET_COLD_UNITY=SUCCESS phase='+label,flush=True)
    db=connection()
    try:
        assert not db.query('SELECT vnum FROM player.mob_proto WHERE vnum BETWEEN 65070 AND 65075'),'Restore temporary probe world first'
        baseline=protected(db);write(work/'protected-before.private.json',baseline)
    finally:db.close()
    armed={}
    for index,(path,config) in enumerate(fixtures):
        unity(path,'arm-'+str(index));db=connection()
        try:
            previous=snapshot(db,[owners[index]]);deadline=time.monotonic()+15
            while True:
                time.sleep(.5);current=snapshot(db,[owners[index]])
                if current==previous:break
                assert time.monotonic()<deadline,'Pet logout settlement did not finish';previous=current
            armed[owners[index]]=current
        finally:db.close()
    db=connection()
    try:
        for owner,state in armed.items():assert snapshot(db,[owner])==state,'Offline pet state changed before restart'
        before=snapshot(db,owners);assert protected(db)==baseline
        write(work/'before.private.json',before)
    finally:db.close()
    old_stack=read(root/'run/supervisor.json')['children'];old_db=read(root/'run/mariadb/process.json')
    command('stop');deadline=time.monotonic()+45
    while backend.describe(root)['running']:
        assert time.monotonic()<deadline,'Owned stack did not stop';time.sleep(.25)
    assert all(not manager.owned(c['identity']) for c in old_stack)
    command('stop-db');assert not manager.owned(old_db)
    command('start-db');new_db=read(root/'run/mariadb/process.json');assert manager.owned(new_db) and new_db!=old_db
    start_stack();deadline=time.monotonic()+60
    while True:
        new_stack=read(root/'run/supervisor.json')['children'] if (root/'run/supervisor.json').exists() else []
        if backend.describe(root)['running'] and len(new_stack)==len(old_stack)==5 and all(manager.owned(c['identity']) for c in new_stack):break
        assert time.monotonic()<deadline,'Owned stack did not start';time.sleep(.25)
    assert {str(c['identity']) for c in old_stack}.isdisjoint({str(c['identity']) for c in new_stack})
    db=connection()
    try:
        after=snapshot(db,owners);assert after==before,'Cold restart changed persisted pet, wallet, cooldown or learning state'
        assert protected(db)==baseline;write(work/'after.private.json',after)
    finally:db.close()
    print('PET_COLD_STORAGE=SUCCESS owners='+str(len(owners))+' fingerprint='+digest(after),flush=True)
    for index,(path,config) in enumerate(fixtures):
        config['RestartMode']=2;write(path,config);unity(path,'verify-'+str(index))
    db=connection()
    try:assert protected(db)==baseline
    finally:db.close()
    write(work/'evidence.json',dict(completed_utc=datetime.now(timezone.utc).isoformat(),owners=len(owners),
        full_stack_and_mariadb_restarted=True,exact_storage_fingerprint=digest(after),protected_items=len(baseline['items']),
        unity_arm_probes=len(fixtures),unity_relogin_probes=len(fixtures)))
    print('PET_COLD_RESTART=SUCCESS owners='+str(len(owners)),flush=True)

if __name__=='__main__':main()
