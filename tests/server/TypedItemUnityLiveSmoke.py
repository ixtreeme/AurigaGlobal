"""Run the Unity typed-item smoke test against an OWNED local server package.

Seed fixtures with SeedTypedItemUnityLive.py while the game stack is stopped,
then start the stack freshly. Keeps dedicated test characters and credentials
in the private fixture file so they can also be used interactively.
"""
import argparse, json, os, secrets, subprocess, sys, time
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO/'SRC/Tools/ServerManager'))
sys.path.insert(0, str(REPO/'db/tools'))
import item_bench
from mariadb_client import Client

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--package',type=Path,required=True)
parser.add_argument('--unity-project',type=Path,required=True)
parser.add_argument('--unity-editor',type=Path,required=True)
parser.add_argument('--bench-tool',type=Path,required=True)
parser.add_argument('--fixture',type=Path,required=True)
parser.add_argument('--mariadb-library',type=Path)
parser.add_argument('--timeout',type=int,default=600)
args=parser.parse_args()
package=args.package.resolve();project=args.unity_project.resolve();config_path=args.fixture.resolve()
_,_,_,auth=item_bench.connection(package)
db=Client(**auth,library=str(args.mariadb_library or package/'bin/libmariadb.dll'))
config=json.loads(config_path.read_text(encoding='utf-8'))
assert config['Host']=='127.0.0.1' and all(config.get(k) for k in ('WeaponLocal','FlaskLocal','MaterialLocal'))
token=config.setdefault('Token',secrets.token_hex(4))
assert len(token)==8 and all(c in '0123456789abcdef' for c in token)
accounts=config.get('Accounts',[])
if not accounts:
    for i in range(2):
        login=f'codexitem{token}{i}';password=secrets.token_hex(8)
        db.query("INSERT INTO account.account(login,password,social_id,status,create_time) VALUES('"+login+"',PASSWORD('"+password+"'),'1234567','OK',NOW())")
        account_id=int(db.query("SELECT id FROM account.account WHERE login='"+login+"'")[0][0])
        accounts.append(dict(Login=login,Password=password,Name=f'CI{token}{i}',AccountId=account_id))
    config['Accounts']=accounts
assert len(accounts)==2
for i,account in enumerate(accounts):
    assert account['Login']==f'codexitem{token}{i}' and account['Name']==f'CI{token}{i}'
    assert db.query('SELECT login FROM account.account WHERE id='+str(int(account['AccountId'])))==[[account['Login']]], 'dedicated account ownership mismatch'
    db.query('INSERT INTO player.player_index(id,empire) VALUES('+str(int(account['AccountId']))+',1) ON DUPLICATE KEY UPDATE empire=1')
    if account.get('Pid'):
        db.query('UPDATE player.player SET map_index='+str(int(config['MapIndex']))+',x='+str(int(config['X']))+',y='+str(int(config['Y']))+',hp=1 WHERE id='+str(int(account['Pid']))+' AND account_id='+str(int(account['AccountId'])))
for i in range(2):
    for extension in ('.json','.ready'):
        marker=project/f'Temp/typed-item-live-{token}-{i}{extension}'
        assert marker.resolve().is_relative_to((project/'Temp').resolve())
        marker.unlink(missing_ok=True)
config_path.write_text(json.dumps(config),encoding='utf-8')
print('Dedicated localhost fixture:',config_path,flush=True)
environment=dict(os.environ,AURIGA_TYPED_ITEM_LIVE_CONFIG=str(config_path))
startup=None
if os.name=='nt':
    startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
log=REPO/'build/typed-items-live-unity.log';log.parent.mkdir(exist_ok=True)
process=subprocess.Popen([str(args.unity_editor),'-batchmode','-nographics','-projectPath',str(project),'-executeMethod','Metin2.Editor.Metin2TypedItemLiveValidator.RunFromCommandLine','-logFile',str(log)],env=environment,startupinfo=startup)
done = set()
helper=args.bench_tool.resolve()
started=time.monotonic()
try:
    while process.poll() is None:
        if time.monotonic()-started>args.timeout:raise TimeoutError('Unity live smoke deadline')
        for i in range(2):
            marker = project/f'Temp/typed-item-live-{token}-{i}.json'
            if i in done or not marker.is_file(): continue
            record = json.loads(marker.read_text(encoding='utf-8'))
            assert record['Token']==token and record['AccountIndex']==i
            pid = int(record['Pid'])
            actual = db.query('SELECT account_id,name FROM player.player WHERE id='+str(pid))
            assert actual == [[str(accounts[i]['AccountId']),accounts[i]['Name']]], 'fixture ownership mismatch'
            db.query('UPDATE player.player SET map_index='+str(int(config['MapIndex']))+',x='+str(int(config['X']))+',y='+str(int(config['Y']))+',hp=1 WHERE id='+str(pid)+' AND account_id='+str(accounts[i]['AccountId']))
            item_bench.run_tool(dict(action='setup',owner=pid,connection=auth,reset=True),helper)
            if i == 0:
                for category,local,count in ((1,config['WeaponLocal'],1),(5,config['FlaskLocal'],3),(6,config['MaterialLocal'],4)):
                    item_bench.run_tool(dict(action='grant',owner=pid,connection=auth,category=category,local=local,count=count,level=0),helper)
            accounts[i]['Pid']=pid
            config_path.write_text(json.dumps(config),encoding='utf-8')
            marker.with_suffix('.ready').write_text(token,encoding='ascii')
            done.add(i)
            print('Provisioned dedicated character',i,'pid',pid,flush=True)
        time.sleep(.2)
except BaseException:
    if process.poll() is None:process.terminate();process.wait(timeout=20)
    raise
finally:
    db.close()
print('Unity live exit:',process.returncode,flush=True)
for line in log.read_text(encoding='utf-8',errors='replace').splitlines():
    if 'METIN2_TYPED_ITEM_LIVE' in line or 'Exception:' in line or 'error CS' in line: print(line)
sys.exit(process.returncode)
