"""Add attribute-service probes and deploy the paired server to a stopped owned local package."""
import argparse,datetime,hashlib,json,shutil,sys
from pathlib import Path
repo=Path(__file__).resolve().parents[2];sys.path[:0]=[str(repo/'SRC/Tools/ServerManager'),str(repo/'db/tools')]
import item_bench,item_catalog
from mariadb_client import Client
p=argparse.ArgumentParser(description=__doc__)
for name in ('package','source-fixture','fixture','catalog-tool','bench-tool','game'):p.add_argument('--'+name,type=Path,required=True)
a=p.parse_args();root=a.package.resolve();_,manager,_,auth=item_bench.connection(root);supervisor=manager.read_json(root/'run/supervisor.json') if (root/'run/supervisor.json').exists() else {}
assert not any(manager.owned(c['identity']) for c in supervisor.get('children',[])),'Stop the owned stack first'
config=json.loads(a.source_fixture.read_text(encoding='utf-8'));token=config['Token'];account=config['Accounts'][0];assert config['Host']=='127.0.0.1' and len(token)==8 and all(c in '0123456789abcdef' for c in token);assert account['Login']=='codexitem'+token+'0' and account['Name']=='CI'+token+'0'
db=Client(**auth,library=str(root/'bin/libmariadb.dll'))
try:assert db.query('SELECT account_id,name FROM player.player WHERE id='+str(int(account['Pid'])))==[[str(account['AccountId']),account['Name']]]
finally:db.close()
backup=root/'run/deployments'/datetime.datetime.now(datetime.timezone.utc).strftime('attributes-%Y%m%dT%H%M%S');backup.mkdir(parents=True)
def read():return item_catalog.run_tool(dict(action='read',connection=auth),executable=a.catalog_tool)
packet=read();(backup/'catalog-before.json').write_text(json.dumps(packet,indent=2),encoding='utf-8')
def save(category,field,code,edit):
 global packet
 editor=item_catalog.Catalog(packet);old=next((r for r in editor.tables['item_base'] if r['content_code']==code),None);key=(category,int(old['local_id'])) if old else None;d=editor.item(key) if key else editor.new(category);d['base'].update(content_code=code,name_key='item.'+code+'.name',description_key='item.'+code+'.description',icon_asset='icon/item/50084.tga');edit(d);config[field]=int(d['base']['local_id']);item_catalog.run_tool(dict(action='commit',connection=auth,tables=editor.apply(d,key),expected_hash=packet['hash']),packet['columns'],a.catalog_tool);packet=read()
services=[('AttrAddLocal',1,1,2,10000,False),('AttrRerollLocal',2,1,1,10000,False),('AttrLockLocal',3,1,1,10000,False),('AttrRelockLocal',4,1,1,10000,False),('AttrUnlockLocal',5,1,1,10000,False),('AttrRareAddLocal',1,2,1,10000,False),('AttrRareRerollLocal',2,2,1,10000,False),('AttrExtraAddLocal',1,3,1,10000,False),('AttrExtraRerollLocal',2,3,1,10000,False),('AttrSpendAddLocal',1,1,2,5000,True),('AttrKeepAddLocal',1,1,2,5000,False),('AttrGapAddLocal',1,1,2,10000,False),('AttrHighLocal',1,1,2,10000,False),('AttrArmorOnlyLocal',1,1,2,10000,False)]
for field,operation,pool,units,chance,consume in services:
 def service(d,field=field,operation=operation,pool=pool,units=units,chance=chance,consume=consume):
  d['base']['min_level']='255' if field=='AttrHighLocal' else '0';d.update(item_catalog.Catalog.attribute_service_defaults());d['attribute_service'].update(operation_kind=str(operation),pool_kind=str(pool),units_per_use=str(units),success_bp=str(chance),target_mask='2' if field=='AttrArmorOnlyLocal' else '7',minimum_count='5' if operation==3 else '1' if operation==2 else '0',maximum_count=str(5 if pool==1 else 2 if pool==2 else 1),add_count='1' if operation==1 else '0',consume_on_failure='1' if consume else '0');d['attribute_tiers']=[] if operation>=3 else [dict(tier='1',chance_bp='5000'),dict(tier='2',chance_bp='5000')];
  if field=='AttrGapAddLocal':d['attribute_tiers']=[dict(tier='1',chance_bp='5000')]
 save(5,field,'consumable.local_probe_'+field.lower(),service)
def target(category,no_candidates=False):
 def edit(d):
  if category==1:d['base'].update(inventory_height='3',icon_asset='icon/item/00010.tga');d['child']['physical_attack_base_raw']='5000'
  d['bonuses']=[dict(bonus_id='1',value_scaled='1000')];d['refinement']['max_level']='1';d['levels']=[dict(d['levels'][0],level=str(level)) for level in range(2)]
  d['attribute_profile']=dict(allow_lock='1');d['attribute_groups']=[dict(pool_kind=str(pool),capacity=str(1 if no_candidates else capacity),exclude_static='1' if pool==1 else '0') for pool,capacity in ((1,5),(2,2),(3,1)) if not no_candidates or pool==1];d['attribute_candidates']=[];d['attribute_values']=[]
  for pool,ids in ((1,[1] if no_candidates else [1,3,4,5,6,29,30,31]),(2,[] if no_candidates else [21,22,23]),(3,[] if no_candidates else [21,22,23])):
   for identity in ids:
    d['attribute_candidates'].append(dict(pool_kind=str(pool),bonus_id=str(identity),weight=str(identity if pool==1 else 1)))
    for tier in (1,2):d['attribute_values'].append(dict(pool_kind=str(pool),bonus_id=str(identity),tier=str(tier),value_scaled=str(tier*(1000 if identity<=4 else 1 if identity>=29 else 100))))
 return edit
for category,field,code,no_candidates in ((1,'AttrWeaponLocal','weapon.local_probe_attributes',False),(2,'AttrArmorLocal','armor.local_probe_attributes',False),(3,'AttrAccessoryLocal','accessory.local_probe_attributes',False),(1,'AttrNoCandidateLocal','weapon.local_probe_attributes_no_choice',True)):
 save(category,field,code,target(category,no_candidates))
(backup/'catalog-after.json').write_text(json.dumps(packet,indent=2),encoding='utf-8');item_bench.run_tool(dict(action='setup',owner=account['Pid'],connection=auth),a.bench_tool)
for name in ('bin/GameServer.exe','package-manifest.json'):shutil.copy2(root/name,backup/Path(name).name)
shutil.copy2(a.game,root/'bin/GameServer.exe');manifest=json.loads((root/'package-manifest.json').read_text());manifest['files']['bin/GameServer.exe']=hashlib.sha256((root/'bin/GameServer.exe').read_bytes()).hexdigest();(root/'package-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8');a.fixture.write_text(json.dumps(config),encoding='utf-8');print('ATTRIBUTE_LOCAL_SETUP=READY; backup:',backup)
