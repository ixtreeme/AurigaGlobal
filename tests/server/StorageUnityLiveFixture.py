"""Own isolated account/alt, guild and delivery providers for the C33 TLS/UI flow."""
from pathlib import Path
from datetime import datetime,timezone
from collections import Counter
import argparse,json,os,secrets,subprocess,sys,hashlib
REPO=Path(__file__).resolve().parents[2]
sys.path[:0]=[str(REPO/'SRC/Tools/ServerManager'),str(REPO/'db/tools')]
import item_bench,item_catalog,backend
from mariadb_client import Client
from PetPassiveUnityLiveFixture import protected
from CostumeUnityLiveFixture import read,write,literal

def others(db,c):
    excluded={(1,int(a['Pid'])) for a in [*c['Accounts'],c['StorageAlt']]}
    excluded|={(2,int(a['AccountId'])) for a in c['Accounts']};excluded.add((3,int(c['GuildId'])))
    return {k:[r for r in rows if (int(r[0]),int(r[1])) not in excluded] for k,rows in protected(db).items()}

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('action',choices=('seed','verify','restore'))
    for n in ('package','fixture'):p.add_argument('--'+n,type=Path,required=True)
    for n in ('template','log'):p.add_argument('--'+n,type=Path)
    a=p.parse_args();fixture=a.fixture.resolve();assert fixture.is_relative_to(REPO/'build')
    root,manager,ownership,auth=item_bench.connection(a.package.resolve());os.environ['PATH']=str(root/'bin')+os.pathsep+os.environ['PATH'];db=Client(**auth,library=str(root/'bin/libmariadb.dll'))
    tool=root/'manager/_internal/validators/ItemBenchTool.exe';catalog_tool=root/'manager/_internal/validators/CatalogEditorTool.exe'
    try:
        if a.action=='restore':
            c=read(fixture);backup=Path(c['Backup'])
            with manager.supervisor_lock():
                assert not backend.describe(root)['running'];target=root/c['KeeperMap'];assert target.read_bytes()==(backup/'regen-installed.json').read_bytes();manifest=read(root/'package-manifest.json');assert hashlib.sha256(target.read_bytes()).hexdigest()==manifest['files'][c['KeeperMap']]
                target.write_bytes((backup/'regen-before.json').read_bytes());manifest['files'][c['KeeperMap']]=hashlib.sha256(target.read_bytes()).hexdigest();write(root/'package-manifest.json',manifest)
            print('STORAGE_KEEPER_RESTORED');return
        if a.action=='verify':
            c=read(fixture);backup=Path(c['Backup']);before=read(backup/'typed-before.json');assert others(db,c)==before
            log=a.log.read_text(encoding='utf-8',errors='replace');assert 'METIN2_TYPED_STORAGE_LIVE=SUCCESS' in log
            lead,receiver,alt=int(c['Accounts'][0]['Pid']),int(c['Accounts'][1]['Pid']),int(c['StorageAlt']['Pid']);account=int(c['Accounts'][0]['AccountId']);gid=int(c['GuildId'])
            def state(kind,owner):return item_bench.run_tool(dict(action='storage_status',owner=owner,owner_kind=kind,connection=auth),tool)['state']
            main,other,alternate,shared,guild=state(1,lead),state(1,receiver),state(1,alt),state(2,account),state(3,gid)
            rich=next(i for i in other['items'] if i['id']==c['StorageSwordId']);initial=read(backup/'rich-before.json')
            for key in ('id','category','local','level','count','attributes','stones','bound','accessory','tool','book_skill','polymorph','toggle','automatic'):assert rich[key]==initial[key],key
            assert rich['storage_kind']==1
            assert next(i for i in main['items'] if i['id']==c['StorageBoundId'])['bound_owner_id']==lead
            assert next(i for i in main['items'] if i['id']==c['StorageMallId'])['bound_owner_id']==lead
            assert next(i for i in main['items'] if i['id']==c['StorageAwardId'])['storage_kind']==1
            assert next(w['balance'] for w in shared['wallets'] if w['currency']==2)==0
            assert sum(i['count'] for i in main['items'] if i['category']==16 and i['local']==c['StorageCoinLocal'])==4
            assert sum(i['count'] for i in alternate['items'] if i['category']==16 and i['local']==c['StorageCoinLocal'])==1
            assert sum(i['count'] for i in [*main['items'],*alternate['items'],*shared['items'],*guild['items']] if i['category']==6 and i['local']==c['StorageMaterialLocal'])==29
            assert all(i['storage_kind']!=5 for i in guild['items'])
            assert shared['session']==1 and all(w['balance']==0 for s in (main,other,alternate) for w in s['wallets'] if w['currency']==1)
            assert db.query(f'SELECT closing FROM player.item_shared_storage_gate WHERE guild_id={gid}')==[['0']]
            for a0 in [*c['Accounts'],c['StorageAlt']]:assert db.query(f"SELECT COUNT(*) FROM player.item WHERE owner_id={int(a0['Pid'])}")==[['0']]
            write(backup/'owners-after.json',dict(main=main,other=other,alt=alternate,account=shared,guild=guild));print('STORAGE_LOCAL_SQL=SUCCESS protected_items='+str(len(before['items']))+' shared_generation=1');return
        assert a.template and not fixture.exists();template=read(a.template);source=template['Accounts'][0];source_pid=int(source['Pid']);assert template['Host']=='127.0.0.1' and len(template['Accounts'])==2
        with manager.supervisor_lock():
            assert not backend.describe(root)['running'];backup=root/'run/deployments'/datetime.now(timezone.utc).strftime('c33-fixture-%Y%m%dT%H%M%S%f');backup.mkdir();before=protected(db);write(backup/'typed-before.json',before)
            with (backup/'player-protos-before.sql').open('wb') as out:
                r=subprocess.run([str(Path(ownership['binary_dir'])/'mariadb-dump.exe'),'--defaults-extra-file='+str(root/'run/mariadb/admin.cnf'),'--protocol=tcp','--host=127.0.0.1','--port='+str(ownership['port']),'--single-transaction','--databases','player','protos'],stdout=out,stderr=subprocess.PIPE,creationflags=item_bench.NO_WINDOW,timeout=300);assert r.returncode==0 and out.tell()>0
            initial=item_catalog.run_tool(dict(action='read',connection=auth),executable=catalog_tool);write(backup/'catalog-before.json',initial);token=secrets.token_hex(4);c={k:template[k] for k in ('Host','ServerName','Pin','Version','AuthPort','GamePort','PetLocal','MountLocal','PetRace','MountRace')};c.update(Token=token,Accounts=[],Storage=True,X=986600,Y=269100,Backup=str(backup));editor=item_catalog.Catalog(initial);steps=[]
            def publish(doc,label):
                category=int(doc['base']['category_id']);local=int(doc['base']['local_id']);code=f'storage.c33_{token}_{category}_{label}';doc['base'].update(content_code=code,name_key='item.'+code+'.name',description_key='item.'+code+'.description');steps.append(editor.apply(doc));editor.packet['sequences'][str(category)]=str(local+1);return local
            for label,binding in [('StorageSwordLocal','1'),('StorageBoundLocal','2'),('StorageGuildLocal','1')]:
                d=editor.new(1);d['base']['binding_policy']=binding;d['refinement']['max_level']='12';d['levels']=[dict(level=str(j),**{k:'0' for k in item_catalog.GAINS},extra_bonus_profile_id=None,bonuses=[]) for j in range(13)]
                if label=='StorageSwordLocal':d.update(editor.attribute_defaults());d['attribute_profile']=dict(allow_lock='1')
                c[label]=publish(d,label.lower())
            d=editor.new(7);c['StorageStoneLocal']=publish(d,'stone')
            d=editor.new(6);c['StorageMaterialLocal']=publish(d,'material')
            d=editor.new(16);d['child'].update(wallet_currency='2',amount_per_unit='100');c['StorageCoinLocal']=publish(d,'coin')
            d=editor.new(5);d.update(editor.attribute_service_defaults());d.update(socket_service=None,recovery=None,effect=None,timed=None,toggle=None,cooldown=None);d['attribute_service']['maximum_count']='5';c['StorageToolLocal']=publish(d,'attribute_tool')
            for name,rows in initial['tables'].items():assert not (Counter(map(tuple,rows))-Counter(map(tuple,steps[-1][name]))),name
            published=initial
            for step in steps:
                item_catalog.run_tool(dict(action='commit',connection=auth,tables=step,expected_hash=published['hash']),initial['columns'],catalog_tool);published=item_catalog.run_tool(dict(action='read',connection=auth),executable=catalog_tool)
            columns=[r[0] for r in db.query('SHOW COLUMNS FROM player.player') if r[0]!='id' and 'GENERATED' not in r[5]]
            def character(aid,name,index):
                overrides=dict(job='0',level='60',st='10',ht='10',dx='10',iq='10',random_hp='30000',random_sp='10000',skill_group='1',account_id=str(aid),name=name,map_index='41',x=str(c['X']+index*100),y=str(c['Y']),z='0',gold='0',hp='30000',mp='10000',typed_book_revision='0',horse_level='0',horse_riding='0');projection=['REPEAT(CHAR(0),OCTET_LENGTH(`'+k+'`))' if k in ('skill_level','quickslot') else literal(overrides[k]) if k in overrides else '`'+k+'`' for k in columns];db.query('INSERT INTO player.player('+','.join('`'+k+'`' for k in columns)+') SELECT '+','.join(projection)+f' FROM player.player WHERE id={source_pid}');return int(db.query('SELECT LAST_INSERT_ID()')[0][0])
            db.query('START TRANSACTION')
            try:
                for index in range(2):
                    login='codexcompanion'+token+str(index);password=secrets.token_hex(8);name='ST'+token+str(index);db.query('INSERT INTO account.account(login,password,social_id,status,create_time) VALUES('+literal(login)+',PASSWORD('+literal(password)+"),'1234567','OK',NOW())");aid=int(db.query('SELECT LAST_INSERT_ID()')[0][0]);pid=character(aid,name,index);db.query(f'INSERT INTO player.player_index(id,pid1,empire) VALUES({aid},{pid},1)');c['Accounts'].append(dict(Login=login,Password=password,Name=name,AccountId=aid,Pid=pid,Slot=0))
                leader=c['Accounts'][0];alt=character(leader['AccountId'],'ST'+token+'A',0);db.query(f"UPDATE player.player_index SET pid2={alt} WHERE id={leader['AccountId']}");c['StorageAlt']=dict(leader,Name='ST'+token+'A',Pid=alt,Slot=1)
                db.query('INSERT INTO player.guild(name,master,level,sp) VALUES('+literal('ST'+token)+f",{leader['Pid']},20,1000)");gid=int(db.query('SELECT LAST_INSERT_ID()')[0][0]);c['GuildId']=gid
                for player,grade in ((leader['Pid'],1),(alt,15)):db.query(f'INSERT INTO player.guild_member(pid,guild_id,grade,is_general,offer) VALUES({player},{gid},{grade},0,0)')
                for grade in range(1,16):db.query(f'INSERT INTO player.guild_grade(guild_id,grade,name,auth) VALUES({gid},{grade},'+literal('Vezető' if grade==1 else 'Tag')+f',{15 if grade==1 else 0})')
                db.query('COMMIT')
            except BaseException:db.query('ROLLBACK');raise
            write(backup/'fixture-pending.private.json',c)
            def bench(action,owner=None,**fields):return item_bench.run_tool(dict(action=action,owner=owner or leader['Pid'],connection=auth,**fields),tool)
            for player in [*c['Accounts'],c['StorageAlt']]:
                bench('setup',owner=player['Pid'],gold=0)
                for local in (c['PetLocal'],c['MountLocal']):bench('grant',owner=player['Pid'],category=15,local=local,count=1)
                bench('storage_setup',owner=player['Pid'],owner_kind=1)
            for kind,owner in ((2,leader['AccountId']),(2,c['Accounts'][1]['AccountId']),(3,gid)):bench('storage_setup',owner=owner,owner_kind=kind)
            stones=[dict(state=3,grade=1,category=7,local=c['StorageStoneLocal'],level=0),dict(state=4,grade=1),dict(state=2,grade=2)]
            c['StorageSwordId']=bench('grant',category=1,local=c['StorageSwordLocal'],level=9,count=1,stones=stones)['granted']
            c['StorageBoundId']=bench('grant',category=1,local=c['StorageBoundLocal'],level=3,count=1)['granted'];c['StorageGuildId']=bench('grant',category=1,local=c['StorageGuildLocal'],level=2,count=1)['granted']
            c['StorageMaterialId']=bench('grant',category=6,local=c['StorageMaterialLocal'],count=20)['granted'];c['StorageCoinId']=bench('grant',category=16,local=c['StorageCoinLocal'],count=5)['granted'];c['StorageAltMaterialId']=bench('grant',owner=alt,category=6,local=c['StorageMaterialLocal'],count=5)['granted']
            tool_id=bench('grant',category=5,local=c['StorageToolLocal'],count=1)['granted'];bench('attribute',tool=tool_id,target=c['StorageSwordId'])
            mall=bench('storage_grant',owner=leader['AccountId'],owner_kind=2,storage_kind=4,category=1,local=c['StorageBoundLocal'],level=5,count=1)['state'];c['StorageMallId']=next(i['id'] for i in mall['items'] if i['storage_kind']==4)
            award=bench('storage_grant',owner_kind=1,storage_kind=9,category=6,local=c['StorageMaterialLocal'],count=4)['state'];c['StorageAwardId']=next(i['id'] for i in award['items'] if i['storage_kind']==9)
            main=bench('storage_status',owner_kind=1)['state'];rich=next(i for i in main['items'] if i['id']==c['StorageSwordId']);assert rich['attributes'] and rich['stones'][0]['state']==3;write(backup/'rich-before.json',rich)
            c['KeeperMap']='data/map/metin2_map_main_razor93/regen.json';target=root/c['KeeperMap'];manifest=read(root/'package-manifest.json');original=target.read_bytes();assert hashlib.sha256(original).hexdigest()==manifest['files'][c['KeeperMap']];world=json.loads(original);world['regens'].append(dict(type='mob',x=651,y=643,range_x=0,range_y=0,z=0,dir=0,time=3600,percent=100,count=1,vnum=9005));installed=(json.dumps(world,indent=2)+'\n').encode('utf-8');(backup/'regen-before.json').write_bytes(original);(backup/'regen-installed.json').write_bytes(installed);target.write_bytes(installed);manifest['files'][c['KeeperMap']]=hashlib.sha256(installed).hexdigest();write(root/'package-manifest.json',manifest)
            assert others(db,c)==before;write(fixture,c);print('STORAGE_FIXTURE=READY characters=3 accounts=2 guild=1 protected_items='+str(len(before['items'])))
    finally:db.close()
if __name__=='__main__':main()
