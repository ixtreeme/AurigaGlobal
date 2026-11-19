"""Read-only retirement proof using two freshly created, owned native test accounts."""
from pathlib import Path
import argparse, hashlib, json, sys

REPO = Path(__file__).resolve().parents[2]
sys.path[:0] = [str(REPO/'SRC/Tools/ServerManager'), str(REPO/'db/tools')]
import item_bench
from mariadb_client import Client


def digest(rows):
    return hashlib.sha256(json.dumps(rows, separators=(',', ':')).encode()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=('prepare', 'verify'))
    parser.add_argument('--package', type=Path, required=True)
    parser.add_argument('--fixture', type=Path, required=True)
    parser.add_argument('--source', type=Path)
    args = parser.parse_args()
    path = args.fixture.resolve()
    assert path.is_relative_to(REPO/'build') and path.name.endswith('.private.json')
    if args.action == 'prepare':
        assert not path.exists() and args.source is not None
        source = args.source.resolve()
        assert source.is_relative_to(REPO/'build') and source.name.endswith('.private.json')
        config = json.loads(source.read_text(encoding='utf-8'))
    else:
        config = json.loads(path.read_text(encoding='utf-8'))
    assert config['Host'] == '127.0.0.1'
    actors = config['Accounts']
    assert len(actors) == 2 and all(a['Login'] == 'codexitem'+config['Token']+str(i) for i,a in enumerate(actors))
    root, manager, _, auth = item_bench.connection(args.package.resolve())
    manager.verify_datadir()
    ids = ','.join(str(int(a['Pid'])) for a in actors)
    account_ids = ','.join(str(int(a['AccountId'])) for a in actors)
    db = Client(**auth, library=str(root/'bin/libmariadb.dll'))
    try:
        retired=json.loads((REPO/'db/retired_item_tables.json').read_text())
        assert db.query("SELECT COUNT(*) FROM information_schema.tables WHERE table_schema='player' AND table_name IN ("+','.join("'"+n+"'" for n in retired)+")") == [['0']]
        for actor in actors:
            assert db.query('SELECT account_id FROM player.player WHERE id='+str(int(actor['Pid']))) == [[str(actor['AccountId'])]]
        proof = dict(
            native_other=digest(db.query(f'SELECT * FROM player.item_instance WHERE NOT(owner_kind=1 AND owner_id IN({ids})) ORDER BY instance_id')),
            wallet_other=digest(db.query(f'SELECT * FROM player.item_wallet WHERE NOT((owner_kind=1 AND owner_id IN({ids})) OR (owner_kind=2 AND owner_id IN({account_ids}))) ORDER BY owner_kind,owner_id,currency')),
            owned_units=db.query(f'SELECT owner_id,category_id,local_id,refinement_level,SUM(stack_count) FROM player.item_instance WHERE owner_kind=1 AND owner_id IN({ids}) AND destroyed=0 GROUP BY owner_id,category_id,local_id,refinement_level ORDER BY owner_id,category_id,local_id,refinement_level'),
            owned_wallets=db.query(f'SELECT owner_id,currency,balance FROM player.item_wallet WHERE owner_kind=1 AND owner_id IN({ids}) ORDER BY owner_id,currency'))
        if args.action == 'prepare':
            config['RetirementProof'] = proof
            path.write_text(json.dumps(config, indent=2)+'\n', encoding='utf-8')
            print('ITEM_RETIREMENT_FIXTURE=READY native_accounts=2 legacy_tables=0 no_database_writes=1')
        else:
            assert proof == config['RetirementProof'], 'Native units, balances or unrelated items changed'
            # These fresh accounts start with zero Dragon Coins. The normal UI
            # may provision their account wallet during the test; that is owned
            # fixture state, and must not be mistaken for another player's money.
            account_wallets = db.query(f'SELECT currency,balance FROM player.item_wallet WHERE owner_kind=2 AND owner_id IN({account_ids})')
            assert all(row == ['2', '0'] for row in account_wallets)
            assert db.query(f'SELECT COUNT(*) FROM player.item_ground_entry g JOIN player.item_instance i ON i.instance_id=g.instance_id WHERE i.owner_kind=1 AND i.owner_id IN({ids})') == [['0']]
            print('ITEM_RETIREMENT_SQL=SUCCESS legacy_tables=0 native_units_wallets_preserved=1 unrelated_items_preserved=1 no_ground_escrow=1')
    finally:
        db.close()


if __name__ == '__main__':
    main()
