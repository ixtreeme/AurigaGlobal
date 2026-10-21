"""SSH profile/config tests and optional Qt process tests; no remote hosts used."""
import argparse
import copy
import json
import os
from pathlib import Path
import shlex
import socket
import subprocess
import sys
import tempfile
import time
import unittest
from unittest import mock

os.environ.setdefault('QT_QPA_PLATFORM', 'offscreen')
REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / 'SRC/Tools/ServerManager'))
from backend import EditorError
import ssh_connection as ssh

GUI = False
ANSWER = 'AURIGA_OS=FreeBSD\nAURIGA_RELEASE=15.1-RELEASE\nAURIGA_USER=auriga\nAURIGA_PYTHON=/usr/local/bin/python3\nAURIGA_DIRECTORY=yes\nAURIGA_PACKAGE=yes\n'


def example(folder):
    profile = ssh.new_profile()
    profile['known_hosts'] = str(folder / 'known hosts')
    Path(profile['known_hosts']).touch()
    for role, host in (('target', 'freebsd.internal'), ('jump', 'jump.example.org')):
        profile[role].update(host=host, user='auriga', key_file=str(folder / (role + ' key')))
        Path(profile[role]['key_file']).touch()
    profile['jump']['port'] = 2222
    profile['remote_root'] = '/usr/home/auriga/server'
    profile['forwards'] = [dict(local_port=13307, remote_host='127.0.0.1', remote_port=3306)]
    return profile


class SshTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='auriga-ssh-tests-')
        self.addCleanup(self.temp.cleanup)
        self.folder = Path(self.temp.name).resolve()
        self.profile = example(self.folder)

    def session(self, profile=None):
        session = ssh.Session(profile or self.profile)
        self.addCleanup(session.close)
        return session

    def test_real_openssh_parses_both_hops_with_separate_keys(self):
        session = self.session()
        for alias, endpoint in ((session.alias, self.profile['target']),
                                ('jump-' + self.profile['id'], self.profile['jump'])):
            result = subprocess.run([ssh.openssh('ssh'), '-G', '-F', str(session.config), alias],
                                    capture_output=True, text=True, timeout=10)
            self.assertEqual(result.returncode, 0, result.stderr)
            values = dict(line.split(' ', 1) for line in result.stdout.splitlines() if ' ' in line)
            self.assertEqual(values['hostname'], endpoint['host'])
            self.assertEqual(values['user'], endpoint['user'])
            self.assertEqual(values['port'], str(endpoint['port']))
            self.assertEqual(values['stricthostkeychecking'], 'true')
            self.assertEqual(values['batchmode'], 'yes')
            self.assertEqual(values['forwardagent'], 'no')
            self.assertEqual(values['identityfile'].strip('"'), Path(endpoint['key_file']).as_posix())
            self.assertEqual(values['userknownhostsfile'].strip('"'), Path(self.profile['known_hosts']).as_posix())
            if alias == session.alias:
                self.assertEqual(values['proxyjump'], 'jump-' + self.profile['id'])
        self.assertNotIn('StrictHostKeyChecking no', session.config.read_text())

    def test_probe_and_sftp_use_same_isolated_route(self):
        session = self.session()
        _, probe = session.probe()
        _, transfer = session.sftp(self.folder / 'batch file')
        self.assertEqual(probe[:2], transfer[:2])
        self.assertEqual(probe[-2], transfer[-1])
        self.assertIn('ClearAllForwardings=yes', probe)
        shell = shlex.split(probe[-1])
        self.assertEqual(shell[:2], ['/bin/sh', '-c'])
        self.assertNotIn('manage.py start', shell[2])
        self.assertNotIn('mkdir', shell[2])

    def test_tunnel_only_binds_loopback_and_preserves_ipv6_destination(self):
        self.profile['forwards'][0]['remote_host'] = '::1'
        _, argv = self.session().tunnel()
        self.assertIn('127.0.0.1:13307:[::1]:3306', argv)
        self.assertIn('ExitOnForwardFailure=yes', argv)
        self.assertIn('-N', argv)
        self.assertNotIn('-f', argv)
        self.assertNotIn('ClearAllForwardings=yes', argv)

    def test_no_direct_fallback_and_no_passwords_in_saved_profile(self):
        store = ssh.ProfileStore(self.folder / 'profiles.json')
        self.assertEqual(store.load(), [])
        store.save([self.profile])
        saved = store.load()[0]
        self.assertEqual(saved['mode'], 'proxyjump')
        self.assertNotIn('password', saved['target'])
        self.assertFalse(saved['target']['remember_password'])
        self.assertEqual(saved['target']['key_file'], self.profile['target']['key_file'])

    def test_rejects_ssh_argument_and_config_injection(self):
        cases = [('host', '-oProxyCommand=calc'), ('host', 'host\nProxyCommand calc'),
                 ('host', '$(calc)'), ('host', 'fe80::1%zone;calc'),
                 ('host', 'fe80::1%eth0'), ('user', 'user@host'), ('user', 'user;id'),
                 ('host_key_alias', 'bad\nHost *'), ('key_file', 'relative_key'),
                 ('key_file', str(self.folder / '%h-key'))]
        for key, value in cases:
            with self.subTest(key=key, value=value):
                profile = copy.deepcopy(self.profile)
                profile['target'][key] = value
                with self.assertRaises(EditorError):
                    ssh.validate_profile(profile)

    def test_remote_path_is_quoted_through_both_shell_layers(self):
        root = "/usr/home/a user's data/$(touch bad); x"
        self.profile['remote_root'] = root
        _, argv = self.session().probe()
        script = shlex.split(argv[-1])[2]
        lexer = shlex.shlex(script, posix=True, punctuation_chars=True)
        lexer.whitespace_split = True
        words = list(lexer)
        self.assertIn(root, words)
        self.assertIn(root + '/server.json', words)
        self.assertNotIn('$(touch', words)

    def test_profile_validation_rejects_invalid_and_repeated_ports(self):
        for port in (0, 65536, '22', True):
            with self.subTest(port=port):
                profile = copy.deepcopy(self.profile)
                profile['target']['port'] = port
                with self.assertRaises(EditorError):
                    ssh.validate_profile(profile)
        self.profile['forwards'] *= 2
        with self.assertRaises(EditorError):
            ssh.validate_profile(self.profile)

    def test_missing_known_hosts_and_key_fail_before_any_process(self):
        Path(self.profile['known_hosts']).unlink()
        with self.assertRaises(EditorError):
            ssh.Session(self.profile)
        self.assertFalse(list(self.folder.glob('auriga-ssh-*')))

    def test_direct_existing_tunnel_uses_explicit_host_key_alias(self):
        self.profile['mode'] = 'direct'
        self.profile['target'].update(host='127.0.0.1', port=2222, host_key_alias='freebsd.internal')
        config = ssh.config_text(self.profile)
        self.assertNotIn('ProxyJump', config)
        self.assertIn('HostKeyAlias freebsd.internal', config)

    def test_real_direct_ssh_uses_target_key_port_and_needs_no_jump(self):
        self.profile['mode'] = 'direct'
        self.profile['target']['port'] = 22220
        self.profile['jump'].update(host='', user='', key_file=str(self.folder / 'unused-missing-key'))
        session = self.session()
        result = subprocess.run([ssh.openssh('ssh'), '-G', '-F', str(session.config), session.alias],
                                capture_output=True, text=True, timeout=10)
        self.assertEqual(result.returncode, 0, result.stderr)
        values = dict(line.split(' ', 1) for line in result.stdout.splitlines() if ' ' in line)
        self.assertEqual(values['hostname'], self.profile['target']['host'])
        self.assertEqual(values['port'], '22220')
        self.assertEqual(values['identityfile'].strip('"'), Path(self.profile['target']['key_file']).as_posix())
        self.assertIn(values.get('proxyjump', 'none'), ('none', ''))
        self.assertNotIn('jump-' + self.profile['id'], session.config.read_text())

    def test_concurrent_edits_rejected_without_losing_other_profile(self):
        first = ssh.ProfileStore(self.folder / 'profiles.json')
        second = ssh.ProfileStore(first.path)
        first.load()
        second.load()
        first.save([self.profile])
        with self.assertRaises(EditorError):
            second.save([])
        self.assertEqual(first.load(), [self.profile])

    def test_strict_profile_json_and_duplicate_ids(self):
        store = ssh.ProfileStore(self.folder / 'profiles.json')
        store.path.write_text('{"version":1,"version":1,"profiles":[]}', encoding='utf-8')
        with self.assertRaises(EditorError):
            store.load()
        with self.assertRaises(EditorError):
            store.save([self.profile, self.profile])
        original = store.path.read_bytes()
        with self.assertRaises(EditorError):
            store.save([self.profile])
        self.assertEqual(store.path.read_bytes(), original)

    def test_profile_lock_is_exclusive_and_can_be_reused(self):
        store = ssh.ProfileStore(self.folder / 'profiles.json')
        store.load()
        with store.lock():
            with self.assertRaises(EditorError):
                store.save([self.profile])
        store.save([self.profile])
        self.assertEqual(store.load(), [self.profile])

    def test_probe_parser_requires_complete_unambiguous_response(self):
        self.assertEqual(ssh.probe_result('login banner\n' + ANSWER)['OS'], 'FreeBSD')
        for text in (ANSWER.replace('AURIGA_OS=FreeBSD\n', ''), ANSWER + 'AURIGA_USER=root\n'):
            with self.assertRaises(EditorError):
                ssh.probe_result(text)

    def test_session_cleanup_removes_config_without_copying_keys(self):
        session = self.session()
        config = session.config
        self.assertEqual([p.name for p in config.parent.iterdir()], ['config'])
        session.close()
        self.assertFalse(config.parent.exists())
        self.assertTrue(Path(self.profile['target']['key_file']).exists())


class SshGuiTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not GUI:
            raise unittest.SkipTest('Pass --gui using the manager Qt environment')
        from PySide6.QtWidgets import QApplication, QWidget
        from remote_page import RemotePage
        cls.app = QApplication.instance() or QApplication([])
        cls.Widget, cls.RemotePage = QWidget, RemotePage

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='auriga-ssh-gui-tests-')
        self.folder = Path(self.temp.name).resolve()
        self.parent = self.Widget()
        self.errors = []
        self.parent.error = self.errors.append
        self.page = self.RemotePage(self.parent, self.folder / 'settings/profiles.json')
        self.profile = example(self.folder)
        self.page.load(self.profile)
        self.page.resize(1080, 780)
        self.page.show()
        self.app.processEvents()

    def tearDown(self):
        self.page.shutdown()
        self.page.close()
        self.parent.close()
        self.app.processEvents()
        self.temp.cleanup()

    def wait(self, predicate):
        deadline = time.monotonic() + 8
        while not predicate() and time.monotonic() < deadline:
            self.app.processEvents()
            time.sleep(.01)
        self.assertTrue(predicate(), self.page.output.toPlainText())

    def test_opening_and_saving_does_not_connect_or_create_keys(self):
        with mock.patch.object(self.page, 'launch') as launch:
            self.assertFalse(self.page.active())
            self.page.save()
            launch.assert_not_called()
        self.assertEqual(self.errors, [])
        self.assertEqual(self.page.store.load(), [self.profile])

    def test_proxyjump_mode_and_different_identity_fields_survive_roundtrip(self):
        self.assertTrue(self.page.jump['host'].isEnabled())
        self.assertEqual(self.page.current(), self.profile)
        self.page.mode.setCurrentIndex(1)
        self.assertFalse(self.page.jump['host'].isEnabled())
        self.page.mode.setCurrentIndex(0)
        self.assertEqual(self.page.current(), self.profile)

    def test_direct_mode_hides_jump_and_saves_without_jump_credentials(self):
        self.profile['mode'] = 'direct'
        self.profile['jump'].update(host='', user='', key_file='')
        self.page.load(self.profile)
        self.app.processEvents()
        self.assertFalse(self.page.jump['widgets'][0].isVisible())
        self.assertTrue(self.page.target['widgets'][0].isVisible())
        self.assertIn('Közvetlenül', self.page.status.text())
        self.page.save()
        self.assertEqual(self.errors, [])
        self.assertEqual(self.page.store.load(), [self.profile])

    def test_real_async_probe_process_and_session_cleanup(self):
        with mock.patch.object(ssh.Session, 'probe', return_value=(sys.executable, ['-c', 'print(' + repr(ANSWER) + ')'])):
            self.page.check()
        path = self.page.probe_session.config
        self.assertFalse(self.page.tabs.isEnabled())
        self.wait(lambda: self.page.probe_process is None)
        self.assertIn('FreeBSD 15.1-RELEASE', self.page.output.toPlainText())
        self.assertFalse(path.exists())
        self.assertTrue(self.page.tabs.isEnabled())

    def test_untrusted_host_error_is_shown_without_retry_or_fallback(self):
        command = (sys.executable, ['-c', "import sys; print('Host key verification failed.'); sys.exit(255)"])
        with mock.patch.object(ssh.Session, 'probe', return_value=command), mock.patch.object(self.page, 'launch', wraps=self.page.launch) as launch:
            self.page.check()
            self.wait(lambda: self.page.probe_process is None)
            self.assertEqual(launch.call_count, 1)
        self.assertIn('Host key verification failed', self.page.output.toPlainText())
        self.assertEqual(self.page.mode.currentData(), 'proxyjump')

    def test_remote_reload_uses_profile_root_and_preserves_async_controls(self):
        self.page.remote_root.setText('/usr/home/auriga/server with space')
        command = (sys.executable, ['-c', "print('QUEST_RELOAD_OK revision')"])
        with mock.patch.object(ssh.Session, 'ssh', return_value=command) as launch:
            self.page.reload_quests()
            self.assertEqual(shlex.split(launch.call_args[0][0]),
                ['python3', '-B', '/usr/home/auriga/server with space/manage.py', 'reload-quests'])
            self.assertFalse(self.page.quest_reload_button.isEnabled())
            self.wait(lambda: self.page.probe_process is None)
        self.assertIn('frissítése kész', self.page.output.toPlainText())
        self.assertTrue(self.page.quest_reload_button.isEnabled())
        self.assertEqual(self.errors, [])

    def test_frozen_binary_failed_start_recovers_controls(self):
        with mock.patch.object(ssh.Session, 'probe', return_value=(str(self.folder / 'missing.exe'), [])):
            self.page.check()
        self.wait(lambda: self.page.probe_process is None)
        self.assertTrue(self.page.check_button.isEnabled())
        self.assertIn('OpenSSH kliens nem indítható', self.page.output.toPlainText())

    def test_probe_timeout_kills_only_owned_process(self):
        with mock.patch.object(ssh.Session, 'probe', return_value=(sys.executable, ['-c', 'import time; time.sleep(30)'])):
            self.page.check()
        self.page.probe_timeout()
        self.wait(lambda: self.page.probe_process is None)
        self.assertIn('45 másodperces', self.page.output.toPlainText())

    def test_busy_local_port_is_rejected_before_launch(self):
        with socket.socket() as listener:
            listener.bind(('127.0.0.1', 0))
            listener.listen()
            self.page.forward_table.item(0, 0).setText(str(listener.getsockname()[1]))
            with mock.patch.object(self.page, 'launch') as launch:
                self.page.start_tunnel()
                launch.assert_not_called()
        self.assertEqual(len(self.errors), 1)
        self.assertFalse(self.page.active())

    def test_loopback_forward_process_stop_and_config_cleanup(self):
        with socket.socket() as probe:
            probe.bind(('127.0.0.1', 0))
            port = probe.getsockname()[1]
        self.page.forward_table.item(0, 0).setText(str(port))
        script = ('import socket,time; s=socket.socket(); s.bind(("127.0.0.1",%d)); '
                  's.listen(4); time.sleep(30)' % port)
        with mock.patch.object(ssh.Session, 'tunnel', return_value=(sys.executable, ['-c', script])):
            self.page.start_tunnel()
        path = self.page.tunnel_session.config
        self.wait(lambda: 'figyelnek' in self.page.status.text())
        self.assertFalse(self.page.tabs.isTabEnabled(0))
        self.assertFalse(self.page.tabs.isTabEnabled(1))
        self.assertTrue(self.page.tabs.isTabEnabled(2))
        self.page.stop_tunnel()
        self.wait(lambda: self.page.tunnel_process is None)
        self.assertFalse(path.exists())
        self.assertTrue(self.page.tabs.isEnabled())
        self.assertIn('bontottad', self.page.status.text())


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--gui', action='store_true')
    args, remaining = parser.parse_known_args()
    GUI = args.gui
    unittest.main(argv=[sys.argv[0], *remaining])
