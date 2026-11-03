"""Install C20 and add probes to the verified local test owner; preserve prior items."""
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
backup=root/'run/deployments'/datetime.datetime.now(datetime.timezone.utc).strftime('automatic-%Y%m%dT%H%M%S');backup.mkdir(parents=True)
manifest=json.loads((root/'package-manifest.json').read_text());shutil.copy2(root/'package-manifest.json',backup/'package-manifest.json')
for relative,source in [('manager/_internal/validators/ItemBenchTool.exe',a.bench_tool),('bin/GameServer.exe',a.game),('manager/_internal/migrations/005_automatic_recovery.sql',repo/'db/migrations/item_instance/005_automatic_recovery.sql')]:
 target=root/relative
 if target.exists():
  assert hashlib.sha256(target.read_bytes()).hexdigest()==manifest['files'][relative];shutil.copy2(target,backup/target.name)
 target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source,target);manifest['files'][relative]=hashlib.sha256(target.read_bytes()).hexdigest()
(root/'package-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
item_bench.helper=lambda:a.bench_tool
item_bench.install(root)
def read():return item_catalog.run_tool(dict(action='read',connection=auth),executable=a.catalog_tool)
packet=read();(backup/'catalog-before.json').write_text(json.dumps(packet,indent=2),encoding='utf-8')
profiles=[('AutoHp',1,1,100000000,2000,700,400,0,11),('AutoSp',2,1,50000000,2000,700,200,0,11),('AutoUnlimited',1,2,0,1000,700,0,0,11),('AutoTiny',1,1,1000,1000,10000,0,0,11),('AutoHigh',1,1,1000000,1000,700,0,255,11),('AutoContext',1,1,1000000,1000,700,0,0,8)]
for field,pool,supply,capacity,interval,rate,bonus,level,contexts in profiles:
 editor=item_catalog.Catalog(packet);code='consumable.local_probe_'+field.lower();old=next((r for r in editor.tables['item_base'] if r['content_code']==code),None);key=(5,int(old['local_id'])) if old else None;d=editor.item(key) if key else editor.new(5)
 d['base'].update(content_code=code,name_key='item.'+code+'.name',description_key='item.'+code+'.description',icon_asset='icon/item/27001.tga',min_level=str(level),max_stack='1');d['recovery'].update(delivery_kind='4',units_per_use='1',allowed_contexts=str(contexts),allow_during_pvp='1',arena_quota_cost='0');d['cooldown']=None
 d['effect']=dict(delivery_kind='4',pool_kind=str(pool),supply_kind=str(supply),capacity_raw=str(capacity),tick_interval_ms=str(interval),max_pool_per_tick_bp=str(rate),active_max_pool_bonus_bp=str(bonus))
 config[field]=int(d['base']['local_id']);item_catalog.run_tool(dict(action='commit',connection=auth,tables=editor.apply(d,key),expected_hash=packet['hash']),packet['columns'],a.catalog_tool);packet=read()
(backup/'catalog-after.json').write_text(json.dumps(packet,indent=2),encoding='utf-8')
before=item_bench.run_tool(dict(action='setup',owner=pid,connection=auth),a.bench_tool);(backup/'owner-before.json').write_text(json.dumps(before,indent=2),encoding='utf-8')
for field,*_ in profiles:
 state=item_bench.run_tool(dict(action='load',owner=pid,connection=auth),a.bench_tool)
 if not any(i['category']==5 and i['local']==config[field] for i in state.get('items',state.get('state',{}).get('items',[]))):item_bench.run_tool(dict(action='grant',owner=pid,category=5,local=config[field],count=1,connection=auth),a.bench_tool)
db.query('UPDATE player.player SET hp=1000,mp=20 WHERE id='+str(pid)+' AND account_id='+str(int(account['AccountId'])))
db.close();a.fixture.write_text(json.dumps(config),encoding='utf-8');print('AUTOMATIC_LOCAL_SETUP=READY; previous item inventory preserved; backup:',backup)
