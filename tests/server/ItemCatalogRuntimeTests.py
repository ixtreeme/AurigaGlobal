"""GameServer boot-time typed catalog against a NEW owned MariaDB, never a live DB.

Installs the native protos schema, publishes one item through the real
CatalogEditorTool commit, then lets the native test load it with a
SELECT-only reader account, as the game server does at boot.
"""
import argparse
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


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mariadb-bin', type=Path, required=True)
    parser.add_argument('--test-executable', type=Path, required=True)
    parser.add_argument('--catalog-tool', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    sandbox = Sandbox(args.work_dir / uuid.uuid4().hex, args.mariadb_bin)
    try:
        sandbox.start(); sandbox.verify()
        sandbox.client.query('CREATE DATABASE player CHARACTER SET utf8mb4 COLLATE utf8mb4_bin')
        paths = sorted((ROOT / 'db/migrations/item_combat').glob('[0-9][0-9][0-9]_*.sql'))
        for path in paths:
            for statement in statements(path.read_text(encoding='utf-8-sig')): sandbox.client.query(statement)
        sandbox.client.query("CREATE USER 'game_reader'@'localhost' IDENTIFIED BY '" + sandbox.password + "'")
        sandbox.client.query("GRANT SELECT ON protos.* TO 'game_reader'@'localhost'")
        sandbox.client.query("GRANT SELECT ON player.* TO 'game_reader'@'localhost'")
        for statement in statements((ROOT / 'db/migrations/item_instance/001_item_persistence.sql').read_text(encoding='utf-8-sig')):
            sandbox.client.query(statement)
        # The game account may claim sessions; only the importer (root here) provisions.
        sandbox.client.query("GRANT UPDATE ON player.item_owner_fence TO 'game_reader'@'localhost'")
        # ...and write its typed items like the item writer (C3 moves), never the storages.
        for table in ('item_owner_fence', 'item_wallet', 'item_operation_receipt', 'item_instance',
                      'item_instance_attribute', 'item_instance_socket', 'item_occupied_cell'):
            sandbox.client.query("GRANT INSERT,UPDATE,DELETE ON player." + table + " TO 'game_reader'@'localhost'")
        admin = dict(port=sandbox.port, user='root', password=sandbox.password)
        packet = catalog.run_tool(dict(action='read', connection=admin), executable=args.catalog_tool)
        document = catalog.Catalog(packet).new(1)
        tables = catalog.Catalog(packet).apply(document)
        catalog.run_tool(dict(action='commit', connection=admin, tables=tables, expected_hash=packet['hash']), packet['columns'], args.catalog_tool)
        packet = catalog.run_tool(dict(action='read', connection=admin), executable=args.catalog_tool)
        if len(packet['hash']) != 64 or any(c not in '0123456789abcdef' for c in packet['hash']): raise SystemExit('unexpected hash')
        sandbox.client.query("INSERT INTO player.item_persistence_format VALUES(1,1,1,UNHEX('" + packet['hash'] + "'))")
        stdin = '\n'.join([str(sandbox.port), sandbox.password, str(len(packet['tables']['item_base'])), packet['hash']]) + '\n'
        result = subprocess.run([str(args.test_executable.resolve()), '--owned-sandbox'], input=stdin, text=True, capture_output=True, timeout=120)
        output = (result.stdout + result.stderr).replace(sandbox.password, '[redacted]')
        (sandbox.work / 'native-test.log').write_text(output, encoding='utf-8')
        print(output, end='')
        if result.returncode: raise SystemExit('native catalog runtime tests failed')
        sandbox.verify()
    finally:
        sandbox.stop()


if __name__ == '__main__':
    main()
