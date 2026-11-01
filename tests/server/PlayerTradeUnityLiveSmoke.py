"""Provision two verified test PCs for the open Unity Editor's TLS trade test.

Uses the existing SeedTypedItemUnityLive private fixture. Resets only its two
codexitem accounts while both clients are at character selection. Saves before
and after owner states in the owned package, then leaves manual demo inventories.
"""
import argparse
import datetime
import json
from pathlib import Path
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
token = config['Token']; accounts = config['Accounts']
assert config['Host'] == '127.0.0.1' and len(accounts) == 2
assert len(token) == 8 and all(c in '0123456789abcdef' for c in token)
_, _, _, auth = item_bench.connection(package)
db = Client(**auth, library=str(package / 'bin/libmariadb.dll'))
for side, account in enumerate(accounts):
    assert account['Login'] == 'codexitem' + token + str(side)
    assert account['Name'] == 'CI' + token + str(side)
    assert db.query('SELECT account_id,name FROM player.player WHERE id=' + str(int(account['Pid']))) == [[str(account['AccountId']), account['Name']]], 'Dedicated character ownership mismatch'
request = project / 'Temp/typed-shop-validation-request.json'
result = project / 'Temp/typed-shop-validation-result.json'
assert not request.exists(), 'An Editor validation request is already pending'
result.unlink(missing_ok=True)
for phase in range(2):
    for suffix in ('.json', '.ready', '.failed'):
        marker = project / f'Temp/typed-trade-live-{token}-{phase}{suffix}'
        assert marker.resolve().is_relative_to((project / 'Temp').resolve())
        marker.unlink(missing_ok=True)
backup = package / 'run/item-trade-live' / datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S')
backup.mkdir(parents=True)
request.write_text(json.dumps(dict(Mode='trade-live', ConfigPath=str(fixture))), encoding='utf-8')
print('Requested two-client player trade live validation in open Editor', flush=True)
done = set(); started = time.monotonic()

def bench(action, side, **values):
    return item_bench.run_tool(dict(action=action, owner=int(accounts[side]['Pid']), connection=auth, **values), helper)

try:
    while not result.exists():
        if time.monotonic() - started > args.timeout:
            raise TimeoutError('Open Editor player trade validation deadline')
        for phase in range(2):
            marker = project / f'Temp/typed-trade-live-{token}-{phase}.json'
            if phase in done or not marker.exists(): continue
            record = json.loads(marker.read_text(encoding='utf-8'))
            assert record['Token'] == token and record['Phase'] == phase and record['Pids'] == [int(a['Pid']) for a in accounts]
            for side, account in enumerate(accounts):
                pid = int(account['Pid'])
                assert db.query('SELECT account_id,name FROM player.player WHERE id=' + str(pid)) == [[str(account['AccountId']), account['Name']]]
                state = bench('load', side)
                (backup / f'phase-{phase}-before-{side}.json').write_text(json.dumps(state, ensure_ascii=False, indent=2), encoding='utf-8')
                db.query('UPDATE player.player SET map_index=' + str(int(config['MapIndex'])) + ',x=' + str(int(config['X']) + side * 100) + ',y=' + str(int(config['Y'])) + ',hp=1 WHERE id=' + str(pid) + ' AND account_id=' + str(int(account['AccountId'])))
                prepared = bench('setup', side, reset=True, gold=1000 if side == 0 else 500)
                if side == 0:
                    bench('grant', side, category=1, local=config['WeaponLocal'], level=1 if phase == 0 else 0, count=1)
                    bench('grant', side, category=6, local=config['MaterialLocal'], level=0, count=4 if phase == 0 else 1)
                elif phase == 0:
                    bench('grant', side, category=6, local=config['MaterialLocal'], level=0, count=10)
                else:
                    layout = prepared['state']['layout']
                    for _ in range(layout['columns'] * layout['rows'] * layout['pages']):
                        bench('grant', side, category=6, local=config['MaterialLocal'], level=0, count=200)
            marker.with_suffix('.ready').write_text(token, encoding='ascii')
            done.add(phase); print('Provisioned both dedicated trade clients for phase', phase, flush=True)
        time.sleep(.2)
    outcome = json.loads(result.read_text(encoding='utf-8'))
    if not outcome['Success']: raise RuntimeError(outcome.get('Error', 'Player trade validation failed'))
    assert done == {0, 1}, 'Both trade fixtures must run'
    for side in range(2):
        final = bench('load', side)
        (backup / f'after-{side}.json').write_text(json.dumps(final, ensure_ascii=False, indent=2), encoding='utf-8')
        bench('setup', side, reset=True, gold=1000 if side == 0 else 500)
        for category, local, level, count in ((1, config['WeaponLocal'], 1 if side == 0 else 0, 1), (5, config['FlaskLocal'], 0, 3), (6, config['MaterialLocal'], 0, 4 if side == 0 else 10)):
            bench('grant', side, category=category, local=local, level=level, count=count)
    print('Both dedicated characters left ready for manual trade testing', flush=True)
    print('PLAYER_TRADE_UNITY_LIVE=SUCCESS; typed owner backups:', backup, flush=True)
except BaseException:
    for phase in range(2):
        marker = project / f'Temp/typed-trade-live-{token}-{phase}.json'
        if phase not in done and marker.exists(): marker.with_suffix('.failed').write_text('fixture provisioning failed', encoding='ascii')
    raise
finally:
    db.close()
