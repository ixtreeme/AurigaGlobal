"""Editor transactions: real files/locks, native validation when requested."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import shutil
import sys
import tempfile
import unittest
from types import SimpleNamespace
from unittest import mock

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / 'SRC/Tools/ServerManager'))
import backend as api
import install_manager
PACKAGE = None


class SqlStatusTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix='auriga-sql-status-test-')
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name).resolve()
        (self.root / '.auriga-package.json').write_text('{}')
        shutil.copy2(REPO / 'SRC/Tools/server_package/manage.py', self.root / 'manage.py')
        self.config = json.loads((REPO / 'config/server.json.example').read_text())
        for value in self.config['common']['sql'].values(): value['password'] = 'PRIVATE_SQL_"\\with\nline'
        (self.root / 'server.json').write_text(json.dumps(self.config))

    def healthy(self, binary, entry):
        value = entry['connection']
        return dict(roles=entry['roles'],host=value['host'],port=value['port'],database=value['database'],
                    status='available',message='Kapcsolat rendben.')

    def test_effective_instance_overrides_are_checked_and_common_connections_are_deduplicated(self):
        entries = api.sql_connections(self.config)
        self.assertEqual(len(entries),4)
        self.config['database']['sql'] = {'player': {'port': 3311}}
        self.config['login']['account_sql'] = {'port': 3312}
        self.config['game_defaults']['sql'] = {'log': {'port': 3313}}
        core = next(iter(self.config['game_servers'].values()))
        core['sql'] = {'common': {'port': 3314}}
        entries = api.sql_connections(self.config)
        ports = {role:entry['connection']['port'] for entry in entries for role in entry['roles']}
        self.assertEqual(ports['database/player'],3311);self.assertEqual(ports['login/account'],3312)
        self.assertEqual(ports['ch1-core1/log'],3313);self.assertEqual(ports['ch1-core1/common'],3314)
        self.assertEqual(ports['database/common'],3306)

    def test_read_only_queries_private_options_and_secret_free_cache(self):
        temporary, calls = [], []
        def client(argv, **kwargs):
            path = Path(argv[1].split('=',1)[1]);temporary.append(path)
            content = path.read_text(encoding='utf-8')
            self.assertIn('password="PRIVATE_SQL_',content)
            self.assertNotIn('PRIVATE_SQL',str(argv));self.assertEqual(kwargs['input'],'SELECT 1;\n')
            if os.name != 'nt': self.assertEqual(path.stat().st_mode & 0o777,0o600)
            calls.append(argv)
            return SimpleNamespace(returncode=0,stdout='1\n',stderr='')
        before = (self.root / 'server.json').read_bytes()
        with mock.patch.object(api,'sql_client',return_value='/client'),mock.patch.object(api.subprocess,'run',side_effect=client):
            result = api.external_sql_status(self.root,self.config)
        self.assertEqual(result['status'],'available');self.assertEqual(len(calls),4)
        self.assertTrue(all(not path.exists() for path in temporary))
        self.assertNotIn('PRIVATE_SQL',json.dumps(result))
        self.assertNotIn('PRIVATE_SQL',(self.root / 'run/sql-status.json').read_text())
        self.assertEqual((self.root / 'server.json').read_bytes(),before)
        self.assertFalse((self.root / 'run/mariadb').exists())

    def test_cache_expires_force_refreshes_and_configuration_changes_invalidate_it(self):
        with mock.patch.object(api,'sql_client',return_value='/client'),mock.patch.object(api,'probe_sql',side_effect=self.healthy) as probe:
            first = api.external_sql_status(self.root,self.config)
            self.assertEqual(probe.call_count,4)
            self.assertEqual(api.external_sql_status(self.root,self.config),first)
            self.assertEqual(probe.call_count,4)
            api.external_sql_status(self.root,self.config,force=True);self.assertEqual(probe.call_count,8)
            self.config['common']['sql']['account']['password'] = 'changed-password'
            api.external_sql_status(self.root,self.config);self.assertEqual(probe.call_count,12)
            with mock.patch.object(api.time,'time',return_value=first['checked_at']+40):
                api.external_sql_status(self.root,self.config)
            self.assertEqual(probe.call_count,16)

    def test_connection_failures_are_distinct_and_do_not_reveal_client_output(self):
        entry = api.sql_connections(self.config)[0]
        for code,status in ((1045,'error'),(1049,'error'),(2003,'unavailable')):
            failed = SimpleNamespace(returncode=1,stdout='',stderr='ERROR %d PRIVATE_SQL_ diagnostic' % code)
            with self.subTest(code=code),mock.patch.object(api.subprocess,'run',return_value=failed):
                result = api.probe_sql('/client',entry)
                self.assertEqual(result['status'],status);self.assertEqual(result['code'],code)
                self.assertNotIn('PRIVATE_SQL',json.dumps(result))

    def test_timeout_removes_credentials_and_returns_an_unavailable_status(self):
        entry = api.sql_connections(self.config)[0];options=[]
        def timeout(argv,**kwargs):
            options.append(Path(argv[1].split('=',1)[1]))
            raise api.subprocess.TimeoutExpired(argv,3)
        with mock.patch.object(api.subprocess,'run',side_effect=timeout):result = api.probe_sql('/client',entry)
        self.assertEqual(result['status'],'unavailable')
        self.assertTrue(all(not path.exists() for path in options))

    def test_missing_client_does_not_claim_database_is_stopped(self):
        with mock.patch.object(api,'sql_client',return_value=None),mock.patch.object(api,'probe_sql') as probe:
            result = api.external_sql_status(self.root,self.config)
        self.assertEqual(result['status'],'unknown');probe.assert_not_called()

    def test_invalid_cache_and_sql_settings_do_not_disconnect_the_package(self):
        cache = self.root / 'run/sql-status.json';cache.parent.mkdir();cache.write_text('{broken')
        with mock.patch.object(api,'sql_client',return_value='/client'),mock.patch.object(api,'probe_sql',side_effect=self.healthy):
            self.assertEqual(api.external_sql_status(self.root,self.config)['status'],'available')
            self.config['common']['sql'] = 'invalid'
            self.assertEqual(api.external_sql_status(self.root,self.config)['status'],'error')

    def test_copied_windows_owner_cannot_select_managed_sql_on_freebsd(self):
        folder = self.root / 'run/mariadb';folder.mkdir(parents=True)
        (folder / 'owner.json').write_text('{"binary_dir":"D:/old/mariadb"}')
        manager = api.load_manager(self.root);manager.save_json(folder / 'process.json',manager.identity(os.getpid()))
        environment = SimpleNamespace(**dict(vars(os),name='posix'))
        before = {p.relative_to(self.root):p.read_bytes() for p in self.root.rglob('*') if p.is_file()}
        with mock.patch.object(api,'os',environment):state = api.describe(self.root)
        self.assertEqual(state['database_kind'],'external');self.assertFalse(state['database_running'])
        self.assertFalse(state['can_setup']);self.assertTrue(state['can_start'])
        self.assertEqual(before,{p.relative_to(self.root):p.read_bytes() for p in self.root.rglob('*') if p.is_file()})

    def test_rpc_health_failure_preserves_connection_and_disables_start(self):
        import server_target_worker
        base = api.describe(self.root);base.update(database_kind='external',can_start=True)
        health = dict(status='unavailable',connections=[],checked_at=0)
        with mock.patch.object(api,'describe',return_value=base),mock.patch.object(api,'external_sql_status',return_value=health) as probe:
            state = server_target_worker.dispatch(api,self.root,dict(operation='status',arguments=dict(force_sql=True)))['state']
        self.assertFalse(state['can_start']);self.assertFalse(state['database_running'])
        self.assertEqual(state['database_status'],health);probe.assert_called_once_with(self.root,base['config'],force=True)


class ServerManagerTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='auriga-manager-tests-')
        self.root = Path(self.temp.name).resolve()
        self.root.joinpath('.auriga-package.json').write_text('{}', encoding='utf-8')
        shutil.copy2(REPO / 'SRC/Tools/server_package/manage.py', self.root / 'manage.py')
        shutil.copy2(REPO / 'config/server.json.example', self.root / 'server.json')
        (self.root / 'data/quest').mkdir(parents=True)
        (self.root / 'data/example.json').write_text('{"rows": [{"vnum": 1, "enabled": true}]}\n', encoding='utf-8')

    def tearDown(self):
        self.assertEqual(self.root.parent, Path(tempfile.gettempdir()).resolve())
        self.assertTrue(self.root.name.startswith('auriga-manager-tests-'))
        self.temp.cleanup()

    def test_strict_json_rejects_duplicate_keys_nonfinite_and_bad_syntax(self):
        for source in ('{"a": 1, "a": 2}', '{"a": NaN}', '{"a": Infinity}', '{bad}'):
            with self.subTest(source=source), self.assertRaises(api.EditorError):
                api.strict_json(source)
        self.assertEqual(api.strict_json('{"a": [true, null, 12]}'), {'a': [True, None, 12]})

    def test_owned_package_and_editable_boundaries(self):
        self.assertEqual(api.package_root(self.root), self.root)
        for path in ('../outside.json', 'run/mariadb/admin.cnf', 'bin/GameServer.exe', '.auriga-package.json', 'data/asset.tga'):
            with self.subTest(path=path), self.assertRaises(api.EditorError):
                api.editable_path(self.root, path)
        self.assertEqual(api.Document(self.root, 'data/../server.json').relative, 'server.json')

    def test_nested_merge_preserves_shared_values_and_array_replacement(self):
        first = dict(server=dict(channel=1), map_allow=[1, 2])
        second = dict(server=dict(hostname='core'), map_allow=[5])
        self.assertEqual(api.merge(first, second), dict(server=dict(channel=1, hostname='core'), map_allow=[5]))
        self.assertEqual(first['map_allow'], [1, 2])

    def test_latin1_and_line_endings_roundtrip(self):
        path = self.root / 'data/quest/example.lua'
        raw = b'-- \xe1\xe9\r\nreturn 1\r\n'
        path.write_bytes(raw)
        document = api.Document(self.root, 'data/quest/example.lua')
        self.assertEqual(document.encoding, 'latin1')
        self.assertEqual(document.encoded(document.text), raw)
        with self.assertRaises(api.EditorError):
            document.encoded(document.text + '\U0001f600')

    def test_save_creates_exact_backup_then_atomic_replacement(self):
        document = api.Document(self.root, 'data/example.json')
        replacement = b'{"rows": []}\n'
        with mock.patch.object(api, 'validate_candidate', return_value='VALID\n'):
            output = api.save_document(self.root, document.relative, replacement, document.digest)
        self.assertIn('MENTVE', output)
        self.assertEqual(document.path.read_bytes(), replacement)
        backups = api.backup_files(self.root, document.relative)
        self.assertEqual(len(backups), 1)
        self.assertEqual(api.restore_payload(self.root, document.relative, backups[0]), document.original)
        self.assertEqual(list(document.path.parent.glob('.*.edit-*')), [])
        self.assertFalse((self.root / 'run/mariadb').exists())

    def test_external_change_never_overwritten(self):
        document = api.Document(self.root, 'data/example.json')
        document.path.write_text('{"changed": true}')
        with self.assertRaises(api.EditorError):
            api.save_document(self.root, document.relative, b'{}', document.digest)
        self.assertEqual(document.path.read_text(), '{"changed": true}')
        self.assertEqual(api.backup_files(self.root, document.relative), [])

    def test_validation_failure_keeps_original_and_cleans_stage(self):
        document = api.Document(self.root, 'data/example.json')
        with mock.patch.object(api, 'validate_candidate', side_effect=api.EditorError('invalid')):
            with self.assertRaises(api.EditorError):
                api.save_document(self.root, document.relative, b'{}', document.digest)
        self.assertEqual(document.path.read_bytes(), document.original)
        self.assertEqual(api.backup_files(self.root, document.relative), [])
        self.assertEqual(list(document.path.parent.glob('.*.edit-*')), [])

    def test_concurrent_change_during_validation_keeps_newer_bytes(self):
        document = api.Document(self.root, 'data/example.json')
        def change(*_):
            document.path.write_bytes(b'{"external": 2}')
            return 'VALID'
        with mock.patch.object(api, 'validate_candidate', side_effect=change), self.assertRaises(api.EditorError):
            api.save_document(self.root, document.relative, b'{}', document.digest)
        self.assertEqual(document.path.read_bytes(), b'{"external": 2}')

    def test_running_owned_stack_blocks_save(self):
        manager = api.load_manager(self.root)
        manager.save_json(self.root / 'run/supervisor.json', dict(supervisor=manager.identity(os.getpid()), children=[], phase='starting'))
        document = api.Document(self.root, 'data/example.json')
        with self.assertRaises(api.EditorError):
            api.save_document(self.root, document.relative, b'{}', document.digest)
        self.assertEqual(document.path.read_bytes(), document.original)

    def test_running_sql_blocks_config_alias_too(self):
        manager = api.load_manager(self.root)
        manager.save_json(self.root / 'run/mariadb/process.json', manager.identity(os.getpid()))
        document = api.Document(self.root, 'data/../server.json')
        with self.assertRaises(api.EditorError):
            api.save_document(self.root, document.relative, document.original, document.digest)

    def test_restore_cannot_read_other_files(self):
        with self.assertRaises(api.EditorError):
            api.restore_payload(self.root, 'data/example.json', self.root / 'server.json')

    def test_live_quest_save_uses_publication_lock_with_supervisor_running(self):
        self.root.joinpath('data/quest/live.lua').write_text('return 1\n', encoding='utf-8')
        document = api.Document(self.root, 'data/quest/live.lua')
        manager = api.load_manager(self.root)
        state = dict(running=True, hotreload=True, database_running=False)
        with manager.supervisor_lock(), mock.patch.object(api, 'describe', return_value=state), \
                mock.patch.object(api, 'validate_candidate', return_value='LUA_SYNTAX_OK\n'):
            output = api.save_document(self.root, document.relative, b'return 2\n', document.digest)
        self.assertIn('MENTVE', output)
        self.assertEqual(document.path.read_bytes(), b'return 2\n')
        self.assertEqual(api.restore_payload(self.root, document.relative, api.backup_files(self.root, document.relative)[0]), document.original)

    def test_live_edit_scope_excludes_shared_setup_and_generated_objects(self):
        for relative in ('server.json', 'data/settings.lua', 'data/restart_city_pos.lua', 'data/mob_proto.json', 'data/quest/object/x/login.lua'):
            self.assertFalse(api.hotreload_file(relative), relative)
        for relative in ('data/quest/Quest/demo.quest', 'data/quest/quest_list', 'data/quest/questnpc.json', 'data/quest/questlib.lua'):
            self.assertTrue(api.hotreload_file(relative), relative)

    def test_redaction_covers_passwords_and_peer_child_tokens(self):
        config = api.strict_json((self.root / 'server.json').read_text())
        config['common']['sql']['player']['password'] = 'private-password-xyz'
        config['common']['interserver_key'] = 'private-peer-key'
        (self.root / 'server.json').write_text(json.dumps(config))
        output = api.redact('private-password-xyz and private-peer-key', self.root)
        self.assertNotIn('private-password-xyz', output)
        self.assertNotIn('private-peer-key', output)

    def test_status_does_not_start_or_initialize_services(self):
        before = {p.relative_to(self.root): p.read_bytes() for p in self.root.rglob('*') if p.is_file()}
        state = api.describe(self.root)
        self.assertFalse(state['running'])
        self.assertFalse(state['database_initialized'])
        self.assertEqual(len(state['rows']), 3)
        self.assertEqual(before, {p.relative_to(self.root): p.read_bytes() for p in self.root.rglob('*') if p.is_file()})

    def test_running_external_sql_does_not_block_editing_a_stopped_servers_configuration(self):
        document = api.Document(self.root,'server.json')
        state = dict(running=False,hotreload=False,database_running=True,database_kind='external')
        value = json.loads(document.text);value['login']['port'] += 1
        with mock.patch.object(api,'describe',return_value=state),mock.patch.object(api,'validate_candidate',return_value='checked'):
            api.save_document(self.root,'server.json',json.dumps(value).encode(),document.digest)
        self.assertEqual(json.loads((self.root / 'server.json').read_text())['login']['port'],value['login']['port'])

    def test_installer_preserves_config_and_refuses_second_install(self):
        bundle = self.root / 'test-bundle'
        bundle.mkdir()
        (bundle / 'AurigaServerManager.exe').write_bytes(b'test-bundle')
        # A test bundle is outside the release so it cannot alter its manifest.
        release = self.root / 'release'
        release.mkdir()
        for name in ('.auriga-package.json', 'server.json'):
            shutil.copy2(self.root / name, release / name)
        (release / 'README.md').write_text('Release\n')
        manifest = dict(version=1, files={relative: install_manager.digest(path) for relative, path in install_manager.members(release)})
        (release / 'package-manifest.json').write_text(json.dumps(manifest))
        before = (release / 'server.json').read_bytes()
        install_manager.install(release, bundle)
        self.assertEqual((release / 'server.json').read_bytes(), before)
        self.assertTrue((release / 'Szerverkezelo.cmd').is_file())
        with self.assertRaises(FileExistsError):
            install_manager.install(release, bundle)

    def test_installer_refuses_changed_release_before_writing(self):
        bundle = self.root / 'test-bundle'
        bundle.mkdir()
        (bundle / 'AurigaServerManager.exe').write_bytes(b'test-bundle')
        # Removing this from member enumeration is unnecessary: it is recorded,
        # then a different recorded file is deliberately changed.
        manifest = dict(version=1, files={relative: install_manager.digest(path) for relative, path in install_manager.members(self.root)})
        (self.root / 'package-manifest.json').write_text(json.dumps(manifest))
        (self.root / 'data/example.json').write_text('{}')
        with self.assertRaises(ValueError):
            install_manager.install(self.root, bundle)
        self.assertFalse((self.root / 'manager').exists())

    def test_gui_update_preserves_operator_config_edits_and_their_baseline(self):
        bundle = self.root / 'test-bundle'
        bundle.mkdir()
        (bundle / 'AurigaServerManager.exe').write_bytes(b'new-gui')
        (self.root / 'manager').mkdir()
        (self.root / 'manager/AurigaServerManager.exe').write_bytes(b'old-gui')
        baseline = dict(version=1, files={relative: install_manager.digest(path) for relative, path in install_manager.members(self.root)})
        (self.root / 'package-manifest.json').write_text(json.dumps(baseline))
        original = (self.root / 'server.json').read_bytes()
        changed = api.strict_json(original)
        changed['operator_note'] = 'keep this user edit'
        (self.root / 'server.json').write_text(json.dumps(changed))
        before = (self.root / 'server.json').read_bytes()
        install_manager.update(self.root, bundle)
        self.assertEqual((self.root / 'server.json').read_bytes(), before)
        self.assertEqual((self.root / 'manager/AurigaServerManager.exe').read_bytes(), b'new-gui')
        updated = json.loads((self.root / 'package-manifest.json').read_text())
        self.assertEqual(updated['files']['server.json'], baseline['files']['server.json'])
        self.assertEqual([p.read_bytes() for p in (self.root / 'run/manager-updates').rglob('AurigaServerManager.exe')], [b'old-gui'])

    def test_gui_update_refuses_untracked_changes_to_the_gui_itself(self):
        bundle = self.root / 'test-bundle'
        bundle.mkdir()
        (bundle / 'AurigaServerManager.exe').write_bytes(b'new-gui')
        (self.root / 'manager').mkdir()
        target = self.root / 'manager/AurigaServerManager.exe'
        target.write_bytes(b'original-gui')
        manifest = dict(version=1, files={relative: install_manager.digest(path) for relative, path in install_manager.members(self.root)})
        (self.root / 'package-manifest.json').write_text(json.dumps(manifest))
        target.write_bytes(b'user-custom-gui')
        with self.assertRaises(ValueError):
            install_manager.update(self.root, bundle)
        self.assertEqual(target.read_bytes(), b'user-custom-gui')

    def test_real_native_config_and_lua_validation(self):
        if not PACKAGE:
            self.skipTest('Pass --package for real native validators')
        shutil.copy2(PACKAGE / 'server.json', self.root / 'server.json')
        (self.root / 'bin').mkdir()
        for path in (PACKAGE / 'bin').iterdir():
            if path.is_file():
                shutil.copy2(path, self.root / 'bin' / path.name)
        for name in ('questnpc.json', 'questcategory.json'):
            shutil.copy2(PACKAGE / 'data/quest' / name, self.root / 'data/quest' / name)
        config = api.Document(self.root, 'server.json')
        output = api.save_document(self.root, config.relative, config.original, config.digest)
        self.assertEqual(output.count('CONFIG_OK'), 15)
        value = api.strict_json(config.text)
        value['login']['port'] = value['database']['network']['bind_port']
        with self.assertRaises(api.EditorError):
            api.save_document(self.root, config.relative, json.dumps(value).encode(), config.digest)
        script = self.root / 'data/quest/check.lua'
        script.write_text('return 1\n')
        document = api.Document(self.root, 'data/quest/check.lua')
        with self.assertRaises(api.EditorError):
            api.save_document(self.root, document.relative, b'function broken(', document.digest)
        self.assertEqual(script.read_bytes(), document.original)
        self.assertIn('PACKAGE_DATA_OK', api.save_document(self.root, document.relative, b'return 2\n', document.digest))


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--package', type=Path)
    args, rest = parser.parse_known_args()
    PACKAGE = args.package.resolve() if args.package else None
    unittest.main(argv=[sys.argv[0], *rest])
