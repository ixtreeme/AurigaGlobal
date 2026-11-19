"""Move reviewed account collection seals from a verified private checkpoint."""

import argparse
import csv
from decimal import Decimal
import json
from pathlib import Path

from capture_legacy_items import ROOT, local_source
from equipment_cutover import import_tool
from item_bench import connection
from legacy_item_migration import MigrationError, decode_table, file_hash, strict_json, value_hash
from mariadb_client import statements


COLLECTIONS = {1: 'player.account_mount_inventory', 2: 'player.account_pet_inventory'}
RECEIPTS = ROOT / 'db/migrations/item_instance/045_legacy_collection_import.sql'


def read_json(path):
    return strict_json(path.read_text(encoding='utf-8-sig'))


def checkpoint_rows(checkpoint, manifest, table):
    entry = manifest['tables'].get(table)
    if entry is None:
        return None
    path = checkpoint / (table + '.json')
    if entry['file'] != path.name or file_hash(path) != entry['sha256']:
        raise MigrationError('Collection checkpoint digest differs: ' + table)
    rows = decode_table(read_json(path))
    if len(rows) != entry['rows']:
        raise MigrationError('Collection checkpoint count differs: ' + table)
    return rows


def latest_mount_positions(archive, csv_path):
    if archive is None:
        raise MigrationError('A verified mount checkpoint is required with the newer export')
    originals = {row['id']: row for row in archive}
    merged = []
    with csv_path.open(encoding='utf-8-sig', newline='') as stream:
        for row in csv.DictReader(stream):
            numbers = {name: Decimal(value) for name, value in row.items()}
            if any(not number.is_finite() or number != number.to_integral_value() for number in numbers.values()):
                raise MigrationError('Non-integer value in newer collection export')
            updated = {name: int(value) for name, value in numbers.items()}
            source = originals.pop(updated['id'], None)
            if source is None or dict(source, slot=updated['slot']) != updated:
                raise MigrationError('Newer mount export differs beyond the collection slot')
            merged.append(updated)
    if originals:
        raise MigrationError('Newer collection export omits archived mounts')
    return merged


def reviewed_collections(checkpoint, candidate_path, exclusions_path, mount_csv=None):
    manifest = read_json(checkpoint / 'manifest.json')
    if manifest.get('complete') is not True:
        raise MigrationError('Incomplete collection checkpoint')
    candidate = read_json(candidate_path)
    exclusions = read_json(exclusions_path)
    if exclusions['candidate_sha256'] != file_hash(candidate_path):
        raise MigrationError('Collection mapping does not match the excluded-item manifest')
    if exclusions['deferred_count'] != len(exclusions['items']):
        raise MigrationError('Excluded-item manifest count differs')
    excluded = {row['source_id'] for row in exclusions['items'] + candidate['blocked'] + candidate['retired']}
    accepted = {row['source_id']: row for row in candidate['accepted']}
    items, counts = [], {}
    hashes = {}
    for store, table in COLLECTIONS.items():
        rows = checkpoint_rows(checkpoint, manifest, table)
        counts[table] = None if rows is None else len(rows)
        if rows is not None:
            hashes[table] = manifest['tables'][table]['sha256']
        if store == 1 and mount_csv is not None:
            rows = latest_mount_positions(rows, mount_csv)
            hashes['newer_mount_export'] = file_hash(mount_csv)
        for row in sorted(rows or (), key=lambda row: row['id']):
            source = row['vnum']
            mapping = accepted.get(source)
            if source in excluded or not mapping or mapping['refinement_level'] != 0:
                raise MigrationError('Collection source needs an approved typed seal: ' + str(source))
            items.append(dict(mapping['item'], source_store=store, source=row))
    return counts, items, value_hash(hashes)


def migrate(package, helper, counts, items, checkpoint_hash):
    _, _, _, auth = connection(package)
    # The native write uses account fences and atomic receipts. This lock
    # serializes administrative imports until the owning connection closes.
    with local_source(package) as (_, _, _, db):
        if db.query("SELECT GET_LOCK('auriga.legacy_collection_import', 0)") != [['1']]:
            raise MigrationError('Another collection import is in progress')
        live = db.query("SELECT TABLE_NAME FROM information_schema.TABLES WHERE TABLE_SCHEMA='player' "
                        "AND TABLE_NAME IN ('account_mount_inventory','account_pet_inventory')")
        for (table,) in live:
            # Never assume an archived snapshot covers a legacy store still open for writes.
            if db.query('SELECT 1 FROM player.' + table + ' LIMIT 1'):
                raise MigrationError('Live legacy collection still holds rows: ' + table)
        if not items:
            return dict(source_rows=counts, imported=0, replayed=0)
        for sql in statements(RECEIPTS.read_text(encoding='utf-8')):
            db.query(sql)
        result = import_tool(helper, auth, dict(action='import_collections', owned_package=True,
                                               source_checkpoint_sha256=checkpoint_hash, collections=items))
        return dict(source_rows=counts, **result)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--package', type=Path, required=True)
    parser.add_argument('--helper', type=Path, required=True)
    parser.add_argument('--checkpoint', type=Path, required=True)
    parser.add_argument('--candidate', type=Path, required=True)
    parser.add_argument('--exclusions', type=Path, required=True)
    parser.add_argument('--newer-mount-csv', type=Path)
    args = parser.parse_args()
    counts, items, checkpoint = reviewed_collections(args.checkpoint, args.candidate, args.exclusions,
                                                     args.newer_mount_csv)
    print(json.dumps(migrate(args.package, args.helper, counts, items, checkpoint), sort_keys=True))


if __name__ == '__main__':
    main()
