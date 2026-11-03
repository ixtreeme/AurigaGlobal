"""GameServer boot-time typed catalog against a NEW owned MariaDB, never a live DB.

Installs the native protos schema, publishes one item through the real
CatalogEditorTool commit, then lets the native test load it with a
SELECT-only reader account, as the game server does at boot.
"""
import argparse
import os
from pathlib import Path
import subprocess
import sys
import uuid

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'SRC/Tools/ServerManager'))
sys.path.insert(0, str(ROOT / 'db/tools'))
import item_catalog as catalog
from backup_sandbox import Sandbox
from mariadb_client import statements
from AsyncSqlMariaDbTests import CommitResponseProxy


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mariadb-bin', type=Path, required=True)
    parser.add_argument('--test-executable', type=Path, required=True)
    parser.add_argument('--catalog-tool', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    sandbox = Sandbox(args.work_dir / uuid.uuid4().hex, args.mariadb_bin)
    proxy = None
    try:
        sandbox.start(); sandbox.verify()
        sandbox.client.query('CREATE DATABASE player CHARACTER SET utf8mb4 COLLATE utf8mb4_bin')
        paths = sorted((ROOT / 'db/migrations/item_combat').glob('[0-9][0-9][0-9]_*.sql'))
        for path in paths:
            for statement in statements(path.read_text(encoding='utf-8-sig')): sandbox.client.query(statement)
        sandbox.client.query("CREATE USER 'game_reader'@'localhost' IDENTIFIED BY '" + sandbox.password + "'")
        sandbox.client.query("GRANT SELECT ON protos.* TO 'game_reader'@'localhost'")
        sandbox.client.query("GRANT SELECT ON player.* TO 'game_reader'@'localhost'")
        sandbox.client.query("CREATE TABLE player.player(id INT UNSIGNED PRIMARY KEY) ENGINE=InnoDB")
        sandbox.client.query("CREATE TABLE player.player_deleted LIKE player.player")
        for name in ('001_item_persistence.sql' , '002_instance_id_allocator.sql', '003_owned_ground.sql', '004_recovery_schedule.sql', '005_automatic_recovery.sql', '006_item_effects.sql', '007_book_learning.sql'):
            for statement in statements((ROOT / 'db/migrations/item_instance' / name).read_text(encoding='utf-8-sig')):
                sandbox.client.query(statement)
        # The game account may claim sessions; only the importer (root here) provisions.
        sandbox.client.query("GRANT UPDATE ON player.item_owner_fence TO 'game_reader'@'localhost'")
        # ...and update the runtime registry, but cannot provision owners/storages.
        for table in ('item_owner_fence', 'item_wallet', 'item_operation_receipt', 'item_instance',
                      'item_instance_attribute', 'item_instance_socket', 'item_occupied_cell', 'item_ground_core', 'item_ground_entry'):
            sandbox.client.query("GRANT INSERT,UPDATE,DELETE ON player." + table + " TO 'game_reader'@'localhost'")
        # ...and reserve instance ID blocks for the items it creates (C7a).
        sandbox.client.query("GRANT UPDATE ON player.item_instance_id_allocator TO 'game_reader'@'localhost'")
        admin = dict(port=sandbox.port, user='root', password=sandbox.password)
        packet = catalog.run_tool(dict(action='read', connection=admin), executable=args.catalog_tool)
        document = catalog.Catalog(packet).new(1)
        tables = catalog.Catalog(packet).apply(document)
        catalog.run_tool(dict(action='commit', connection=admin, tables=tables, expected_hash=packet['hash']), packet['columns'], args.catalog_tool)
        packet = catalog.run_tool(dict(action='read', connection=admin), executable=args.catalog_tool)
        # C5: an instant recovery flask (5/1) for the consume test.
        document = catalog.Catalog(packet).new(5)
        tables = catalog.Catalog(packet).apply(document)
        catalog.run_tool(dict(action='commit', connection=admin, tables=tables, expected_hash=packet['hash']), packet['columns'], args.catalog_tool)
        packet = catalog.run_tool(dict(action='read', connection=admin), executable=args.catalog_tool)
        # C6: a material (6/1), and two recipes on the sword (1/1): +0 -> +1 always
        # succeeds for 100 gold and two materials; +1 -> +2 always fails and destroys.
        document = catalog.Catalog(packet).new(6)
        material = document['base']['local_id']
        tables = catalog.Catalog(packet).apply(document)
        catalog.run_tool(dict(action='commit', connection=admin, tables=tables, expected_hash=packet['hash']), packet['columns'], args.catalog_tool)
        packet = catalog.run_tool(dict(action='read', connection=admin), executable=args.catalog_tool)
        editor = catalog.Catalog(packet)
        document = editor.item((1, 1))
        document['refinement']['max_level'] = '2'
        document['levels'] = [dict(document['levels'][0], level=str(level)) for level in range(3)]
        document['recipes'] = [
            dict(source_level='0', target_category_id=None, target_local_id=None, target_level='1', gold_cost='100',
                 success_chance_bp='10000', failure_kind='1', failure_level=None,
                 ingredients=[dict(category_id='6', local_id=material, refinement_level='0', amount='2')]),
            dict(source_level='1', target_category_id=None, target_local_id=None, target_level='2', gold_cost='50',
                 success_chance_bp='0', failure_kind='2', failure_level=None, ingredients=[]),
        ]
        tables = editor.apply(document, (1, 1))
        catalog.run_tool(dict(action='commit', connection=admin, tables=tables, expected_hash=packet['hash']), packet['columns'], args.catalog_tool)
        packet = catalog.run_tool(dict(action='read', connection=admin), executable=args.catalog_tool)
        if len(packet['hash']) != 64 or any(c not in '0123456789abcdef' for c in packet['hash']): raise SystemExit('unexpected hash')
        sandbox.client.query("INSERT INTO player.item_persistence_format VALUES(1,1,1,UNHEX('" + packet['hash'] + "'))")
        # The final store test uses this connection: ID reservation and receipt
        # lookup succeed; lose the real write COMMIT and its first receipt-read
        # acknowledgement to exercise both reconciliation and delayed retries.
        proxy = CommitResponseProxy(sandbox.port, drop_count=2, skip_commits=2)
        stdin = '\n'.join([str(sandbox.port), sandbox.password, str(len(packet['tables']['item_base'])), packet['hash'], str(proxy.port)]) + '\n'
        environment = dict(os.environ, MARIADB_TLS_DISABLE_PEER_VERIFICATION='1')
        result = subprocess.run([str(args.test_executable.resolve()), '--owned-sandbox'], input=stdin, text=True, capture_output=True, timeout=120, env=environment)
        output = (result.stdout + result.stderr).replace(sandbox.password, '[redacted]')
        (sandbox.work / 'native-test.log').write_text(output, encoding='utf-8')
        print(output, end='')
        if result.returncode: raise SystemExit('native catalog runtime tests failed')
        if proxy.error: raise proxy.error
        if proxy.dropped_count != 2: raise RuntimeError('lost-COMMIT reconciliation was not exercised')
        sandbox.verify()
    finally:
        if proxy: proxy.close()
        sandbox.stop()


if __name__ == '__main__':
    main()
