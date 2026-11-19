"""Remove empty legacy item state on a stopped, owned local package.

Definitions and the deferred merge inventory remain offline conversion inputs.
No account, character, native item, native wallet or Mission row is deleted.
"""
from pathlib import Path
import argparse
import hashlib
import json
import sys

ROOT = Path(__file__).resolve().parents[2]
LEGACY_TABLES = tuple(json.loads((ROOT/'db/retired_item_tables.json').read_text()))


def digest(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, ensure_ascii=False,
                                    separators=(',', ':')).encode()).hexdigest()


def inspect(client):
    present = {r[0] for r in client.query(
        "SELECT TABLE_NAME FROM information_schema.tables WHERE TABLE_SCHEMA='player'")}
    counts = {name: int(client.query('SELECT COUNT(*) FROM player.`'+name+'`')[0][0])
              for name in LEGACY_TABLES if name in present}
    if any(counts.values()):
        raise RuntimeError('Legacy item state is not empty; retirement refused without deleting data')
    if 'item_persistence_format' not in present:
        raise RuntimeError('Native item persistence is not installed')
    # A stable, content-based proof, including native rows that survived earlier
    # playtests. Only hashes leave the database connection.
    fingerprints = {}
    for name in sorted(present):
        if not name.startswith('item_') or name in LEGACY_TABLES:
            continue
        if not name.replace('_', '').isalnum():
            raise RuntimeError('Unexpected native table name')
        rows = client.query('SELECT * FROM player.`'+name+'`')
        rows = sorted(rows, key=lambda row: json.dumps(row, ensure_ascii=False))
        fingerprints[name] = dict(rows=len(rows), sha256=digest(rows))
    return counts, fingerprints


def retire(client, apply=False):
    counts, before = inspect(client)
    # Refuse unknown dependents before the first DDL; drop known children first.
    dependencies = client.query("SELECT TABLE_SCHEMA,TABLE_NAME,REFERENCED_TABLE_NAME FROM information_schema.key_column_usage WHERE REFERENCED_TABLE_SCHEMA='player' AND REFERENCED_TABLE_NAME IS NOT NULL")
    pending = set(counts)
    for schema, child, parent in dependencies:
        if parent in pending and (schema != 'player' or child not in pending):
            raise RuntimeError('Unexpected dependent of retired item state: '+schema+'.'+child)
    ordered = []
    while pending:
        leaves = sorted(name for name in pending if not any(parent == name and child in pending for _, child, parent in dependencies))
        if not leaves:
            raise RuntimeError('Cyclic retired item table dependencies')
        ordered.extend(leaves)
        pending.difference_update(leaves)
    columns = {r[0] for r in client.query(
        "SELECT COLUMN_NAME FROM information_schema.columns WHERE TABLE_SCHEMA='player' "
        "AND TABLE_NAME='item_persistence_format'")}
    result = dict(legacy_tables=counts, native_tables=before, applied=False)
    if not apply:
        return result
    if 'native_runtime_version' not in columns:
        client.query((ROOT/'db/migrations/item_instance/041_native_runtime.sql').read_text())
    if 'item_native_award' not in before:
        client.query((ROOT/'db/migrations/item_instance/042_native_item_awards.sql').read_text())
    # The caller has checked the owned package's service supervisor before
    # entering this function. Recheck the empty state immediately before DDL.
    inspect(client)
    if counts:
        client.query('DROP TABLE '+', '.join('player.`'+name+'`' for name in ordered))
    remaining, after = inspect(client)
    if remaining:
        raise RuntimeError('Legacy table retirement incomplete')
    # Only the format marker and a new, empty native award queue may be added.
    before.pop('item_persistence_format', None)
    after.pop('item_persistence_format', None)
    if 'item_native_award' not in before:
        if after.get('item_native_award') != dict(rows=0, sha256=digest([])):
            raise RuntimeError('New native award queue is not empty')
        after.pop('item_native_award')
    if before != after:
        raise RuntimeError('Native state changed during retirement')
    result.update(applied=True, native_tables=after, native_state_unchanged=True)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--package', type=Path, default=Path('D:/AurigaGlobal/Server2-Local'))
    parser.add_argument('--apply', action='store_true')
    parser.add_argument('--evidence', type=Path)
    args = parser.parse_args()
    package = args.package.resolve()
    if package != Path('D:/AurigaGlobal/Server2-Local').resolve():
        raise RuntimeError('Only the explicitly owned local package is authorized')
    sys.path[:0] = [str(ROOT/'SRC/Tools/ServerManager'), str(ROOT/'db/tools')]
    import backend
    import item_bench
    from mariadb_client import Client
    _, managed, _, auth = item_bench.connection(package)
    if auth.get('host', '127.0.0.1') != '127.0.0.1' or int(auth['port']) != 3307:
        raise RuntimeError('Only the owned loopback database is authorized')
    managed.verify_datadir()
    client = Client(**auth, library=str(package/'bin/libmariadb.dll'))
    try:
        with managed.supervisor_lock():
            if any(row['running'] for row in backend.describe(package)['rows']):
                raise RuntimeError('Stop the owned game services before item retirement')
            result = retire(client, args.apply)
    finally:
        client.close()
    result['package'] = str(package)
    result['deferred_merge_items'] = 637
    if args.evidence:
        args.evidence.parent.mkdir(parents=True, exist_ok=True)
        args.evidence.write_text(json.dumps(result, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    print(json.dumps(dict(applied=result['applied'], legacy_tables=result['legacy_tables'],
                         native_tables=len(result['native_tables']), deferred_merge_items=637)))


if __name__ == '__main__':
    main()
