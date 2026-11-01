"""Provision verified private crafting fixtures and request the open Unity Editor.

Only resets the existing dedicated codexitem character, while it is at
character selection. Backs up each phase and leaves a usable manual fixture.
Requires the owned local package with the demo typed_crafting.json installed.
"""
import argparse
import datetime
import json
from pathlib import Path
import socket
import sys
import time

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / 'SRC/Tools/ServerManager'))
sys.path.insert(0, str(REPO / 'db/tools'))
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
fixture = args.fixture.resolve(); helper = args.bench_tool.resolve()
config = json.loads(fixture.read_text(encoding='utf-8'))
token = config['Token']; account = config['Accounts'][0]
assert config['Host'] == '127.0.0.1' and config['CraftNpcRace'] > 0
assert len(token) == 8 and all(c in '0123456789abcdef' for c in token)
assert account['Login'] == 'codexitem' + token + '0' and account['Name'] == 'CI' + token + '0'
_, _, _, auth = item_bench.connection(package)
db = Client(**auth, library=str(package / 'bin/libmariadb.dll'))
pid = int(account['Pid'])
assert db.query('SELECT account_id,name FROM player.player WHERE id=' + str(pid)) == [[str(account['AccountId']), account['Name']]], 'Dedicated character ownership mismatch'
# The owning supervisor starts the cores and login endpoint in order.
# Wait for both endpoints before handing the request to the Editor.
ready_deadline = time.monotonic() + 60
while True:
    try:
        for port in (int(config['AuthPort']), int(config['GamePort'])):
            with socket.create_connection(('127.0.0.1', port), timeout=2): pass
        break
    except OSError:
        if time.monotonic() > ready_deadline: raise TimeoutError('Owned game endpoints are not listening') from None
        time.sleep(.2)
request = project / 'Temp/typed-shop-validation-request.json'; result = project / 'Temp/typed-shop-validation-result.json'
assert not request.exists(), 'An Editor validation request is already pending'
result.unlink(missing_ok=True)
for phase in range(4):
    for suffix in ('.json', '.ready', '.failed'):
        marker = project / f'Temp/typed-craft-live-{token}-{phase}{suffix}'
        assert marker.resolve().is_relative_to((project / 'Temp').resolve())
        marker.unlink(missing_ok=True)
backup = package / 'run/item-craft-live' / datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S')
backup.mkdir(parents=True)
request.write_text(json.dumps(dict(Mode='craft-live', ConfigPath=str(fixture))), encoding='utf-8')
print('Requested real TLS crafting validation in open Editor', flush=True)
done = set(); started = time.monotonic()

def bench(action, **values):
    return item_bench.run_tool(dict(action=action, owner=pid, connection=auth, **values), helper)

try:
    while not result.exists():
        if time.monotonic() - started > args.timeout: raise TimeoutError('Open Editor crafting validation deadline')
        for phase in range(4):
            marker = project / f'Temp/typed-craft-live-{token}-{phase}.json'
            if phase in done or not marker.exists(): continue
            record = json.loads(marker.read_text(encoding='utf-8'))
            assert record['Token'] == token and record['Phase'] == phase and int(record['Pid']) == pid
            assert db.query('SELECT account_id,name FROM player.player WHERE id=' + str(pid)) == [[str(account['AccountId']), account['Name']]]
            state = bench('load'); (backup / f'phase-{phase}-before.json').write_text(json.dumps(state, ensure_ascii=False, indent=2), encoding='utf-8')
            db.query('UPDATE player.player SET map_index=' + str(int(config['MapIndex'])) + ',x=' + str(int(config['X'])) + ',y=' + str(int(config['Y'])) + ',hp=1 WHERE id=' + str(pid) + ' AND account_id=' + str(int(account['AccountId'])))
            prepared = bench('setup', reset=True, gold=0 if phase == 1 else 1000)
            if phase in (0, 1): bench('grant', category=6, local=config['MaterialLocal'], level=0, count=20)
            elif phase == 3:
                layout = prepared['state']['layout']
                for _ in range(layout['columns'] * layout['rows'] * layout['pages']): bench('grant', category=6, local=config['MaterialLocal'], level=0, count=200)
            marker.with_suffix('.ready').write_text(token, encoding='ascii'); done.add(phase)
            print('Provisioned dedicated crafting phase', phase, 'pid', pid, flush=True)
        time.sleep(.2)
    outcome = json.loads(result.read_text(encoding='utf-8'))
    if not outcome['Success']: raise RuntimeError(outcome.get('Error', 'Crafting validation failed'))
    assert done == {0, 1, 2, 3}, 'Every crafting fixture must run'
    final = bench('load'); (backup / 'after.json').write_text(json.dumps(final, ensure_ascii=False, indent=2), encoding='utf-8')
    bench('setup', reset=True, gold=1000)
    for category, local, level, count in ((1, config['WeaponLocal'], 1, 1), (5, config['FlaskLocal'], 0, 3), (6, config['MaterialLocal'], 0, 20)):
        bench('grant', category=category, local=local, level=level, count=count)
    print('Dedicated character ready for manual crafting: 1000 Yang, weapon +1, flask x3, material x20', flush=True)
    print('CRAFTING_UNITY_LIVE=SUCCESS; typed owner backups:', backup, flush=True)
except BaseException:
    for phase in range(4):
        marker = project / f'Temp/typed-craft-live-{token}-{phase}.json'
        if phase not in done and marker.exists(): marker.with_suffix('.failed').write_text('fixture provisioning failed', encoding='ascii')
    raise
finally:
    db.close()
