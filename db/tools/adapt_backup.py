"""Adapt a Navicat four-database backup in an owned, isolated MariaDB instance.

Original files are read only. The new output contains canonical data dumps,
exact source copies, schema hashes and count/byte verification results.
"""
import argparse
import collections
import datetime
import json
from pathlib import Path
import re
import secrets
import shutil
import sys

from backup_sandbox import BackupError, ROOT, ROLES, Sandbox, digest, navicat_tables, quote, write_json
from mariadb_client import statements

INTEGER_TYPES = {'tinyint', 'smallint', 'mediumint', 'int', 'bigint'}
NUMERIC_TYPES = INTEGER_TYPES | {'float', 'double', 'decimal'}
TEXT_TYPES = {'char', 'varchar', 'text', 'mediumtext', 'longtext', 'tinytext'}
BINARY_TYPES = {'binary', 'varbinary', 'blob', 'tinyblob', 'mediumblob', 'longblob'}
LOCALE_CODECS = {'ro': 'cp1250', 'pl': 'cp1250', 'cz': 'cp1250', 'hu': 'cp1250', 'tr': 'cp1254'}
RENAMES = {
    ('player', 'change_empire', 'last_change'): 'time',
    ('log', 'shout_log', 'time'): 'when',
    ('log', 'shout_log', 'channel'): 'where',
    ('log', 'shout_log', 'text'): 'message',
    ('log', 'dragon_slay_log', 'dragon_vnum'): 'vnum',
    ('log', 'fish_log', 'pid'): 'player_id',
    ('log', 'fish_log', 'prob_idx'): 'map_index',
    ('log', 'fish_log', 'fish_level'): 'fishing_level',
    ('log', 'fish_log', 'milliseconds'): 'waiting_time',
    ('log', 'fish_log', 'vnum'): 'success',
    ('log', 'fish_log', 'value'): 'size',
    ('log', 'quest_reward_log', 'value1'): 'how',
    ('log', 'quest_reward_log', 'value2'): 'hint',
    ('log', 'quest_reward_log', 'time'): 'when',
}


def metadata(client):
    result = {}
    for row in client.query("SELECT TABLE_SCHEMA,TABLE_NAME,COLUMN_NAME,DATA_TYPE,COLUMN_TYPE,IS_NULLABLE,COLUMN_DEFAULT,CHARACTER_MAXIMUM_LENGTH,CHARACTER_SET_NAME FROM information_schema.COLUMNS WHERE TABLE_SCHEMA IN ('account','common','player','log','legacy_account','legacy_common','legacy_player','legacy_log') ORDER BY TABLE_SCHEMA,TABLE_NAME,ORDINAL_POSITION"):
        result.setdefault((row[0], row[1]), collections.OrderedDict())[row[2]] = dict(zip(
            ('name', 'type', 'full_type', 'nullable', 'default', 'length', 'charset'), row[2:]))
    return result


def default_sql(column):
    value = column['default']
    if value is not None and value.upper() != 'NULL':
        return value
    if column['nullable'] == 'YES':
        return 'NULL'
    if column['type'] in NUMERIC_TYPES:
        return '0'
    if column['type'] in ('date', 'datetime', 'timestamp'):
        return "'1970-01-01 00:00:00'"
    if column['type'] in TEXT_TYPES | BINARY_TYPES:
        return "''"
    raise BackupError('No reviewed default for column ' + column['name'])


def enum_labels(column):
    return [value.replace("''", "'").replace("\\'", "'") for value in re.findall(r"'((?:''|\\.|[^'])*)'", column['full_type'])]


class Adapter:
    def __init__(self, sandbox, report):
        self.sandbox, self.client, self.report = sandbox, sandbox.client, report
        self.client.query("SET SESSION sql_mode=CONCAT(@@sql_mode,',NO_AUTO_VALUE_ON_ZERO')")
        self.tables = metadata(self.client)
        self.text_cache = {}
        self.client.query('CREATE DATABASE IF NOT EXISTS backup_meta CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci')
        self.client.query('CREATE TABLE IF NOT EXISTS backup_meta.text_values_v2 (codec VARCHAR(16) NOT NULL, raw VARBINARY(1024) NOT NULL, utf8 LONGBLOB NOT NULL, PRIMARY KEY(codec,raw))')
        old = self.tables['legacy_player', 'item_attr']['apply']
        new = self.tables['player', 'item_attr']['apply']
        current = {name: i + 1 for i, name in enumerate(enum_labels(new))}
        self.apply_changes = {i + 1: current.get(name, 0) for i, name in enumerate(enum_labels(old)) if current.get(name, 0) != i + 1}
        self.report['apply_id_mapping'] = self.apply_changes

    def count(self, sql):
        return int(self.client.query(sql)[0][0] or 0)

    def convert_text(self, db, table, source, expression):
        key = (db, table, source['name'])
        if key in self.text_cache:
            return self.text_cache[key]
        rows = self.client.query('SELECT DISTINCT HEX(' + quote(source['name']) + ') FROM ' + quote('legacy_' + db) + '.' + quote(table) + ' WHERE ' + quote(source['name']) + ' IS NOT NULL')
        language = source['name'].removeprefix('locale_name_')
        codec = LOCALE_CODECS.get(language, 'cp1250' if db == 'log' else 'cp1252')
        decoded, fallback = [], 0
        for row in rows:
            raw = bytes.fromhex(row[0])
            try:
                text = raw.decode('utf-8')
            except UnicodeDecodeError:
                fallback += 1
                # Legacy localized VARBINARY names use their language's Windows
                # code page. Keep undecodable control bytes as code points;
                # exact original bytes remain in the archived backup.
                pieces = []
                for byte in raw:
                    try:
                        pieces.append(bytes([byte]).decode(codec))
                    except UnicodeDecodeError:
                        pieces.append(chr(byte))
                text = ''.join(pieces)
            decoded.append((raw.hex(), text.encode('utf-8').hex()))
        if fallback:
            for start in range(0, len(decoded), 200):
                values = ','.join("('%s',UNHEX('%s'),UNHEX('%s'))" % (codec, *pair) for pair in decoded[start:start + 200])
                self.client.query('INSERT INTO backup_meta.text_values_v2 VALUES ' + values + ' ON DUPLICATE KEY UPDATE utf8=VALUES(utf8)')
            converted = "(SELECT CONVERT(t.utf8 USING utf8mb4) FROM backup_meta.text_values_v2 t WHERE t.codec='" + codec + "' AND t.raw=" + expression + ')'
            self.report['text_decoding'].append(dict(table=db + '.' + table, column=source['name'], legacy_codec=codec, decoded_values=fallback))
        else:
            converted = 'CONVERT(' + expression + ' USING utf8mb4)'
        self.text_cache[key] = converted
        return converted

    def expression(self, db, table, column, old):
        name = column['name']
        source_name = RENAMES.get((db, table, name), name)
        if source_name not in old and name == 'locale_name' and table in ('mob_proto', 'item_proto'):
            source_name = 'name'
        if source_name not in old:
            self.report['new_defaults'].append(dict(table=db + '.' + table, column=name))
            return default_sql(column)
        source = old[source_name]
        expression = 's.' + quote(source_name)
        if column['type'] in NUMERIC_TYPES:
            if source['type'] in ('enum', 'set'):
                expression += '+0'
            elif source['type'] in TEXT_TYPES:
                expression = 'CASE WHEN ' + expression + " REGEXP '^[+-]?[0-9]+([.][0-9]+)?$' THEN CAST(" + expression + ' AS DECIMAL(30,6)) ELSE 0 END'
            if name.startswith(('attrtype', 'applytype', 'bonus_type')) or re.fullmatch(r'attr[0-7]', name):
                expression = 'CASE ' + expression + ''.join(' WHEN %d THEN %d' % pair for pair in self.apply_changes.items()) + ' ELSE ' + expression + ' END'
        elif column['type'] in TEXT_TYPES and source['type'] in BINARY_TYPES:
            expression = self.convert_text(db, table, source, expression)
        elif column['type'] in ('enum', 'set'):
            expression = 'CAST(' + expression + ' AS CHAR CHARACTER SET utf8mb4)'
        elif column['type'] in ('date', 'datetime', 'timestamp'):
            condition = "CAST(" + expression + " AS CHAR) REGEXP '(^0000-|^[0-9]{4}-00-|^[0-9]{4}-[0-9]{2}-00)'"
            invalid = self.count('SELECT COUNT(*) FROM ' + quote('legacy_' + db) + '.' + quote(table) + ' s WHERE ' + condition)
            if invalid:
                self.report['zero_dates'].append(dict(table=db + '.' + table, column=name, rows=invalid))
                expression = 'CASE WHEN ' + condition + ' THEN ' + default_sql(column) + ' ELSE ' + expression + ' END'
        if column['nullable'] == 'NO':
            expression = 'COALESCE(' + expression + ',' + default_sql(column) + ')'
        return expression

    def copy(self):
        for (db, table), columns in self.tables.items():
            if db not in ROLES:
                continue
            old = self.tables.get(('legacy_' + db, table))
            target = quote(db) + '.' + quote(table)
            if not old:
                self.report['tables'].append(dict(database=db, table=table, source_rows=0, target_rows=0, created_empty=True))
                continue
            source = quote('legacy_' + db) + '.' + quote(table)
            count = self.count('SELECT COUNT(*) FROM ' + source)
            where, retired = '', 0
            if db == 'player' and table in ('item_attr', 'item_attr_rare', 'item_attr_rare_8'):
                allowed = set(enum_labels(columns['apply']))
                missing = [row for row in self.client.query('SELECT CAST(apply AS CHAR),COUNT(*) FROM ' + source + ' GROUP BY apply') if row[0] not in allowed]
                for label, amount in missing:
                    retired += int(amount)
                    self.report.setdefault('retired_bonus_definitions', []).append(dict(table=table, legacy_label=label, rows=int(amount), reason='No matching active EApplyTypes value; original definition remains in archive'))
                if missing:
                    where = ' WHERE CAST(s.apply AS CHAR) NOT IN (' + ','.join("CONVERT(UNHEX('" + row[0].encode('utf-8').hex() + "') USING utf8mb4)" for row in missing) + ')'
            self.report['legacy_only_columns'][db + '.' + table] = [name for name in old if name not in columns]
            if count:
                expressions = [self.expression(db, table, col, old) for col in columns.values()]
                sql = 'INSERT INTO ' + target + ' (' + ','.join(map(quote, columns)) + ') SELECT ' + ','.join(expressions) + ' FROM ' + source + ' s' + where
                self.client.query('START TRANSACTION')
                try:
                    self.client.query(sql)
                    warnings = self.client.query('SHOW WARNINGS')
                    if warnings:
                        raise BackupError('Conversion produced SQL warnings: ' + db + '.' + table)
                    self.client.query('COMMIT')
                except Exception as error:
                    self.client.query('ROLLBACK')
                    (self.sandbox.work / 'conversion-error.txt').write_text(str(error), encoding='utf-8')
                    raise BackupError('Conversion failed for ' + db + '.' + table + '; inspect private conversion-error.txt') from None
            copied = self.count('SELECT COUNT(*) FROM ' + target)
            if count - retired != copied:
                raise BackupError('Row count differs: ' + db + '.' + table)
            self.report['tables'].append(dict(database=db, table=table, source_rows=count, target_rows=copied, archived_retired_rows=retired))
            print('ADAPTED', db + '.' + table, copied, flush=True)

    def verify(self):
        self.report['fifth_slots_removed'] = self.count('SELECT COUNT(*) FROM legacy_player.player_index WHERE pid5<>0')
        self.report['fifth_characters_fully_occupied'] = self.count('SELECT COUNT(*) FROM legacy_player.player_index WHERE pid5<>0 AND pid1<>0 AND pid2<>0 AND pid3<>0 AND pid4<>0 AND pid5 NOT IN(pid1,pid2,pid3,pid4)')
        self.report['fifth_slot_policy'] = 'Only pid1-pid4 are active. Original pid5 mapping and all character data retained in archive; no new account.'
        for table in ('player', 'player_deleted'):
            mismatches = self.count('SELECT COUNT(*) FROM player.' + quote(table) + ' n JOIN legacy_player.' + quote(table) + ' o ON n.id=o.id WHERE NOT (n.skill_level <=> o.skill_level) OR NOT (n.quickslot <=> o.quickslot)')
            if mismatches:
                raise BackupError('Character binary blobs changed')
        mismatch = self.count('SELECT COUNT(*) FROM account.account n JOIN legacy_account.account o ON n.id=o.id WHERE NOT (BINARY n.password <=> BINARY o.password)')
        if mismatch:
            raise BackupError('Account password hashes changed')
        slots = self.client.query("SELECT COLUMN_NAME FROM information_schema.COLUMNS WHERE TABLE_SCHEMA='player' AND TABLE_NAME='player_index' AND COLUMN_NAME REGEXP '^pid[0-9]+$' ORDER BY ORDINAL_POSITION")
        if slots != [['pid1'], ['pid2'], ['pid3'], ['pid4']]:
            raise BackupError('Active character index does not contain exactly four slots')
        self.report['byte_checks'] = dict(character_skills_and_quickslots='identical', account_password_hashes='identical', active_character_slots=4)
        self.report['legacy_only_tables'] = {db: sorted(table for (schema, table) in self.tables if schema == 'legacy_' + db and (db, table) not in self.tables) for db in ROLES}

    def preserve_sequences(self):
        self.report['auto_increment'] = []
        for (db, table) in self.tables:
            if db not in ROLES or ('legacy_' + db, table) not in self.tables:
                continue
            target = self.client.query("SELECT AUTO_INCREMENT FROM information_schema.TABLES WHERE TABLE_SCHEMA='" + db + "' AND TABLE_NAME='" + table + "'")[0][0]
            source = self.client.query("SELECT AUTO_INCREMENT FROM information_schema.TABLES WHERE TABLE_SCHEMA='legacy_" + db + "' AND TABLE_NAME='" + table + "'")[0][0]
            if target is not None and source is not None:
                value = max(int(target), int(source))
                self.client.query('ALTER TABLE ' + quote(db) + '.' + quote(table) + ' AUTO_INCREMENT=' + str(value))
                self.report['auto_increment'].append(dict(database=db, table=table, next_id=value))

    def fingerprints(self):
        self.report['table_checksums'] = {}
        self.report['target_counts'] = {}
        for (db, table) in self.tables:
            if db not in ROLES:
                continue
            name = db + '.' + table
            self.report['table_checksums'][name] = self.client.query('CHECKSUM TABLE ' + quote(db) + '.' + quote(table) + ' EXTENDED')[0][1]
            self.report['target_counts'][name] = self.count('SELECT COUNT(*) FROM ' + quote(db) + '.' + quote(table))


def sequence_sql(report):
    return '\n'.join('ALTER TABLE ' + quote(row['database']) + '.' + quote(row['table']) + ' AUTO_INCREMENT=' + str(row['next_id']) + ';' for row in report['auto_increment']) + '\n'


def publish_report(output, report):
    (output / 'auto-increment.sql').write_text(sequence_sql(report), encoding='utf-8')
    write_json(output / 'report.json', report)
    write_json(output / 'manifest.json', dict(version=1, files={p.relative_to(output).as_posix(): digest(p) for p in sorted(output.rglob('*')) if p.is_file() and p.name != 'manifest.json'}))


def adapt(source, output, work, binary_dir, resume=False):
    source, output, work = Path(source).resolve(), Path(output).resolve(), Path(work).resolve()
    if output == source or output.is_relative_to(source) or source.is_relative_to(output) or work.is_relative_to(source):
        raise BackupError('Source, output and work directories must be separate')
    if output.exists():
        raise BackupError('Output directory must be new')
    hashes = {role + '.sql': digest(source / (role + '.sql')) for role in ROLES}
    report = dict(version=1, source=str(source), source_sha256=hashes, schema_sha256=digest(ROOT / 'db/schema.sql'),
                  generated_at=datetime.datetime.now(datetime.timezone.utc).isoformat(), tables=[],
                  zero_dates=[], text_decoding=[], new_defaults=[], legacy_only_columns={})
    sandbox = Sandbox(work, binary_dir)
    try:
        if resume:
            if json.loads((work / 'input-sha256.json').read_text(encoding='utf-8')) != hashes:
                raise BackupError('The source dump differs from the sandbox input')
            sandbox.resume()
            report['source_objects'] = json.loads((work / 'source-objects.json').read_text(encoding='utf-8'))
        else:
            sandbox.start()
            write_json(work / 'input-sha256.json', hashes)
            report['source_objects'] = {}
            for role in ROLES:
                database = 'legacy_' + role
                sandbox.client.query('CREATE DATABASE ' + quote(database) + ' CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci')
                cleaned = work / (role + '-tables.sql')
                report['source_objects'][role] = navicat_tables(source / (role + '.sql'), cleaned)
                sandbox.import_file(cleaned, database, 'raw-import-' + role)
                print('RAW_IMPORT_OK', role, flush=True)
            write_json(work / 'source-objects.json', report['source_objects'])
        sandbox.verify()
        # These application databases exist only in the new, owned sandbox.
        # Keep raw legacy_* databases unchanged throughout conversion.
        for role in ROLES:
            sandbox.client.query('DROP DATABASE IF EXISTS ' + quote(role))
        for statement in statements((ROOT / 'db/schema.sql').read_text(encoding='utf-8')):
            sandbox.client.query(statement)
        adapter = Adapter(sandbox, report)
        adapter.copy()
        adapter.verify()
        adapter.preserve_sequences()
        report['skill_curve_normalization'] = []
        for key, value in sandbox.client.query("SELECT mKey,mValue FROM common.locale WHERE mKey REGEXP '^SKILL_POWER_BY_LEVEL(_TYPE[0-9]+)?$'"):
            numbers = value.split()
            if not all(re.fullmatch(r'[+-]?[0-9]+', number) for number in numbers) or len(numbers) < 41:
                raise BackupError('A skill-power curve has fewer than 41 valid levels: ' + key)
            if len(numbers) > 41:
                normalized = ' '.join(numbers[:41]).encode('ascii').hex()
                sandbox.client.query("UPDATE common.locale SET mValue=CONVERT(UNHEX('" + normalized + "') USING utf8mb4) WHERE BINARY mKey=UNHEX('" + key.encode('ascii').hex() + "')")
                report['skill_curve_normalization'].append(dict(key=key, old_levels=len(numbers), active_levels=41, policy='Keep existing levels 0-40; archive unsupported trailing values'))
        for statement in statements((ROOT / 'db/seed_core.sql').read_text(encoding='utf-8')):
            sandbox.client.query(statement)
        report['readiness'] = sandbox.client.query(next(statements((ROOT / 'db/check_ready.sql').read_text(encoding='utf-8'))))
        if any(row[1] != 'READY' for row in report['readiness']):
            raise BackupError('Startup reference tables are incomplete')
        adapter.fingerprints()
        output.mkdir(parents=True)
        (output / 'data').mkdir()
        (output / 'archive-original').mkdir()
        shutil.copy2(ROOT / 'db/schema.sql', output / 'schema.sql')
        for role in ROLES:
            shutil.copy2(source / (role + '.sql'), output / 'archive-original' / (role + '.sql'))
            sandbox.run('mariadb-dump', [*sandbox.cli_args(), '--default-character-set=utf8mb4', '--single-transaction',
                        '--skip-lock-tables', '--skip-add-locks', '--skip-disable-keys', '--no-create-info',
                        '--complete-insert', '--hex-blob', '--skip-triggers', '--skip-comments', '--skip-set-charset', role],
                        'export-' + role + '-' + secrets.token_hex(4), output_file=output / 'data' / (role + '.sql'))
        publish_report(output, report)
        if hashes != {name: digest(source / name) for name in hashes}:
            raise BackupError('Original dump changed during conversion')
        print('ADAPTED_BACKUP_READY', output, flush=True)
        return report
    finally:
        write_json(work / 'adaptation-progress.json', report) if work.exists() else None
        sandbox.stop()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--work', type=Path, required=True)
    parser.add_argument('--mariadb-bin', type=Path, required=True)
    parser.add_argument('--resume-owned-work', action='store_true')
    args = parser.parse_args()
    try:
        adapt(args.source, args.output, args.work, args.mariadb_bin, args.resume_owned_work)
    except (BackupError, OSError, ValueError) as error:
        print('BACKUP_ADAPTATION_FAILED:', str(error), file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
