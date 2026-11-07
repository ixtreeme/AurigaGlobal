"""Seed and verify isolated localhost accounts for costume equipment, appearance and clock tests.

Uses the owned package only. Passwords, SQL backups and instance snapshots stay
in ignored private files; no existing player item is copied or reset.
"""
from collections import Counter
from datetime import datetime, timezone
from pathlib import Path
import argparse, copy, json, os, secrets, subprocess, sys

REPO = Path(__file__).resolve().parents[2]
sys.path[:0] = [str(REPO/'SRC/Tools/ServerManager'), str(REPO/'db/tools')]
import backend, item_bench, item_catalog
from mariadb_client import Client
from PetPassiveUnityLiveFixture import protected

def read(path): return json.loads(path.read_text(encoding='utf-8-sig'))
def write(path, value): path.write_text(json.dumps(value, indent=2)+'\n', encoding='utf-8')
def literal(value): return 'CONVERT(0x'+str(value).encode('utf-8').hex()+' USING utf8mb4)'

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=('seed', 'verify'))
    parser.add_argument('--package', type=Path, required=True)
    parser.add_argument('--fixture', type=Path, required=True)
    parser.add_argument('--template', type=Path)
    parser.add_argument('--log', type=Path)
    args = parser.parse_args(); fixture = args.fixture.resolve()
    assert fixture.is_relative_to(REPO/'build')
    root, manager, ownership, auth = item_bench.connection(args.package.resolve())
    os.environ['PATH'] = str(root/'bin')+os.pathsep+os.environ['PATH']
    tool = root/'manager/_internal/validators/ItemBenchTool.exe'
    catalog_tool = root/'manager/_internal/validators/CatalogEditorTool.exe'
    db = Client(**auth, library=str(root/'bin/libmariadb.dll'))
    try:
        if args.action == 'verify':
            config = read(fixture); backup = Path(config['Backup']).resolve()
            assert backup.parent == root/'run/deployments'
            assert all(a['Login']=='codexcompanion'+config['Token']+str(i) for i,a in enumerate(config['Accounts']))
            owners = [int(a['Pid']) for a in config['Accounts']]
            before = read(backup/'typed-before.json'); assert protected(db, owners)==before
            log = args.log.read_text(encoding='utf-8', errors='replace')
            assert 'METIN2_TYPED_COSTUME_LIVE=SUCCESS cases=12' in log
            owner=owners[0]
            state=item_bench.run_tool(dict(action='status',owner=owner,connection=auth),tool)['state']
            initial=read(backup/'owner-before.json');assert len(state['items'])==8
            for test in config['CostumeCases']:
                original=next(i for i in initial['items'] if i['category']==4 and i['local']==test['Local'])
                assert f"METIN2_TYPED_COSTUME_CASE=SUCCESS kind={test['Kind']} clock={test['Clock']} item={original['id']}" in log
                rows=db.query(f"SELECT destroyed,stack_count,HEX(costume_lifetime_blob) FROM player.item_instance WHERE owner_kind=1 AND owner_id={owner} AND instance_id={original['id']}")
                assert len(rows)==1;blob=bytes.fromhex(rows[0][2]);assert len(blob)==22 and blob[:4]==b'CTL\x01'
                if test['Clock']==1:
                    assert rows[0][:2]==['0','1'] and blob[4:]==bytes(18)
                    current=next(i for i in state['items'] if i['id']==original['id'])
                    assert current['bound'] and current['equipped_slot'] is None and current['level']==original['level'] and current['attributes']==original['attributes']
                else:
                    assert rows[0][:2]==['1','0'] and blob[4:6]==bytes([1,1])
                    assert f"COSTUME_PHASE=SUCCESS kind={test['Kind']} clock={test['Clock']} phase=worn-relog" in log
                    assert f"COSTUME_PHASE=SUCCESS kind={test['Kind']} clock={test['Clock']} phase=bag-relog" in log
                    if test['Clock']==4:assert int.from_bytes(blob[14:22],'little')==0
                    else:assert int.from_bytes(blob[6:14],'little')>0
            for local in (config['PetLocal'],config['MountLocal']):assert next(i for i in initial['items'] if i['category']==15 and i['local']==local)==next(i for i in state['items'] if i['category']==15 and i['local']==local)
            for field,slot in [('SwordLocal',1),('ArmorLocal',2),('DaggerLocal',None)]:assert next(i for i in state['items'] if i['local']==config[field] and i['category']==(2 if field=='ArmorLocal' else 1))['equipped_slot']==slot
            assert all('COSTUME_OUTFIT_PHASE=SUCCESS phase='+p in log for p in ('worn-relog','cleared-relog'))
            assert db.query(f'SELECT COUNT(*) FROM player.item WHERE owner_id IN ({owners[0]},{owners[1]})')==[['0']]
            print('COSTUME_LOCAL_SQL=SUCCESS cases=12 expired=9 permanent=3 preserved_items='+str(len(before['items'])))
            return
        assert args.template and not fixture.exists()
        template=read(args.template)
        assert template['Host']=='127.0.0.1' and len(template['Accounts'])==2
        assert all(a['Login']=='codexcompanion'+template['Token']+str(i) for i,a in enumerate(template['Accounts']))
        source=template['Accounts'][0]; source_pid=int(source['Pid'])
        assert db.query(f'SELECT account_id,name FROM player.player WHERE id={source_pid}')==[[str(source['AccountId']),source['Name']]]
        with manager.supervisor_lock():
            assert not backend.describe(root)['running']
            backup=root/'run/deployments'/datetime.now(timezone.utc).strftime('c28-fixture-%Y%m%dT%H%M%S%f');backup.mkdir()
            before=protected(db);write(backup/'typed-before.json',before)
            with (backup/'player-protos-before.sql').open('wb') as out:
                result=subprocess.run([str(Path(ownership['binary_dir'])/'mariadb-dump.exe'),'--defaults-extra-file='+str(root/'run/mariadb/admin.cnf'),'--protocol=tcp','--host=127.0.0.1','--port='+str(ownership['port']),'--single-transaction','--databases','player','protos'],stdout=out,stderr=subprocess.PIPE,creationflags=item_bench.NO_WINDOW,timeout=300)
                assert result.returncode==0 and out.tell()>0,'Owned SQL backup failed'
            initial=item_catalog.run_tool(dict(action='read',connection=auth),executable=catalog_tool)
            write(backup/'catalog-before.json',initial)
            config={k:template[k] for k in ('Host','ServerName','Pin','Version','AuthPort','GamePort','PetLocal','MountLocal','PetRace','MountRace')}
            token=secrets.token_hex(4)
            config.update(Token=token,Accounts=[],MapIndex=41,X=986600,Y=269100,RidingRace=template['MountRace'],CostumeCases=[],CostumePreview=str(REPO/'build/c28-costumes-visual.png'),Backup=str(backup))
            editor=item_catalog.Catalog(initial);steps=[]
            def publish(doc):
                category=int(doc['base']['category_id']);local=int(doc['base']['local_id'])
                doc['base'].update(binding_policy='3' if category==4 else '2');doc['trade'].update(allow_trade='0',allow_drop='0',allow_sell='0')
                steps.append(editor.apply(doc));editor.packet['sequences'][str(category)]=str(local+1)
            for field,category,subtype,model in [('SwordLocal',1,'1','Metin2Equipment/w19'),('DaggerLocal',1,'2','Metin2Equipment/w1000'),('ArmorLocal',2,None,'Metin2Equipment/a0_1')]:
                doc=editor.new(category);config[field]=int(doc['base']['local_id']);code=f'costume_probe.{token}_{field.lower()}'
                doc['base'].update(content_code=code,name_key='item.'+code+'.name',description_key='item.'+code+'.description')
                doc['appearance']=dict(model_asset=model)
                if category==1:doc['child'].update(weapon_subtype=subtype,physical_attack_base_raw='1000')
                else:doc['child']['physical_defense_base_raw']='1000'
                publish(doc)
            race={(1,1):0,(1,2):4,(2,1):5,(2,2):1,(3,1):2,(3,2):6,(4,1):7,(4,2):3}
            for kind in (1,2,3):
                for clock in (1,2,3,4):
                    duration=0 if clock==1 else 180000 if clock==2 else 60000 if clock==3 else 20000
                    doc=editor.new(4);local=int(doc['base']['local_id']);code=f'costume.c28_{token}_{kind}_{clock}'
                    doc['base'].update(content_code=code,name_key='item.'+code+'.name',description_key='item.'+code+'.description')
                    doc['child'].update(costume_kind=str(kind),weapon_subtype='1' if kind==3 else None,clock_kind=str(clock),duration_ms=str(duration))
                    doc['visuals']=[dict(character_class=str(c),sex=str(sex),model_asset=f'Metin2Equipment/a{r}_4' if kind==1 else f'Metin2Equipment/h{r}_1' if kind==2 else 'Metin2Equipment/w10') for (c,sex),r in race.items()]
                    for visual in doc['visuals']:assert (Path('D:/Auriga-Unity6/SRC/AurigaUnity-20261103/Assets/Metin2/AddressableContent/Equipment')/(visual['model_asset'].split('/')[-1]+'.asset')).is_file()
                    doc['bonuses']=[dict(bonus_id='29',value_scaled='3'),dict(bonus_id='1',value_scaled='1000')]
                    doc['refinement'].update(profile_kind='6',max_level='1')
                    level=copy.deepcopy(doc['levels'][0]);level.update(level='1',bonuses=[dict(bonus_id='29',value_scaled='2'),dict(bonus_id='1',value_scaled='2000')]);doc['levels'].append(level)
                    publish(doc)
                    config['CostumeCases'].append(dict(Local=local,Kind=kind,Clock=clock,DurationMs=duration,Model='Metin2Equipment/a0_4' if kind==1 else 'Metin2Equipment/h0_1' if kind==2 else 'Metin2Equipment/w10'))
            desired=steps[-1]
            for name,rows in initial['tables'].items():assert not (Counter(map(tuple,rows))-Counter(map(tuple,desired[name]))),name
            planned=item_catalog.run_tool(dict(action='validate',tables=desired),initial['columns'],catalog_tool)
            published=initial
            for step in steps:
                item_catalog.run_tool(dict(action='commit',connection=auth,tables=step,expected_hash=published['hash']),initial['columns'],catalog_tool)
                published=item_catalog.run_tool(dict(action='read',connection=auth),executable=catalog_tool)
            assert published['hash']==planned['hash']
            columns=[c[0] for c in db.query('SHOW COLUMNS FROM player.player') if c[0]!='id' and 'GENERATED' not in c[5]]
            assert all(c.isascii() and c.replace('_','').isalnum() for c in columns)
            db.query('START TRANSACTION')
            try:
                for index in range(2):
                    login='codexcompanion'+token+str(index);password=secrets.token_hex(8);name='CF'+token+str(index)
                    assert not db.query('SELECT id FROM account.account WHERE BINARY login='+literal(login))
                    db.query('INSERT INTO account.account(login,password,social_id,status,create_time) VALUES('+literal(login)+',PASSWORD('+literal(password)+"),'1234567','OK',NOW())")
                    aid=int(db.query('SELECT LAST_INSERT_ID()')[0][0])
                    overrides=dict(job='0',account_id=str(aid),name=name,map_index='41',x=str(config['X']+index*100),y=str(config['Y']),z='0',gold='0',hp='1000',mp='20',typed_book_revision='0',horse_level='0',horse_riding='0')
                    projection=[literal(overrides[c]) if c in overrides else '`'+c+'`' for c in columns]
                    db.query('INSERT INTO player.player ('+','.join('`'+c+'`' for c in columns)+') SELECT '+','.join(projection)+f' FROM player.player WHERE id={source_pid}')
                    pid=int(db.query('SELECT LAST_INSERT_ID()')[0][0]);db.query(f'INSERT INTO player.player_index(id,pid1,empire) VALUES({aid},{pid},1)')
                    config['Accounts'].append(dict(Login=login,Password=password,Name=name,AccountId=aid,Pid=pid))
                db.query('COMMIT')
            except BaseException:
                db.query('ROLLBACK');raise
            owner=config['Accounts'][0]['Pid']
            write(backup/'fixture-pending.private.json',config)
            def bench(action,**fields):return item_bench.run_tool(dict(action=action,owner=owner,connection=auth,**fields),tool)
            bench('setup',gold=0)
            for local in (config['PetLocal'],config['MountLocal']):bench('grant',category=15,local=local,count=1)
            for field,category in [('SwordLocal',1),('DaggerLocal',1),('ArmorLocal',2)]:bench('grant',category=category,local=config[field],count=1)
            for test in config['CostumeCases']:bench('grant',category=4,local=test['Local'],count=1,level=1)
            state=bench('status')['state'];assert len(state['items'])==17
            for field,category in [('SwordLocal',1),('ArmorLocal',2)]:bench('equip',item=next(i for i in state['items'] if i['category']==category and i['local']==config[field])['id'])
            state=bench('status')['state']
            assert protected(db,[a['Pid'] for a in config['Accounts']])==before
            write(backup/'owner-before.json',state);write(fixture,config)
            print('COSTUME_FIXTURE=READY cases=12 items=17 preserved_items='+str(len(before['items'])))
    finally:db.close()

if __name__=='__main__':main()
