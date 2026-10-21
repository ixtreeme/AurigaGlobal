"""Converter/launcher regression tests; optionally check the actual server executables."""
import argparse
import copy
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
def module(name):
    spec = importlib.util.spec_from_file_location(name, ROOT / 'SRC/Tools' / (name + '.py'))
    result = importlib.util.module_from_spec(spec); spec.loader.exec_module(result); return result
merge = module('merge_server_config')
runner = module('run_server')
BIN_DIR = None


class ConfigToolsTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='auriga-config-tools-')
        self.root = Path(self.temp.name).resolve()
        self.db = self.root / 'old/database/conf.json'; self.login = self.root / 'old/login/conf.json'
        self.game = self.root / 'old/ch1-core1/conf.json'; self.output = self.root / 'server.json'
        sql = {name: {'host': '127.0.0.1', 'port': 3306, 'user': 'fixture', 'password': 'PASSWORD_DO_NOT_LOG', 'database': name} for name in ('player','account','common','log')}
        self.legacy = [
            {'server': {'locale': 'utf8mb4', 'interserver_key': 'key', 'table_postfix': ''}, 'sql': {k: v for k, v in sql.items() if k != 'log'}, 'item_id_range': [1, 100]},
            {'locale': 'utf8mb4', 'interserver_key': 'key', 'account_sql': sql['account'], 'port': 30052, 'tls': {'cert_file': '../tls/server.pem', 'key_file': '../tls/server.key'}},
            {'server': {'hostname': 'core1', 'channel': 1, 'locale': 'utf8mb4', 'table_postfix': ''}, 'interserver_key': 'key', 'network': {'port': 30054, 'p2p_port': 30055, 'bind_ip': '127.0.0.1', 'tls': {'cert_file': '../tls/server.pem', 'key_file': '../tls/server.key'}}, 'sql': {k: v for k, v in sql.items() if k != 'account'}, 'map_allow': [1, 21, 41], 'quest': {'dir': 'quest', 'object_dir': ['quest/object']}}
        ]
        for path, value in zip((self.db, self.login, self.game), self.legacy):
            path.parent.mkdir(parents=True); path.write_text(json.dumps(value), encoding='utf-8')
    def tearDown(self):
        self.assertEqual(self.root.parent, Path(tempfile.gettempdir()).resolve())
        self.assertTrue(self.root.name.startswith('auriga-config-tools-'))
        self.temp.cleanup()
    def converted(self):
        return merge.merge_configs(self.db, self.login, [('ch1-core1', self.game)], self.output)
    def test_shared_fields_and_originals(self):
        original = [p.read_bytes() for p in (self.db, self.login, self.game)]
        result = self.converted()
        self.assertEqual(result['common']['interserver_key'], 'key')
        self.assertEqual(set(result['common']['sql']), {'player','account','common','log'})
        self.assertEqual(result['common']['tls']['cert_file'], 'old/tls/server.pem')
        self.assertNotIn('tls', result['login'])
        self.assertEqual(original, [p.read_bytes() for p in (self.db, self.login, self.game)])
    def test_paths_keep_old_meaning(self):
        value = self.converted()['game_servers']['ch1-core1']
        self.assertEqual((self.root / value['working_directory']).resolve(), self.game.parent)
        self.assertEqual((self.root / value['server']['data_path']).resolve(), self.game.parent / 'data')
        self.assertEqual((self.root / value['quest']['object_dir'][0]).resolve(), self.game.parent / 'quest/object')
    def test_different_sql_credentials_survive(self):
        value = copy.deepcopy(self.legacy[2]); value['sql']['player']['user'] = 'game-only'
        self.game.write_text(json.dumps(value), encoding='utf-8')
        result = self.converted()
        self.assertNotIn('player', result['common']['sql'])
        self.assertEqual(result['database']['sql']['player']['user'], 'fixture')
        self.assertEqual(result['game_servers']['ch1-core1']['sql']['player']['user'], 'game-only')
    def test_duplicate_and_reserved_names(self):
        for games in ([('same', self.game), ('same', self.game)], [('database', self.game)], []):
            with self.assertRaises(ValueError): merge.merge_configs(self.db, self.login, games, self.output)
    def test_duplicate_json_and_bad_paths(self):
        self.game.write_text('{"server":{},"server":{}}', encoding='utf-8')
        with self.assertRaises(ValueError): self.converted()
        value = copy.deepcopy(self.legacy[2]); value['quest']['object_dir'] = 'bad'
        self.game.write_text(json.dumps(value), encoding='utf-8')
        with self.assertRaises(ValueError): self.converted()
    def test_cli_does_not_overwrite(self):
        self.output.write_text('KEEP THIS', encoding='utf-8')
        completed = subprocess.run([sys.executable, '-B', str(ROOT / 'SRC/Tools/merge_server_config.py'), '--database', str(self.db), '--login', str(self.login), '--game', 'core=' + str(self.game), '--output', str(self.output)], capture_output=True, text=True)
        self.assertNotEqual(completed.returncode, 0)
        self.assertEqual(self.output.read_text(encoding='utf-8'), 'KEEP THIS')
        self.assertNotIn('PASSWORD_DO_NOT_LOG', completed.stdout + completed.stderr)
    def test_cli_creates_new_private_file(self):
        completed = subprocess.run([sys.executable, '-B', str(ROOT / 'SRC/Tools/merge_server_config.py'), '--database', str(self.db), '--login', str(self.login), '--game', 'ch1-core1=' + str(self.game), '--output', str(self.output)], capture_output=True, text=True)
        self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertEqual(json.loads(self.output.read_text(encoding='utf-8')), self.converted())
        self.assertNotIn('PASSWORD_DO_NOT_LOG', completed.stdout + completed.stderr)
        if os.name != 'nt': self.assertEqual(self.output.stat().st_mode & 0o777, 0o600)
    def test_launcher_uses_absolute_config_and_instance(self):
        args = runner.command(self.root / 'bin', self.output, 'ch1-core1', check=True)
        self.assertEqual(args[1:], ['--config', str(self.output.resolve()), '--instance', 'ch1-core1', '--check-config'])

    @unittest.skipUnless(BIN_DIR, 'native executable paths not supplied')
    def test_native_startup_and_rejection(self):
        config = self.converted(); self.output.write_text(json.dumps(config), encoding='utf-8')
        foreign = self.root / 'unrelated-cwd'; foreign.mkdir()
        for instance in ('database', 'login', 'ch1-core1'):
            completed = subprocess.run(runner.command(BIN_DIR, self.output, instance, check=True), cwd=foreign, capture_output=True, text=True, timeout=10)
            self.assertEqual(completed.returncode, 0, instance + ': ' + completed.stdout + completed.stderr)
            self.assertIn('CONFIG_OK', completed.stdout)
            self.assertNotIn('PASSWORD_DO_NOT_LOG', completed.stdout + completed.stderr)
        self.assertEqual(list(foreign.iterdir()), [], 'check-config created runtime files or logs')
        self.assertFalse((self.root / 'run').exists())
        completed = subprocess.run([sys.executable, '-B', str(ROOT / 'SRC/Tools/run_server.py'), '--config', str(self.output), '--bin-dir', str(BIN_DIR), '--check-config'], cwd=foreign, capture_output=True, text=True, timeout=15)
        self.assertEqual(completed.returncode, 0, completed.stdout + completed.stderr)
        self.assertEqual(completed.stdout.count('CONFIG_OK'), 3, 'launcher hides native validation results')
        self.assertNotIn('PASSWORD_DO_NOT_LOG', completed.stdout + completed.stderr)
        args = runner.command(BIN_DIR, self.output, 'missing', check=True)
        self.assertNotEqual(subprocess.run(args, cwd=foreign, capture_output=True, timeout=10).returncode, 0)
        config['game_servers']['ch1-core1']['network']['port'] = 30052
        self.output.write_text(json.dumps(config), encoding='utf-8')
        completed = subprocess.run(runner.command(BIN_DIR, self.output, 'database', check=True), cwd=foreign, capture_output=True, text=True, timeout=10)
        self.assertNotEqual(completed.returncode, 0)
        self.assertIn('overlapping', completed.stderr)
        completed = subprocess.run([sys.executable, '-B', str(ROOT / 'SRC/Tools/run_server.py'), '--config', str(self.output), '--bin-dir', str(BIN_DIR), '--check-config'], cwd=foreign, capture_output=True, text=True, timeout=15)
        self.assertNotEqual(completed.returncode, 0)
        self.assertIn('overlapping', completed.stderr, 'launcher hides native validation failure')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument('--bin-dir', type=Path)
    args, remaining = parser.parse_known_args()
    BIN_DIR = args.bin_dir.resolve() if args.bin_dir else None
    if BIN_DIR:
        ConfigToolsTests.test_native_startup_and_rejection = ConfigToolsTests.test_native_startup_and_rejection.__wrapped__
    unittest.main(argv=[sys.argv[0]] + remaining)
