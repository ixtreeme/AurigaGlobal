"""Seed/verify isolated local typed forge aid Unity accounts."""
from pathlib import Path
from datetime import datetime,timezone
from collections import Counter
import argparse,copy,json,os,re,secrets,subprocess,sys,hashlib,time
REPO=Path(__file__).resolve().parents[2]
sys.path[:0]=[str(REPO/'SRC/Tools/ServerManager'),str(REPO/'db/tools')]
import item_bench,item_catalog,backend
from mariadb_client import Client
from PetPassiveUnityLiveFixture import protected
from CostumeUnityLiveFixture import read,write,literal

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('action',choices=('seed','verify','restore'))
 for name in ('package','fixture'):p.add_argument('--'+name,type=Path,required=True)
 for name in ('template','log'):p.add_argument('--'+name,type=Path)
 a=p.parse_args();fixture=a.fixture.resolve();assert fixture.is_relative_to(REPO/'build')
 root,manager,ownership,auth=item_bench.connection(a.package.resolve());os.environ['PATH']=str(root/'bin')+os.pathsep+os.environ['PATH']
 tool=root/'manager/_internal/validators/ItemBenchTool.exe';catalog_tool=root/'manager/_internal/validators/CatalogEditorTool.exe';db=Client(**auth,library=str(root/'bin/libmariadb.dll'))
 try:
  if a.action=='restore':
   c=read(fixture);backup=Path(c['Backup'])
   with manager.supervisor_lock():
    assert not backend.describe(root)['running'];target=root/c['ProbeMap'];assert target.read_bytes()==(backup/'regen-installed.json').read_bytes();manifest=read(root/'package-manifest.json');assert hashlib.sha256(target.read_bytes()).hexdigest()==manifest['files'][c['ProbeMap']]
    target.write_bytes((backup/'regen-before.json').read_bytes());manifest['files'][c['ProbeMap']]=hashlib.sha256(target.read_bytes()).hexdigest();write(root/'package-manifest.json',manifest)
   print('FORGE_SMITH_RESTORED');return
  if a.action=='verify':
   c=read(fixture);owners=[int(a['Pid']) for a in c['Accounts']];backup=Path(c['Backup']);before=read(backup/'typed-before.json');assert protected(db,owners)==before
   log=a.log.read_text(encoding='utf-8',errors='replace');assert 'METIN2_TYPED_FORGE_LIVE=SUCCESS' in log
   state=item_bench.run_tool(dict(action='status',owner=owners[0],connection=auth),tool)['state'];original=read(backup/'owner-before.json');items=state['items']
   for case in c['ForgeCases']:
    old=next(i for i in original['items'] if i['category']==case['Category'] and i['local']==case['TargetLocal']);now=[i for i in items if i['id']==old['id']]
    if case['ResultLevel']<0:assert not now;assert db.query(f"SELECT destroyed,stack_count FROM player.item_instance WHERE instance_id={old['id']}")==[['1','0']]
    else:assert len(now)==1 and now[0]['level']==case['ResultLevel']
   assert sum(i['count'] for i in items if i['category']==6 and i['local']==c['MaterialLocal'])==100-len(c['ForgeCases'])*3
   assert next(w['balance'] for w in state['wallets'] if w['currency']==1)==10000-len(c['ForgeCases'])*100
   targets=[i['id'] for i in original['items'] if any(i['category']==case['Category'] and i['local']==case['TargetLocal'] for case in c['ForgeCases'])];target_hex=','.join(literal(int(i).to_bytes(8,'little').hex().upper()) for i in targets)
   assert db.query('SELECT COUNT(*) FROM player.item_operation_receipt WHERE outcome_code=12801 AND HEX(SUBSTRING(result_payload,5,8)) IN ('+target_hex+')')==[[str(len(c['ForgeCases']))]]
   for aid in c['ForgeAids']:
    uses=sum((case['AidLocal']==aid) + (aid in case['Boosters']) for case in c['ForgeCases']);assert sum(i['count'] for i in items if i['category']==22 and i['local']==aid)==20-uses
   write(backup/'owner-after.json',state);assert db.query(f'SELECT COUNT(*) FROM player.item WHERE owner_id IN ({owners[0]},{owners[1]})')==[['0']]
   print('FORGE_LOCAL_SQL=SUCCESS cases='+str(len(c['ForgeCases']))+' protected_items='+str(len(before['items'])));return
  assert a.template and not fixture.exists();template=read(a.template);source=template['Accounts'][0];source_pid=int(source['Pid'])
  assert template['Host']=='127.0.0.1' and len(template['Accounts'])==2 and source['Login']=='codexcompanion'+template['Token']+'0'
  assert db.query(f'SELECT account_id,name FROM player.player WHERE id={source_pid}')==[[str(source['AccountId']),source['Name']]]
  with manager.supervisor_lock():
   assert not backend.describe(root)['running'];backup=root/'run/deployments'/datetime.now(timezone.utc).strftime('c32-fixture-%Y%m%dT%H%M%S%f');backup.mkdir()
   before=protected(db);write(backup/'typed-before.json',before)
   with (backup/'player-protos-before.sql').open('wb') as out:
    result=subprocess.run([str(Path(ownership['binary_dir'])/'mariadb-dump.exe'),'--defaults-extra-file='+str(root/'run/mariadb/admin.cnf'),'--protocol=tcp','--host=127.0.0.1','--port='+str(ownership['port']),'--single-transaction','--databases','player','protos'],stdout=out,stderr=subprocess.PIPE,creationflags=item_bench.NO_WINDOW,timeout=300);assert result.returncode==0 and out.tell()>0
   initial=item_catalog.run_tool(dict(action='read',connection=auth),executable=catalog_tool);write(backup/'catalog-before.json',initial)
   c={k:template[k] for k in ('Host','ServerName','Pin','Version','AuthPort','GamePort','PetLocal','MountLocal','PetRace','MountRace')};token=secrets.token_hex(4)
   c.update(Token=token,Accounts=[],MapIndex=41,X=986600,Y=269100,RidingRace=template['MountRace'],Forge=True,ForgeCases=[],Backup=str(backup))
   editor=item_catalog.Catalog(initial);steps=[]
   def publish(doc,label):
    category=int(doc['base']['category_id']);local=int(doc['base']['local_id']);code=f'forge.c32_{token}_{category}_{label}'
    doc['base'].update(content_code=code,name_key='item.'+code+'.name',description_key='item.'+code+'.description',binding_policy='2');doc['trade'].update(allow_trade='0',allow_drop='0',allow_sell='0');steps.append(editor.apply(doc));editor.packet['sequences'][str(category)]=str(local+1);return local
   doc=editor.new(6);c['MaterialLocal']=publish(doc,'material')
   c['ForgeAids']=[];aids={}
   for preset in ('blessing','metal','dragon_god','smith_book','musin','memo','black_dragon','low','medium','high'):
    doc=editor.new(22);row,levels=editor.forge_defaults(preset);doc['child'].update(row);doc['forge_levels']=levels
    # Deterministic failure profile for live stone downgrade; native tests cover 80% boundaries.
    if preset=='black_dragon':doc['child']['chance_bp']='0'
    aids[preset]=publish(doc,preset);c['ForgeAids'].append(aids[preset])
   for label,category,preset,start,finish,rate,boost in [('blessing',1,'blessing',9,8,0,False),('metal',1,'metal',9,-1,0,False),('dragon',1,'dragon_god',0,1,0,False),('smith',1,'smith_book',1,2,0,False),('musin',1,'musin',9,10,0,False),('memo',1,'memo',0,1,0,False),('stone',7,'black_dragon',4,3,10000,False),('boosters',1,None,9,10,7000,True),('combined',1,'dragon_god',0,1,0,True),('armor',2,'musin',1,2,0,False),('accessory',3,'musin',9,10,0,False)]:
    doc=editor.new(category);maximum=max(10,start+1);doc['refinement']['max_level']=str(maximum);doc['levels']=[dict(level=str(j),**{k:'0' for k in item_catalog.GAINS},extra_bonus_profile_id=None,bonuses=[]) for j in range(maximum+1)]
    doc['recipes']=[dict(recipe_id=None,code=None,source_level=str(start),target_level=str(start+1),target_category_id=None,target_local_id=None,gold_cost='100',success_chance_bp=str(rate),failure_kind='1',failure_level=None,ingredients=[dict(category_id='6',local_id=str(c['MaterialLocal']),refinement_level='0',amount='3')])]
    local=publish(doc,label);rr=next(r for r in editor.tables['refinement_recipe'] if int(r['source_category_id'])==category and int(r['source_local_id'])==local)
    chance=0 if label in ('blessing','metal','stone') else 10000
    c['ForgeCases'].append(dict(Label=label,Category=category,TargetLocal=local,AidLocal=aids[preset] if preset else 0,StartLevel=start,ResultLevel=finish,Recipe=int(rr['recipe_id']),Chance=chance,Boosters=[aids[g] for g in ('low','medium','high')] if boost else [0,0,0]))
   desired=steps[-1]
   for name,rows in initial['tables'].items():assert not (Counter(map(tuple,rows))-Counter(map(tuple,desired[name]))),name
   planned=item_catalog.run_tool(dict(action='validate',tables=desired),initial['columns'],catalog_tool);published=initial
   for step in steps:
    item_catalog.run_tool(dict(action='commit',connection=auth,tables=step,expected_hash=published['hash']),initial['columns'],catalog_tool);published=item_catalog.run_tool(dict(action='read',connection=auth),executable=catalog_tool)
   assert published['hash']==planned['hash']
   columns=[r[0] for r in db.query('SHOW COLUMNS FROM player.player') if r[0]!='id' and 'GENERATED' not in r[5]];assert all(k.isascii() and k.replace('_','').isalnum() for k in columns)
   db.query('START TRANSACTION')
   try:
    for index in range(2):
     login='codexcompanion'+token+str(index);password=secrets.token_hex(8);name='FG'+token+str(index);db.query('INSERT INTO account.account(login,password,social_id,status,create_time) VALUES('+literal(login)+',PASSWORD('+literal(password)+"),'1234567','OK',NOW())");aid=int(db.query('SELECT LAST_INSERT_ID()')[0][0]);overrides=dict(job='0',level='50',st='60',ht='4',dx='3',iq='3',random_hp='30000',random_sp='10000',skill_group='1',account_id=str(aid),name=name,map_index='41',x=str(c['X']+index*100),y=str(c['Y']),z='0',gold='0',hp='30000',mp='10000',typed_book_revision='0',horse_level='0',horse_riding='0');skill=bytearray(2550);skill[10]=1;skill[11]=20;projection=['0x'+skill.hex() if k=='skill_level' else 'REPEAT(CHAR(0),OCTET_LENGTH(`quickslot`))' if k=='quickslot' else literal(overrides[k]) if k in overrides else '`'+k+'`' for k in columns];db.query('INSERT INTO player.player('+','.join('`'+k+'`' for k in columns)+') SELECT '+','.join(projection)+f' FROM player.player WHERE id={source_pid}');pid=int(db.query('SELECT LAST_INSERT_ID()')[0][0]);db.query(f'INSERT INTO player.player_index(id,pid1,empire) VALUES({aid},{pid},1)');c['Accounts'].append(dict(Login=login,Password=password,Name=name,AccountId=aid,Pid=pid))
    db.query('COMMIT')
   except BaseException:db.query('ROLLBACK');raise
   write(backup/'fixture-pending.private.json',c);owner=c['Accounts'][0]['Pid']
   def bench(action,**fields):return item_bench.run_tool(dict(action=action,owner=owner,connection=auth,**fields),tool)
   bench('setup',gold=10000)
   for local in (c['PetLocal'],c['MountLocal']):bench('grant',category=15,local=local,count=1)
   for case in c['ForgeCases']:bench('grant',category=case['Category'],local=case['TargetLocal'],level=case['StartLevel'],count=1)
   for local in c['ForgeAids']:bench('grant',category=22,local=local,count=20)
   bench('grant',category=6,local=c['MaterialLocal'],count=100)
   assert db.query('SELECT vnum FROM player.mob_proto WHERE vnum=20016')==[['20016']]
   c['ProbeMap']='data/map/metin2_map_main_razor93/regen.json';target=root/c['ProbeMap'];manifest=read(root/'package-manifest.json');original=target.read_bytes();assert hashlib.sha256(original).hexdigest()==manifest['files'][c['ProbeMap']];world=json.loads(original);world['regens'].append(dict(type='mob',x=651,y=643,range_x=0,range_y=0,z=0,dir=0,time=3600,percent=100,count=1,vnum=20016));installed=(json.dumps(world,indent=2)+'\n').encode('utf-8');(backup/'regen-before.json').write_bytes(original);(backup/'regen-installed.json').write_bytes(installed);target.write_bytes(installed);manifest['files'][c['ProbeMap']]=hashlib.sha256(installed).hexdigest();write(root/'package-manifest.json',manifest)
   state=bench('status')['state'];assert protected(db,[a['Pid'] for a in c['Accounts']])==before;write(backup/'owner-before.json',state);write(fixture,c)
   print('FORGE_FIXTURE=READY cases='+str(len(c['ForgeCases']))+' protected_items='+str(len(before['items'])))
 finally:db.close()
if __name__=='__main__':main()
