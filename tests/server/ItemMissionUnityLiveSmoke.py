"""Provision private item-mission fixtures in the owned local package.

Only resets the verified dedicated PC at character selection. Historical
mission receipts are preserved; use an explicitly fresh demo content version
when rerunning. Every reset is backed up, and a manual fixture is left ready.
"""
import argparse
import datetime
import json
from pathlib import Path
import socket
import sys
import time

REPO = Path(__file__).resolve().parents[2]
sys.path[:0] = [str(REPO / 'SRC/Tools/ServerManager'), str(REPO / 'db/tools')]
import item_bench
from mariadb_client import Client

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--package', type=Path, required=True)
parser.add_argument('--unity-project', type=Path, required=True)
parser.add_argument('--fixture', type=Path, required=True)
parser.add_argument('--bench-tool', type=Path, required=True)
parser.add_argument('--timeout', type=int, default=600)
args = parser.parse_args()
package = args.package.resolve(); project = args.unity_project.resolve()
config = json.loads(args.fixture.read_text(encoding='utf-8'))
token = config['Token']; account = config['Accounts'][0]; pid = int(account['Pid'])
assert config['Host'] == '127.0.0.1' and config['MissionNpcRace'] > 0 and config['MissionVersion'] > 0
assert len(token) == 8 and all(c in '0123456789abcdef' for c in token)
assert account['Login'] == 'codexitem' + token + '0' and account['Name'] == 'CI' + token + '0'
_, _, _, auth = item_bench.connection(package)
db = Client(**auth, library=str(package / 'bin/libmariadb.dll'))
identity = 'SELECT account_id,name FROM player.player WHERE id=' + str(pid)
assert db.query(identity) == [[str(account['AccountId']), account['Name']]]
deadline = time.monotonic() + 60
while True:
    try:
        for port in (config['AuthPort'], config['GamePort']):
            with socket.create_connection(('127.0.0.1', int(port)), timeout=2): pass
        break
    except OSError:
        if time.monotonic() > deadline: raise TimeoutError('Owned game endpoints did not start') from None
        time.sleep(.2)
request = project / 'Temp/typed-shop-validation-request.json'; result = project / 'Temp/typed-shop-validation-result.json'
assert not request.exists(), 'An Editor validation request is pending'
result.unlink(missing_ok=True)
for phase in range(4):
    for suffix in ('.json', '.ready', '.failed'):
        (project / f'Temp/typed-item-mission-live-{token}-{phase}{suffix}').unlink(missing_ok=True)
backup = package / 'run/item-mission-live' / datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S')
backup.mkdir(parents=True)
request.write_text(json.dumps(dict(Mode='item-mission-live', ConfigPath=str(args.fixture.resolve()))), encoding='utf-8')
print('Requested real TLS item-mission validation in open Editor', flush=True)
def bench(action, **values): return item_bench.run_tool(dict(action=action, owner=pid, connection=auth, **values), args.bench_tool)
done = set(); started = time.monotonic()
try:
    while not result.exists():
        if time.monotonic() - started > args.timeout: raise TimeoutError('Open Editor item-mission validation deadline')
        for phase in range(4):
            marker = project / f'Temp/typed-item-mission-live-{token}-{phase}.json'
            if phase in done or not marker.exists(): continue
            record = json.loads(marker.read_text(encoding='utf-8'))
            assert record['Token'] == token and record['Phase'] == phase and record['Pid'] == pid
            assert db.query(identity) == [[str(account['AccountId']), account['Name']]]
            before = dict(typed=bench('load'), player=db.query('SELECT map_index,x,y,hp FROM player.player WHERE id=' + str(pid)))
            (backup / f'phase-{phase}-before.json').write_text(json.dumps(before, indent=2), encoding='utf-8')
            db.query('UPDATE player.player SET map_index=41,x=986600,y=269100,hp=8924 WHERE id=' + str(pid) + ' AND account_id=' + str(int(account['AccountId'])))
            prepared = bench('setup', reset=True, gold=0 if phase == 2 else 1000)
            if phase == 1:
                layout = prepared['state']['layout']
                for _ in range(layout['columns'] * layout['rows'] * layout['pages']): bench('grant', category=6, local=config['MaterialLocal'], level=0, count=200)
            else:
                bench('grant', category=6, local=config['MaterialLocal'], level=0, count=9 if phase == 3 else 10)
                if phase == 0: bench('grant', category=5, local=config['FlaskLocal'], level=0, count=3)
            marker.with_suffix('.ready').write_text(token, encoding='ascii'); done.add(phase)
            print('Provisioned dedicated item-mission phase', phase, 'pid', pid, flush=True)
        time.sleep(.2)
    outcome = json.loads(result.read_text(encoding='utf-8'))
    if not outcome['Success']: raise RuntimeError(outcome.get('Error', 'Item-mission validation failed'))
    assert done == {0, 1, 2, 3}
    (backup / 'after.json').write_text(json.dumps(bench('load'), indent=2), encoding='utf-8')
    bench('setup', reset=True, gold=1000)
    for category, local, level, count in ((1, config['WeaponLocal'], 1, 1), (2, config['ArmorLocal'], 0, 1), (5, config['FlaskLocal'], 0, 3), (6, config['MaterialLocal'], 0, 20)):
        bench('grant', category=category, local=local, level=level, count=count)
    print('ITEM_MISSION_UNITY_LIVE=SUCCESS; typed owner backups:', backup, flush=True)
except BaseException:
    for phase in range(4):
        marker = project / f'Temp/typed-item-mission-live-{token}-{phase}.json'
        if phase not in done and marker.exists(): marker.with_suffix('.failed').write_text('fixture provisioning failed', encoding='ascii')
    raise
finally: db.close()
