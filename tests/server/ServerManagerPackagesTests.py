"""FreeBSD package operations and the Qt/SSH request lifecycle."""
import base64
import contextlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import unittest
from unittest import mock

os.environ.setdefault('QT_QPA_PLATFORM', 'offscreen')
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT/'SRC/Tools/ServerManager'))
import remote_jobs as jobs
import remote_packages as pkg
import ssh_connection as ssh


class PackageTests(unittest.TestCase):
    def test_untrusted_names_and_options_never_enter_pkg_arguments(self):
        for name in ('-y', 'x;id', '$(id)', '../pkg', '/tmp/a.pkg', 'a*', 'a"b', 'x\ny', 'x\t-y'):
            with self.subTest(name=name), self.assertRaises(ValueError):
                pkg.validate(dict(operation='plan', packages=[name]))
        value = pkg.request('plan', packages='ninja cmake ninja libjpeg-turbo pkgconf')
        self.assertEqual(value['packages'], ['ninja', 'cmake', 'libjpeg-turbo', 'pkgconf'])
        with self.assertRaises(ValueError): pkg.request('install', packages='ninja')
        with self.assertRaises(ValueError): pkg.request('search', query='*')

    def test_embedded_worker_is_self_contained_and_rejects_non_freebsd(self):
        result = subprocess.run([sys.executable, '-B', '-'], input=jobs.worker_input(pkg.request('installed')),
                                capture_output=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('FreeBSD célgépen', result.stdout.decode('utf-8'))
        self.assertNotIn(pkg.RESULT.encode(), result.stdout)

    def test_no_pkg_bootstrap_or_platform_writes_on_failed_preflight(self):
        with mock.patch.object(pkg.platform, 'system', return_value='FreeBSD'), \
             mock.patch.object(pkg.Path, 'is_file', return_value=False), \
             mock.patch.object(pkg, 'mutation_log') as lock:
            with self.assertRaisesRegex(ValueError, 'bootstrap'):
                pkg.execute(pkg.request('update'), lambda _: None)
            lock.assert_not_called()

    def test_list_and_search_are_read_only_literal_argv_and_bounded(self):
        for operation in ('search', 'installed'):
            with self.subTest(operation=operation), mock.patch.object(pkg, 'Pkg') as program:
                program.return_value.run.return_value = ''.join('ninja\t1.13\tBuild system\n' for _ in range(501))
                out = []
                pkg.execute(pkg.request(operation, query='ninja'), out.append)
                result = json.loads(out[-1][len(pkg.RESULT):])
                self.assertEqual((result['total'], len(result['rows'])), (501, 500))
                args = program.return_value.run.call_args.args[0]
                self.assertNotIn('install', args)
                self.assertNotIn('update', args)
                if operation == 'search':
                    self.assertIn('-U', args)
                    self.assertEqual(args[-1], '*ninja*')

    def test_plan_fingerprint_detects_both_installed_state_and_resolver_changes(self):
        program = object.__new__(pkg.Pkg)
        with mock.patch.object(program, 'run', side_effect=['ninja\t1.0\t0\t0', 'Install ninja 1.1']):
            first = program.plan(['ninja'])
        for state, plan in [('ninja\t1.1\t0\t0', 'Install ninja 1.1'),
                            ('ninja\t1.0\t0\t0', 'Install ninja 1.2')]:
            with mock.patch.object(program, 'run', side_effect=[state, plan]):
                self.assertNotEqual(program.plan(['ninja'])['fingerprint'], first['fingerprint'])

    def test_stale_plan_refused_before_install_and_success_requires_installed_packages(self):
        request = pkg.request('install', packages='ninja cmake', fingerprint='a'*64)
        @contextlib.contextmanager
        def lock(*_): yield lambda _: None
        with mock.patch.object(pkg, 'mutation_log', side_effect=lock), mock.patch.object(pkg, 'Pkg') as program:
            program.return_value.plan.return_value = dict(fingerprint='b'*64)
            with self.assertRaisesRegex(ValueError, 'megváltozott'): pkg.execute(request, lambda _: None)
            program.return_value.run.assert_not_called()
            program.return_value.plan.return_value = dict(fingerprint='a'*64)
            out = []
            pkg.execute(request, out.append)
            self.assertEqual(program.return_value.run.call_args_list[0].args[0],
                             ['install', '-U', '-y', '--', 'ninja', 'cmake'])
            self.assertEqual(program.return_value.run.call_args_list[-1].args[0], ['info', '-e', 'cmake'])
            self.assertTrue(out[-1].startswith(pkg.RESULT))
            program.return_value.run.side_effect = ValueError('installation failed')
            out.clear()
            with self.assertRaises(ValueError): pkg.execute(request, out.append)
            self.assertFalse(any(line.startswith(pkg.RESULT) for line in out))

    def test_non_root_install_does_not_create_lock_or_write_system_files(self):
        with mock.patch.object(pkg.os, 'geteuid', return_value=1001, create=True), \
             mock.patch.object(pkg.Path, 'mkdir') as mkdir:
            with self.assertRaisesRegex(ValueError, 'root'), pkg.mutation_log(pkg.request('update'), print):
                self.fail('unreachable')
            mkdir.assert_not_called()

    def test_command_subprocess_streaming_and_nonzero_error(self):
        program = object.__new__(pkg.Pkg)
        program.program = sys.executable
        program.environment = dict(os.environ)
        out = []; program.report = out.append
        actual = subprocess.Popen
        def spawn(argv, **kwargs):
            return actual([sys.executable, '-u', '-c',
                'import sys; print("real streamed output"); sys.exit(' + ('7' if argv[-1] == 'fail' else '0') + ')'], **kwargs)
        with mock.patch.object(pkg.subprocess, 'Popen', side_effect=spawn) as command:
            self.assertIn('real streamed output', program.run(['query', '-a', '%n']))
            self.assertEqual(command.call_args.args[0][1:4], ['-o', 'REPO_AUTOUPDATE=false', 'query'])
            with self.assertRaisesRegex(ValueError, '7'): program.run(['fail'])
        self.assertIn('real streamed output', out)

    def test_pkg_dry_run_exit_one_requires_complete_summary_and_no_stderr(self):
        program = object.__new__(pkg.Pkg)
        program.program = sys.executable
        program.environment = dict(os.environ)
        program.report = lambda _: None
        actual = subprocess.Popen
        summary = ('The following 1 package(s) will be affected (of 0 checked):\n'
                   'New packages to be INSTALLED:\n ninja: 1.13\nNumber of packages to be installed: 1\n')
        for output, error, accepted in ((summary, '', True), ('failure', '', False), (summary, 'pkg: error', False)):
            def spawn(argv, **kwargs):
                return actual([sys.executable, '-u', '-c', 'import sys;print('+repr(output)+');'
                    'sys.stderr.write('+repr(error)+');sys.exit(1)'], **kwargs)
            with self.subTest(accepted=accepted, error=error), mock.patch.object(pkg.subprocess, 'Popen', side_effect=spawn):
                if accepted:
                    self.assertIn('ninja', program.run(['install','-U','-n','--','ninja'], dry_run=True))
                else:
                    with self.assertRaises(ValueError):
                        program.run(['install','-U','-n','--','ninja'], dry_run=True)

    def test_transaction_logs_survive_broken_client_and_lock_rejects_overlap(self):
        with tempfile.TemporaryDirectory(prefix='auriga-pkg-transaction-') as directory:
            root = Path(directory)
            def path(value):
                if value == '/var/run/auriga-manager-pkg.lock': return root/'lock'
                if value == '/var/log/auriga-manager/packages': return root/'logs'
                raise AssertionError('Unexpected system write: '+str(value))
            request = pkg.request('update')
            def disconnected(_): raise BrokenPipeError('SSH disconnected')
            with mock.patch.object(pkg, 'Path', side_effect=path), \
                 mock.patch.object(pkg.os, 'geteuid', return_value=0, create=True):
                with pkg.mutation_log(request, disconnected) as report:
                    report('Transaction continued')
                    with self.assertRaisesRegex(ValueError, 'Már fut'), pkg.mutation_log(pkg.request('update'), print):
                        self.fail('overlapping transaction')
                    self.assertTrue((root/'lock/owner.json').is_file())
                self.assertFalse((root/'lock').exists())
                log = (root/'logs'/(request['id']+'.log')).read_text(encoding='utf-8')
                self.assertIn('Transaction continued', log)
                failed = pkg.request('update')
                with self.assertRaisesRegex(ValueError, 'test failure'):
                    with pkg.mutation_log(failed, lambda _: None): raise ValueError('test failure')
                self.assertFalse((root/'lock').exists())
                self.assertIn('SIKERTELEN: test failure', (root/'logs'/(failed['id']+'.log')).read_text(encoding='utf-8'))


class PackageGuiTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        from PySide6.QtWidgets import QApplication
        cls.app = QApplication.instance() or QApplication([])

    def setUp(self):
        from remote_page import RemotePage
        self.temp = tempfile.TemporaryDirectory(prefix='auriga-pkg-ui-')
        self.addCleanup(self.temp.cleanup)
        self.errors = []
        owner = type('Owner', (), {'error': lambda _, message: self.errors.append(message)})()
        self.page = RemotePage(owner, Path(self.temp.name)/'profiles.json')
        self.addCleanup(self.page.close)
        self.addCleanup(self.page.shutdown)
        profile = ssh.new_profile()
        profile['mode'] = 'direct'
        profile['target'].update(host='example.invalid', user='root')
        profile['known_hosts'] = str(Path(self.temp.name)/'known_hosts')
        Path(profile['known_hosts']).touch()
        self.page.load(profile)
        self.packages = self.page.work.packages
        self.page.controls()

    def wait(self):
        end = time.monotonic() + 8
        while self.page.work.active() and time.monotonic() < end:
            self.app.processEvents();time.sleep(.01)
        self.assertFalse(self.page.work.active())

    @staticmethod
    def worker(response):
        # A real subprocess consumes the serialized production request and returns its own id.
        source = ('import sys,json,base64,re; data=sys.stdin.read(); '
                  'encoded=re.search(r"    main\\(([^\\n]+)\\)",data).group(1); '
                  'request=json.loads(base64.b64decode(__import__("ast").literal_eval(encoded))); '
                  'result=' + repr(response) + '; result.update(id=request["id"],operation=request["operation"]); '
                  'print("pkg output"); print("AURIGA_PACKAGES_RESULT "+json.dumps(result))')
        return sys.executable, ['-u', '-c', source]

    def test_async_search_add_selection_plan_and_install(self):
        self.packages.search.setText('ninja')
        self.assertFalse(self.page.dirty, 'Transient package fields must not dirty the SSH profile')
        with mock.patch.object(ssh.Session, 'ssh', return_value=self.worker(dict(rows=[['ninja','1.13','Build']],total=1))):
            self.packages.start('search');self.wait()
        self.assertEqual(self.packages.table.rowCount(), 1)
        self.assertNotIn('AURIGA_PACKAGES_RESULT', self.page.work.log.toPlainText())
        self.packages.table.selectRow(0);self.packages.add_selected()
        self.assertEqual(self.packages.names.text(), 'ninja')
        self.assertFalse(self.packages.install_button.isEnabled())
        plan = dict(packages=['ninja'], fingerprint='a'*64, plan='Install ninja 1.13\nDownload 1 MB')
        with mock.patch.object(ssh.Session, 'ssh', return_value=self.worker(plan)):
            self.packages.start('plan');self.wait()
        self.assertTrue(self.packages.install_button.isEnabled())
        self.assertIn('Download 1 MB', self.packages.preview.toPlainText())
        with mock.patch.object(ssh.Session, 'ssh', return_value=self.worker({})):
            self.packages.start('install')
            self.assertFalse(self.page.work.cancel_button.isEnabled())
            self.page.work.cancel();self.assertFalse(self.page.work.cancelling)
            self.wait()
        self.assertIn('sikeresen', self.page.work.status.text())
        self.assertFalse(self.packages.install_button.isEnabled())
        self.assertEqual(self.errors, [])

    def test_profile_and_name_changes_invalidate_plan_and_local_mode_blocks_actions(self):
        self.packages.plan = dict(fingerprint='a'*64)
        self.packages.names.setText('ninja');self.assertIsNone(self.packages.plan)
        self.packages.plan = dict(fingerprint='a'*64)
        self.page.target['host'].setText('other.invalid');self.assertIsNone(self.packages.plan)
        self.page.owner.is_remote = lambda: False
        self.page.controls()
        self.assertFalse(self.packages.search_button.isEnabled())
        self.packages.start('installed');self.assertTrue(self.errors)

    def test_missing_corrupted_and_wrong_id_results_never_enable_install(self):
        for text in ('pkg output', pkg.RESULT+'{bad', pkg.RESULT+json.dumps(dict(id='other',operation='plan'))):
            with mock.patch.object(ssh.Session, 'ssh', return_value=(sys.executable,['-c','print('+repr(text)+')'])):
                self.packages.names.setText('ninja')
                self.packages.start('plan');self.wait()
                self.assertFalse(self.packages.install_button.isEnabled())
                self.assertNotIn('elkészült', self.page.work.status.text())

    def test_small_window_scrolls_content_and_keeps_install_button_outside(self):
        self.page.resize(1100, 650)
        self.page.tabs.setCurrentIndex(4)
        self.page.show();self.app.processEvents()
        self.assertGreater(self.packages.scroll.verticalScrollBar().maximum(), 0)
        self.assertFalse(self.packages.scroll.widget().isAncestorOf(self.packages.install_button))
        self.assertGreaterEqual(self.packages.table.height(), 150)
        self.assertGreaterEqual(self.packages.preview.height(), 150)


if __name__ == '__main__':
    unittest.main()
