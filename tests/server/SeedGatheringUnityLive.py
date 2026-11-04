"""Seed paired C25c tools on the stopped, owned local test PC; preserve prior items."""
import argparse,datetime,hashlib,json,os,shutil,subprocess,sys
from pathlib import Path
repo=Path(__file__).resolve().parents[2];sys.path[:0]=[str(repo/'SRC/Tools/ServerManager'),str(repo/'db/tools')]
import item_bench,item_catalog
from mariadb_client import Client,statements
p=argparse.ArgumentParser(description=__doc__)
for n in ('package','source-fixture','fixture','catalog-tool','bench-tool','game','database','login'):p.add_argument('--'+n,type=Path,required=True)
p.add_argument('--reset-live',action='store_true',help='Recreate only new C25c tool/bait probes for a repeat smoke run')
a=p.parse_args();root=a.package.resolve();_,manager,owner,auth=item_bench.connection(root);os.environ['PATH']=str(root/'bin')+os.pathsep+os.environ.get('PATH','')
supervisor=manager.read_json(root/'run/supervisor.json') if (root/'run/supervisor.json').exists() else {};assert not any(manager.owned(c['identity']) for c in supervisor.get('children',[])),'Stop the owned stack first'
config=json.loads(a.source_fixture.read_text());account=config['Accounts'][0];pid=int(account['Pid']);token=config['Token'];assert config['Host']=='127.0.0.1' and len(token)==8 and all(c in '0123456789abcdef' for c in token) and account['Login']=='codexitem'+token+'0' and account['Name']=='CI'+token+'0'
db=Client(**auth,library=str(root/'bin/libmariadb.dll'));assert db.query('SELECT account_id,name FROM player.player WHERE id='+str(pid))==[[str(account['AccountId']),account['Name']]]
backup=root/'run/deployments'/datetime.datetime.now(datetime.timezone.utc).strftime('resource-%Y%m%dT%H%M%S');backup.mkdir(parents=True)
with (backup/'player-protos-before.sql').open('wb') as out:
 r=subprocess.run([str(Path(owner['binary_dir'])/'mariadb-dump.exe'),'--defaults-extra-file='+str(root/'run/mariadb/admin.cnf'),'--protocol=tcp','--host=127.0.0.1','--port='+str(owner['port']),'--single-transaction','--databases','player','protos'],stdout=out,stderr=subprocess.PIPE,creationflags=item_bench.NO_WINDOW,timeout=300);assert r.returncode==0,'SQL backup failed'
manifest_path=root/'package-manifest.json';manifest=json.loads(manifest_path.read_text());shutil.copy2(manifest_path,backup/manifest_path.name)
has_tool=db.query("SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA='player' AND TABLE_NAME='item_instance' AND COLUMN_NAME='tool_training_points'")[0][0]=='1'
before=item_bench.run_tool(dict(action='load',owner=pid,connection=auth),a.bench_tool if has_tool else root/'manager/_internal/validators/ItemBenchTool.exe')['state'];(backup/'owner-before.json').write_text(json.dumps(before,indent=2),encoding='utf-8')
hashes=db.query('SELECT instance_id,HEX(state_sha256),state_revision FROM player.item_instance WHERE owner_kind=1 AND owner_id='+str(pid)+' ORDER BY instance_id');(backup/'instance-hashes-before.json').write_text(json.dumps(hashes),encoding='utf-8')
columns='job,level,exp,skill_point,sub_skill_point,horse_skill_point,HEX(skill_level),typed_book_revision,hp,mp,map_index,x,y,z';character=db.query('SELECT '+columns+' FROM player.player WHERE id='+str(pid))[0];(backup/'character-before.json').write_text(json.dumps(dict(columns=columns.split(','),values=character)),encoding='utf-8')
learning=db.query('SELECT state_revision,HEX(state_blob),HEX(state_sha256),HEX(last_operation) FROM player.item_book_learning WHERE owner_kind=1 AND owner_id='+str(pid));(backup/'learning-before.json').write_text(json.dumps(learning),encoding='utf-8')
baseline_file=repo/'build/resource-original-baseline.json'
if not baseline_file.exists():baseline_file.write_text(json.dumps(dict(backup=str(backup))),encoding='utf-8')
original=Path(json.loads(baseline_file.read_text())['backup']).resolve();assert original.is_relative_to((root/'run/deployments').resolve());preserved=json.loads((original/'owner-before.json').read_text());original_ids={i['id'] for i in preserved['items']}
for i in preserved['items']:i.setdefault('tool',None)
deployment=[('bin/GameServer.exe',a.game),('bin/Database.exe',a.database),('bin/LoginServer.exe',a.login),('manager/_internal/validators/ItemBenchTool.exe',a.bench_tool),('manager/_internal/validators/CatalogEditorTool.exe',a.catalog_tool)]
for relative,source in deployment:assert source.is_file();shutil.copy2(root/relative,backup/Path(relative).name)
for sql in statements((repo/'db/migrations/item_instance/009_gathering_tools.sql').read_text()):db.query(sql)
def read():return item_catalog.run_tool(dict(action='read',connection=auth),executable=a.catalog_tool)
packet=read();(backup/'catalog-before.json').write_text(json.dumps(packet,indent=2),encoding='utf-8')
def save(category,field,code,configure):
 global packet
 editor=item_catalog.Catalog(packet);old=next((r for r in editor.tables['item_base'] if r['content_code']==code),None);key=(category,int(old['local_id'])) if old else None;d=editor.item(key) if key else editor.new(category)
 d['base'].update(content_code=code,name_key='item.'+code+'.name',description_key='item.'+code+'.description',icon_asset='icon/item/27590.tga',min_level='0',inventory_width='1',inventory_height='1',max_stack='1' if category in (1,11) else '200');configure(d);config[field]=int(d['base']['local_id'])
 item_catalog.run_tool(dict(action='commit',connection=auth,tables=editor.apply(d,key),expected_hash=packet['hash']),packet['columns'],a.catalog_tool);packet=read()

def tool(d,kind,pool=1,fail=False):
 d['child'].update(tool_kind=str(kind),fishing_pool=str(pool));d['refinement'].update(profile_kind='7',max_level='2');d['levels']=[dict(d['levels'][0],level=str(i)) for i in range(3)]
 d['tool_levels']=[item_catalog.Catalog.tool_level(i,terminal=i==2) for i in range(3)]
 for i,r in enumerate(d['tool_levels']):r.update(power=str(100*(i+1)),gather_bonus_bp='0',required_points='0' if i==2 else '2',practice_numerator='0' if i==2 else '1',practice_denominator='1')
for field,kind,pool in [('ResourceRodFirst',2,2),('ResourceRodSecond',2,3),('ResourcePick',1,1),('ResourceAxe',3,1),('ResourceShears',4,1),('ResourceFail',1,1)]:
 def configure(d,k=kind,p=pool,f=field):
  tool(d,k,p)
  if f=='ResourceFail':
   for r in d['tool_levels']:r['power']='1'
 save(11,field,'tool.local_c25c_'+field[8:].lower(),configure)
save(12,'ResourceBait','fishing.local_c25c_bait',lambda d:d['child'].update(bait_power='50'))
for field in ('ResourceFishFirst','ResourceFishSecond'):
 def fish(d):
  d['base']['max_stack']='3';d['child'].update(fishing_kind='2',bait_power='0',use_loot_profile_id=None,grill_category_id='5',grill_local_id=str(config['FishCooked']));d['loot']=None
 save(12,field,'fishing.local_c25c_'+field[8:].lower(),fish)
for field in ('ResourceOre','ResourceWood','ResourceHerb'):save(6,field,'material.local_c25c_'+field[8:].lower(),lambda d:d['base'].update(max_stack='3'))
def load():return item_bench.run_tool(dict(action='load',owner=pid,connection=auth),a.bench_tool)['state']
fields=['ResourceRodFirst','ResourceRodSecond','ResourcePick','ResourceAxe','ResourceShears','ResourceFail','ResourceBait','ResourceFishFirst','ResourceFishSecond','ResourceOre','ResourceWood','ResourceHerb']
def category(f):return 12 if f in ('ResourceBait','ResourceFishFirst','ResourceFishSecond') else 6 if f in ('ResourceOre','ResourceWood','ResourceHerb') else 11
if a.reset_live:
 for i in load()['items']:
  if any(i['local']==config[f] and i['category']==category(f) for f in fields):
   assert i['id'] not in original_ids,'Never reset a prior item';item_bench.run_tool(dict(action='destroy',owner=pid,item=i['id'],connection=auth),a.bench_tool)
for field in fields[:7]:
 total=8 if field=='ResourceBait' else 1;count=sum(i['count'] for i in load()['items'] if i['category']==category(field) and i['local']==config[field]);assert count<=total
 if count<total:item_bench.run_tool(dict(action='grant',owner=pid,category=category(field),local=config[field],count=total-count,connection=auth),a.bench_tool)
map_index,x,y=int(character[10]),int(character[11]),int(character[12]);config['ResourceX']=x;config['ResourceY']=y
maps=json.loads((root/'data/map/index.json').read_text())['maps'];name=next(r['name'] for r in maps if int(r['index'])==map_index);folder=(root/'data/map'/name).resolve();assert folder.is_relative_to((root/'data/map').resolve())
setting=json.loads((folder/'setting.json').read_text());bx,by=setting['base_position'];assert (x-bx)%100==0 and (y-by)%100==0
npc_path=folder/'npc.json';shutil.copy2(npc_path,backup/'npc-before.json');npc=json.loads(npc_path.read_text())
for offset,vnum in [(3,20047),(4,20048),(5,20049),(6,20050)]:
 assert len(db.query('SELECT vnum FROM player.mob_proto WHERE vnum='+str(vnum)))==1
 entry=dict(type='mob',x=(x-bx)//100+offset,y=(y-by)//100,range_x=0,range_y=0,z=0,dir=1,time=60,percent=100,count=1,vnum=vnum)
 if entry not in npc['regens']:npc['regens'].append(entry)
npc_path.write_text(json.dumps(npc,indent=2)+'\n',encoding='utf-8');manifest['files'][npc_path.relative_to(root).as_posix()]=hashlib.sha256(npc_path.read_bytes()).hexdigest()

rules_path=root/'data/typed_gathering.json'
if rules_path.exists():shutil.copy2(rules_path,backup/'gathering-before.json');rules=json.loads(rules_path.read_text())
else:rules=dict(version=1,sites=[])
assert rules['version']==1
local_ids=set(range(1,7));assert not any(s['id'] in local_ids for s in rules['sites']) or all(s.get('name','').startswith('Helyi C25c') for s in rules['sites'] if s['id'] in local_ids),'Do not replace unrelated world content'
rules['sites']=[s for s in rules['sites'] if s['id'] not in local_ids]
for identity,kind,pool,race,field,count in [(1,2,2,9009,'ResourceFishFirst',7),(2,2,3,9009,'ResourceFishSecond',2),(3,1,1,20047,'ResourceOre',4),(4,3,1,20048,'ResourceWood',3),(5,4,1,20049,'ResourceHerb',2),(6,1,1,20050,'ResourceOre',4)]:
 rules['sites'].append(dict(id=identity,map=map_index,race=race,name='Helyi C25c '+field,kind=kind,pool=pool,range=900,duration_ms=2000,minimum_power=1 if identity==6 else 100,power_step=100,base_chance_bp=0 if identity==6 else 10000,power_chance_bp=0,bait_chance_bp=0,rewards=[dict(category=category(field),local=config[field],count=count,weight=1)]))
rules_path.write_text(json.dumps(rules,indent=2)+'\n',encoding='utf-8');manifest['files'][rules_path.relative_to(root).as_posix()]=hashlib.sha256(rules_path.read_bytes()).hexdigest()
actual={i['id']:i for i in load()['items']};assert all(actual.get(i['id'])==i for i in preserved['items']),'Prior item changed'
current_hashes={r[0]:r for r in db.query('SELECT instance_id,HEX(state_sha256),state_revision FROM player.item_instance WHERE owner_kind=1 AND owner_id='+str(pid)+' ORDER BY instance_id')};assert all(current_hashes.get(r[0])==r for r in json.loads((original/'instance-hashes-before.json').read_text()))
assert db.query('SELECT '+columns+' FROM player.player WHERE id='+str(pid))[0]==character and db.query('SELECT state_revision,HEX(state_blob),HEX(state_sha256),HEX(last_operation) FROM player.item_book_learning WHERE owner_kind=1 AND owner_id='+str(pid))==learning
for relative,source in deployment:target=root/relative;shutil.copy2(source,target);manifest['files'][relative]=hashlib.sha256(target.read_bytes()).hexdigest()
manifest_path.write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8');db.close();a.fixture.write_text(json.dumps(config),encoding='utf-8')
(repo/'build/resource-seed-evidence.json').write_text(json.dumps(dict(backup=str(backup),original_backup=str(original),previous_items=len(preserved['items']),all_prior_items_and_hashes_exact=True,prior_learning_exact=True,service_file=npc_path.relative_to(root).as_posix(),rules_file=rules_path.relative_to(root).as_posix()),indent=2));print('RESOURCE_LOCAL_SETUP=READY; prior item states preserved:',len(preserved['items']),'; backup:',backup)
