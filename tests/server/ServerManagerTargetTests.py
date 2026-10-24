"""Real private RPC and Qt target-isolation tests, with SSH replaced by local pipes."""
import argparse
import base64
import copy
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time
import unittest
from unittest import mock

os.environ.setdefault('QT_QPA_PLATFORM', 'offscreen')
REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / 'SRC/Tools/ServerManager'))
import backend as api
import server_targets as targets
import ssh_connection as ssh
CHECKER = REPO / 'build/SRC/Server/GameServer/RelWithDebInfo/ServerPackageCheck.exe'
BIN = CHECKER.parent
PYTHON = getattr(sys, '_base_executable', sys.executable)


def fixture(root, name, channel=1):
    root.mkdir()
    (root / '.auriga-package.json').write_text('{}')
    cfg = json.loads((REPO / 'config/server.json.example').read_text())
    key = next(k for k, v in cfg['game_servers'].items() if v['server']['channel'] != 99)
    core = cfg['game_servers'].pop(key)
    core['server']['channel'] = channel
    core['working_directory'] = 'run/' + name
    cfg['game_servers'][name] = core
    cfg['common']['sql']['player']['password'] = 'PRIVATE_' + name
    (root / 'server.json').write_text(json.dumps(cfg), encoding='utf-8')
    (root / 'data/quest').mkdir(parents=True)
    (root / 'data/example.json').write_text('{"value": 1}', encoding='utf-8')
    (root / 'data/quest/example.lua').write_bytes(b'-- \xe1\xe9\r\nreturn 1\r\n')
    (root / 'data/quest/questnpc.json').write_text('{"npcs":[]}', encoding='utf-8')
    (root / 'data/quest/questcategory.json').write_text('{"categories":[]}', encoding='utf-8')
    (root / 'data/quest/quest_list').write_text('', encoding='utf-8')
    (root / 'preflight.log').write_text('PRIVATE_' + name + ' known password', encoding='utf-8')
    source = (REPO / 'SRC/Tools/server_package/manage.py').read_text(encoding='utf-8')
    # Only service control is a double. Reading, process identity, locks, native
    # validation, conflict detection and file transactions remain production.
    at = source.index("if __name__ == '__main__':")
    source = source[:at] + r'''
def main():
    action = sys.argv[1]
    with (ROOT/'actions.trace').open('a') as output: output.write(' '.join(sys.argv[1:])+'\n')
    if action == 'start':
        failure = ROOT/'test-start-failure'
        if failure.is_file():
            time.sleep(float(failure.read_text()))
            print('PACKAGE_DATA_INVALID Quest/login/levelup.lua: duplicate quest id: levelup', flush=True)
            print('SERVER_MANAGER_ERROR: Native data/Lua check failed '+config()['common']['sql']['player']['password'], file=sys.stderr, flush=True)
            return 1
        (ROOT/'run').mkdir(exist_ok=True)
        save_json(ROOT/'run/supervisor.json',dict(supervisor=identity(os.getpid()),token='test',children=[],phase='listening'))
        print('STACK_LISTENING; fixture', flush=True)
        while not (ROOT/'run/fake-stop').exists(): time.sleep(.03)
        (ROOT/'run/supervisor.json').unlink(missing_ok=True)
    elif action == 'stop':
        (ROOT/'run').mkdir(exist_ok=True)
        (ROOT/'run/fake-stop').touch()
    print('TEST_ACTION_OK '+action)
    return 0
if __name__ == '__main__':
    raise SystemExit(main())
'''
    (root / 'manage.py').write_text(source, encoding='utf-8')
    (root / 'bin').mkdir()
    for binary in ('GameServer.exe', 'Database.exe', 'LoginServer.exe', 'ServerPackageCheck.exe', 'libcrypto-3-x64.dll', 'libssl-3-x64.dll'):
        component = 'Database' if binary == 'Database.exe' else 'LoginServer' if binary == 'LoginServer.exe' else 'GameServer'
        original = REPO / 'build/SRC/Server' / component / 'RelWithDebInfo' / binary
        if original.is_file(): shutil.copy2(original, root / 'bin' / binary)
    return cfg


class RpcTests(unittest.TestCase):
    def setUp(self):
        if not CHECKER.is_file(): self.skipTest('Build ServerPackageCheck first')
        self.temp = tempfile.TemporaryDirectory(prefix='auriga-target-rpc-')
        self.base = Path(self.temp.name).resolve()
        self.root = self.base / 'remote'
        fixture(self.root, 'remote-core')

    def tearDown(self):
        if not hasattr(self, 'base'): return
        (self.root / 'run').mkdir(exist_ok=True)
        (self.root / 'run/fake-stop').touch()
        deadline = time.monotonic() + 4
        state = self.root / 'run/supervisor.json'
        manager = api.load_manager(self.root)
        records = [json.loads(path.read_text())['identity'] for path in (self.root / 'run/manager-jobs').glob('remote-start-*.json')]
        while (state.exists() or any(manager.owned(record) for record in records)) and time.monotonic() < deadline: time.sleep(.03)
        self.temp.cleanup()

    def rpc(self, operation, arguments=None):
        request = dict(schema_version=1, id='a' * 24, target='b' * 24, root=str(self.root),
                       operation=operation, arguments=arguments or {})
        result = subprocess.run([PYTHON, '-B', '-u', '-'], input=targets.worker_input(request), capture_output=True, timeout=20,
            creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)
        lines = [line[21:] for line in result.stdout.decode().splitlines() if line.startswith('AURIGA_TARGET_RESULT ')]
        self.assertEqual(len(lines), 1, result.stderr.decode(errors='replace'))
        return json.loads(lines[0])

    def test_remote_inventory_document_encoding_and_redacted_logs(self):
        packet = self.rpc('connect')['result']
        self.assertIn('data/example.json', packet['inventory']['data'])
        self.assertIn('data/quest/example.lua', packet['inventory']['quests'])
        document = targets.document(self.rpc('read', dict(relative='data/quest/example.lua'))['result'])
        self.assertEqual((document.encoding, document.newline), ('latin1', '\r\n'))
        text = self.rpc('log', dict(relative='preflight.log'))['result']['text']
        self.assertNotIn('PRIVATE_remote-core', text)

    def test_remote_save_backup_restore_and_external_change_rejection(self):
        original = self.rpc('read', dict(relative='data/example.json'))['result']
        args = dict(relative=original['relative'], expected_hash=original['sha256'], data=base64.b64encode(b'{"value":2}').decode())
        result = self.rpc('save', args)
        self.assertTrue(result['ok'], result)
        self.assertEqual((self.root / 'data/example.json').read_bytes(), b'{"value":2}')
        backups = self.rpc('backups', dict(relative=args['relative']))['result']
        restored = self.rpc('restore', dict(relative=args['relative'], backup=backups[0]))['result']
        self.assertEqual(base64.b64decode(restored['data']), b'{"value": 1}')
        self.assertFalse(self.rpc('save', args)['ok'])
        self.assertEqual((self.root / 'data/example.json').read_bytes(), b'{"value":2}')

    def test_path_escape_and_private_log_reads_are_rejected(self):
        for operation, args in [('read', dict(relative='../outside.json')), ('log', dict(relative='server.json')),
                                ('restore', dict(relative='data/example.json', backup='server.json')),
                                ('action', dict(argv=['start', '--config', '/another/server.json'])),
                                ('action', dict(argv=['anything']))]:
            with self.subTest(operation=operation): self.assertFalse(self.rpc(operation, args)['ok'])

    def test_old_remote_manager_cannot_report_unverified_processes_as_stopped(self):
        path = self.root / 'manage.py'
        path.write_text(path.read_text(encoding='utf-8').replace('PACKAGE_CONTROL_VERSION = 2', 'PACKAGE_CONTROL_VERSION = 1'), encoding='utf-8')
        result = self.rpc('connect')
        self.assertFalse(result['ok'])
        self.assertIn('frissítést igényel', result['error'])

    def test_core_control_is_validated_and_routed(self):
        self.assertTrue(self.rpc('status')['result']['state']['core_control'])
        result = self.rpc('action', dict(argv=['start-cores', '--instance', 'remote-core', '--channel', '2']))
        self.assertTrue(result['ok'], result)
        self.assertTrue(self.rpc('action', dict(argv=['stop-cores', '--instance', 'remote-core']))['ok'])
        self.assertEqual((self.root / 'actions.trace').read_text().splitlines(),
                         ['start-cores --instance remote-core --channel 2', 'stop-cores --instance remote-core'])
        for argv in (['stop-cores'], ['start-cores', '--instance', '../escape'], ['start-cores', '--channel', 'x'],
                     ['stop-cores', '--instance'], ['start-cores', '--config', '/another/server.json']):
            result = self.rpc('action', dict(argv=argv))
            self.assertFalse(result['ok'], argv)
            self.assertIn('Hibás core- vagy csatornakiválasztás', result['error'])
        # A package whose manage.py predates core control says so instead of failing obscurely.
        path = self.root / 'manage.py'
        path.write_text(path.read_text(encoding='utf-8').replace('def control_cores(', 'def _older_manager('), encoding='utf-8')
        self.assertFalse(self.rpc('status')['result']['state']['core_control'])
        result = self.rpc('action', dict(argv=['start-cores', '--instance', 'remote-core']))
        self.assertFalse(result['ok'])
        self.assertIn('friss manage.py', result['error'])

    def test_detached_remote_start_survives_request_exit_and_stop_is_routed(self):
        result = self.rpc('action', dict(argv=['start', '--channel', '1']))
        self.assertTrue(result['ok'], result)
        self.assertTrue(result['result']['detached'])
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            state = self.rpc('status')['result']['state']
            if state['running']: break
            time.sleep(.05)
        self.assertTrue(state['running'])
        self.assertTrue(self.rpc('action', dict(argv=['stop']))['ok'])
        self.assertIn('stop', (self.root / 'actions.trace').read_text())
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            state = self.rpc('status')['result']['state']
            if state['start_job']['status'] == 'stopped': break
        self.assertEqual(state['start_job']['status'], 'stopped')
        self.assertNotEqual(state['phase'], 'Indítás sikertelen')

    def test_failed_detached_preflight_survives_reconnect_with_redacted_diagnostics(self):
        (self.root/'test-start-failure').write_text('0.7')
        result = self.rpc('action', dict(argv=['start','--channel','1']))
        self.assertTrue(result['ok'], result)
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            state = self.rpc('connect')['result']['state']
            if state['start_job']['status'] == 'failed': break
            time.sleep(.05)
        self.assertFalse(state['running']);self.assertFalse(state['starting'])
        self.assertEqual(state['phase'], 'Indítás sikertelen')
        self.assertIn('duplicate quest id: levelup', state['start_job']['output'])
        self.assertNotIn('PRIVATE_remote-core', state['start_job']['output'])

    def test_immediate_start_failure_keeps_journal_and_rejects_foreign_log_path(self):
        (self.root/'test-start-failure').write_text('0')
        result = self.rpc('action', dict(argv=['start']))
        # Process startup can itself exceed the 100 ms acknowledgement window.
        # Either response must leave a durable, readable failure once it exits.
        if result['ok']: self.assertTrue(result['result']['detached'])
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            state = self.rpc('status')['result']['state']
            if state['start_job']['status'] == 'failed': break
            time.sleep(.05)
        self.assertEqual(state['start_job']['status'], 'failed')
        self.assertIn('duplicate quest id', state['start_job']['output'])
        self.assertNotIn('PRIVATE_', state['start_job']['output'])
        journal = next((self.root/'run/manager-jobs').glob('remote-start-*.json'))
        record = json.loads(journal.read_text());record['log'] = 'server.json'
        journal.write_text(json.dumps(record))
        self.assertIsNone(self.rpc('status')['result']['state']['start_job'])

    def test_unfinished_binary_installation_blocks_remote_start(self):
        path = self.root / 'run/deployments' / ('d'*24) / 'result.json'
        path.parent.mkdir(parents=True)
        path.write_text(json.dumps(dict(status='installing')))
        state = self.rpc('status')['result']['state']
        self.assertFalse(state['can_start']);self.assertIn('Befejezetlen', state['phase'])
        result = self.rpc('action', dict(argv=['start']))
        self.assertFalse(result['ok']);self.assertIn('Befejezetlen', result['error'])
        self.assertFalse((self.root / 'actions.trace').exists())


class FakeSession:
    calls = []
    slow = False
    fail = False
    def __init__(self, profile, **kwargs):
        self.profile = copy.deepcopy(profile)
        self.directory = tempfile.TemporaryDirectory(prefix='auriga-target-ssh-')
    def close(self): self.directory.cleanup()
    def ssh(self, command):
        self.calls.append((self.profile['id'], command))
        if self.fail: return PYTHON, ['-c', "import sys; print('Host key verification failed.',file=sys.stderr);sys.exit(255)"]
        if self.slow: return PYTHON, ['-B', '-u', '-c', "import sys,time; source=sys.stdin.read();time.sleep(1);exec(compile(source,'<test-ssh>','exec'))"]
        return PYTHON, ['-B', '-u', '-']


class TargetGuiTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not CHECKER.is_file(): raise unittest.SkipTest('Build native validators first')
        from PySide6.QtCore import QSettings
        from PySide6.QtWidgets import QApplication
        import window
        cls.app = QApplication.instance() or QApplication([])
        cls.ui = window
        cls.settings = tempfile.TemporaryDirectory(prefix='auriga-target-settings-')
        QSettings.setDefaultFormat(QSettings.Format.IniFormat)
        QSettings.setPath(QSettings.Format.IniFormat, QSettings.Scope.UserScope, cls.settings.name)

    @classmethod
    def tearDownClass(cls): cls.settings.cleanup()

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='auriga-target-gui-')
        self.base = Path(self.temp.name).resolve()
        self.local, self.remote = self.base / 'local', self.base / 'remote'
        fixture(self.local, 'local-core', 1);fixture(self.remote, 'remote-core', 2)
        self.patch = mock.patch.object(ssh, 'Session', FakeSession);self.patch.start()
        FakeSession.calls=[];FakeSession.fail=False;FakeSession.slow=False
        self.window = self.ui.MainWindow(self.local)
        self.errors=[];self.window.error=self.errors.append
        self.profile=ssh.new_profile()
        self.profile.update(name='Teszt távoli', remote_root=str(self.remote))
        self.profile['target'].update(host='remote.test',user='auriga')
        self.window.remote_page.profiles=[self.profile]
        self.window.profiles_changed()
        self.window.show();self.app.processEvents()

    def tearDown(self):
        for editor in self.window.editors: editor.dirty=False
        self.window.remote_page.dirty=False
        self.window.close();self.window.deleteLater();self.app.processEvents()
        self.patch.stop();FakeSession.fail=FakeSession.slow=False
        self.temp.cleanup()

    def wait(self, predicate):
        from PySide6.QtTest import QTest
        deadline=time.monotonic()+15
        while not predicate() and time.monotonic()<deadline: QTest.qWait(20)
        self.assertTrue(predicate(), str(self.errors))

    def connect(self):
        self.assertTrue(self.window.set_remote(self.profile))
        self.wait(lambda:self.window.remote_online and not self.window.busy())

    def test_local_open_never_contacts_remote_and_switch_changes_all_pages(self):
        self.assertEqual(FakeSession.calls, [])
        self.assertIn('local-core', self.window.config_editor.document.value['game_servers'])
        self.connect()
        self.assertIn('remote-core', self.window.config_editor.document.value['game_servers'])
        self.assertNotIn('local-core', self.window.config_editor.document.value['game_servers'])
        self.assertIn('Távoli szerver', self.window.location.text())
        self.assertEqual({b.property('channel') for b in self.window.channel_boxes}, {2})
        self.assertTrue(self.window.set_root(self.local))
        self.assertIsNone(self.window.remote_client)
        self.assertIn('local-core', self.window.config_editor.document.value['game_servers'])
        self.assertEqual({b.property('channel') for b in self.window.channel_boxes}, {1})

    def test_external_sql_status_shows_real_connection_result_and_hides_local_sql_controls(self):
        self.connect()
        state = self.window.remote_state
        state.update(database_kind='external',database_initialized=False,database_running=False,can_start=False)
        row = dict(host='127.0.0.1',port=3307,database='player',status='unavailable',message='Az SQL-végpont nem érhető el.')
        state['database_status'] = dict(status='unavailable',connections=[row],checked_at=0)
        self.window.refresh()
        self.assertEqual(self.window.db_metric.text(),'SQL: nem elérhető')
        self.assertIn('3307',self.window.db_metric.toolTip());self.assertIn('3307',self.window.target_hint.text())
        self.assertTrue(self.window.database_tools_panel.isHidden());self.assertTrue(self.window.stop_db_button.isHidden())
        self.assertTrue(self.window.db_check_button.isEnabled());self.assertFalse(self.window.start_button.isEnabled())
        row.update(status='available',message='Kapcsolat rendben.')
        state['database_status']['status']='available';state.update(database_running=True,can_start=True)
        self.window.refresh();self.assertEqual(self.window.db_metric.text(),'SQL: elérhető')
        self.assertTrue(self.window.start_button.isEnabled());self.assertTrue(self.window.remote_online)

    def test_failed_start_is_visible_once_with_reason_and_clears_after_success(self):
        self.connect()
        state = copy.deepcopy(self.window.remote_state)
        state.update(running=False,starting=False,phase='Indítás sikertelen',start_job=dict(
            id='failure-1',status='failed',log='run/manager-jobs/remote-start-1.log',
            output='PACKAGE_DATA_INVALID Quest/login/levelup.lua: duplicate quest id: levelup'))
        self.window.console.clear()
        self.window.receive_remote_state(state);self.window.receive_remote_state(state)
        self.window.refresh()
        self.assertEqual(self.window.console.toPlainText().count('duplicate quest id: levelup'),1)
        self.assertIn('sikertelen',self.window.stack_metric.text())
        self.assertIn('duplicate quest id',self.window.stack_metric.toolTip())
        self.assertIn('remote-start-1.log',self.window.target_hint.text())
        self.assertTrue(self.window.console_toggle.isChecked())
        self.assertTrue(self.window.remote_online)
        state.update(phase='Fut',running=True)
        state['start_job']['status']='running'
        self.window.receive_remote_state(state);self.window.refresh()
        self.assertEqual(self.window.stack_metric.text(),'Fut')
        self.assertEqual(self.window.stack_metric.toolTip(),'')

    def test_remote_form_save_changes_only_remote_file_and_creates_remote_backup(self):
        self.connect();editor=self.window.config_editor
        local=(self.local/'server.json').read_bytes()
        value=copy.deepcopy(editor.document.value)
        value['game_servers']['remote-core']['server']['max_level']=161
        editor.config_form.set_value(value);editor.changed();editor.save()
        self.wait(lambda:not self.window.busy())
        self.assertEqual(self.errors, [])
        self.assertEqual(json.loads((self.remote/'server.json').read_text())['game_servers']['remote-core']['server']['max_level'],161)
        self.assertEqual((self.local/'server.json').read_bytes(),local)
        self.assertTrue(api.backup_files(self.remote,'server.json'))
        self.assertFalse(api.backup_files(self.local,'server.json'))
        self.assertNotIn('PRIVATE_',self.window.console.toPlainText())

    def test_remote_read_and_action_never_fall_back_to_local(self):
        self.connect()
        self.window.data_page.editor.load('data/example.json')
        self.wait(lambda:not self.window.busy())
        self.assertEqual(self.window.data_page.editor.document.value,dict(value=1))
        self.window.job(['check']);self.wait(lambda:not self.window.busy())
        self.assertEqual((self.remote/'actions.trace').read_text().splitlines(),['check'])
        self.assertFalse((self.local/'actions.trace').exists())

    def test_password_retention_change_preserves_active_server_editor(self):
        self.connect();editor=self.window.config_editor;document=editor.document
        editor.changed()
        changed=copy.deepcopy(self.profile);changed['target']['remember_password']=True
        self.window.remote_page.profiles=[changed]
        with mock.patch.object(editor,'may_discard',side_effect=AssertionError('No target change is needed')):
            self.window.profiles_changed()
        self.assertIs(editor.document,document)
        self.assertTrue(editor.dirty)
        self.assertTrue(self.window.remote_online)
        self.assertTrue(self.window.remote_client.profile['target']['remember_password'])

    def test_saving_active_password_route_never_connects_or_checks_key_files(self):
        self.profile['mode']='direct'
        self.connect();page=self.window.remote_page
        previous=copy.deepcopy(self.profile);previous['remote_root']='/usr/home/auriga/server'
        page.store=ssh.ProfileStore(self.base/'isolated-profiles.json');page.store.load();page.store.save([previous])
        page.profiles=[previous];page.remote_root.setText(previous['remote_root'])
        count=len(FakeSession.calls)
        page.target['auth'].setCurrentIndex(page.target['auth'].findData('password'))
        page.target['password'].setText('test-password')
        page.target['port'].setValue(self.profile['target']['port']+1)
        page.target['key_file'].setText('inactive-invalid-private-key')
        page.save()
        self.assertEqual(self.errors,[])
        self.assertEqual(len(FakeSession.calls),count)
        self.assertTrue(self.window.is_remote())
        self.assertFalse(self.window.remote_online)
        self.assertIsNone(self.window.config_editor.document)
        self.assertEqual(page.store.load()[0]['target']['auth'],'password')
        self.assertIsNone(self.window.remote_client.process)

    def test_failed_remote_native_validation_preserves_source_and_connection(self):
        self.connect();editor=self.window.quest_page.editor
        editor.load('data/quest/questnpc.json');self.wait(lambda:not self.window.busy())
        original=(self.remote/'data/quest/questnpc.json').read_bytes()
        editor.fields.set_value(dict(npcs=[dict(vnum='bad',name='npc')]))
        editor.changed();editor.save();self.wait(lambda:not self.window.busy())
        self.assertTrue(self.errors)
        self.assertTrue(self.window.remote_online)
        self.assertTrue(editor.dirty)
        self.assertEqual((self.remote/'data/quest/questnpc.json').read_bytes(),original)

    def test_failed_connection_clears_local_status_and_disables_remote_actions(self):
        FakeSession.fail=True
        self.window.set_remote(self.profile)
        self.wait(lambda:self.window.remote_client.process is None)
        self.assertFalse(self.window.remote_online)
        self.assertFalse(self.window.start_button.isEnabled())
        self.assertEqual(self.window.core_table.rowCount(),0)
        self.assertIsNone(self.window.config_editor.document)
        self.assertEqual(self.window.target_badge.text(),'TÁVOLI')
        self.assertIn('Host key',self.errors[0])

    def test_remote_write_blocks_target_switch_until_acknowledged(self):
        self.connect();editor=self.window.data_page.editor
        editor.load('data/example.json');self.wait(lambda:not self.window.busy())
        editor.fields.set_value(dict(value=2));editor.changed()
        FakeSession.slow=True
        editor.save()
        self.assertTrue(self.window.remote_client.mutating)
        self.assertFalse(self.window.set_root(self.local))
        self.assertTrue(self.window.is_remote())
        self.wait(lambda:not self.window.busy())
        self.assertEqual(json.loads((self.remote/'data/example.json').read_text()),dict(value=2))
        self.assertEqual(json.loads((self.local/'data/example.json').read_text()),dict(value=1))

    def test_reply_for_another_target_is_rejected(self):
        response=dict(schema_version=1,id='0'*24,target='0'*24,root=str(self.remote),ok=True,result={})
        command=(PYTHON,['-c',"print("+repr('AURIGA_TARGET_RESULT '+json.dumps(response))+")"])
        with mock.patch.object(FakeSession,'ssh',return_value=command):
            self.window.set_remote(self.profile)
            self.wait(lambda:self.window.remote_client.process is None)
        self.assertFalse(self.window.remote_online)
        self.assertIsNone(self.window.config_editor.document)
        self.assertIn('másik szerverhez',self.errors[0])

    def test_active_profile_deletion_cannot_hide_remote_target_with_unsaved_work(self):
        self.connect();editor=self.window.config_editor
        editor.changed()
        from PySide6.QtWidgets import QMessageBox
        with mock.patch.object(QMessageBox,'question',return_value=QMessageBox.StandardButton.Yes), \
                mock.patch.object(editor,'may_discard',return_value=False), \
                mock.patch.object(self.window.remote_page.store,'save') as save:
            self.window.remote_page.remove()
        save.assert_not_called()
        self.assertTrue(self.window.is_remote())
        self.assertEqual(self.window.target_choice.currentData(),'remote:'+self.profile['id'])
        # Reloading an externally deleted profile must preserve the actual
        # target label and editor, while blocking further remote operations.
        self.window.remote_page.profiles=[]
        with mock.patch.object(editor,'may_discard',return_value=False):
            self.window.profiles_changed()
        self.assertFalse(self.window.remote_online)
        self.assertTrue(editor.dirty)
        self.assertEqual(self.window.target_choice.currentData(),'remote:'+self.profile['id'])
        self.window.reconnect_target()
        self.assertFalse(self.window.remote_online)
        self.assertIn('nincs a mentett profilok',self.errors[-1])

    def test_unsaved_edit_blocks_switch_and_late_reply_cannot_replace_local_data(self):
        self.connect();editor=self.window.config_editor
        editor.changed()
        with mock.patch.object(editor,'may_discard',return_value=False):
            self.assertFalse(self.window.set_root(self.local))
        self.assertTrue(self.window.is_remote())
        editor.dirty=False
        self.window.set_root(self.local)
        FakeSession.slow=True
        self.window.set_remote(self.profile)
        self.assertTrue(self.window.set_root(self.local))
        self.wait(lambda:not self.window.busy())
        self.assertIn('local-core',self.window.config_editor.document.value['game_servers'])
        self.assertFalse(self.window.is_remote())


if __name__ == '__main__':
    unittest.main()
