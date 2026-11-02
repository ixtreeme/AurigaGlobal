"""Provision five socket TLS cases for the verified dedicated local PC."""
import argparse
import datetime
import json
from pathlib import Path
import socket
import sys
import time

repo=Path(__file__).resolve().parents[2]
sys.path[:0]=[str(repo/'SRC/Tools/ServerManager'),str(repo/'db/tools')]
import item_bench
from mariadb_client import Client
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--package',type=Path,required=True)
parser.add_argument('--unity-project',type=Path,required=True)
parser.add_argument('--fixture',type=Path,required=True)
parser.add_argument('--bench-tool',type=Path,required=True)
parser.add_argument('--timeout',type=int,default=900)
args=parser.parse_args();root=args.package.resolve();project=args.unity_project.resolve()
config=json.loads(args.fixture.read_text(encoding='utf-8'));token=config['Token'];account=config['Accounts'][0];pid=int(account['Pid'])
assert config['Host']=='127.0.0.1' and len(token)==8 and all(c in '0123456789abcdef' for c in token)
assert account['Login']=='codexitem'+token+'0' and account['Name']=='CI'+token+'0'
_,_,_,auth=item_bench.connection(root);db=Client(**auth,library=str(root/'bin/libmariadb.dll'))
identity='SELECT account_id,name FROM player.player WHERE id='+str(pid)
assert db.query(identity)==[[str(account['AccountId']),account['Name']]]
deadline=time.monotonic()+60
while True:
    try:
        for port in (config['AuthPort'],config['GamePort']):
            with socket.create_connection(('127.0.0.1',int(port)),timeout=2):pass
        break
    except OSError:
        if time.monotonic()>deadline:raise TimeoutError('Owned local endpoints not listening') from None
        time.sleep(.2)
request=project/'Temp/typed-shop-validation-request.json';result=project/'Temp/typed-shop-validation-result.json'
persisted=project/'validation-source-snapshot-result.json' if (project/'validation-source-snapshot.json').is_file() else None
assert not request.exists(),'An Editor request is already pending'
request.parent.mkdir(parents=True,exist_ok=True);result.unlink(missing_ok=True)
if persisted:persisted.unlink(missing_ok=True)
for phase in range(5):
    for suffix in ('.json','.ready','.failed'):(project/f'Temp/typed-socket-live-{token}-{phase}{suffix}').unlink(missing_ok=True)
backup=root/'run/socket-live'/datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S');backup.mkdir(parents=True)
request.write_text(json.dumps(dict(Mode='socket-live',ConfigPath=str(args.fixture.resolve()))),encoding='utf-8')
print('Requested real Unity TLS socket validation',flush=True)
def bench(action,**values):return item_bench.run_tool(dict(action=action,owner=pid,connection=auth,**values),args.bench_tool)
def grant(category,field,count=1):return bench('grant',category=category,local=config[field],level=0,count=count)
done=set();started=time.monotonic()
def read_outcome():
    for path in (persisted,result):
        if path is None:continue
        try:return json.loads(path.read_text(encoding='utf-8'))
        except (FileNotFoundError,json.JSONDecodeError):pass
    return None
try:
    while (outcome:=read_outcome()) is None:
        if time.monotonic()-started>args.timeout:raise TimeoutError('Unity socket validation deadline')
        # A cold Editor clears Temp during startup. Republish a lost request
        # until the bridge records that it owns the running validation.
        status=project/'Temp/typed-shop-validation-status.txt'
        if not request.exists() and not status.exists() and not done:
            request.parent.mkdir(parents=True,exist_ok=True)
            request.write_text(json.dumps(dict(Mode='socket-live',ConfigPath=str(args.fixture.resolve()))),encoding='utf-8')
        for phase in range(5):
            marker=project/f'Temp/typed-socket-live-{token}-{phase}.json'
            if phase in done or not marker.exists():continue
            record=json.loads(marker.read_text());assert record['Token']==token and record['Phase']==phase and record['Pid']==pid
            assert db.query(identity)==[[str(account['AccountId']),account['Name']]]
            (backup/f'phase-{phase}-before.json').write_text(json.dumps(dict(typed=bench('load'),player=db.query('SELECT map_index,x,y,hp FROM player.player WHERE id='+str(pid))),indent=2),encoding='utf-8')
            db.query('UPDATE player.player SET map_index=41,x=986600,y=269100,hp=8924 WHERE id='+str(pid)+' AND account_id='+str(int(account['AccountId'])))
            state=bench('setup',reset=True,gold=1000)['state']
            def occupied(local,level=0,grade=1):return dict(state=3,grade=grade,category=7,local=config[local],level=level)
            broken=lambda grade=1:dict(state=4,grade=grade)
            empty=lambda grade=1:dict(state=2,grade=grade)
            def weapon(slots,level=1):return bench('grant',category=1,local=config['SocketWeaponLocal'],level=level,count=1,stones=slots)
            filled=lambda:[occupied('SocketStoneLocal',3),broken(2),occupied('SocketSecondStoneLocal',0,2)]
            if phase==0:
                weapon(filled());weapon([broken(2),empty(),dict(state=1,grade=0)],0)
                for field,count in (('CleanLocal',3),('ExtractAllLocal',3),('ExtractLastLocal',3),('HighSocketLocal',1),('TwoUnitSocketLocal',1),('FlaskLocal',3)):grant(5,field,count)
            elif phase in (1,2,3):
                if phase==1:weapon(filled());grant(5,'ExtractAllLocal',2);grant(5,'CleanLocal',2);used=5
                elif phase==2:weapon([occupied('RetiredSocketStoneLocal',3,2),broken(),empty(2)]);grant(5,'ExtractAllLocal');used=4
                else:weapon(filled());grant(5,'ExtractLastLocal');used=4
                bag=state['layout']
                for _ in range(bag['columns']*bag['rows']*bag['pages']-used):grant(6,'MaterialLocal',200)
            else:grant(5,'CleanLocal',3);grant(5,'FlaskLocal',3)
            marker.with_suffix('.ready').write_text(token,encoding='ascii');done.add(phase)
            print('Provisioned socket phase',phase,'pid',pid,flush=True)
        time.sleep(.2)
    if not outcome['Success']:raise RuntimeError(outcome.get('Error','Socket validation failed'))
    assert done=={0,1,2,3,4}
    (backup/'after.json').write_text(json.dumps(bench('load'),indent=2),encoding='utf-8')
    bench('setup',reset=True,gold=1000)
    for category,field,count in ((8,'BoxLocal',3),(8,'ChestLocal',3),(18,'KeyLocal',3),(18,'AltKeyLocal',2),(18,'WrongKeyLocal',1),(8,'UnsupportedBoxLocal',1),(8,'EmptyBoxLocal',2),(5,'FlaskLocal',3),(6,'MaterialLocal',20),(16,'GoldTokenLocal',10),(16,'CoinTokenLocal',3),(16,'HugeTokenLocal',2)):grant(category,field,count)
    for field in ('CleanLocal','ExtractAllLocal','ExtractLastLocal'):grant(5,field,3)
    bench('grant',category=1,local=config['SocketWeaponLocal'],level=1,count=1,stones=[dict(state=3,grade=1,category=7,local=config['SocketStoneLocal'],level=3),dict(state=4,grade=2),dict(state=3,grade=2,category=7,local=config['SocketSecondStoneLocal'],level=0)])
    print('SOCKET_UNITY_LIVE=SUCCESS; backups:',backup,flush=True)
except BaseException:
    for phase in range(5):
        marker=project/f'Temp/typed-socket-live-{token}-{phase}.json'
        if phase not in done and marker.exists():marker.with_suffix('.failed').write_text('fixture preparation failed',encoding='ascii')
    raise
finally:db.close()
