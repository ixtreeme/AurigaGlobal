"""Deploy paired recovery support to a stopped, ownership-verified local package."""
import argparse,datetime,hashlib,json,shutil,sys
from pathlib import Path
repo=Path(__file__).resolve().parents[2];sys.path[:0]=[str(repo/'SRC/Tools/ServerManager'),str(repo/'db/tools')]
import item_bench,item_catalog
from mariadb_client import Client
p=argparse.ArgumentParser(description=__doc__)
for n in ('package','source-fixture','fixture','catalog-tool','bench-tool','game'):p.add_argument('--'+n,type=Path,required=True)
a=p.parse_args();root=a.package.resolve();_,manager,_,auth=item_bench.connection(root)
supervisor=manager.read_json(root/'run/supervisor.json') if (root/'run/supervisor.json').exists() else {};assert not any(manager.owned(c['identity']) for c in supervisor.get('children',[])),'Stop the owned stack first'
config=json.loads(a.source_fixture.read_text(encoding='utf-8'));token=config['Token'];account=config['Accounts'][0];assert config['Host']=='127.0.0.1' and len(token)==8 and all(c in '0123456789abcdef' for c in token) and account['Login']=='codexitem'+token+'0' and account['Name']=='CI'+token+'0'
db=Client(**auth,library=str(root/'bin/libmariadb.dll'))
try:assert db.query('SELECT account_id,name FROM player.player WHERE id='+str(int(account['Pid'])))==[[str(account['AccountId']),account['Name']]]
finally:db.close()
backup=root/'run/deployments'/datetime.datetime.now(datetime.timezone.utc).strftime('recovery-%Y%m%dT%H%M%S');backup.mkdir(parents=True)
item_bench.helper=lambda:a.bench_tool
item_bench.install(root)
def read():return item_catalog.run_tool(dict(action='read',connection=auth),executable=a.catalog_tool)
packet=read();(backup/'catalog-before.json').write_text(json.dumps(packet,indent=2),encoding='utf-8')
# Dedupe the duration across the two cooldown probes through the real catalog editor.
profiles=[('RecoveryReserveHp',2,2000000,0,1000,0,500,None,0,15),('RecoveryReserveSp',2,0,300000,1000,0,700,None,0,15),('RecoveryReserveDual',2,1000000,200000,1000,0,500,None,0,15),('RecoveryPeriodicHp',3,250000,0,1000,12000,0,None,0,15),('RecoveryPeriodicSp',3,0,50000,1000,12000,0,None,0,15),('RecoveryReplaceHp',3,400000,0,1000,6000,0,None,0,15),('RecoveryLongHp',3,50000,0,2000,60000,0,None,0,15),('RecoveryCooldownReserve',2,1000000,0,1000,0,500,10000,0,15),('RecoveryCooldownInstant',1,100000,0,0,0,0,10000,0,15),('RecoveryHigh',2,1000000,0,1000,0,500,None,255,15),('RecoveryContext',2,1000000,0,1000,0,500,None,0,8)]
for field,kind,hp,sp,interval,duration,rate,cd,level,contexts in profiles:
 editor=item_catalog.Catalog(packet);code='consumable.local_probe_'+field.lower();old=next((r for r in editor.tables['item_base'] if r['content_code']==code),None);key=(5,int(old['local_id'])) if old else None;d=editor.item(key) if key else editor.new(5)
 d['base'].update(content_code=code,name_key='item.'+code+'.name',description_key='item.'+code+'.description',icon_asset='icon/item/27001.tga',min_level=str(level));d['recovery'].update(delivery_kind=str(kind),units_per_use='1',allowed_contexts=str(contexts),allow_during_pvp='1',arena_quota_cost='0');d['cooldown']=dict(duration_ms=str(cd)) if cd else None
 if kind==1:d['effect']=dict(delivery_kind='1',hp_raw=str(hp),sp_raw=str(sp),max_hp_bp='0',max_sp_bp='0',flat_bonus_cap_bp='0')
 elif kind==2:d['effect']=dict(delivery_kind='2',hp_raw=str(hp),sp_raw=str(sp),flat_bonus_cap_bp='10000',tick_interval_ms=str(interval),max_pool_per_tick_bp=str(rate))
 else:d['effect']=dict(delivery_kind='3',hp_per_tick_raw=str(hp),sp_per_tick_raw=str(sp),tick_interval_ms=str(interval),duration_ms=str(duration))
 config[field]=int(d['base']['local_id']);item_catalog.run_tool(dict(action='commit',connection=auth,tables=editor.apply(d,key),expected_hash=packet['hash']),packet['columns'],a.catalog_tool);packet=read()
(backup/'catalog-after.json').write_text(json.dumps(packet,indent=2),encoding='utf-8');item_bench.run_tool(dict(action='setup',owner=account['Pid'],connection=auth),a.bench_tool)
for name in ('bin/GameServer.exe','package-manifest.json'):shutil.copy2(root/name,backup/Path(name).name)
shutil.copy2(a.game,root/'bin/GameServer.exe');manifest=json.loads((root/'package-manifest.json').read_text());manifest['files']['bin/GameServer.exe']=hashlib.sha256((root/'bin/GameServer.exe').read_bytes()).hexdigest();(root/'package-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8');a.fixture.write_text(json.dumps(config),encoding='utf-8');print('RECOVERY_LOCAL_SETUP=READY; backup:',backup)
