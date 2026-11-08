"""Seed/verify isolated local independent ammunition Unity accounts."""
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
    original=(backup/'regen-before.json').read_bytes();target.write_bytes(original);manifest['files'][c['ProbeMap']]=hashlib.sha256(original).hexdigest();write(root/'package-manifest.json',manifest)
    columns=[r[0] for r in db.query('SHOW COLUMNS FROM player.mob_proto')];assert db.query('SELECT '+','.join('`'+k+'`' for k in columns)+' FROM player.mob_proto WHERE vnum=65076')==read(backup/'probe-proto.json');db.query('DELETE FROM player.mob_proto WHERE vnum=65076')
   print('AMMO_PROBE_RESTORED');return
  if a.action=='verify':
   c=read(fixture);owners=[int(x['Pid']) for x in c['Accounts']];backup=Path(c['Backup']);before=read(backup/'typed-before.json');assert protected(db,owners)==before
   log=a.log.read_text(encoding='utf-8',errors='replace');assert 'METIN2_TYPED_AMMO_LIVE=SUCCESS' in log
   state=item_bench.run_tool(dict(action='status',owner=owners[0],connection=auth),tool)['state'];original=read(backup/'owner-before.json');items=state['items'];assert not any(i['category']==21 and i['local']==c['ArrowLocal'] for i in items)
   assert sum(i['count'] for i in items if i['category']==21 and i['local']==c['WrongArrowLocal'])==2
   for local in (c['BowLocal'],c['RedSoulLocal'],c['BlueSoulLocal']):assert next(i['id'] for i in items if i['local']==local and i['category'] in (1,14)) in [i['id'] for i in original['items']]
   events=db.query(f'SELECT event_id,bow_id,ammo_id,reserve_id,units,input_damage_raw,output_damage_raw FROM player.item_ammo_shot_event WHERE owner_kind=1 AND owner_id={owners[0]}');assert len(events)==9 and sum(int(r[4]) for r in events)==9;assert sum(r[3]!='0' for r in events)==2
   observed=Counter(int(x) for x in re.findall(r'AMMO_HIT_SAVED amount=(\d+)',log));assert observed==Counter(int(r[6]) for r in events)
   assert db.query(f'SELECT COUNT(*) FROM player.item_soul_strike_event WHERE owner_id={owners[0]}')==[['7']]
   arrows=[i['id'] for i in original['items'] if i['category']==21 and i['local']==c['ArrowLocal']];assert len(arrows)==3
   assert db.query('SELECT COUNT(*) FROM player.item_instance WHERE instance_id IN('+','.join(str(i) for i in arrows)+') AND destroyed=1 AND stack_count=0 AND storage_kind IS NULL')==[['3']]
   query=f'SELECT instance_id,state_revision,HEX(state_sha256) FROM player.item_instance WHERE owner_kind=1 AND owner_id={owners[0]} ORDER BY instance_id';saved=db.query(query);time.sleep(1);assert db.query(query)==saved
   assert db.query(f'SELECT COUNT(*) FROM player.item WHERE owner_id IN ({owners[0]},{owners[1]})')==[['0']];write(backup/'owner-after.json',state);write(backup/'ammo-events.json',events)
   print('AMMO_LOCAL_SQL=SUCCESS events=9 protected_items='+str(len(before['items'])));return
  assert a.template and not fixture.exists();template=read(a.template);source=template['Accounts'][0];source_pid=int(source['Pid'])
  assert template['Host']=='127.0.0.1' and len(template['Accounts'])==2 and source['Login']=='codexcompanion'+template['Token']+'0'
  assert db.query(f'SELECT account_id,name FROM player.player WHERE id={source_pid}')==[[str(source['AccountId']),source['Name']]]
  with manager.supervisor_lock():
   assert not backend.describe(root)['running'];backup=root/'run/deployments'/datetime.now(timezone.utc).strftime('c31-fixture-%Y%m%dT%H%M%S%f');backup.mkdir()
   before=protected(db);write(backup/'typed-before.json',before)
   with (backup/'player-protos-before.sql').open('wb') as out:
    result=subprocess.run([str(Path(ownership['binary_dir'])/'mariadb-dump.exe'),'--defaults-extra-file='+str(root/'run/mariadb/admin.cnf'),'--protocol=tcp','--host=127.0.0.1','--port='+str(ownership['port']),'--single-transaction','--databases','player','protos'],stdout=out,stderr=subprocess.PIPE,creationflags=item_bench.NO_WINDOW,timeout=300);assert result.returncode==0 and out.tell()>0
   initial=item_catalog.run_tool(dict(action='read',connection=auth),executable=catalog_tool);write(backup/'catalog-before.json',initial)
   c={k:template[k] for k in ('Host','ServerName','Pin','Version','AuthPort','GamePort','PetLocal','MountLocal','PetRace','MountRace')};token=secrets.token_hex(4)
   c.update(Token=token,Accounts=[],MapIndex=41,X=986600,Y=269100,RidingRace=template['MountRace'],Ammo=True,SoulProbeRace=65076,SoulSkill=46,Backup=str(backup))
   editor=item_catalog.Catalog(initial);steps=[]
   def publish(doc,label):
    category=int(doc['base']['category_id']);local=int(doc['base']['local_id']);code=f'ammo.c31_{token}_{label}'
    doc['base'].update(content_code=code,name_key='item.'+code+'.name',description_key='item.'+code+'.description',binding_policy='2');doc['trade'].update(allow_trade='0',allow_drop='0',allow_sell='0');steps.append(editor.apply(doc));editor.packet['sequences'][str(category)]=str(local+1);return local
   for field,category in [('BowLocal',1),('ArmorLocal',2)]:
    doc=editor.new(category)
    if category==1:doc['child'].update(weapon_subtype='3',physical_attack_base_raw='500000');doc['attack']['reach_millimeters']='30000';doc['ammo'].update(ammo_kind='2',units_per_hit='1')
    else:doc['child']['physical_defense_base_raw']='1000'
    c[field]=publish(doc,field.lower())
   bow_policy=next(r['ammo_policy_id'] for r in editor.tables['item_weapon'] if int(r['local_id'])==c['BowLocal'])
   for field in ('ArrowLocal','WrongArrowLocal'):
    doc=editor.new(21);doc['child']['ammo_policy_id']=bow_policy if field=='ArrowLocal' else editor.profile('item_ammo_policy',dict(ammo_kind='2',units_per_hit='2'),None,idcol='policy_id');doc['base']['max_stack']='3';c[field]=publish(doc,field.lower())
   for field,channel in [('RedSoulLocal',1),('BlueSoulLocal',2)]:
    doc=editor.new(14);doc['child']['damage_channel']=str(channel);doc['refinement']['max_level']='0';doc['levels']=[dict(level='0',**{k:'0' for k in item_catalog.GAINS},extra_bonus_profile_id=None,bonuses=[])];row=editor.soul_level(0);row.update(charge_capacity='6',cycle_charge_cost='2',strikes_per_cycle='2',initial_charge='6',initial_strikes='2',recharge_interval_ms='600000',first_multiplier_bp='15000',second_multiplier_bp='18000',third_multiplier_bp='25000');doc['soul_levels']=[row];c[field]=publish(doc,field.lower())
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
     login='codexcompanion'+token+str(index);password=secrets.token_hex(8);name='AM'+token+str(index);db.query('INSERT INTO account.account(login,password,social_id,status,create_time) VALUES('+literal(login)+',PASSWORD('+literal(password)+"),'1234567','OK',NOW())");aid=int(db.query('SELECT LAST_INSERT_ID()')[0][0]);overrides=dict(job='1',level='50',st='60',ht='4',dx='3',iq='3',random_hp='30000',random_sp='10000',skill_group='2',account_id=str(aid),name=name,map_index='41',x=str(c['X']+index*100),y=str(c['Y']),z='0',gold='0',hp='30000',mp='10000',typed_book_revision='0',horse_level='0',horse_riding='0');skill=bytearray(2550);skill[460]=1;skill[461]=20;projection=['0x'+skill.hex() if k=='skill_level' else 'REPEAT(CHAR(0),OCTET_LENGTH(`quickslot`))' if k=='quickslot' else literal(overrides[k]) if k in overrides else '`'+k+'`' for k in columns];db.query('INSERT INTO player.player('+','.join('`'+k+'`' for k in columns)+') SELECT '+','.join(projection)+f' FROM player.player WHERE id={source_pid}');pid=int(db.query('SELECT LAST_INSERT_ID()')[0][0]);db.query(f'INSERT INTO player.player_index(id,pid1,empire) VALUES({aid},{pid},1)');c['Accounts'].append(dict(Login=login,Password=password,Name=name,AccountId=aid,Pid=pid))
    db.query('COMMIT')
   except BaseException:db.query('ROLLBACK');raise
   write(backup/'fixture-pending.private.json',c);owner=c['Accounts'][0]['Pid']
   def bench(action,**fields):return item_bench.run_tool(dict(action=action,owner=owner,connection=auth,**fields),tool)
   bench('setup',gold=0)
   for local in (c['PetLocal'],c['MountLocal']):bench('grant',category=15,local=local,count=1)
   for field,category,count in [('BowLocal',1,1),('ArmorLocal',2,1),('ArrowLocal',21,9),('WrongArrowLocal',21,2),('RedSoulLocal',14,1),('BlueSoulLocal',14,1)]:
    while count:units=min(count,3);bench('grant',category=category,local=c[field],count=units);count-=units
   state=bench('status')['state'];assert len(state['items'])==10
   bench('equip',item=next(i['id'] for i in state['items'] if i['category']==2))
   assert not db.query('SELECT vnum FROM player.mob_proto WHERE vnum=65076');columns=[r[0] for r in db.query('SHOW COLUMNS FROM player.mob_proto')];overrides=dict(vnum=65076,name='SoulCombatProbe',locale_name='SoulCombatProbe',type=0,rank=0,battle_type=0,level=50,ai_flag=2,setRaceFlag=0,setImmuneFlag=0,st=0,dx=0,ht=0,iq=0,damage_min=0,damage_max=0,max_hp=100000000,regen_cycle=255,regen_percent=0,exp=0,gold_min=0,gold_max=0,attack_speed=100,move_speed=0,aggressive_hp_pct=0,aggressive_sight=0,attack_range=0,resurrection_vnum=0,drop_item=0,summon=0,drain_sp=0,dam_multiply=1);overrides.update({k:0 for k in columns if k.startswith(('enchant_','resist_','skill_','sp_'))});overrides['def']=0
   db.query('INSERT INTO player.mob_proto('+','.join('`'+k+'`' for k in columns)+') SELECT '+','.join(literal(overrides[k]) if k in overrides else '`'+k+'`' for k in columns)+' FROM player.mob_proto WHERE vnum=103');write(backup/'probe-proto.json',db.query('SELECT '+','.join('`'+k+'`' for k in columns)+' FROM player.mob_proto WHERE vnum=65076'))
   c['ProbeMap']='data/map/metin2_map_main_razor93/regen.json';target=root/c['ProbeMap'];manifest=read(root/'package-manifest.json');original=target.read_bytes();assert hashlib.sha256(original).hexdigest()==manifest['files'][c['ProbeMap']];world=json.loads(original);world['regens'].append(dict(type='mob',x=651,y=643,range_x=0,range_y=0,z=0,dir=0,time=3600,percent=100,count=1,vnum=65076));installed=(json.dumps(world,indent=2)+'\n').encode('utf-8');(backup/'regen-before.json').write_bytes(original);(backup/'regen-installed.json').write_bytes(installed);target.write_bytes(installed);manifest['files'][c['ProbeMap']]=hashlib.sha256(installed).hexdigest();write(root/'package-manifest.json',manifest)
   state=bench('status')['state'];assert protected(db,[a['Pid'] for a in c['Accounts']])==before;write(backup/'owner-before.json',state);write(fixture,c)
   print('AMMO_FIXTURE=READY items=10 arrows=9 protected_items='+str(len(before['items'])))
 finally:db.close()
if __name__=='__main__':main()
