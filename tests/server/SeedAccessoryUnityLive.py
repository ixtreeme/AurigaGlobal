"""Add accessory probes and deploy the paired server to a stopped owned local package."""
import argparse,datetime,hashlib,json,shutil,sys
from pathlib import Path
repo=Path(__file__).resolve().parents[2]
sys.path[:0]=[str(repo/'SRC/Tools/ServerManager'),str(repo/'db/tools')]
import item_bench,item_catalog
from mariadb_client import Client
p=argparse.ArgumentParser(description=__doc__)
for name in ('package','source-fixture','fixture','catalog-tool','bench-tool','game'):p.add_argument('--'+name,type=Path,required=True)
a=p.parse_args();root=a.package.resolve();_,manager,_,auth=item_bench.connection(root)
supervisor=manager.read_json(root/'run/supervisor.json') if (root/'run/supervisor.json').exists() else {}
assert not any(manager.owned(c['identity']) for c in supervisor.get('children',[])),'Stop the owned package first'
config=json.loads(a.source_fixture.read_text(encoding='utf-8'));token=config['Token'];account=config['Accounts'][0]
assert config['Host']=='127.0.0.1' and len(token)==8 and all(c in '0123456789abcdef' for c in token)
assert account['Login']=='codexitem'+token+'0' and account['Name']=='CI'+token+'0'
db=Client(**auth,library=str(root/'bin/libmariadb.dll'))
try:assert db.query('SELECT account_id,name FROM player.player WHERE id='+str(int(account['Pid'])))==[[str(account['AccountId']),account['Name']]]
finally:db.close()
backup=root/'run/deployments'/datetime.datetime.now(datetime.timezone.utc).strftime('accessory-%Y%m%dT%H%M%S');backup.mkdir(parents=True)
def read():return item_catalog.run_tool(dict(action='read',connection=auth),executable=a.catalog_tool)
packet=read();(backup/'catalog-before.json').write_text(json.dumps(packet,indent=2),encoding='utf-8')
def commit(tables):
 global packet
 item_catalog.run_tool(dict(action='commit',connection=auth,tables=tables,expected_hash=packet['hash']),packet['columns'],a.catalog_tool);packet=read()
def save(category,field,code,edit):
 editor=item_catalog.Catalog(packet);old=next((r for r in editor.tables['item_base'] if r['content_code']==code),None);key=(category,int(old['local_id'])) if old else None
 document=editor.item(key) if key else editor.new(category);document['base'].update(content_code=code,name_key='item.'+code+'.name',description_key='item.'+code+'.description',icon_asset='icon/item/50084.tga');edit(document)
 config[field]=int(document['base']['local_id']);commit(editor.apply(document,key))
for field,code,kind in (('AccessoryDrillLocal','material.local_probe_socket_drill',4),('AccessoryJewelLocal','material.local_probe_socket_jewel',5),('AccessoryPermanentLocal','material.local_probe_socket_permanent',5)):
 save(6,field,code,lambda d,kind=kind:d['child'].update(material_kind=str(kind)))
profiles={}
for suffix,chance,durations in (('timed',10000,[6000,4000,3000]),('random',5000,[60000,60000,60000]),('manual',10000,[1800000,1200000,600000])):
 editor=item_catalog.Catalog(packet);tables=editor.tables;code='socket.local_probe_'+suffix;existing=next((r for r in tables['accessory_socket_profile'] if r['code']==code),None)
 profile=int(existing['profile_id']) if existing else max([int(r['profile_id']) for r in tables['accessory_socket_profile']]+[0])+1;profiles[suffix]=profile
 for name in ('accessory_socket_profile','accessory_socket_grade','accessory_socket_material'):tables[name]=[r for r in tables[name] if int(r['profile_id'])!=profile]
 tables['accessory_socket_profile'].append(dict(profile_id=str(profile),code=code,max_grade='3',unlock_units_per_attempt='2',charge_units_per_attempt='3'))
 for grade in range(4):tables['accessory_socket_grade'].append(dict(profile_id=str(profile),grade=str(grade),duration_ms=str(durations[grade-1] if grade else 0),bonus_increase_bp=str(grade*1000),minimum_raw=str(grade*1000),minimum_bp=str(grade*100),minimum_integer=str(grade),unlock_chance_bp=str(chance if grade<3 else 0),charge_chance_bp=str(chance if grade<3 else 0)))
 for operation,field in ((1,'AccessoryDrillLocal'),(2,'AccessoryJewelLocal'),(3,'AccessoryPermanentLocal')):tables['accessory_socket_material'].append(dict(profile_id=str(profile),operation_kind=str(operation),category_id='6',local_id=str(config[field])))
 commit(editor.encode())
for field,suffix in (('AccessoryLocal','timed'),('AccessoryRandomLocal','random'),('AccessoryManualLocal','manual')):
 def accessory(d,suffix=suffix):
  d['child']['socket_profile_id']=str(profiles[suffix]);d['bonuses']=[dict(bonus_id='3',value_scaled='10000')]
 save(3,field,'accessory.local_probe_'+suffix,accessory)
(backup/'catalog-after.json').write_text(json.dumps(packet,indent=2),encoding='utf-8')
item_bench.run_tool(dict(action='setup',owner=account['Pid'],connection=auth),a.bench_tool) # validates every stored owner before following the pin
for name in ('bin/GameServer.exe','package-manifest.json'):shutil.copy2(root/name,backup/Path(name).name)
shutil.copy2(a.game,root/'bin/GameServer.exe');manifest=json.loads((root/'package-manifest.json').read_text());manifest['files']['bin/GameServer.exe']=hashlib.sha256((root/'bin/GameServer.exe').read_bytes()).hexdigest();(root/'package-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
a.fixture.write_text(json.dumps(config),encoding='utf-8');print('ACCESSORY_LOCAL_SETUP=READY; backup:',backup)
