"""Source preservation, conversion fidelity, runtime ownership and native Lua/JSON checks."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import unittest
from unittest import mock
import zipfile

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / 'SRC/Tools'))
from server_package.legacy import central_config, sql_connection, PackageError
from server_package.data import DataBuilder, write_json, module as converter_module
from server_package import manage
import prepare_server
CHECKER = None


class PackageToolsTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='auriga-package-tests-')
        self.root = Path(self.temp.name).resolve()
        self.previous_root = manage.ROOT
        manage.ROOT = self.root
    def tearDown(self):
        manage.ROOT = self.previous_root
        self.assertEqual(self.root.parent, Path(tempfile.gettempdir()).resolve())
        self.assertTrue(self.root.name.startswith('auriga-package-tests-'))
        self.temp.cleanup()
    def test_named_rows_without_header_keep_every_positional_cell(self):
        source, target = self.root / 'rows.json', self.root / 'named.json'
        write_json(source, {'groups': [{'name': 'grade_brilliant', 'tokens': {
            'step_lowest': ['3','4','5','6','0','0','0'], 'short': ['8','10']}}]})
        converter_module('txt2json_typed').convert_named_rows(str(source), str(target))
        node = json.loads(target.read_text(encoding='utf-8'))['groups'][0]
        self.assertEqual(node['columns'], list(map(str, range(7))))
        self.assertEqual(list(node['rows']['step_lowest'].values()), ['3','4','5','6','0','0','0'])
        self.assertEqual(node['rows']['short'], {'0':'8','1':'10'})
    def test_packaged_database_runtime_is_preferred(self):
        binaries = self.root / 'tools/mariadb/bin'
        binaries.mkdir(parents=True)
        for name in ('mariadbd','mariadb','mariadb-admin'):
            (binaries / (name + ('.exe' if os.name == 'nt' else ''))).touch()
        self.assertEqual(manage.mariadb_bin(), binaries)
    def test_database_setup_preserves_an_existing_data_directory(self):
        write_json(self.root / '.auriga-package.json', {'version': 1})
        write_json(self.root / 'server.json', {})
        (self.root / 'run/mariadb/data').mkdir(parents=True)
        with self.assertRaisesRegex(RuntimeError, 'already exists'):
            manage.setup_database()
    def legacy(self):
        source = self.root / 'original'
        (source / 'db').mkdir(parents=True)
        (source / 'auth').mkdir()
        (source / 'chan/ch1/core1').mkdir(parents=True)
        (source / 'db/conf.txt').write_text('SQL_PLAYER = "127.0.0.1 player user PASSWORD_DO_NOT_LOG 3306"\nSQL_ACCOUNT = "127.0.0.1 account user PASSWORD_DO_NOT_LOG 3306"\nSQL_COMMON = "127.0.0.1 common user PASSWORD_DO_NOT_LOG 3306"\nBIND_PORT=30051\nITEM_ID_RANGE=100 1000\nLOCALE=latin1\n', encoding='ascii')
        (source / 'auth/CONFIG').write_text('PORT: 30052\n', encoding='ascii')
        (source / 'chan/ch1/core1/CONFIG').write_text('HOSTNAME: test-core\nCHANNEL: 1\nPORT: 30054\nP2P_PORT: 30055\nDB_PORT: 30051\nBIND_IP: 1.2.3.4\nMAP_ALLOW: 1\nEMOTION_MASK_REQUIRE: 0\nPRISM_ITEM_REQUIRE: 1\nSHOP_PRICE_3X_TAX: 0\nEMPIRE_LANGUAGE_CHECK: 0\nITEM_COUNT_LIMIT: 50000\nPLAYER_SQL: 127.0.0.1 user PASSWORD_DO_NOT_LOG player 3306\nCOMMON_SQL: 127.0.0.1 user PASSWORD_DO_NOT_LOG common 3306\nLOG_SQL: 127.0.0.1 user PASSWORD_DO_NOT_LOG log 3306\n', encoding='ascii')
        return source
    def test_sql_field_orders(self):
        db = sql_connection('"127.0.0.1 account user private 3307"', True)
        game = sql_connection('127.0.0.1 user private account 3307')
        self.assertEqual(db, game)
        self.assertEqual(db['database'], 'account')
    def test_central_conversion_preserves_flags_and_originals(self):
        source = self.legacy()
        before = {p: p.read_bytes() for p in source.rglob('*') if p.is_file()}
        cfg, report = central_config(source, 3307)
        self.assertEqual(report['core_count'], 1)
        self.assertEqual(cfg['database']['network']['bind_port'], 30051)
        self.assertEqual(cfg['game_servers']['ch1-core1']['network']['port'], 30054)
        gameplay = cfg['game_defaults']['gameplay']
        self.assertTrue(gameplay['disable_emotion_mask'])
        self.assertFalse(gameplay['disable_prism_need'])
        self.assertTrue(gameplay['empire_shop_price_triple_disable'])
        self.assertTrue(gameplay['disable_empire_language_check'])
        self.assertEqual(gameplay['item_count_limit'], 50000)
        self.assertEqual(cfg['game_defaults']['network']['bind_ip'], '127.0.0.1')
        self.assertNotIn('PASSWORD_DO_NOT_LOG', json.dumps(cfg))
        self.assertEqual(before, {p: p.read_bytes() for p in before})
    def test_package_rejects_source_overlap_and_existing_output(self):
        source = self.legacy()
        with self.assertRaises(PackageError): prepare_server.build(source, source / 'new', self.root)
        existing = self.root / 'existing'; existing.mkdir(); (existing / 'keep').write_text('KEEP')
        with self.assertRaises(FileExistsError): prepare_server.build(source, existing, self.root)
        self.assertEqual((existing / 'keep').read_text(), 'KEEP')
    def test_lua_resource_rewrite_preserves_non_utf8_bytes(self):
        source, output = self.root / 'source', self.root / 'package'
        source.mkdir(); (output / 'data/dungeon/test').mkdir(parents=True)
        write_json(output / 'data/dungeon/test/regen.json', {'regens': []})
        path = output / 'data/script.lua'
        raw = b'-- \xe1\xe9\nlocal x="data/dungeon/test/regen.txt"\n'
        path.write_bytes(raw)
        report = DataBuilder(source, output).patch_lua_paths(output / 'data')
        self.assertEqual(path.read_bytes(), raw.replace(b'regen.txt', b'regen.json'))
        self.assertEqual(len(report['lua_path_changes']), 1)
        self.assertEqual(report['unresolved_lua_resources'], [])
    def test_motion_bake_millisecond_contract(self):
        folder = self.root / 'test-monster'; folder.mkdir()
        (folder / 'motlist.txt').write_text('GENERAL NORMAL_ATTACK attack.msa 100\n')
        (folder / 'attack.msa').write_text('MotionDuration 1.234\n')
        import io
        out = io.StringIO()
        self.assertEqual(converter_module('motion_bake').bake_folder(str(folder), out), 1)
        self.assertIn("'test-monster', 0, 0, 1234,", out.getvalue())
    def test_process_identity_rejects_reused_pid(self):
        current = manage.identity(os.getpid())
        self.assertIsNotNone(current)
        self.assertTrue(manage.owned(current))
        fake = dict(current, created=current['created'] + 1 if isinstance(current['created'], int) else 'different')
        self.assertFalse(manage.owned(fake))
        process = subprocess.Popen([sys.executable, '-B', '-c', 'pass'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        process.wait(timeout=5)
        self.assertIsNone(manage.identity(process.pid))
    def test_supervisor_lock_is_exclusive_and_released(self):
        script = 'import sys; from pathlib import Path; sys.path.insert(0,sys.argv[1]); from server_package import manage; manage.ROOT=Path(sys.argv[2]);\nwith manage.supervisor_lock(): print("LOCK_ACQUIRED")'
        argv = [sys.executable, '-B', '-c', script, str(REPO / 'SRC/Tools'), str(self.root)]
        with manage.supervisor_lock():
            result = subprocess.run(argv, capture_output=True, text=True, timeout=5)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('already in progress', result.stderr)
        result = subprocess.run(argv, capture_output=True, text=True, timeout=5)
        self.assertEqual(result.returncode, 0, result.stderr)
    def test_channel_selection_keeps_shared_core(self):
        cfg = {'game_servers': {'ch1-core1': {'server': {'channel': 1}}, 'ch2-core1': {'server': {'channel': 2}}, 'ch99-core99': {'server': {'channel': 99}}}}
        self.assertEqual(manage.select_instances(cfg, [1]), ['database', 'ch1-core1', 'ch99-core99', 'login'])
        with self.assertRaises(RuntimeError): manage.select_instances(cfg, [3])
    def test_external_working_directory_is_rejected(self):
        with self.assertRaises(RuntimeError): manage.instance_directory({'login': {'working_directory': '../outside'}}, 'login')
    def test_file_prepare_needs_no_database_and_preserves_runtime_state(self):
        cfg, _ = central_config(self.legacy(), 3307)
        defaults = self.root / 'runtime_defaults'
        defaults.mkdir(); (defaults / 'CMD').write_text('DEFAULT')
        (defaults / 'mark').mkdir(); (defaults / 'mark/mark_index').write_text('BASELINE')
        with mock.patch.object(manage, 'file_check', return_value=cfg), \
             mock.patch.object(manage, 'sql', side_effect=AssertionError('SQL must stay deferred')), \
             mock.patch.object(manage, 'native', side_effect=AssertionError('No additional process needed')):
            result = manage.prepare_package()
            self.assertEqual(result['status'], 'SERVER_FILES_READY')
            self.assertFalse(result['services_started'])
            self.assertEqual(result['database_stage'], 'DEFERRED')
            self.assertEqual(len(result['instances']), 3)
            state = self.root / 'run/ch1-core1/CMD'
            state.write_text('CUSTOM')
            (defaults / 'mark/mark_index').write_text('CHANGED SOURCE')
            second = manage.prepare_package()
            self.assertEqual(second['seeded_files'], 0)
            self.assertEqual(state.read_text(), 'CUSTOM')
            self.assertEqual((self.root / 'run/ch1-core1/mark/mark_index').read_text(), 'BASELINE')
        self.assertFalse((self.root / 'run/mariadb').exists())
    def archive_fixture(self):
        package = self.root / 'bundle'; package.mkdir()
        write_json(package / '.auriga-package.json', {'version': 1, 'source': str(self.root / 'original')})
        (package / 'asset.bin').write_bytes(b'ORIGINAL ASSET')
        write_json(package / 'run/mariadb/admin.json', {'must_stay_local': True})
        write_json(package / 'server-files-ready.json', {'status': 'SERVER_FILES_READY'})
        write_json(package / 'package-manifest.json', {'version': 1, 'files': {name: prepare_server.digest(path) for name, path in prepare_server.package_files(package)}})
        return package, self.root / 'bundle.zip'
    def test_archive_integrity_and_mutable_database_exclusion(self):
        package, archive = self.archive_fixture()
        checksum = prepare_server.create_archive(package, archive)
        with zipfile.ZipFile(archive) as check:
            self.assertIsNone(check.testzip())
            self.assertIn('bundle/asset.bin', check.namelist())
            self.assertFalse(any('/run/' in name or name.endswith('server-files-ready.json') for name in check.namelist()))
        self.assertEqual(checksum, prepare_server.digest(archive))
        self.assertTrue(archive.with_name('bundle.zip.sha256').read_text().startswith(checksum))
    def test_archive_does_not_replace_existing_output(self):
        package, archive = self.archive_fixture()
        archive.write_bytes(b'KEEP')
        with self.assertRaises(FileExistsError): prepare_server.create_archive(package, archive)
        self.assertEqual(archive.read_bytes(), b'KEEP')
    def test_archive_rejects_changed_or_missing_assets(self):
        package, archive = self.archive_fixture()
        asset = package / 'asset.bin'; asset.write_bytes(b'CHANGED')
        with self.assertRaises(PackageError): prepare_server.create_archive(package, archive)
        asset.unlink()
        with self.assertRaises(PackageError): prepare_server.create_archive(package, archive)
        self.assertFalse(archive.exists())
    def supervised_fixture(self, fail=None, game_bind=None):
        import socket
        probes = [socket.socket() for _ in range(5)]
        for probe in probes: probe.bind(('127.0.0.1',0))
        ports = [probe.getsockname()[1] for probe in probes]
        for probe in probes: probe.close()
        cfg = {'database': {'network': {'bind_port': ports[0]}}, 'login': {'port': ports[1]},
               'game_servers': {'ch1-core1': {'server': {'channel': 1}, 'network': {'port': ports[2], 'p2p_port': ports[3]}}}}
        if game_bind: cfg['game_servers']['ch1-core1']['network']['bind_ip'] = game_bind
        write_json(self.root / 'fixture.json', cfg)
        (self.root / 'bin').mkdir(exist_ok=True)
        for program in ('Database','GameServer','LoginServer'):
            (self.root / 'bin' / (program + ('.exe' if os.name == 'nt' else ''))).write_bytes(b'owned fake program '+program.encode())
        child = self.root / 'child.py'
        child.write_text('''import json,os,socket,sys,time
from pathlib import Path
name=sys.argv[1]; root=Path(sys.argv[2]); listeners=[]
with (root/'trace').open('a') as trace: trace.write('START '+name+'\\n')
if name==sys.argv[4]: raise SystemExit(3)
for host,port in ([] if sys.argv[4]=='stall:'+name else json.loads(sys.argv[3])):
 s=socket.socket(); s.bind((host,port)); s.listen(); listeners.append(s)
while True:
 if (root/'crash.request').is_file() and (root/'crash.request').read_text()==name: raise SystemExit(7)
 p=Path('.auriga-stop')
 if p.exists() and p.read_text().split()==[str(os.getpid()),os.environ['AURIGA_SHUTDOWN_TOKEN']]: break
 time.sleep(0.02)
with (root/'trace').open('a') as trace: trace.write('STOP '+name+'\\n')
''', encoding='utf-8')
        worker = self.root / 'worker.py'
        worker.write_text('''import json,sys,subprocess
from pathlib import Path
sys.path.insert(0,sys.argv[1])
from server_package import manage
manage.ROOT=Path(sys.argv[2]); cfg=json.loads((manage.ROOT/'fixture.json').read_text())
manage.file_check=lambda:cfg
manage.database_check=lambda:None
original=subprocess.Popen
def spawn(argv,**kwargs):
 name=argv[-1]
 # A Windows venv executable is a launcher with a separate child PID. Use
 # the real interpreter so the fixture has the native server's one-PID shape.
 runtime=getattr(sys,'_base_executable',sys.executable)
 return original([runtime,'-B',str(manage.ROOT/'child.py'),name,str(manage.ROOT),json.dumps(manage.instance_endpoints(cfg,name)),sys.argv[3]],**kwargs)
manage.subprocess.Popen=spawn
manage.supervise([1])
''', encoding='utf-8')
        log = (self.root / 'worker.log').open('wb')
        process = subprocess.Popen([sys.executable, '-B', str(worker), str(REPO / 'SRC/Tools'), str(self.root), fail or '-'], stdout=log, stderr=subprocess.STDOUT)
        return process, log
    def cleanup_fixture(self, process, log):
        state_path = self.root / 'run/supervisor.json'
        if state_path.exists():
            state = json.loads(state_path.read_text())
            for child in state['children']:
                if manage.owned(child['identity']):
                    (Path(child['directory']) / '.auriga-stop').write_text(str(child['identity']['pid'])+' '+child['token'])
        if process.poll() is None:
            try: process.wait(timeout=5)
            except subprocess.TimeoutExpired: process.terminate(); process.wait(timeout=5)
        log.close()
    def test_supervisor_start_and_ordered_graceful_stop(self):
        process, log = self.supervised_fixture()
        try:
            state = self.root / 'run/supervisor.json'
            deadline = time.monotonic() + 15
            while time.monotonic() < deadline:
                if process.poll() is not None: self.fail((self.root / 'worker.log').read_text())
                if state.exists() and json.loads(state.read_text())['phase'] == 'listening': break
                time.sleep(0.05)
            else: self.fail('Supervisor readiness deadline')
            manage.stop()
            self.assertEqual(process.wait(timeout=15), 0, (self.root / 'worker.log').read_text())
            trace = (self.root / 'trace').read_text().splitlines()
            self.assertEqual(trace, ['START database','START ch1-core1','START login','STOP login','STOP ch1-core1','STOP database'])
            self.assertFalse(state.exists())
            self.assertFalse(list((self.root/'run/crashes').glob('*/event.json')), 'Graceful stop must not become a crash')
        finally: self.cleanup_fixture(process, log)
    def test_supervisor_reaches_cores_on_their_bind_address(self):
        # A core bound to the VPS address does not answer on 127.0.0.1; readiness must probe
        # where server.json binds it (BSD, 2026-10-22: the stack stopped after 90 s).
        import socket
        with socket.socket() as probe:
            try: probe.bind(('127.0.0.2', 0))
            except OSError: self.skipTest('127.0.0.2 is not a local address here')
        cfg = {'database': {'network': {'bind_port': 1, 'bind_ip': '0.0.0.0'}}, 'login': {'port': 2},
               'game_defaults': {'network': {'bind_ip': '10.0.0.5'}},
               'game_servers': {'a': {'network': {'port': 3, 'p2p_port': 4, 'bind_ip': '203.0.113.7'}},
                                'b': {'network': {'port': 5, 'p2p_port': 6}}}}
        self.assertEqual(manage.instance_endpoints(cfg, 'database'), [('127.0.0.1', 1)])
        self.assertEqual(manage.instance_endpoints(cfg, 'login'), [('127.0.0.1', 2)])
        self.assertEqual(manage.instance_endpoints(cfg, 'a'), [('203.0.113.7', 3), ('203.0.113.7', 4)])
        self.assertEqual(manage.instance_endpoints(cfg, 'b'), [('10.0.0.5', 5), ('10.0.0.5', 6)])
        process, log = self.supervised_fixture(game_bind='127.0.0.2')
        try:
            state = self.root / 'run/supervisor.json'
            deadline = time.monotonic() + 15
            while time.monotonic() < deadline:
                if process.poll() is not None: self.fail((self.root / 'worker.log').read_text())
                if state.exists() and json.loads(state.read_text())['phase'] == 'listening': break
                time.sleep(0.05)
            else: self.fail('The core listening on 127.0.0.2 was never found ready')
            manage.stop()
            self.assertEqual(process.wait(timeout=15), 0, (self.root / 'worker.log').read_text())
        finally: self.cleanup_fixture(process, log)
    def test_supervisor_names_a_foreign_bind_address(self):
        cfg = {'database': {'network': {'bind_port': 1}}, 'login': {'port': 2},
               'game_servers': {'ch1-core1': {'network': {'port': 3, 'p2p_port': 4, 'bind_ip': '203.0.113.7'}}}}
        with self.assertRaisesRegex(RuntimeError, 'ch1-core1 cannot listen on 203.0.113.7:3'):
            for host, port in manage.instance_endpoints(cfg, 'ch1-core1'): manage.free_endpoint('ch1-core1', host, port)
    def test_supervisor_rolls_back_failed_startup(self):
        process, log = self.supervised_fixture(fail='ch1-core1')
        try:
            self.assertNotEqual(process.wait(timeout=15), 0)
            trace = (self.root / 'trace').read_text().splitlines()
            self.assertEqual(trace, ['START database','START ch1-core1','STOP database'])
            self.assertFalse((self.root / 'run/supervisor.json').exists())
            reports=list((self.root/'run/crashes').glob('*/event.json'))
            self.assertEqual(len(reports),1)
            event=json.loads(reports[0].read_text())
            self.assertEqual((event['instance'],event['exit_code'],event['phase']),('ch1-core1',3,'starting'))
        finally: self.cleanup_fixture(process, log)
    def test_supervisor_records_exit_after_readiness_and_stops_siblings(self):
        process, log = self.supervised_fixture()
        try:
            state = self.root / 'run/supervisor.json'
            deadline = time.monotonic() + 15
            while time.monotonic() < deadline:
                if process.poll() is not None: self.fail((self.root / 'worker.log').read_text())
                if state.exists() and json.loads(state.read_text())['phase'] == 'listening': break
                time.sleep(.05)
            else: self.fail('Supervisor readiness deadline')
            (self.root/'crash.request').write_text('ch1-core1')
            self.assertNotEqual(process.wait(timeout=15),0)
            reports=list((self.root/'run/crashes').glob('*/event.json'))
            self.assertEqual(len(reports),1)
            event=json.loads(reports[0].read_text())
            self.assertEqual((event['instance'],event['exit_code'],event['phase']),('ch1-core1',7,'listening'))
            self.assertTrue((self.root/event['binary']).is_file())
            self.assertEqual((self.root/'trace').read_text().splitlines()[-2:],['STOP login','STOP database'])
        finally: self.cleanup_fixture(process,log)
    def test_supervisor_can_cancel_startup_before_listener(self):
        process, log = self.supervised_fixture(fail='stall:ch1-core1')
        try:
            state = self.root / 'run/supervisor.json'
            deadline = time.monotonic() + 10
            while time.monotonic() < deadline:
                if process.poll() is not None: self.fail((self.root / 'worker.log').read_text())
                if state.exists() and len(json.loads(state.read_text())['children']) == 2: break
                time.sleep(0.05)
            else: self.fail('Did not reach the pending listener')
            manage.stop()
            self.assertEqual(process.wait(timeout=10), 0, (self.root / 'worker.log').read_text())
            self.assertEqual((self.root / 'trace').read_text().splitlines(), ['START database','START ch1-core1','STOP ch1-core1','STOP database'])
        finally: self.cleanup_fixture(process, log)
    def test_database_stop_rejects_running_stack(self):
        write_json(self.root / '.auriga-package.json', {'version': 1})
        write_json(self.root / 'server.json', {})
        write_json(self.root / 'run/supervisor.json', {'children': [{'identity': manage.identity(os.getpid())}]})
        with self.assertRaisesRegex(RuntimeError, 'Stop the owned server stack'):
            manage.stop_db()
    @unittest.skipUnless(CHECKER, 'native checker path not provided')
    def test_native_json_paths_and_direct_quest_sources(self):
        data = self.root / 'data'
        write_json(data / 'quest/questnpc.json', {'npcs': []})
        write_json(data / 'quest/questcategory.json', {'categories': []})
        write_json(data / 'fixture.json', {'value': 1})
        path = data / 'quest/library.lua'
        path.write_text('return true')
        (data / 'quest/fixture.quest').write_text('quest fixture begin state start begin when login begin return end end end')
        (data / 'quest/quest_list').write_text('fixture.quest\n')
        foreign = self.root / 'foreign'; foreign.mkdir()
        result = subprocess.run([str(CHECKER), str(data)], cwd=foreign, capture_output=True, text=True, timeout=10)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn('PACKAGE_DATA_OK json=3 lua=1', result.stdout)
        path.write_text('return return invalid')
        result = subprocess.run([str(CHECKER), str(data)], cwd=foreign, capture_output=True, text=True, timeout=10)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('LUA_INVALID', result.stderr)



class ExternalSqlTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='auriga-external-sql-')
        self.root = Path(self.temp.name).resolve()
        self.previous, manage.ROOT = manage.ROOT, self.root
        self.root.joinpath('.auriga-package.json').write_text('{}')
        self.cfg = json.loads((REPO / 'config/server.json.example').read_text())
        self.cfg['common']['table_postfix'] = '_realm'
        self.cfg['common']['sql']['player']['password'] = 'private"\\with\nline'
        (self.root / 'server.json').write_text(json.dumps(self.cfg))
        (self.root / 'db').mkdir()
        shutil = __import__('shutil')
        shutil.copy2(REPO / 'db/check_ready.sql', self.root / 'db/check_ready.sql')

    def tearDown(self):
        manage.ROOT = self.previous
        self.temp.cleanup()

    def test_external_sql_uses_each_role_private_options_and_read_only_queries(self):
        calls, temporary = [], []
        def client(argv, input, timeout):
            options = Path(argv[1].split('=', 1)[1]); temporary.append(options)
            content = options.read_text(encoding='utf-8')
            self.assertNotIn(self.cfg['common']['sql']['player']['password'], ' '.join(map(str, argv)))
            if os.name != 'nt': self.assertEqual(options.stat().st_mode & 0o777, 0o600)
            calls.append((content, input))
            count = input.count('SELECT ') - 1
            from types import SimpleNamespace
            return SimpleNamespace(returncode=0, stdout='1\n' + 'requirement\tREADY\n' * count, stderr='')
        with mock.patch.object(manage.shutil, 'which', return_value='/usr/local/bin/mariadb'), mock.patch.object(manage, 'native', side_effect=client):
            manage.external_database_check()
        self.assertEqual(len(calls), 4)
        player = next(query for content, query in calls if 'database="player"' in content)
        common = next(query for content, query in calls if 'database="common"' in content)
        self.assertIn('FROM `mob_proto_realm`', player)
        self.assertIn('FROM `shop`', player)
        self.assertNotIn('shop_realm', player)
        self.assertIn('FROM `locale`', common)
        self.assertNotIn('locale_realm', common)
        self.assertTrue(all(not path.exists() for path in temporary))
        self.assertFalse((self.root / 'run/mariadb').exists())

    def test_external_sql_errors_hide_credentials_and_remove_option_files(self):
        from types import SimpleNamespace
        options = []
        def failure(argv, **kwargs):
            options.append(Path(argv[1].split('=', 1)[1]))
            return SimpleNamespace(returncode=1, stdout='', stderr='SQL access denied')
        with mock.patch.object(manage.shutil, 'which', return_value='/usr/local/bin/mariadb'), mock.patch.object(manage, 'native', side_effect=failure):
            with self.assertRaisesRegex(RuntimeError, 'External SQL check failed') as error:
                manage.external_database_check()
        self.assertNotIn('private', str(error.exception))
        self.assertTrue(all(not path.exists() for path in options))
        self.assertTrue((self.root / 'run/sql-check-error.log').is_file())

    def test_readiness_script_cannot_perform_a_write(self):
        (self.root / 'db/check_ready.sql').write_text('DROP TABLE player.mob_proto; SELECT 1 FROM player.mob_proto;')
        with mock.patch.object(manage.shutil, 'which', return_value='/usr/local/bin/mariadb'), mock.patch.object(manage, 'native') as native:
            with self.assertRaisesRegex(RuntimeError, 'read-only'):
                manage.external_database_check()
        native.assert_not_called()

    def test_system_sql_branch_does_not_bootstrap_or_start_a_database(self):
        from types import SimpleNamespace
        import inspect
        namespace = dict(vars(manage), os=SimpleNamespace(name='posix'))
        # A copied Windows package must still use the FreeBSD system database.
        (self.root / 'run/mariadb').mkdir(parents=True)
        (self.root / 'run/mariadb/owner.json').write_text('{"binary_dir":"D:/old/mariadb"}')
        callback = mock.Mock()
        namespace['external_database_check'] = callback
        exec(inspect.getsource(manage.database_check), namespace)
        with mock.patch.object(manage, 'start_db') as start, mock.patch.object(manage, 'init_db') as init:
            namespace['database_check']()
        callback.assert_called_once(); start.assert_not_called(); init.assert_not_called()


if __name__ == '__main__':
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument('--checker', type=Path)
    args, remaining = parser.parse_known_args()
    CHECKER = args.checker.resolve() if args.checker else None
    if CHECKER: PackageToolsTests.test_native_json_paths_and_direct_quest_sources = PackageToolsTests.test_native_json_paths_and_direct_quest_sources.__wrapped__
    unittest.main(argv=[sys.argv[0]] + remaining)
