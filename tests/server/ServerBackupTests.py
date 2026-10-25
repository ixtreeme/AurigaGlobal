"""Backup parsing/guards and optional owned MariaDB import/rollback integration."""
import argparse
import importlib.util
import json
from pathlib import Path
import shutil
import socket
import sys
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'db/tools'))
sys.path.insert(0, str(ROOT / 'SRC/Tools/server_package'))
from backup_sandbox import BackupError, digest, navicat_tables, quote
from adapt_backup import default_sql, enum_labels, sequence_sql
import backup_import

BACKUP = BINARIES = None


class BackupToolsTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='auriga-backup-tests-')
        self.addCleanup(self.temp.cleanup)
        self.folder = Path(self.temp.name)

    def test_navicat_reader_keeps_binary_literals_and_skips_all_routine_code(self):
        source = self.folder / 'source.sql'
        source.write_text("""-- Table structure for fixture
DROP TABLE IF EXISTS `fixture`;
CREATE TABLE `fixture` (`id` INT, `payload` BLOB);
-- Records of fixture
INSERT INTO `fixture` VALUES (0,0x0001FF);
INSERT INTO `fixture` VALUES (1,'a; -- data, not a comment');
-- Procedure structure for old_test
CREATE PROCEDURE old_test() BEGIN
INSERT INTO `fixture` VALUES(99,'must not run'); END;
-- Triggers structure for table fixture
CREATE TRIGGER old_trigger BEFORE INSERT ON fixture FOR EACH ROW SET new.id=99;
""", encoding='utf-8')
        target = self.folder / 'clean.sql'
        report = navicat_tables(source, target)
        text = target.read_text(encoding='utf-8')
        self.assertEqual(report['insert_statements'], {'fixture': 2})
        self.assertEqual(len(report['excluded_objects']), 2)
        self.assertIn('0x0001FF', text)
        self.assertIn('a; -- data, not a comment', text)
        self.assertNotIn('must not run', text)
        self.assertNotIn('DROP TABLE', text)

    def test_unexpected_statement_in_data_section_is_rejected(self):
        source = self.folder / 'source.sql'
        source.write_text('-- Table structure for fixture\nCREATE TABLE `fixture` (`id` INT);\n-- Records of fixture\nDELETE FROM mysql.user;', encoding='utf-8')
        with self.assertRaises(BackupError):
            navicat_tables(source, self.folder / 'clean.sql')

    def test_identifier_guards_reject_extra_database_and_shell_content(self):
        for value in ('player.item; DROP DATABASE account', 'mysql.user', '../account', 'account.item.extra'):
            with self.subTest(value=value), self.assertRaises(RuntimeError):
                backup_import.qualified(value)
        with self.assertRaises(BackupError):
            quote('a`b')

    def test_schema_defaults_and_enum_labels_preserve_contract(self):
        column = dict(name='value', nullable='NO', default=None, type='int')
        self.assertEqual(default_sql(column), '0')
        column.update(nullable='YES')
        self.assertEqual(default_sql(column), 'NULL')
        self.assertEqual(enum_labels(dict(full_type="enum('A','B','C')")), ['A', 'B', 'C'])

    def test_backup_requires_manifest_and_all_verified_tables(self):
        (self.folder / 'manifest.json').write_text('{"version":1,"files":{}}')
        (self.folder / 'report.json').write_text('{"version":1}')
        with self.assertRaisesRegex(RuntimeError, 'Incomplete'):
            backup_import.validate_backup(self.folder, ROOT / 'db/schema.sql')

    def test_auto_increment_restore_is_separate_and_qualified(self):
        text = sequence_sql(dict(auto_increment=[dict(database='player', table='guild', next_id=120)]))
        self.assertEqual(text, 'ALTER TABLE `player`.`guild` AUTO_INCREMENT=120;\n')


class NativeBackupTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not BACKUP or not BINARIES:
            raise unittest.SkipTest('Pass --backup and --mariadb-bin for owned SQL integration')
        cls.temp = tempfile.TemporaryDirectory(prefix='auriga-backup-native-tests-')
        cls.base = Path(cls.temp.name).resolve()
        cls.managers = []

    @classmethod
    def tearDownClass(cls):
        for manager in cls.managers:
            manager.stop_db()
        cls.temp.cleanup()

    def package(self, name):
        root = self.base / name
        root.mkdir()
        (root / 'db').mkdir()
        shutil.copy2(ROOT / 'db/schema.sql', root / 'db/schema.sql')
        shutil.copy2(ROOT / 'db/seed_core.sql', root / 'db/seed_core.sql')
        shutil.copy2(ROOT / 'db/check_ready.sql', root / 'db/check_ready.sql')
        shutil.copy2(ROOT / 'db/mission_schema.sql', root / 'db/mission_schema.sql')
        shutil.copytree(ROOT / 'db/migrations', root / 'db/migrations')
        (root / '.auriga-package.json').write_text('{"version":1}')
        with socket.socket() as listener:
            listener.bind(('127.0.0.1', 0))
            port = listener.getsockname()[1]
        roles = {role:dict(host='127.0.0.1',port=port,user='backup_fixture',password='fixture_sql_secret',database=role)
                 for role in backup_import.ROLES}
        (root / 'server.json').write_text(json.dumps(dict(common=dict(locale='utf8mb4',table_postfix='',sql=roles),game_servers={})))
        spec = importlib.util.spec_from_file_location('_backup_native_' + name, ROOT / 'SRC/Tools/server_package/manage.py')
        manager = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(manager)
        manager.ROOT = root
        self.managers.append(manager)
        manager.init_db(BINARIES)
        return manager

    def test_successful_import_verifies_all_counts_and_checksums_and_rejects_repeat(self):
        manager = self.package('success')
        backup_import.import_backup(manager, BACKUP)
        result = manager.read_json(manager.db_folder() / 'imported-backup.json')
        self.assertEqual(result['phase'], 'COMPLETE')
        self.assertEqual(result['verified_tables'], 102)
        with self.assertRaisesRegex(RuntimeError, 'contains data already'):
            backup_import.import_backup(manager, BACKUP)
        self.assertFalse((manager.db_folder() / 'import-in-progress.json').exists())

    def test_partial_import_failure_restores_pristine_database(self):
        manager = self.package('rollback')
        original = backup_import.restore_file
        def fail_common(module, path, log, database=None):
            if database == 'common':
                raise RuntimeError('Injected restore failure')
            return original(module, path, log, database)
        with mock.patch.object(backup_import, 'restore_file', side_effect=fail_common):
            with self.assertRaisesRegex(RuntimeError, 'Injected'):
                backup_import.import_backup(manager, BACKUP)
        self.assertFalse((manager.db_folder() / 'import-in-progress.json').exists())
        manager.start_db()
        self.assertEqual(manager.sql('SELECT COUNT(*) FROM account.account;').strip(), '0')
        self.assertEqual(manager.sql('SELECT COUNT(*) FROM common.locale;').strip(), '2')
        self.assertFalse((manager.db_folder() / 'imported-backup.json').exists())
        manager.stop_db()


class NativeReadinessTests(unittest.TestCase):
    def test_legacy_skill_curves_accept_supported_prefix_and_reject_invalid_data(self):
        if not BINARIES: self.skipTest('Pass --mariadb-bin for owned SQL integration')
        from backup_sandbox import Sandbox
        with tempfile.TemporaryDirectory(prefix='auriga-readiness-sql-') as directory:
            sandbox = Sandbox(Path(directory)/'sql', BINARIES)
            try:
                sandbox.start()
                sandbox.client.query('CREATE DATABASE common')
                sandbox.client.query('CREATE TABLE common.locale(mKey VARCHAR(64),mValue TEXT)')
                query = (ROOT/'db/check_ready.sql').read_text().split('UNION ALL')[0]
                for values, expected in [
                    (' '.join(map(str,range(41))), 'READY'),
                    (' '.join(map(str,range(45))), 'READY'),
                    (' '.join(['-1','+2']+['3']*39), 'READY'),
                    (' '.join(map(str,range(40))), 'MISSING_OR_INVALID'),
                    (' '.join(['0']*40+['invalid']), 'MISSING_OR_INVALID'),
                    (' '.join(['0']*41+['invalid']), 'MISSING_OR_INVALID'),
                    ('', 'MISSING_OR_INVALID'),
                ]:
                    with self.subTest(values=values):
                        sandbox.client.query('DELETE FROM common.locale')
                        sandbox.client.query("INSERT INTO common.locale VALUES('SKILL_POWER_BY_LEVEL','"+values+"')")
                        self.assertEqual(sandbox.client.query(query)[0][1], expected)
                sandbox.client.query('DELETE FROM common.locale')
                self.assertEqual(sandbox.client.query(query)[0][1],'MISSING_OR_INVALID')
                values=' '.join(['0']*41)
                sandbox.client.query("INSERT INTO common.locale VALUES('SKILL_POWER_BY_LEVEL','"+values+"'),('SKILL_POWER_BY_LEVEL','"+values+"')")
                self.assertEqual(sandbox.client.query(query)[0][1],'MISSING_OR_INVALID')
            finally:
                sandbox.stop()


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--backup', type=Path)
    parser.add_argument('--mariadb-bin', type=Path)
    args, remaining = parser.parse_known_args()
    BACKUP, BINARIES = args.backup, args.mariadb_bin.resolve() if args.mariadb_bin else None
    unittest.main(argv=[sys.argv[0], *remaining])
