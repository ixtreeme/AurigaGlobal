"""Seed/verify isolated local stole, rune and special-bonus Unity accounts."""
from pathlib import Path
from datetime import datetime,timezone
from collections import Counter
import argparse,copy,json,os,re,secrets,subprocess,sys
REPO=Path(__file__).resolve().parents[2]
sys.path[:0]=[str(REPO/'SRC/Tools/ServerManager'),str(REPO/'db/tools')]
import item_bench,item_catalog,backend
from mariadb_client import Client
from PetPassiveUnityLiveFixture import protected
from CostumeUnityLiveFixture import read,write,literal

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('action',choices=('seed','verify'))
 for name in ('package','fixture'):p.add_argument('--'+name,type=Path,required=True)
 for name in ('template','log'):p.add_argument('--'+name,type=Path)
 a=p.parse_args();fixture=a.fixture.resolve();assert fixture.is_relative_to(REPO/'build')
 root,manager,ownership,auth=item_bench.connection(a.package.resolve());os.environ['PATH']=str(root/'bin')+os.pathsep+os.environ['PATH']
 tool=root/'manager/_internal/validators/ItemBenchTool.exe';catalog_tool=root/'manager/_internal/validators/CatalogEditorTool.exe';db=Client(**auth,library=str(root/'bin/libmariadb.dll'))
 try:
  if a.action=='verify':
   c=read(fixture);owners=[int(account['Pid']) for account in c['Accounts']];assert all(account['Login']=='codexcompanion'+c['Token']+str(i) for i,account in enumerate(c['Accounts']))
   backup=Path(c['Backup']);assert backup.parent==root/'run/deployments';before=read(backup/'typed-before.json');assert protected(db,owners)==before
   log=a.log.read_text(encoding='utf-8',errors='replace');assert 'METIN2_TYPED_SPECIAL_BONUS_LIVE=SUCCESS' in log
   for phase in ('stole-worn','stole-rerolled','costume-reset','costume-donors','costume-remove','paired-addon','runes-active','runes-paused','runes-exhausted','runes-recharged','runes-cleared'):assert 'SPECIAL_BONUS_PHASE=SUCCESS phase='+phase in log,phase
   state=item_bench.run_tool(dict(action='status',owner=owners[0],connection=auth),tool)['state'];items=state['items'];original=read(backup/'owner-before.json')
   for local in (c['PetLocal'],c['MountLocal']):assert next(i for i in original['items'] if i['category']==15 and i['local']==local)==next(i for i in items if i['category']==15 and i['local']==local)
   regalia=[i for i in items if i['category']==19];assert len(regalia)==8 and all(i['equipped_slot'] is None and i['bound'] for i in regalia)
   assert all(not i['regalia']['active'] for i in regalia)
   saved={int(slot):int(remaining) for slot,remaining in re.findall(r'SPECIAL_RUNE_SAVED slot=(\d+) remaining_ms=(\d+)',log)}
   assert set(saved)==set(range(1,7)),saved
   for slot,local in enumerate(c['RuneLocals'][:6],1):
    rune=next(i for i in regalia if i['local']==local)
    assert rune['regalia']['remaining_ms']==saved[slot],(slot,rune['regalia']['remaining_ms'],saved[slot])
   for i in regalia:assert i['id']==next(o['id'] for o in original['items'] if o['category']==19 and o['local']==i['local'])
   costume=next(i for i in items if i['category']==4 and i['local']==c['SpecialCostumeLocal']);assert costume['bound'] and costume['equipped_slot'] is None and costume['costume']['started'] and not costume['costume']['expired']
   assert len([b for b in costume['attributes'] if b['slot']<5])==3 and len([b for b in costume['attributes'] if 5<=b['slot']<7])==0
   sword=next(i for i in items if i['category']==1 and i['local']==c['SwordLocal']);assert sword['equipped_slot']==1 and any(b['bonus'] in (7,8) for b in sword['attributes'])
   assert db.query(f'SELECT COUNT(*) FROM player.item WHERE owner_id IN ({owners[0]},{owners[1]})')==[['0']]
   final=db.query(f'SELECT instance_id,HEX(regalia_blob),HEX(bonus_tool_blob),HEX(state_sha256) FROM player.item_instance WHERE owner_kind=1 AND owner_id={owners[0]} ORDER BY instance_id');write(backup/'owner-after.json',state);write(backup/'special-after.json',final)
   print('REGALIA_LOCAL_SQL=SUCCESS stoles=1 runes=7 protected_items='+str(len(before['items'])));return
  assert a.template and not fixture.exists();template=read(a.template);source=template['Accounts'][0];source_pid=int(source['Pid'])
  assert template['Host']=='127.0.0.1' and len(template['Accounts'])==2 and source['Login']=='codexcompanion'+template['Token']+'0'
  assert db.query(f'SELECT account_id,name FROM player.player WHERE id={source_pid}')==[[str(source['AccountId']),source['Name']]]
  with manager.supervisor_lock():
   assert not backend.describe(root)['running'];backup=root/'run/deployments'/datetime.now(timezone.utc).strftime('c29-fixture-%Y%m%dT%H%M%S%f');backup.mkdir()
   before=protected(db);write(backup/'typed-before.json',before)
   with (backup/'player-protos-before.sql').open('wb') as out:
    result=subprocess.run([str(Path(ownership['binary_dir'])/'mariadb-dump.exe'),'--defaults-extra-file='+str(root/'run/mariadb/admin.cnf'),'--protocol=tcp','--host=127.0.0.1','--port='+str(ownership['port']),'--single-transaction','--databases','player','protos'],stdout=out,stderr=subprocess.PIPE,creationflags=item_bench.NO_WINDOW,timeout=300);assert result.returncode==0 and out.tell()>0
   initial=item_catalog.run_tool(dict(action='read',connection=auth),executable=catalog_tool);write(backup/'catalog-before.json',initial)
   c={k:template[k] for k in ('Host','ServerName','Pin','Version','AuthPort','GamePort','PetLocal','MountLocal','PetRace','MountRace')};token=secrets.token_hex(4)
   c.update(Token=token,Accounts=[],MapIndex=41,X=986600,Y=269100,RidingRace=template['MountRace'],SpecialBonuses=True,RuneLocals=[],BonusToolLocals=[],StoleModel='Metin2Sash/item/wing/plechito/necromancer/necromancer_sash.gr2',SpecialPreview=str(REPO/'build/c29-regalia-visual.png'),Backup=str(backup))
   editor=item_catalog.Catalog(initial);steps=[]
   def publish(doc,label):
    category=int(doc['base']['category_id']);local=int(doc['base']['local_id']);code=f'regalia.c29_{token}_{label}'
    doc['base'].update(content_code=code,name_key='item.'+code+'.name',description_key='item.'+code+'.description',binding_policy='3' if category in (4,19) else '2');doc['trade'].update(allow_trade='0',allow_drop='0',allow_sell='0');steps.append(editor.apply(doc));editor.packet['sequences'][str(category)]=str(local+1);return local
   def attributes(doc):
    doc['attribute_profile']=dict(allow_lock='0');doc['attribute_groups']=[dict(pool_kind=str(pool),capacity=str(capacity),exclude_static='0') for pool,capacity in ((1,5),(2,2),(3,1))];doc['attribute_candidates']=[];doc['attribute_values']=[]
    for pool,ids in [(1,(29,30,31,32,7,8)),(2,(29,30,31,32)),(3,(29,30,31,32))]:
     for bonus in ids:
      doc['attribute_candidates'].append(dict(pool_kind=str(pool),bonus_id=str(bonus),weight='1'))
      for tier in (1,2):doc['attribute_values'].append(dict(pool_kind=str(pool),bonus_id=str(bonus),tier=str(tier),value_scaled=str(tier*(100 if bonus<29 else 1))))
   for field,category,model in [('SwordLocal',1,'Metin2Equipment/w19'),('DaggerLocal',1,'Metin2Equipment/w1000'),('ArmorLocal',2,'Metin2Equipment/a0_1')]:
    doc=editor.new(category);doc['appearance']=dict(model_asset=model)
    if category==1:doc['child'].update(weapon_subtype='1' if field=='SwordLocal' else '2',physical_attack_base_raw='1000');attributes(doc)
    else:doc['child']['physical_defense_base_raw']='1000'
    c[field]=publish(doc,field.lower())
   doc=editor.new(19);doc['child'].update(stole_grade='4');doc['appearance']=dict(model_asset=c['StoleModel']);c['StoleLocal']=publish(doc,'stole')
   for slot in range(1,8):
    doc=editor.new(19);capacity=30000 if slot==1 else 600000 if slot<7 else 0;doc['child'].update(regalia_kind='2',rune_slot=str(slot),stole_grade='0',capacity_ms=str(capacity),initial_ms=str(capacity));doc['regalia_rules']=[]
    for index,bonus in enumerate((29,1)):
     for bp,value in ([(0,11)] if slot==7 else [(0,2),(5000,7)]):doc['regalia_rules'].append(dict(bonus_index=str(index),variant_no='0',minimum_charge_bp=str(bp),bonus_id=str(bonus),value_scaled=str(value*(1000 if bonus==1 else 1))))
    c['RuneLocals'].append(publish(doc,'rune_'+str(slot)))
   for op in range(1,7):
    doc=editor.new(20);doc['child'].update(operation_kind=str(op),parameter='10000' if op==2 else '0',bonus_id='30' if op==4 else None,value_scaled='7' if op==4 else None);doc['base']['max_stack']='1' if op in (2,4) else '50';doc['special_tiers']=[dict(draw_no=str(j),tier='2' if j==0 else '1',chance_bp='10000') for j in range(3)] if op==3 else [];c['BonusToolLocals'].append(publish(doc,'tool_'+str(op)))
   doc=editor.new(20);doc['child'].update(operation_kind='4',bonus_id='32',value_scaled='9');doc['base']['max_stack']='1';c['ExtraDonorLocal']=publish(doc,'donor_int')
   doc=editor.new(4);doc['child'].update(clock_kind='3',duration_ms='3600000');doc['visuals']=[dict(character_class=str(cls),sex=str(sex),model_asset=f'Metin2Equipment/a{race}_4') for cls,sex,race in [(1,1,0),(1,2,4),(2,1,5),(2,2,1),(3,1,2),(3,2,6),(4,1,7),(4,2,3)]];attributes(doc);c['SpecialCostumeLocal']=publish(doc,'costume')
   doc=editor.new(5);doc.update(item_catalog.Catalog.attribute_service_defaults());doc['attribute_service'].update(operation_kind='1',pool_kind='1',units_per_use='1',success_bp='10000',target_mask='8',minimum_count='0',maximum_count='4',add_count='1');doc['attribute_tiers']=[dict(tier='1',chance_bp='10000')];doc['base']['max_stack']='50';c['BonusAdderLocal']=publish(doc,'normal_add')
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
     login='codexcompanion'+token+str(index);password=secrets.token_hex(8);name='RG'+token+str(index);db.query('INSERT INTO account.account(login,password,social_id,status,create_time) VALUES('+literal(login)+',PASSWORD('+literal(password)+"),'1234567','OK',NOW())");aid=int(db.query('SELECT LAST_INSERT_ID()')[0][0]);overrides=dict(job='0',account_id=str(aid),name=name,map_index='41',x=str(c['X']+index*100),y=str(c['Y']),z='0',gold='0',hp='1000',mp='20',typed_book_revision='0',horse_level='0',horse_riding='0');projection=[literal(overrides[k]) if k in overrides else '`'+k+'`' for k in columns];db.query('INSERT INTO player.player('+','.join('`'+k+'`' for k in columns)+') SELECT '+','.join(projection)+f' FROM player.player WHERE id={source_pid}');pid=int(db.query('SELECT LAST_INSERT_ID()')[0][0]);db.query(f'INSERT INTO player.player_index(id,pid1,empire) VALUES({aid},{pid},1)');c['Accounts'].append(dict(Login=login,Password=password,Name=name,AccountId=aid,Pid=pid))
    db.query('COMMIT')
   except BaseException:db.query('ROLLBACK');raise
   write(backup/'fixture-pending.private.json',c);owner=c['Accounts'][0]['Pid']
   def bench(action,**fields):return item_bench.run_tool(dict(action=action,owner=owner,connection=auth,**fields),tool)
   bench('setup',gold=0)
   for local in (c['PetLocal'],c['MountLocal']):bench('grant',category=15,local=local,count=1)
   for field,category in [('SwordLocal',1),('DaggerLocal',1),('ArmorLocal',2),('StoleLocal',19),('SpecialCostumeLocal',4),('BonusAdderLocal',5)]:bench('grant',category=category,local=c[field],count=3 if category==5 else 1)
   for local in c['RuneLocals']:bench('grant',category=19,local=local,count=1)
   for op,local in enumerate(c['BonusToolLocals'],1):
    for _ in range(2 if op in (2,4) else 1):bench('grant',category=20,local=local,count=1 if op in (2,4) else 4)
   bench('grant',category=20,local=c['ExtraDonorLocal'],count=1)
   state=bench('status')['state'];assert len(state['items'])==24
   item=lambda category,local:next(i for i in state['items'] if i['category']==category and i['local']==local)
   bench('attribute',tool=item(5,c['BonusAdderLocal'])['id'],target=item(4,c['SpecialCostumeLocal'])['id'])
   for field,category in [('SwordLocal',1),('ArmorLocal',2)]:bench('equip',item=item(category,c[field])['id'])
   state=bench('status')['state'];assert protected(db,[a['Pid'] for a in c['Accounts']])==before;write(backup/'owner-before.json',state);write(fixture,c)
   print('REGALIA_FIXTURE=READY items=24 stoles=1 runes=7 tools=6 protected_items='+str(len(before['items'])))
 finally:db.close()
if __name__=='__main__':main()
