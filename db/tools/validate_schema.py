"""Validate DDL and source SQL on fresh, isolated test namespaces.

All schema names are remapped to auriga_schema_<run>_*; application databases
are never modified. Uses a loopback test instance with an empty root password.
"""
import argparse
import json
import re
import uuid
from pathlib import Path

from mariadb_client import Client, statements
from source_sql import ROOT, inventory, formatted_sql, source_digest
from sql_assemblies import assembled
from schema_checks import check_schema
from four_slot_checks import check_four_slot_migration


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', required=True, type=int)
    parser.add_argument('--library')
    parser.add_argument('--password-file', type=Path, help='Private client option file for an owned test instance')
    parser.add_argument('--schema', type=Path, default=ROOT / 'db/schema.sql')
    parser.add_argument('--report', type=Path, required=True)
    args = parser.parse_args()
    prefix = 'auriga_schema_' + uuid.uuid4().hex[:10] + '_'
    names = {name: prefix + name for name in ('player', 'common', 'account', 'log')}
    def remap(sql):
        return re.sub(r'(?<!\w)(`?)(player|common|account|log)\1\s*(?=\.)',
                      lambda m: '`' + names[m[2]] + '`', sql, flags=re.I)
    password = re.search(r'^password=([^\r\n]*)', args.password_file.read_text(encoding='utf-8'), re.M)[1] if args.password_file else ''
    client = Client(args.port, library=args.library, password=password)
    report = {'namespaces': names, 'version': client.query('SELECT VERSION()')[0][0], 'prepared': [], 'executed': [], 'runtime_variants': [], 'failed': []}
    registry = json.loads((ROOT / 'db/sql_assemblies.json').read_text(encoding='utf-8'))
    try:
        rows = inventory()
        stale = set(registry) - {row['id'] for row in rows}
        if stale:
            raise RuntimeError('Review obsolete assembly contracts: ' + ', '.join(sorted(stale)))
        client.query("SET SESSION sql_mode='STRICT_ALL_TABLES,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION,NO_ZERO_DATE,NO_ZERO_IN_DATE'")
        for name in names.values(): client.query('CREATE DATABASE `' + name + '` CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci')
        ddl = list(statements(args.schema.read_text(encoding='utf-8')))
        for _ in range(2):
            for statement in ddl:
                if re.match(r'CREATE DATABASE', statement, re.I): continue
                client.query(remap(statement))
        report['ddl_statements'] = len(ddl)
        report['tables'] = client.query("SELECT TABLE_SCHEMA,TABLE_NAME FROM information_schema.TABLES WHERE TABLE_SCHEMA LIKE '" + prefix + "%'")
        for row in rows:
            reviewed = registry.get(row['id'])
            if reviewed:
                actual = source_digest(ROOT / row['file'])
                if actual != reviewed['source_hash']:
                    raise RuntimeError('Review changed query builder: ' + row['file'])
                if reviewed['kind'] == 'sjis':
                    report['runtime_variants'].append(dict(row, reason='Not selected by the latin1/utf8mb4 deployment profile; requires an SJIS schema/connection'))
                    continue
                if reviewed['kind'] == 'route_common': row['database'] = 'common'
            query = remap(assembled(row, reviewed['kind']) if reviewed and reviewed['kind'] not in ('route_common', 'sjis') else formatted_sql(row['sql']))
            try:
                fields, params = client.prepare(names[row['database']], query)
                if 'FROM skill_color' in row['sql'] and fields != 55: raise RuntimeError('Skill color SELECT must return all 55 fields')
                if 'FROM account_mount_inventory' in row['sql'] and row['sql'].startswith('SELECT id, slot') and fields != 23: raise RuntimeError('Mount SELECT must return 3 sockets and all 8 attributes')
                report['prepared'].append(dict(row, fields=fields, parameters=params, assembly=reviewed['kind'] if reviewed else None))
                # Numeric test values are substituted only outside SQL literals.
                # Every mutation is rolled back in an owned test namespace.
                token = re.compile(r"'(?:''|\\.|[^'\\])*'|`(?:``|[^`])*`|\"(?:\\.|[^\"\\])*\"|\?")
                values = []
                if reviewed and reviewed['kind'] == 'player_create':
                    columns = re.search(r'\((.*?)\)', query)[1].replace('`','').split(',')
                    values = ["REPEAT(CHAR(0),2550)" if col == 'skill_level' else "REPEAT(CHAR(0),108)" if col == 'quickslot' else '1' for col in columns]
                value_iter = iter(values)
                executable = token.sub(lambda m: next(value_iter, '1') if m[0] == '?' else m[0], query)
                client.query('START TRANSACTION')
                try:
                    client.query(executable)
                    report['executed'].append(row['id'])
                finally: client.query('ROLLBACK')
            except RuntimeError as error:
                report['failed'].append(dict(row, normalized=query, error=str(error)))
        report['invariants'] = check_schema(client, names, remap)
        report['four_slot_migration'] = check_four_slot_migration(client, names['player'])
        for _ in range(2):
            for statement in statements((ROOT / 'db/seed_core.sql').read_text(encoding='utf-8')):
                client.query(remap(statement))
        report['prerequisites'] = client.query(remap(next(statements((ROOT / 'db/check_ready.sql').read_text(encoding='utf-8')))))
        args.report.write_text(json.dumps(report, indent=2), encoding='utf-8')
        print('Tables:', len(report['tables']), 'prepared:', len(report['prepared']), 'executed:', len(report['executed']), 'invariants:', len(report['invariants']), 'unresolved:', len(report['failed']))
        for row in report['failed']:
            print(row['id'], row['file'].replace('SRC/Server/', ''), row['line'], row['error'][:180])
        return bool(report['failed'])
    finally: client.close()


if __name__ == '__main__':
    raise SystemExit(main())
