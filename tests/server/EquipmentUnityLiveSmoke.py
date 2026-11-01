"""Run C11 equipment in the open Unity Editor against one verified local test PC.

Requires a private equipment fixture, the owned local package, and a harmless
metin next to its dedicated spawn. Backs up typed state and player position;
never provisions a normal account or changes the legacy item inventory.
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
parser.add_argument('--timeout', type=int, default=360)
args = parser.parse_args()
package = args.package.resolve(); project = args.unity_project.resolve()
config = json.loads(args.fixture.read_text(encoding='utf-8'))
token = config['Token']; account = config['Accounts'][0]; pid = int(account['Pid'])
assert config['Host'] == '127.0.0.1' and config['DamageRace'] == 8001
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
        if time.monotonic() > deadline: raise TimeoutError('Owned endpoints did not start') from None
        time.sleep(.2)
request = project / 'Temp/typed-shop-validation-request.json'; result = project / 'Temp/typed-shop-validation-result.json'
assert not request.exists(), 'An Editor validation request is pending'
result.unlink(missing_ok=True)
marker = project / f'Temp/typed-equipment-live-{token}-0.json'
for suffix in ('.json', '.ready', '.failed'): marker.with_suffix(suffix).unlink(missing_ok=True)
backup = package / 'run/item-equipment-live' / datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S')
backup.mkdir(parents=True)
request.write_text(json.dumps(dict(Mode='equipment-live', ConfigPath=str(args.fixture.resolve()))), encoding='utf-8')
print('Requested real TLS equipment validation in open Editor', flush=True)
def bench(action, **values): return item_bench.run_tool(dict(action=action, owner=pid, connection=auth, **values), args.bench_tool)
prepared = False; started = time.monotonic()
try:
    while not result.exists():
        if time.monotonic() - started > args.timeout: raise TimeoutError('Open Editor equipment validation deadline')
        if not prepared and marker.exists():
            record = json.loads(marker.read_text(encoding='utf-8'))
            assert record['Token'] == token and record['Phase'] == 0 and record['Pid'] == pid
            assert db.query(identity) == [[str(account['AccountId']), account['Name']]]
            before = dict(typed=bench('load'), player=db.query('SELECT map_index,x,y,hp FROM player.player WHERE id=' + str(pid)))
            (backup / 'before.json').write_text(json.dumps(before, indent=2), encoding='utf-8')
            db.query('UPDATE player.player SET map_index=' + str(int(config['MapIndex'])) + ',x=' + str(int(config['X'])) + ',y=' + str(int(config['Y'])) + ',hp=99999 WHERE id=' + str(pid) + ' AND account_id=' + str(int(account['AccountId'])))
            bench('setup', reset=True, gold=1000)
            for category, local, count in ((1, config['WeaponLocal'], 1), (2, config['ArmorLocal'], 1), (5, config['FlaskLocal'], 3), (6, config['MaterialLocal'], 20)):
                bench('grant', category=category, local=local, level=0, count=count)
            marker.with_suffix('.ready').write_text(token, encoding='ascii'); prepared = True
            print('Provisioned dedicated equipment fixture pid', pid, flush=True)
        time.sleep(.2)
    outcome = json.loads(result.read_text(encoding='utf-8'))
    if not outcome['Success']: raise RuntimeError(outcome.get('Error', 'Equipment validation failed'))
    assert prepared
    (backup / 'after.json').write_text(json.dumps(bench('load'), indent=2), encoding='utf-8')
    print('EQUIPMENT_UNITY_LIVE=SUCCESS; owner and position backup:', backup, flush=True)
except BaseException:
    if marker.exists() and not prepared: marker.with_suffix('.failed').write_text('fixture failed', encoding='ascii')
    raise
finally: db.close()
