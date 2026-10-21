"""Remote build/upload regressions, including real local subprocesses and Qt flows."""
import base64
import copy
import io
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
import remote_jobs as jobs
import remote_worker as worker
import ssh_connection as ssh
from backend import EditorError


class WorkTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='auriga-remote-test-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()
        self.source = self.root / 'source space'
        self.source.mkdir()
        self.target = self.root / 'destination space'
        self.settings = dict(jobs.defaults(), source_dir='/home/a source', build_dir='/home/build')

    def snapshot(self, files=None, contents=True):
        snapshot = jobs.UploadSnapshot(files or [self.source], '/home/destination', contents=contents)
        self.addCleanup(snapshot.close)
        return snapshot

    def uploaded(self, snapshot):
        request = dict(snapshot.request, destination=str(self.target))
        worker.prepare(request)
        _, stage = worker.upload_stage(request)
        for row in request['files']:
            shutil.copy2(snapshot.root / row['key'], stage / 'files' / row['key'])
        return request, stage

    def build_request(self, configure=None, compile=None):
        (self.source / 'CMakeLists.txt').write_text('project(Test NONE)', encoding='utf-8')
        return dict(id='b' * 24, action='build', source=str(self.source), build=str(self.root / 'build'),
                    configure=configure or [sys.executable, '-c', 'print("configured")'],
                    compile=compile or [sys.executable, '-c', 'print("compiled")'])

    def test_build_plan_preserves_paths_and_extra_values_as_arguments(self):
        settings = dict(self.settings, configure_args='-DNAME="a b;$(id)" -DAURIGA_USE_VCPKG=OFF')
        request = jobs.build_request(settings)
        self.assertEqual(request['configure'][2], '/home/a source')
        self.assertIn('-DNAME=a b;$(id)', request['configure'])
        self.assertEqual(request['compile'][-4:], ['GameServer', 'Database', 'LoginServer', 'ServerPackageCheck'])
        self.assertNotIn('--install', request['compile'])


    def test_build_rejects_shell_and_path_mode_overrides(self):
        for extra in ('-S /tmp/evil', '--preset evil', '-P code.cmake', '-DTEST=1 ; rm -rf /'):
            with self.subTest(extra=extra), self.assertRaises(EditorError):
                jobs.build_request(dict(self.settings, configure_args=extra))
        for build in ('/', '../build', '/home/a source', '/home'):
            with self.subTest(build=build), self.assertRaises(EditorError):
                jobs.build_request(dict(self.settings, build_dir=build))

    def test_freebsd_build_path_includes_package_tools_and_base_compilers(self):
        with mock.patch.object(worker.sys,'platform','freebsd15'),mock.patch.dict(os.environ,{'PATH':'/custom/bin'},clear=True):
            environment=worker.build_environment()
            self.assertEqual(environment['PATH'].split(os.pathsep),['/custom/bin','/usr/local/bin','/usr/bin','/bin'])
            self.assertEqual(os.environ['PATH'],'/custom/bin')

    def test_missing_ninja_stops_before_cmake_with_freebsd_install_hint(self):
        request=self.build_request()
        request['configure']=['cmake','-S',str(self.source),'-B',request['build'],'-G','Ninja']
        find=lambda name,**kwargs:'/tools/cmake' if name=='cmake' else None
        with mock.patch.object(worker.sys,'platform','freebsd15'),mock.patch.object(worker.shutil,'which',side_effect=find),\
                mock.patch.object(worker,'run_command') as run,self.assertRaisesRegex(ValueError,'pkg install ninja'):
            worker.build_job(request)
        run.assert_not_called()
        folder=Path(request['build'])/'.auriga-manager/jobs'/request['id']
        self.assertEqual(json.loads((folder/'result.json').read_text())['status'],'failed')
        self.assertFalse((folder/'active').exists())

    def test_cmake_preflight_repairs_notfound_without_editing_cache_or_request(self):
        request=jobs.build_request(self.settings);build=self.root/'build';build.mkdir()
        cache=build/'CMakeCache.txt'
        original='CMAKE_GENERATOR:INTERNAL=Ninja\nCMAKE_MAKE_PROGRAM:FILEPATH=CMAKE_MAKE_PROGRAM-NOTFOUND\n'
        cache.write_text(original)
        before=copy.deepcopy(request);messages=[]
        find=lambda name,**kwargs:{'cmake':'/tools/cmake','ninja':'/tools with space/ninja'}.get(name)
        with mock.patch.object(worker.shutil,'which',side_effect=find):
            configure,compile_argv=worker.build_commands(request,build,{'PATH':'/tools'},messages.append)
        self.assertIn('-DCMAKE_MAKE_PROGRAM:FILEPATH=/tools with space/ninja',configure)
        self.assertEqual(configure[0],'/tools/cmake');self.assertEqual(compile_argv[0],'/tools/cmake')
        self.assertTrue(messages);self.assertEqual(request,before);self.assertEqual(cache.read_text(),original)

    def test_preflight_keeps_explicit_build_tool_and_rejects_missing_override(self):
        request=jobs.build_request(dict(self.settings,configure_args='-DCMAKE_MAKE_PROGRAM:FILEPATH=/custom/ninja'))
        find=lambda name,**kwargs:{'cmake':'/tools/cmake','/custom/ninja':'/custom/ninja'}.get(name)
        with mock.patch.object(worker.shutil,'which',side_effect=find):
            configure,_=worker.build_commands(request,self.root/'build',{},lambda _:None)
        self.assertEqual(sum('CMAKE_MAKE_PROGRAM' in arg for arg in configure),1)
        with mock.patch.object(worker.shutil,'which',side_effect=lambda name,**kwargs:'/tools/cmake' if name=='cmake' else None),\
                self.assertRaisesRegex(ValueError,'CMAKE_MAKE_PROGRAM'):
            worker.build_commands(request,self.root/'build',{},lambda _:None)

    def test_changing_cached_generator_requires_new_build_directory(self):
        request=jobs.build_request(self.settings);build=self.root/'build';build.mkdir()
        cache=build/'CMakeCache.txt';cache.write_text('CMAKE_GENERATOR:INTERNAL=Unix Makefiles\n')
        with self.assertRaisesRegex(ValueError,'új buildmappát'):
            worker.build_commands(request,build,{},lambda _:None)
        self.assertEqual(cache.read_text(),'CMAKE_GENERATOR:INTERNAL=Unix Makefiles\n')

    def test_old_profiles_gain_defaults_and_roundtrip_preserves_work(self):
        profile = ssh.new_profile()
        profile['mode'] = 'direct'
        profile['target'].update(host='example.invalid', user='auriga')
        legacy = copy.deepcopy(profile)
        del legacy['work']
        self.assertEqual(ssh.validate_profile(legacy)['work'], jobs.defaults())
        profile['work'] = self.settings
        store = ssh.ProfileStore(self.root / 'profiles.json')
        store.load()
        store.save([profile])
        self.assertEqual(store.load()[0]['work'], self.settings)

    def test_snapshot_excludes_and_bytes_survive_later_source_edits(self):
        (self.source / 'a [1].txt').write_bytes(b'original')
        (self.source / '.git').mkdir()
        (self.source / '.git/config').write_text('ignored')
        (self.source / 'empty').mkdir()
        snapshot = jobs.UploadSnapshot([self.source], '/home/destination', '.git', contents=True)
        self.addCleanup(snapshot.close)
        (self.source / 'a [1].txt').write_bytes(b'edited')
        self.assertEqual(snapshot.request['directories'], ['empty'])
        self.assertEqual(len(snapshot.request['files']), 1)
        self.assertEqual((snapshot.root / '00000000').read_bytes(), b'original')
        batch = snapshot.batch().read_text()
        self.assertNotIn('a [1].txt', batch)
        self.assertIn('00000000', batch)

    def test_same_named_selections_and_cancel_rejected(self):
        (self.source / 'file').write_bytes(b'x')
        with self.assertRaises(EditorError):
            self.snapshot([self.source, self.source], contents=False)
        with self.assertRaises(EditorError):
            jobs.UploadSnapshot([self.source], '/home/destination', cancelled=lambda: True)

    def test_upload_rejects_collision_before_any_original_changes(self):
        (self.source / 'first').write_bytes(b'new one')
        (self.source / 'second').write_bytes(b'new two')
        request, stage = self.uploaded(self.snapshot())
        (self.target / 'second').write_bytes(b'old two')
        with self.assertRaisesRegex(ValueError, 'felülírás'):
            worker.commit(request)
        self.assertFalse((self.target / 'first').exists())
        self.assertEqual((self.target / 'second').read_bytes(), b'old two')

    def test_checksum_failure_preserves_destination(self):
        (self.source / 'file').write_bytes(b'original')
        request, stage = self.uploaded(self.snapshot())
        (self.target / 'file').write_bytes(b'old')
        (stage / 'files/00000000').write_bytes(b'bad')
        with self.assertRaisesRegex(ValueError, 'ellenőrzőösszeg'):
            worker.commit(dict(request, overwrite=True))
        self.assertEqual((self.target / 'file').read_bytes(), b'old')

    def test_successful_upload_backup_empty_directories_and_unrelated_files(self):
        (self.source / 'nested').mkdir()
        (self.source / 'nested/file').write_bytes(b'new')
        (self.source / 'empty').mkdir()
        request, stage = self.uploaded(self.snapshot())
        (self.target / 'nested').mkdir()
        (self.target / 'nested/file').write_bytes(b'old')
        (self.target / 'unrelated').write_bytes(b'keep')
        worker.commit(dict(request, overwrite=True))
        self.assertEqual((self.target / 'nested/file').read_bytes(), b'new')
        self.assertEqual((stage / 'backup/00000000').read_bytes(), b'old')
        self.assertEqual((self.target / 'unrelated').read_bytes(), b'keep')
        self.assertTrue((self.target / 'empty').is_dir())

    def test_mid_install_failure_rolls_back_already_installed_files(self):
        for name in ('a', 'b'):
            (self.source / name).write_bytes(b'new')
        request, stage = self.uploaded(self.snapshot())
        (self.target / 'a').write_bytes(b'old')
        replace = os.replace
        def fail(source, destination):
            if Path(destination) == self.target / 'b':
                raise OSError('simulated disk failure')
            return replace(source, destination)
        with mock.patch.object(worker.os, 'replace', side_effect=fail), self.assertRaises(OSError):
            worker.commit(dict(request, overwrite=True))
        self.assertEqual((self.target / 'a').read_bytes(), b'old')
        self.assertFalse((self.target / 'b').exists())

    def test_remote_symlink_escape_and_relative_escape_rejected(self):
        with self.assertRaises(ValueError):
            worker.path_at(self.target, '../outside')
        outside = self.root / 'outside'
        outside.mkdir()
        self.target.mkdir()
        link = self.target / 'link'
        try:
            link.symlink_to(outside, target_is_directory=True)
        except OSError:
            self.skipTest('Creating symlinks requires Windows developer mode/privilege')
        with self.assertRaises(ValueError):
            worker.path_at(self.target, 'link/file')

    def test_real_subprocess_build_logs_and_failure_does_not_run_compile(self):
        request = self.build_request()
        worker.build_job(request)
        folder = Path(request['build']) / '.auriga-manager/jobs' / request['id']
        self.assertIn('compiled', (folder / 'build.log').read_text())
        self.assertEqual(json.loads((folder / 'result.json').read_text())['status'], 'success')
        self.assertFalse((folder / 'active').exists())
        request['id'] = 'c' * 24
        request['configure'] = [sys.executable, '-c', 'import sys; sys.exit(7)']
        with mock.patch.object(worker, 'run_command', wraps=worker.run_command) as run, self.assertRaises(RuntimeError):
            worker.build_job(request)
        self.assertEqual(run.call_count, 1)
        self.assertFalse((Path(request['build']) / '.auriga-manager/operation.lock').exists())

    def test_worker_stdin_entrypoint_runs_and_cancellation_stops_build(self):
        request = self.build_request(configure=[sys.executable, '-c', 'import time; print("started",flush=True); time.sleep(30)'])
        process = subprocess.Popen([sys.executable, '-u', '-'], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        process.stdin.write(jobs.worker_input(request))
        process.stdin.close()
        folder = Path(request['build']) / '.auriga-manager/jobs' / request['id']
        deadline = time.monotonic() + 8
        while not (folder / 'active').exists() and time.monotonic() < deadline:
            time.sleep(.03)
        worker.cancel_build(request)
        self.assertNotEqual(process.wait(timeout=8), 0)
        output = process.stdout.read().decode('utf-8', errors='replace')
        process.stdout.close()
        self.assertIn('megszakítva', output)
        self.assertEqual(json.loads((folder / 'result.json').read_text())['status'], 'cancelled')

    def test_real_cmake_configure_and_ninja_build_with_space_paths(self):
        cmake = shutil.which('cmake')
        ninja = shutil.which('ninja')
        if not ninja and os.name == 'nt':
            candidate = Path('C:/Program Files/Microsoft Visual Studio/18/Professional/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe')
            ninja = str(candidate) if candidate.is_file() else None
        if not cmake or not ninja:
            self.skipTest('CMake and Ninja are needed for the native build smoke test')
        request = self.build_request()
        (self.source / 'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.20)\nproject(RemoteSmoke NONE)\n'
            'add_custom_target(GameServer COMMAND "${CMAKE_COMMAND}" -E touch "${CMAKE_BINARY_DIR}/built.ok")\n', encoding='utf-8')
        request['configure'] = [cmake, '-S', str(self.source), '-B', request['build'], '-G', 'Ninja', '-DCMAKE_MAKE_PROGRAM=' + ninja]
        request['compile'] = [cmake, '--build', request['build'], '--config', 'Release', '--parallel', '2', '--target', 'GameServer']
        worker.build_job(request)
        self.assertTrue((Path(request['build']) / 'built.ok').is_file())




class GuiTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        from PySide6.QtWidgets import QApplication
        cls.application = QApplication.instance() or QApplication([])

    def setUp(self):
        from remote_page import RemotePage
        self.temp = tempfile.TemporaryDirectory(prefix='auriga-remote-gui-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.errors = []
        self.owner = type('Owner', (), {'error': lambda _, text: self.errors.append(text)})()
        self.page = RemotePage(self.owner, self.root / 'profiles.json')
        self.addCleanup(self.page.shutdown)
        self.addCleanup(self.page.close)
        profile = ssh.new_profile()
        profile['mode'] = 'direct'
        profile['known_hosts'] = str(self.root / 'known_hosts')
        Path(profile['known_hosts']).touch()
        profile['target'].update(host='example.invalid', user='auriga')
        profile['work'].update(source_dir='/home/source', build_dir='/home/build', upload_dir='/home/upload')
        self.page.load(profile)



    def wait(self, predicate):
        deadline = time.monotonic() + 8
        while not predicate() and time.monotonic() < deadline:
            self.application.processEvents()
            time.sleep(.01)
        self.assertTrue(predicate(), 'Qt operation did not finish')

    def test_build_async_success_failed_start_and_missing_success_marker(self):
        for command, success in (([sys.executable, '-c', 'print("AURIGA_BUILD_SUCCESS log=test")'], True),
                                 ([str(self.root / 'missing.exe')], False),
                                 ([sys.executable, '-c', 'print("no actual job")'], False)):
            with self.subTest(command=command), mock.patch.object(ssh.Session, 'ssh', return_value=(command[0], command[1:])):
                self.page.work.start_build('build')
                self.wait(lambda: not self.page.work.active())
                self.assertEqual('sikeresen' in self.page.work.status.text(), success)
                self.assertTrue(self.page.work.build_button.isEnabled())
        self.assertEqual(self.errors, [])

    def test_worker_error_is_visible_above_build_log(self):
        command=[sys.executable,'-c','import sys; print("AURIGA_JOB_FAILED Missing Ninja: pkg install ninja"); sys.exit(1)']
        with mock.patch.object(ssh.Session,'ssh',return_value=(command[0],command[1:])):
            self.page.work.start_build('build')
            self.wait(lambda:not self.page.work.active())
        self.assertEqual(self.page.work.status.text(),'Missing Ninja: pkg install ninja')

    def test_upload_background_snapshot_then_three_stage_qprocess_pipeline(self):
        source = self.root / 'source'
        source.mkdir()
        (source / 'file').write_bytes(b'upload')
        self.page.work.choose([str(source)])
        self.page.work.contents.setChecked(True)
        self.page.work.prepare()
        self.wait(lambda: self.page.work.planner is None)
        self.assertIsNotNone(self.page.work.snapshot)
        self.assertTrue(self.page.work.upload_button.isEnabled())
        programs = [(sys.executable, ['-c', 'print("AURIGA_UPLOAD_PREPARED /home/upload/stage")']),
                    (sys.executable, ['-c', 'print("AURIGA_UPLOAD_SUCCESS files=1 backup=test")'])]
        with mock.patch.object(ssh.Session, 'ssh', side_effect=programs) as commands, \
             mock.patch.object(ssh.Session, 'sftp', return_value=(sys.executable, ['-c', 'print("uploaded")'])):
            self.page.work.upload()
            self.wait(lambda: not self.page.work.active())
            self.assertEqual(commands.call_count, 2)
        self.assertIn('sikeresen', self.page.work.status.text())
        self.assertIsNone(self.page.work.snapshot)
        self.assertEqual(self.errors, [])

    def test_profile_change_invalidates_reviewed_upload_and_saves_build_fields(self):
        self.page.work.settings['parallel'].setValue(5)
        self.page.save()
        self.assertEqual(self.page.store.load()[0]['work']['parallel'], 5)
        source = self.root / 'file'
        source.write_bytes(b'x')
        self.page.work.snapshot = jobs.UploadSnapshot([source], '/home/upload')
        copied = self.page.work.snapshot.root
        self.page.work.settings['upload_dir'].setText('/home/another-target')
        self.assertIsNone(self.page.work.snapshot)
        self.assertFalse(copied.exists())
        self.assertFalse(self.page.work.upload_button.isEnabled())


if __name__ == '__main__':
    unittest.main()
