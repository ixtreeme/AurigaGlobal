"""Install C21 in a stopped owned package and add dedicated effect probes."""
import argparse,datetime,hashlib,json,shutil,sys
from pathlib import Path
repo=Path(__file__).resolve().parents[2];sys.path[:0]=[str(repo/'SRC/Tools/ServerManager'),str(repo/'db/tools')]
import item_bench,item_catalog
from mariadb_client import Client
p=argparse.ArgumentParser(description=__doc__)
for n in ('package','source-fixture','fixture','catalog-tool','bench-tool','game'):p.add_argument('--'+n,type=Path,required=True)
a=p.parse_args();root=a.package.resolve();_,manager,_,auth=item_bench.connection(root)
supervisor=manager.read_json(root/'run/supervisor.json') if (root/'run/supervisor.json').exists() else {};assert not any(manager.owned(c['identity']) for c in supervisor.get('children',[])),'Stop the owned stack first'
config=json.loads(a.source_fixture.read_text(encoding='utf-8'));token=config['Token'];account=config['Accounts'][0];pid=int(account['Pid']);assert config['Host']=='127.0.0.1' and len(token)==8 and all(c in '0123456789abcdef' for c in token) and account['Login']=='codexitem'+token+'0' and account['Name']=='CI'+token+'0'
db=Client(**auth,library=str(root/'bin/libmariadb.dll'));assert db.query('SELECT account_id,name FROM player.player WHERE id='+str(pid))==[[str(account['AccountId']),account['Name']]]
backup=root/'run/deployments'/datetime.datetime.now(datetime.timezone.utc).strftime('effects-%Y%m%dT%H%M%S');backup.mkdir(parents=True)
manifest=json.loads((root/'package-manifest.json').read_text());shutil.copy2(root/'package-manifest.json',backup/'package-manifest.json')
for relative,source in [('manager/_internal/validators/ItemBenchTool.exe',a.bench_tool),('bin/GameServer.exe',a.game),('manager/_internal/migrations/006_item_effects.sql',repo/'db/migrations/item_instance/006_item_effects.sql')]:
 target=root/relative
 if target.exists():
  assert hashlib.sha256(target.read_bytes()).hexdigest()==manifest['files'][relative];shutil.copy2(target,backup/target.name)
 target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source,target);manifest['files'][relative]=hashlib.sha256(target.read_bytes()).hexdigest()
(root/'package-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8');item_bench.helper=lambda:a.bench_tool;item_bench.install(root)
def read():return item_catalog.run_tool(dict(action='read',connection=auth),executable=a.catalog_tool)
packet=read();(backup/'catalog-before.json').write_text(json.dumps(packet,indent=2),encoding='utf-8')
# Separate groups preserve all existing catalog definitions. Shared group codes
# deliberately exercise reject, whole-package replacement and toggle variants.
profiles=[('EffectWall','timed','wall',1,2,12000,0,5,1500),('EffectOnline','timed','online',1,1,30000,0,5,1500),('EffectReplace','timed','replace',2,1,30000,0,5,1500),('EffectOther','timed','replace',2,1,20000,0,9,-500),('ToggleMain','toggle','primary',1,1,60000,0,5,1500),('ToggleOther','toggle','primary',2,1,60000,0,9,-500),('ToggleSame','toggle','primary',1,1,60000,0,5,1500),('ToggleMarried','toggle','married',1,1,60000,1,5,1500),('ToggleLeader','toggle','leader',1,1,60000,2,5,1500),('ToggleTiny','toggle','tiny',1,1,2000,0,5,1500)]
for field,kind,group,variant,clock,duration,requirements,strength,movement in profiles:
 editor=item_catalog.Catalog(packet);code='consumable.local_probe_'+field.lower();old=next((r for r in editor.tables['item_base'] if r['content_code']==code),None);key=(5,int(old['local_id'])) if old else None;d=editor.item(key) if key else editor.new(5)
 d['base'].update(content_code=code,name_key='item.'+code+'.name',description_key='item.'+code+'.description',icon_asset='icon/item/27001.tga',min_level='0',max_stack='20' if kind=='timed' else '1')
 for name in ('recovery','effect','timed','toggle','timed_group','toggle_group','cooldown'):d[name]=None
 d.update(editor.timed_defaults(d['base']['local_id']) if kind=='timed' else editor.toggle_defaults(d['base']['local_id']))
 d[kind].update(duration_ms=str(duration),allowed_contexts='15',allow_during_pvp='1')
 d[kind+'_group'].update(code=('effect' if kind=='timed' else 'toggle')+'.local_probe_'+group,remove_on_death='1')
 if kind=='timed':d[kind+'_group'].update(reapply_kind=str(variant),clock_kind=str(clock))
 else:d[kind].update(variant_id=str(variant),requirements=str(requirements))
 d[kind+'_bonuses']=[dict(bonus_id='29',value_scaled=str(strength)),dict(bonus_id='26',value_scaled=str(movement))]
 config[field]=int(d['base']['local_id']);item_catalog.run_tool(dict(action='commit',connection=auth,tables=editor.apply(d,key),expected_hash=packet['hash']),packet['columns'],a.catalog_tool);packet=read()
(backup/'catalog-after.json').write_text(json.dumps(packet,indent=2),encoding='utf-8');before=item_bench.run_tool(dict(action='setup',owner=pid,connection=auth),a.bench_tool);(backup/'owner-before.json').write_text(json.dumps(before,indent=2),encoding='utf-8')
for field,kind,*_ in profiles:
 state=item_bench.run_tool(dict(action='load',owner=pid,connection=auth),a.bench_tool)
 if not any(i['category']==5 and i['local']==config[field] for i in state.get('items',state.get('state',{}).get('items',[]))):item_bench.run_tool(dict(action='grant',owner=pid,category=5,local=config[field],count=5 if kind=='timed' else 1,connection=auth),a.bench_tool)
db.close();a.fixture.write_text(json.dumps(config),encoding='utf-8');print('EFFECTS_LOCAL_SETUP=READY; prior inventory preserved; backup:',backup)
