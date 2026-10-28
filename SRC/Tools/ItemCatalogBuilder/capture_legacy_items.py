"""Capture an owned LOCAL package for the one-time item migration.

This administrative executable is never imported by a game runtime. It takes a
logical backup and an exact, typed input inventory while a global read lock is
held. Credentials remain in the package's private option file. Outputs contain
player data and belong in the private run directory, never in Git.
"""
from __future__ import annotations

import argparse
import configparser
from contextlib import contextmanager
import datetime as dt
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[3]
sys.path[:0] = [str(ROOT / 'db/tools'), str(ROOT / 'SRC/Tools/ServerManager')]
from mariadb_client import Client
import backend

DATABASES = ('account', 'common', 'log', 'player', 'mission', 'protos')
INPUT_TABLES = (
    'player.item_proto', 'player.item_extra_proto', 'player.refine_proto',
    'player.item', 'player.item_award', 'player.shop', 'player.shop_item',
    'player.item_attr', 'player.item_attr_rare', 'player.item_attr_rare_8',
    'player.quest_item_proto', 'player.daily_reward_items',
    'player.guild_renewal_kisado_item', 'player.myshop_pricelist',
    'mission.mission_engine_item_awards',
    'player.account_mount_inventory', 'player.guild_renewal_storage',
    'player.ishop_data', 'player.new_petsystem', 'player.object_proto',
    'player.quest', 'player.affect',
)


def digest(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def write_json(path, value):
    with Path(path).open('x', encoding='utf-8', newline='\n') as stream:
        json.dump(value, stream, ensure_ascii=True, sort_keys=True, indent=2)
        stream.write('\n')


def identifier(name):
    if not re.fullmatch(r'[a-zA-Z0-9_]+', name):
        raise ValueError('Unsupported database identifier')
    return '`' + name + '`'


def table_input(client, schema, table):
    """Use HEX even for text: never substitute undecodable source bytes."""
    columns = client.query(
        "SELECT column_name,column_type,data_type,character_set_name,collation_name "
        "FROM information_schema.columns WHERE table_schema='" + schema +
        "' AND table_name='" + table + "' ORDER BY ordinal_position")
    if not columns:
        raise ValueError('Missing input table: ' + schema + '.' + table)
    text_types = {'char', 'varchar', 'tinytext', 'text', 'mediumtext', 'longtext', 'enum', 'set', 'json'}
    binary_types = {'binary', 'varbinary', 'tinyblob', 'blob', 'mediumblob', 'longblob', 'bit'}
    expressions, encodings = [], []
    for name, _, kind, charset, _ in columns:
        quoted = identifier(name)
        if kind in text_types:
            expressions.append('HEX(CONVERT(' + quoted + ' USING utf8mb4))')
            encodings.append('utf8-hex')
        elif kind in binary_types:
            expressions.append('HEX(' + quoted + ')')
            encodings.append('binary-hex')
        else:
            expressions.append(quoted)
            encodings.append('text')
    rows = client.query('SELECT ' + ','.join(expressions) + ' FROM ' + identifier(schema) + '.' + identifier(table))
    # Order is canonical even for a table without a primary key; NULL != ''.
    rows.sort(key=lambda row: json.dumps(row, ensure_ascii=True, separators=(',', ':')))
    return dict(columns=[dict(zip(('name', 'sql_type', 'data_type', 'charset', 'collation'), column),
                              encoding=encoding) for column, encoding in zip(columns, encodings)], rows=rows)


def owner_keys(client):
    return {kind:[int(row[0]) for row in client.query(query)] for kind,query in (
        ('player','SELECT id FROM player.player ORDER BY id'),
        ('account','SELECT id FROM account.account ORDER BY id'),
        ('guild','SELECT id FROM player.guild ORDER BY id'))}


@contextmanager
def local_source(package):
    package = backend.package_root(package)
    manager = backend.load_manager(package)
    owner = manager.read_json(package / 'run/mariadb/owner.json')
    record = manager.read_json(package / 'run/mariadb/process.json')
    if not manager.owned(record):
        raise RuntimeError('The package-owned local MariaDB is not running')
    manager.verify_datadir()
    private = configparser.ConfigParser(interpolation=None)
    private.read(package / 'run/mariadb/admin.cnf', encoding='utf-8')
    binary_dir = Path(owner['binary_dir']).resolve()
    dll = binary_dir.parent / 'lib/libmariadb.dll'
    dll_path = os.add_dll_directory(str(dll.parent)) if os.name == 'nt' else None
    client = None
    try:
        client = Client(owner['port'], library=str(dll), user=private['client']['user'], password=private['client']['password'])
        client.query('SET NAMES utf8mb4')
        actual = client.query('SELECT @@datadir')[0][0]
        if Path(actual).resolve() != (package / 'run/mariadb/data').resolve():
            raise RuntimeError('The SQL connection does not belong to the selected package')
        yield package, manager, binary_dir, client
    finally:
        if client is not None:
            client.close()
        if dll_path:
            dll_path.close()


def capture(package, output):
    output = Path(output).resolve()
    if output.exists():
        raise ValueError('Output directory must be new')
    with local_source(package) as (package, manager, binaries, client):
        with manager.supervisor_lock():
            if backend.describe(package)['running']:
                raise RuntimeError('Stop the local game before capturing its migration checkpoint')
            output.mkdir(parents=True, exist_ok=False)
            manifest = dict(format_version=1, captured_at=dt.datetime.now(dt.timezone.utc).isoformat(),
                            package=str(package), databases=list(DATABASES), files={}, tables={}, complete=False)
            locked = False
            try:
                client.query('FLUSH TABLES WITH READ LOCK')
                locked = True
                manifest['database_version'] = client.query('SELECT VERSION()')[0][0]
                manifest['owner_keys'] = owner_keys(client)
                all_tables = client.query("SELECT table_schema,table_name,engine,table_collation,table_type FROM information_schema.tables "
                                          "WHERE table_schema IN ('account','common','log','player','mission','protos') ORDER BY 1,2")
                manifest['database_objects'] = all_tables
                manifest['database_columns'] = client.query(
                    "SELECT table_schema,table_name,column_name,column_type FROM information_schema.columns "
                    "WHERE table_schema IN ('account','common','log','player','mission','protos') ORDER BY 1,2,ordinal_position")
                args = [str(binaries / 'mariadb-dump.exe'),
                        '--defaults-extra-file=' + str(package / 'run/mariadb/admin.cnf'),
                        '--protocol=tcp', '--host=127.0.0.1', '--port=' + str(manager.read_json(package / 'run/mariadb/owner.json')['port']),
                        '--default-character-set=utf8mb4', '--skip-lock-tables', '--hex-blob',
                        '--routines', '--events', '--triggers', '--databases', *DATABASES]
                with (output / 'checkpoint.sql').open('xb') as dump, (output / 'dump.error.log').open('xb') as error:
                    result = subprocess.run(args, stdout=dump, stderr=error, timeout=300,
                                            creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)
                if result.returncode:
                    raise RuntimeError('Logical backup failed; inspect the private dump.error.log')
                tables = list(INPUT_TABLES)
                tables.extend(schema + '.' + table for schema, table, _, _, kind in all_tables
                              if schema == 'protos' and kind == 'BASE TABLE')
                for qualified in sorted(tables):
                    schema, table = qualified.split('.')
                    packet = table_input(client, schema, table)
                    filename = qualified + '.json'
                    write_json(output / filename, packet)
                    manifest['tables'][qualified] = dict(file=filename, rows=len(packet['rows']), sha256=digest(output / filename))
                # All base table row counts are evidence for a full restore,
                # including retired stores kept only in the backup archive.
                manifest['row_counts'] = {schema + '.' + table: int(client.query('SELECT COUNT(*) FROM ' + identifier(schema) + '.' + identifier(table))[0][0])
                                          for schema, table, _, _, kind in all_tables if kind == 'BASE TABLE'}
            finally:
                if locked:
                    client.query('UNLOCK TABLES')
            # These files contain static game content, not server.json/passwords.
            shutil.copytree(package / 'data', output / 'data')
            for path in sorted(output.rglob('*')):
                if path.is_file():
                    manifest['files'][path.relative_to(output).as_posix()] = digest(path)
            manifest['complete'] = True
            write_json(output / 'manifest.json', manifest)
            return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--package', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    manifest = capture(args.package, args.output)
    print(json.dumps(dict(complete=manifest['complete'], tables={name: info['rows'] for name, info in manifest['tables'].items()},
                          checkpoint_sha256=manifest['files']['checkpoint.sql']), ensure_ascii=True))


if __name__ == '__main__':
    main()
