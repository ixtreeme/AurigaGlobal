"""Provision dedicated NPC shop cases and request the already open Unity Editor.

Run SeedTypedItemUnityLive.py --npc-shops with the owned game stack stopped,
then start the stack freshly. The Editor bridge waits for edit mode and never
exits the Editor. Only the fixture's verified codexitem character is reset.
"""
import argparse
import datetime
import json
from pathlib import Path
import sys
import time

REPO=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(REPO/'SRC/Tools/ServerManager'))
sys.path.insert(0,str(REPO/'db/tools'))
import item_bench
from mariadb_client import Client

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--package',type=Path,required=True)
parser.add_argument('--unity-project',type=Path,required=True)
parser.add_argument('--fixture',type=Path,required=True)
parser.add_argument('--bench-tool',type=Path,required=True)
parser.add_argument('--timeout',type=int,default=600)
args=parser.parse_args()
package=args.package.resolve();project=args.unity_project.resolve();fixture=args.fixture.resolve()
config=json.loads(fixture.read_text(encoding='utf-8'))
token=config['Token'];account=config['Accounts'][0]
assert len(token)==8 and all(c in '0123456789abcdef' for c in token)
assert config['Host']=='127.0.0.1' and config['ShopNpcRace']>0
assert account['Login']=='codexitem'+token+'0' and account['Name']=='CI'+token+'0'
_,_,_,auth=item_bench.connection(package)
db=Client(**auth,library=str(package/'bin/libmariadb.dll'))
request=project/'Temp/typed-shop-validation-request.json'
result=project/'Temp/typed-shop-validation-result.json'
assert not request.exists(),'An Editor validation request is already pending'
result.unlink(missing_ok=True)
for phase in range(3):
    for suffix in ('.json','.ready','.failed'):
        marker=project/f'Temp/typed-shop-live-{token}-{phase}{suffix}'
        assert marker.resolve().is_relative_to((project/'Temp').resolve())
        marker.unlink(missing_ok=True)
request.write_text(json.dumps(dict(Mode='live',ConfigPath=str(fixture))),encoding='utf-8')
print('Requested NPC shop live validation in open Editor',flush=True)
done=set();started=time.monotonic()
backup=package/'run/item-shop-live'/datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S')
backup.mkdir(parents=True)
try:
    while not result.exists():
        if time.monotonic()-started>args.timeout:raise TimeoutError('Open Editor NPC shop validation deadline')
        for phase in range(3):
            marker=project/f'Temp/typed-shop-live-{token}-{phase}.json'
            if phase in done or not marker.exists():continue
            record=json.loads(marker.read_text(encoding='utf-8'));pid=int(record['Pid'])
            assert record['Token']==token and record['Phase']==phase and pid==int(account['Pid'])
            assert db.query('SELECT account_id,name FROM player.player WHERE id='+str(pid))==[[str(account['AccountId']),account['Name']]],'Dedicated character ownership mismatch'
            if phase==0:
                previous=item_bench.run_tool(dict(action='load',owner=pid,connection=auth),args.bench_tool.resolve())
                (backup/'before.json').write_text(json.dumps(previous,ensure_ascii=False,indent=2),encoding='utf-8')
            db.query('UPDATE player.player SET map_index='+str(int(config['MapIndex']))+',x='+str(int(config['X']))+',y='+str(int(config['Y']))+',hp=1 WHERE id='+str(pid)+' AND account_id='+str(int(account['AccountId'])))
            prepared=item_bench.run_tool(dict(action='setup',owner=pid,connection=auth,reset=True,gold=0 if phase==1 else 1000),args.bench_tool.resolve())
            if phase==2:
                layout=prepared['state']['layout'];cells=layout['columns']*layout['rows']*layout['pages']
                for _ in range(cells):item_bench.run_tool(dict(action='grant',owner=pid,connection=auth,category=6,local=config['MaterialLocal'],level=0,count=200),args.bench_tool.resolve())
            marker.with_suffix('.ready').write_text(token,encoding='ascii')
            done.add(phase);print('Provisioned dedicated NPC shop phase',phase,'pid',pid,flush=True)
        time.sleep(.2)
    outcome=json.loads(result.read_text(encoding='utf-8'))
    if not outcome['Success']:raise RuntimeError(outcome.get('Error','NPC shop live validation failed'))
    assert done=={0,1,2},'All three fixture cases must run'
    pid=int(account['Pid']);helper=args.bench_tool.resolve()
    final=item_bench.run_tool(dict(action='load',owner=pid,connection=auth),helper)
    (backup/'after.json').write_text(json.dumps(final,ensure_ascii=False,indent=2),encoding='utf-8')
    item_bench.run_tool(dict(action='setup',owner=pid,connection=auth,reset=True,gold=1000),helper)
    for category,local,count in ((1,config['WeaponLocal'],1),(5,config['FlaskLocal'],3),(6,config['MaterialLocal'],4)):
        item_bench.run_tool(dict(action='grant',owner=pid,connection=auth,category=category,local=local,level=0,count=count),helper)
    print('Dedicated character left ready for manual testing: 1000 Yang, three demo stacks',flush=True)
    print('NPC_SHOP_UNITY_LIVE=SUCCESS; previous typed owner backup:',backup,flush=True)
except BaseException:
    for phase in range(3):
        marker=project/f'Temp/typed-shop-live-{token}-{phase}.json'
        if phase not in done and marker.exists():marker.with_suffix('.failed').write_text('fixture provisioning failed',encoding='ascii')
    raise
finally:
    db.close()
