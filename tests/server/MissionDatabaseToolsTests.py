"""Dedicated Mission configuration, DDL assembly and explicit package installation."""
import contextlib
import argparse
import copy
import io
import json
import os
from pathlib import Path
import sys
import tempfile
import shutil
import socket
import types
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'SRC/Tools'))
sys.path.insert(0, str(ROOT / 'SRC/Tools/ServerManager'))
from server_package import mission_database as mission, manage, backup_import
import backend
import backup_tools
BINARIES = None


class MissionDatabaseTests(unittest.TestCase):
    def setUp(self):
        self.config = json.loads((ROOT / 'config/server.json.example').read_text(encoding='utf-8'))

    def test_default_sql_is_generated_from_both_real_migrations(self):
        expected = '-- Generated from Mission migrations by mission_database.install_sql. MariaDB/InnoDB.\n'
        expected += mission.install_sql(ROOT / 'db', 'mission')
        self.assertEqual((ROOT / 'db/mission_schema.sql').read_text(encoding='utf-8'), expected)
        for table in mission.TABLES:
            self.assertIn('`mission`.`' + table + '`', expected)
        self.assertNotIn('USE ', expected)
        self.assertNotIn('`player`.', expected)
        self.assertIn('REFERENCES `mission`.`mission_engine_action_outbox`', expected)

    def test_custom_database_is_qualified_without_changing_other_tables(self):
        sql = mission.install_sql(ROOT / 'db', 'story_02')
        self.assertIn('CREATE DATABASE IF NOT EXISTS `story_02`', sql)
        self.assertNotIn('`mission`.', sql)
        self.assertNotIn('item_award', sql)

    def test_unsafe_or_system_identifiers_are_rejected(self):
        for value in ('', 'mysql', 'SYS', 'information_schema', 'a`b', 'a.b', ' a', 'a;DROP', 'a' * 65, None):
            with self.subTest(value=value), self.assertRaises(ValueError):
                mission.install_sql(ROOT / 'db', value)

    def test_legacy_config_derives_a_dedicated_target_and_preserves_input(self):
        self.config['common']['sql'].pop('mission')
        before = copy.deepcopy(self.config)
        target, player = mission.connection(self.config)
        self.assertEqual(target, dict(player, database='mission'))
        self.assertEqual(self.config, before)
        entries = backend.sql_connections(self.config)
        self.assertEqual({entry['connection']['database'] for entry in entries},
                         {'account', 'common', 'log', 'player', 'mission'})

    def test_partial_and_database_role_overrides_keep_credentials(self):
        self.config['common']['sql']['mission'] = {'database': 'story_02'}
        self.config['database']['sql'] = {'mission': {'user': 'mission_writer', 'password': 'PRIVATE_TEST'}}
        target, player = mission.connection(self.config)
        self.assertEqual(target['database'], 'story_02')
        self.assertEqual(target['host'], player['host'])
        self.assertEqual(target['password'], 'PRIVATE_TEST')
        entries = backend.sql_connections(self.config)
        own = next(row for row in entries if 'database/mission' in row['roles'])
        self.assertEqual(own['connection'], dict(target))

    def test_mission_cannot_alias_player_or_a_different_endpoint(self):
        for value in ({'database': 'PLAYER'}, {'host': '192.0.2.8'}, {'port': 3310}):
            changed = copy.deepcopy(self.config)
            changed['common']['sql']['mission'].update(value)
            with self.subTest(value=value), self.assertRaises(ValueError):
                mission.connection(changed)

    def test_player_and_mission_are_in_the_same_backup_consistency_group(self):
        groups = backup_tools.sql_groups(self.config, backend)
        self.assertEqual(len(groups), 1)
        self.assertEqual(groups[0]['databases'], ['account', 'common', 'log', 'mission', 'player'])
        self.config['common']['sql']['mission']['password'] = 'OTHER_BACKUP_CREDENTIAL'
        with self.assertRaises(ValueError):
            backup_tools.sql_groups(self.config, backend)

    def test_local_grants_keep_password_and_user_as_sql_literals(self):
        target, player = mission.connection(self.config)
        target.update(user="mission'writer", password="back\\slash'password")
        grants = mission.local_grants(target, player)
        self.assertIn("SET SESSION sql_mode='NO_BACKSLASH_ESCAPES'", grants)
        self.assertIn("'mission''writer'@'127.0.0.1'", grants)
        self.assertIn("'back\\slash''password'", grants)
        self.assertIn('GRANT SELECT,INSERT ON `player`.`item_award`', grants)

    def install(self, response='0\n', error=False):
        temp = tempfile.TemporaryDirectory(prefix='auriga-mission-install-')
        self.addCleanup(temp.cleanup)
        root = Path(temp.name)
        (root / '.auriga-package.json').write_text('{}')
        (root / 'server.json').write_text(json.dumps(self.config), encoding='utf-8')
        self.addCleanup(setattr, manage, 'ROOT', manage.ROOT)
        manage.ROOT = root
        calls, options = [], []
        def invoke(argv, input, timeout):
            path = Path(argv[1].split('=', 1)[1])
            options.append(path)
            self.assertTrue(path.is_file())
            self.assertNotIn(self.config['common']['sql']['mission']['password'], str(argv))
            self.assertIn('password=', path.read_text(encoding='utf-8'))
            if os.name != 'nt':
                self.assertEqual(path.stat().st_mode & 0o777, 0o600)
            calls.append(input)
            return types.SimpleNamespace(returncode=int(error), stdout=response, stderr='PRIVATE_SQL_ERROR')
        module = types.SimpleNamespace(**mission.__dict__)
        module.install_sql = lambda folder, name: mission.install_sql(ROOT / 'db', name)
        output = io.StringIO()
        with mock.patch.object(manage, 'mission_module', return_value=module), \
             mock.patch.object(manage.shutil, 'which', return_value='/private/mariadb'), \
             mock.patch.object(manage, 'native', side_effect=invoke), contextlib.redirect_stdout(output):
            if response != '0\n' or error:
                with self.assertRaises(RuntimeError):
                    manage.init_mission_db()
            else:
                manage.init_mission_db()
        self.assertTrue(all(not path.exists() for path in options))
        return calls, output.getvalue()

    def test_explicit_install_uses_private_options_and_additive_qualified_ddl(self):
        calls, output = self.install()
        self.assertEqual(len(calls), 2)
        self.assertIn('information_schema.TABLES', calls[0])
        self.assertIn('CREATE DATABASE IF NOT EXISTS `mission`', calls[1])
        self.assertNotIn('DROP TABLE', calls[1])
        self.assertIn('MISSION_DATABASE_INITIALIZED mission', output)

    def test_legacy_tables_and_unreadable_source_prevent_an_empty_campaign(self):
        for result, error in (('8\n', False), ('0\n', True)):
            with self.subTest(result=result, error=error):
                calls, output = self.install(result, error)
                self.assertEqual(len(calls), 1)
                self.assertNotIn('PRIVATE_SQL_ERROR', output)


class OwnedBootstrapTests(unittest.TestCase):
    def test_fresh_local_bootstrap_and_explicit_upgrade_preserve_progress(self):
        if BINARIES is None or os.name != 'nt':
            self.skipTest('Pass --mariadb-bin for a fresh owned Windows SQL instance')
        with tempfile.TemporaryDirectory(prefix='auriga-mission-bootstrap-') as directory:
            root = Path(directory)
            (root / '.auriga-package.json').write_text('{}')
            (root / 'db').mkdir()
            for name in ('schema.sql', 'seed_core.sql', 'check_ready.sql', 'mission_schema.sql'):
                shutil.copy2(ROOT / 'db' / name, root / 'db' / name)
            shutil.copytree(ROOT / 'db/migrations', root / 'db/migrations')
            cfg = json.loads((ROOT / 'config/server.json.example').read_text(encoding='utf-8'))
            with socket.socket() as listener:
                listener.bind(('127.0.0.1', 0))
                port = listener.getsockname()[1]
            for value in cfg['common']['sql'].values():
                value.update(port=port, user='mission_fixture', password='private_fixture_password')
            (root / 'server.json').write_text(json.dumps(cfg), encoding='utf-8')
            previous = manage.ROOT
            manage.ROOT = root
            try:
                manage.init_db(BINARIES)
                manage.start_db()
                self.assertEqual(manage.sql('SELECT schema_version FROM mission.mission_engine_schema;').strip(), '2')
                self.assertEqual(manage.sql("SELECT COUNT(*) FROM information_schema.TABLES WHERE TABLE_SCHEMA='mission';").strip(), '8')
                manage.sql('INSERT INTO mission.mission_engine_characters(character_id) VALUES(7);')
                cfg['common']['sql']['mission'].update(user='mission_upgrade_writer', password="quoted\\and'password")
                (root / 'server.json').write_text(json.dumps(cfg), encoding='utf-8')
                manage.init_mission_db()
                self.assertEqual(manage.sql('SELECT COUNT(*) FROM mission.mission_engine_characters;').strip(), '1')
                entry = dict(connection=cfg['common']['sql']['mission'], roles=['database/mission'])
                status = backend.probe_sql(str(BINARIES / 'mariadb.exe'), entry)
                self.assertEqual(status['status'], 'available')
                with mock.patch.object(backup_import, 'validate_backup', return_value=(root, {})), \
                     self.assertRaisesRegex(RuntimeError, 'Mission progress exists'):
                    backup_import.import_backup(manage, root)
                self.assertEqual(manage.sql('SELECT COUNT(*) FROM mission.mission_engine_characters;').strip(), '1')
            finally:
                try:
                    manage.stop_db()
                finally:
                    manage.ROOT = previous


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--mariadb-bin', type=Path)
    args, remaining = parser.parse_known_args()
    BINARIES = args.mariadb_bin.resolve() if args.mariadb_bin else None
    unittest.main(argv=[sys.argv[0], *remaining])
