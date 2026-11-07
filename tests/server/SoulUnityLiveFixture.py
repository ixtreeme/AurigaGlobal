"""Seed/verify isolated local independent soul Unity accounts."""
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
   print('SOUL_PROBE_RESTORED');return
  if a.action=='verify':
   c=read(fixture);owners=[int(a['Pid']) for a in c['Accounts']];backup=Path(c['Backup']);before=read(backup/'typed-before.json');assert protected(db,owners)==before
   log=a.log.read_text(encoding='utf-8',errors='replace');assert 'METIN2_TYPED_SOUL_LIVE=SUCCESS' in log
   for phase in ('charged','failed-refine','refined-relog','active-relog','red-exhausted','blue-hit','inactive-relog'):assert 'SOUL_PHASE=SUCCESS phase='+phase in log,phase
   state=item_bench.run_tool(dict(action='status',owner=owners[0],connection=auth),tool)['state'];original=read(backup/'owner-before.json');items=state['items']
   for field in ('RedSoulLocal','BlueSoulLocal'):
    i=next(i for i in items if i['category']==14 and i['local']==c[field]);assert i['id']==next(i['id'] for i in original['items'] if i['category']==14 and i['local']==c[field]);assert i['level']==1 and not i['soul']['active']
   red=next(i for i in items if i['category']==14 and i['local']==c['RedSoulLocal']);blue=next(i for i in items if i['category']==14 and i['local']==c['BlueSoulLocal']);assert red['soul']['charge']==0 and red['soul']['strikes']==2;assert blue['soul']['charge']==6 and blue['soul']['strikes']==1
   hit_rows=db.query(f'SELECT e.event_id,e.output_damage_raw,HEX(r.result_payload) FROM player.item_soul_strike_event e JOIN player.item_operation_receipt r USING(operation_id) WHERE e.owner_kind=1 AND e.owner_id={owners[0]}');assert len(hit_rows)==7
   observed=Counter(int(x) for x in re.findall(r'SOUL_HIT_SAVED channel=\d+ amount=(\d+)',log));assert observed==Counter(int(row[1]) for row in hit_rows)
   for event,damage,raw in hit_rows:
    receipt=bytes.fromhex(raw);assert len(receipt)==84 and int.from_bytes(receipt[40:48],'little')==int(event);input_damage=int.from_bytes(receipt[64:72],'little');multiplier=int.from_bytes(receipt[80:84],'little');assert int(damage)==input_damage*multiplier//10000
   for field,count in (('SoulScrollLocal',2),('SoulFailLocal',3)):assert sum(i['count'] for i in items if i['category']==14 and i['local']==c[field])==count
   query=f'SELECT instance_id,HEX(soul_blob),HEX(state_sha256),state_revision FROM player.item_instance WHERE owner_kind=1 AND owner_id={owners[0]} ORDER BY instance_id';saved=db.query(query);time.sleep(1);assert db.query(query)==saved,'offline time never recharges';write(backup/'owner-after.json',state);write(backup/'combat-after.json',hit_rows)
   assert db.query(f'SELECT COUNT(*) FROM player.item WHERE owner_id IN ({owners[0]},{owners[1]})')==[['0']]
   print('SOUL_LOCAL_SQL=SUCCESS events=7 protected_items='+str(len(before['items'])));return
  assert a.template and not fixture.exists();template=read(a.template);source=template['Accounts'][0];source_pid=int(source['Pid'])
  assert template['Host']=='127.0.0.1' and len(template['Accounts'])==2 and source['Login']=='codexcompanion'+template['Token']+'0'
  assert db.query(f'SELECT account_id,name FROM player.player WHERE id={source_pid}')==[[str(source['AccountId']),source['Name']]]
  with manager.supervisor_lock():
   assert not backend.describe(root)['running'];backup=root/'run/deployments'/datetime.now(timezone.utc).strftime('c30-fixture-%Y%m%dT%H%M%S%f');backup.mkdir()
   before=protected(db);write(backup/'typed-before.json',before)
   with (backup/'player-protos-before.sql').open('wb') as out:
    result=subprocess.run([str(Path(ownership['binary_dir'])/'mariadb-dump.exe'),'--defaults-extra-file='+str(root/'run/mariadb/admin.cnf'),'--protocol=tcp','--host=127.0.0.1','--port='+str(ownership['port']),'--single-transaction','--databases','player','protos'],stdout=out,stderr=subprocess.PIPE,creationflags=item_bench.NO_WINDOW,timeout=300);assert result.returncode==0 and out.tell()>0
   initial=item_catalog.run_tool(dict(action='read',connection=auth),executable=catalog_tool);write(backup/'catalog-before.json',initial)
   c={k:template[k] for k in ('Host','ServerName','Pin','Version','AuthPort','GamePort','PetLocal','MountLocal','PetRace','MountRace')};token=secrets.token_hex(4)
   c.update(Token=token,Accounts=[],MapIndex=41,X=986600,Y=269100,RidingRace=template['MountRace'],Souls=True,SoulProbeRace=65076,SoulSkill=1,Backup=str(backup))
   editor=item_catalog.Catalog(initial);steps=[]
   def publish(doc,label):
    category=int(doc['base']['category_id']);local=int(doc['base']['local_id']);code=f'soul.c30_{token}_{label}'
    doc['base'].update(content_code=code,name_key='item.'+code+'.name',description_key='item.'+code+'.description',binding_policy='2');doc['trade'].update(allow_trade='0',allow_drop='0',allow_sell='0');steps.append(editor.apply(doc));editor.packet['sequences'][str(category)]=str(local+1);return local
   for field,category in [('SwordLocal',1),('ArmorLocal',2)]:
    doc=editor.new(category)
    if category==1:doc['child'].update(weapon_subtype='1',physical_attack_base_raw='500000');doc['attack']['reach_millimeters']='30000'
    else:doc['child']['physical_defense_base_raw']='1000'
    c[field]=publish(doc,field.lower())
   for field,channel in [('RedSoulLocal',1),('BlueSoulLocal',2),('SpareSoulLocal',1),('ClockSoulLocal',2)]:
    doc=editor.new(14);doc['child']['damage_channel']=str(channel);doc['refinement']['max_level']='2';doc['levels']=[dict(level=str(j),**{k:'0' for k in item_catalog.GAINS},extra_bonus_profile_id=None,bonuses=[]) for j in range(3)];doc['soul_levels']=[]
    for level in range(3):
     row=editor.soul_level(level);row.update(charge_capacity='6',cycle_charge_cost='2',strikes_per_cycle='2',initial_charge='0' if level==0 or field=='ClockSoulLocal' else '6',initial_strikes='2',recharge_interval_ms='600000' if level>0 or field=='ClockSoulLocal' else '500',first_multiplier_bp='15000',second_multiplier_bp='18000',third_multiplier_bp='25000');doc['soul_levels'].append(row)
    c[field]=publish(doc,field.lower())
   for field,chance in [('SoulScrollLocal',10000),('SoulFailLocal',0)]:
    doc=editor.new(14);doc['child'].update(soul_kind='2',damage_channel=None,profile_id=None,refine_chance_bp=str(chance));doc['base']['max_stack']='20';doc['refinement'].update(profile_kind='1',max_level='0');doc['levels']=[dict(level='0',**{k:'0' for k in item_catalog.GAINS},extra_bonus_profile_id=None,bonuses=[])];doc['soul_levels']=[];c[field]=publish(doc,field.lower())
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
     login='codexcompanion'+token+str(index);password=secrets.token_hex(8);name='SL'+token+str(index);db.query('INSERT INTO account.account(login,password,social_id,status,create_time) VALUES('+literal(login)+',PASSWORD('+literal(password)+"),'1234567','OK',NOW())");aid=int(db.query('SELECT LAST_INSERT_ID()')[0][0]);overrides=dict(job='0',level='50',st='60',ht='4',dx='3',iq='3',random_hp='30000',random_sp='10000',skill_group='1',account_id=str(aid),name=name,map_index='41',x=str(c['X']+index*100),y=str(c['Y']),z='0',gold='0',hp='30000',mp='10000',typed_book_revision='0',horse_level='0',horse_riding='0');skill=bytearray(2550);skill[10]=1;skill[11]=20;projection=['0x'+skill.hex() if k=='skill_level' else 'REPEAT(CHAR(0),OCTET_LENGTH(`quickslot`))' if k=='quickslot' else literal(overrides[k]) if k in overrides else '`'+k+'`' for k in columns];db.query('INSERT INTO player.player('+','.join('`'+k+'`' for k in columns)+') SELECT '+','.join(projection)+f' FROM player.player WHERE id={source_pid}');pid=int(db.query('SELECT LAST_INSERT_ID()')[0][0]);db.query(f'INSERT INTO player.player_index(id,pid1,empire) VALUES({aid},{pid},1)');c['Accounts'].append(dict(Login=login,Password=password,Name=name,AccountId=aid,Pid=pid))
    db.query('COMMIT')
   except BaseException:db.query('ROLLBACK');raise
   write(backup/'fixture-pending.private.json',c);owner=c['Accounts'][0]['Pid']
   def bench(action,**fields):return item_bench.run_tool(dict(action=action,owner=owner,connection=auth,**fields),tool)
   bench('setup',gold=0)
   for local in (c['PetLocal'],c['MountLocal']):bench('grant',category=15,local=local,count=1)
   for field,category in [('SwordLocal',1),('ArmorLocal',2)]+[(field,14) for field in ('RedSoulLocal','BlueSoulLocal','SpareSoulLocal','ClockSoulLocal','SoulScrollLocal','SoulFailLocal')]:bench('grant',category=category,local=c[field],count=4 if field in ('SoulScrollLocal','SoulFailLocal') else 1)
   state=bench('status')['state'];assert len(state['items'])==10
   for field,category in [('SwordLocal',1),('ArmorLocal',2)]:bench('equip',item=next(i['id'] for i in state['items'] if i['category']==category and i['local']==c[field]))
   assert not db.query('SELECT vnum FROM player.mob_proto WHERE vnum=65076');columns=[r[0] for r in db.query('SHOW COLUMNS FROM player.mob_proto')];overrides=dict(vnum=65076,name='SoulCombatProbe',locale_name='SoulCombatProbe',type=0,rank=0,battle_type=0,level=50,ai_flag=2,setRaceFlag=0,setImmuneFlag=0,st=0,dx=0,ht=0,iq=0,damage_min=0,damage_max=0,max_hp=100000000,regen_cycle=255,regen_percent=0,exp=0,gold_min=0,gold_max=0,attack_speed=100,move_speed=0,aggressive_hp_pct=0,aggressive_sight=0,attack_range=0,resurrection_vnum=0,drop_item=0,summon=0,drain_sp=0,dam_multiply=1);overrides.update({k:0 for k in columns if k.startswith(('enchant_','resist_','skill_','sp_'))});overrides['def']=0
   db.query('INSERT INTO player.mob_proto('+','.join('`'+k+'`' for k in columns)+') SELECT '+','.join(literal(overrides[k]) if k in overrides else '`'+k+'`' for k in columns)+' FROM player.mob_proto WHERE vnum=103');write(backup/'probe-proto.json',db.query('SELECT '+','.join('`'+k+'`' for k in columns)+' FROM player.mob_proto WHERE vnum=65076'))
   c['ProbeMap']='data/map/metin2_map_main_razor93/regen.json';target=root/c['ProbeMap'];manifest=read(root/'package-manifest.json');original=target.read_bytes();assert hashlib.sha256(original).hexdigest()==manifest['files'][c['ProbeMap']];world=json.loads(original);world['regens'].append(dict(type='mob',x=651,y=643,range_x=0,range_y=0,z=0,dir=0,time=3600,percent=100,count=1,vnum=65076));installed=(json.dumps(world,indent=2)+'\n').encode('utf-8');(backup/'regen-before.json').write_bytes(original);(backup/'regen-installed.json').write_bytes(installed);target.write_bytes(installed);manifest['files'][c['ProbeMap']]=hashlib.sha256(installed).hexdigest();write(root/'package-manifest.json',manifest)
   state=bench('status')['state'];assert protected(db,[a['Pid'] for a in c['Accounts']])==before;write(backup/'owner-before.json',state);write(fixture,c)
   print('SOUL_FIXTURE=READY items=10 core=4 scrolls=2 protected_items='+str(len(before['items'])))
 finally:db.close()
if __name__=='__main__':main()
