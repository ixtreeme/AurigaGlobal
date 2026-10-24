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
from types import SimpleNamespace
import unittest
from unittest import mock

os.environ.setdefault('QT_QPA_PLATFORM', 'offscreen')
REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / 'SRC/Tools/ServerManager'))
import remote_jobs as jobs
import remote_worker as worker
import remote_deploy as deploy
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

    def compared(self, snapshot):
        with mock.patch.object(worker, 'root_path', return_value=self.target):
            result = worker.upload_comparison(snapshot.request)
        snapshot.apply_comparison(result)
        return result

    def delta_uploaded(self, snapshot, delete=False):
        request = dict(snapshot.upload_request(overwrite=True, delete=delete), destination=str(self.target))
        worker.prepare(request)
        _, stage = worker.upload_stage(request)
        for row in request['files']:
            shutil.copy2(snapshot.root / row['key'], stage / 'files' / row['key'])
        return request, stage

    def test_comparison_is_read_only_and_uses_content_instead_of_timestamps(self):
        (self.source / 'same').write_bytes(b'same')
        (self.source / 'changed').write_bytes(b'new!')
        (self.source / 'new').write_bytes(b'new')
        self.target.mkdir()
        (self.target / 'same').write_bytes(b'same')
        (self.target / 'changed').write_bytes(b'old!')
        (self.target / 'removed').write_bytes(b'remote only')
        os.utime(self.target / 'same', (100, 100))
        stamp = (self.source / 'changed').stat().st_mtime
        os.utime(self.target / 'changed', (stamp, stamp))
        snapshot = self.snapshot()
        unchanged_key = next(row['key'] for row in snapshot.request['files'] if row['path'] == 'same')
        self.compared(snapshot)
        self.assertEqual({row['path']: row['change'] for row in snapshot.changes},
            {'changed': 'modified', 'new': 'new', 'removed': 'deleted'})
        self.assertEqual(snapshot.unchanged, 1)
        self.assertFalse((self.target / '.auriga-manager').exists())
        self.assertEqual({row['path'] for row in snapshot.request['files']}, {'changed', 'new'})
        self.assertNotIn('/' + unchanged_key + '"', snapshot.batch().read_text())

    def test_comparison_missing_destination_does_not_create_it_and_empty_local_can_list_deletions(self):
        snapshot = self.snapshot()
        self.compared(snapshot)
        self.assertFalse(self.target.exists())
        self.assertEqual(snapshot.changes, [])
        self.target.mkdir(); (self.target / 'removed').write_bytes(b'old')
        empty = self.snapshot(); self.compared(empty)
        self.assertEqual([(row['path'], row['change']) for row in empty.changes], [('removed', 'deleted')])
        with self.assertRaises(EditorError): empty.upload_request(delete=False)
        self.assertEqual(len(empty.upload_request(delete=True)['deletions']), 1)

    def test_scopes_excludes_metadata_and_unselected_siblings_are_preserved(self):
        (self.source / 'same').write_bytes(b'same')
        scoped = self.target / self.source.name
        scoped.mkdir(parents=True)
        (scoped / 'same').write_bytes(b'same'); (scoped / 'removed').write_bytes(b'old')
        (scoped / '.git').mkdir(); (scoped / '.git/config').write_bytes(b'excluded')
        (scoped / '.auriga-manager').mkdir(); (scoped / '.auriga-manager/saved').write_bytes(b'private')
        (self.target / 'unrelated').write_bytes(b'keep')
        snapshot = jobs.UploadSnapshot([self.source], '/home/destination', excludes='.git', contents=False)
        self.addCleanup(snapshot.close); self.compared(snapshot)
        self.assertEqual([row['path'] for row in snapshot.request['deletions']], [self.source.name + '/removed'])
        request, stage = self.delta_uploaded(snapshot, delete=True); worker.commit(request)
        self.assertEqual((stage / 'backup/deleted-00000000').read_bytes(), b'old')
        for path in (scoped / '.git/config', scoped / '.auriga-manager/saved', self.target / 'unrelated'):
            self.assertTrue(path.is_file())
        single = self.snapshot([self.source / 'same'], contents=False); self.compared(single)
        self.assertFalse(single.request['deletions'])

    def test_delta_upload_skips_unchanged_and_requires_explicit_delete_and_overwrite(self):
        for name, value in [('same', b'same'), ('modified', b'new'), ('new', b'added')]:
            (self.source / name).write_bytes(value)
        self.target.mkdir()
        for name, value in [('same', b'same'), ('modified', b'old'), ('removed', b'keep unless opted in')]:
            (self.target / name).write_bytes(value)
        snapshot = self.snapshot(); self.compared(snapshot)
        with self.assertRaisesRegex(EditorError, 'felülírás'): snapshot.upload_request()
        request, stage = self.delta_uploaded(snapshot)
        worker.commit(request)
        self.assertEqual((self.target / 'modified').read_bytes(), b'new')
        self.assertEqual((self.target / 'new').read_bytes(), b'added')
        self.assertEqual((self.target / 'removed').read_bytes(), b'keep unless opted in')
        self.assertEqual(len(list((stage / 'backup').iterdir())), 1)
        self.assertEqual(json.loads((stage / 'result.json').read_text())['files'], 2)

    def test_changed_remote_file_new_collision_and_delete_race_are_refused(self):
        for kind in ('modified', 'new', 'deleted'):
            with self.subTest(kind=kind), tempfile.TemporaryDirectory(dir=self.root) as folder:
                self.target = Path(folder)
                for path in self.source.iterdir(): path.unlink()
                if kind != 'deleted': (self.source / 'file').write_bytes(b'new')
                if kind != 'new': (self.target / 'file').write_bytes(b'old')
                snapshot = self.snapshot(); self.compared(snapshot)
                request, stage = self.delta_uploaded(snapshot, delete=True)
                (self.target / 'file').write_bytes(b'concurrent edit')
                with self.assertRaisesRegex(ValueError, 'összehasonlítás óta'): worker.commit(request)
                self.assertEqual((self.target / 'file').read_bytes(), b'concurrent edit')

    def test_delete_failure_rolls_back_uploads_and_prior_deletions_and_retains_backups(self):
        (self.source / 'modified').write_bytes(b'new')
        self.target.mkdir()
        for name in ('modified', 'removed-a', 'removed-b'): (self.target / name).write_bytes(name.encode())
        snapshot = self.snapshot(); self.compared(snapshot)
        request, stage = self.delta_uploaded(snapshot, delete=True)
        original = os.replace
        def fail(source, destination):
            if Path(source) == self.target / 'removed-b': raise OSError('delete failure')
            return original(source, destination)
        with mock.patch.object(worker.os, 'replace', side_effect=fail), self.assertRaisesRegex(OSError, 'delete failure'):
            worker.commit(request)
        for name in ('modified', 'removed-a', 'removed-b'):
            self.assertEqual((self.target / name).read_bytes(), name.encode())
        self.assertEqual(json.loads((stage / 'result.json').read_text())['status'], 'rolled-back')
        self.assertEqual((stage / 'backup/deleted-00000000').read_bytes(), b'removed-a')

    def test_conflicts_and_invalid_cross_scope_comparison_responses_block_upload(self):
        (self.source / 'file').write_bytes(b'new'); self.target.mkdir(); (self.target / 'file').mkdir()
        snapshot = self.snapshot(); self.compared(snapshot)
        self.assertEqual(snapshot.changes[0]['change'], 'conflict')
        with self.assertRaisesRegex(EditorError, 'ütközésmentes'): snapshot.upload_request()
        fresh = self.snapshot()
        with mock.patch.object(worker, 'root_path', return_value=self.target): result = worker.upload_comparison(fresh.request)
        result['deletions'] = [dict(path='../outside', key='deleted-00000000', size=1, sha256='a'*64)]
        with self.assertRaises(EditorError): fresh.apply_comparison(result)
        self.assertFalse(fresh.compared)
        result['id'] = 'f'*24
        with self.assertRaises(EditorError): fresh.apply_comparison(result)

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

    def test_freebsd_process_enumeration_refuses_inaccessible_server_or_failed_probe(self):
        with mock.patch.object(deploy.sys, 'platform', 'freebsd14'), mock.patch.object(deploy.subprocess, 'run') as run:
            run.return_value = SimpleNamespace(returncode=0, stderr='', stdout=' PID COMM OSREL PATH\n123 GameServer 1402000 /a path/GameServer\n')
            self.assertEqual(deploy.process_images(), [(123, Path('/a path/GameServer'))])
            for result in (SimpleNamespace(returncode=1, stderr='denied', stdout=''),
                           SimpleNamespace(returncode=0, stderr='', stdout='123 GameServer 1402000 -\n'),
                           SimpleNamespace(returncode=0, stderr='', stdout='PID COMM OSREL PATH\n')):
                run.return_value = result
                with self.assertRaises(ValueError): deploy.process_images()

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


class DeployTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix='auriga-deploy-test-')
        self.addCleanup(temporary.cleanup)
        self.base = Path(temporary.name).resolve()
        self.root, self.build = self.base / 'server', self.base / 'build'
        (self.root / 'bin').mkdir(parents=True)
        (self.root / '.auriga-package.json').write_text('{}')
        shutil.copy2(REPO / 'SRC/Tools/server_package/manage.py', self.root / 'manage.py')
        cfg = json.loads((REPO / 'config/server.json.example').read_text())
        (self.root / 'server.json').write_text(json.dumps(cfg))
        self.config = cfg
        self.original_config = (self.root / 'server.json').read_bytes()
        self.new, self.old = {}, {}
        header = bytearray(20)
        header[:8] = b'\x7fELF\x02\x01\x01\x09';header[16:20] = b'\x02\x00\x3e\x00'
        for name, component in deploy.PROGRAMS.items():
            path = self.build / 'SRC/Server' / component / name
            path.parent.mkdir(parents=True, exist_ok=True)
            self.new[name] = bytes(header) + name.encode()
            self.old[name] = ('old-' + name).encode()
            path.write_bytes(self.new[name])
            (self.root / 'bin' / name).write_bytes(self.old[name])
        self.request = dict(action='install', id='d'*24, root=str(self.root), build=str(self.build), configuration='Release')
        self.folder = self.root / 'run/deployments' / self.request['id']
        # Production file transactions, hashing and all three locks remain real.
        # ELF execution and POSIX process enumeration have native FreeBSD coverage.
        for patch in (mock.patch.object(deploy, 'process_images', return_value=[]),
                      mock.patch.object(deploy, 'validate_programs'),
                      mock.patch.object(deploy.platform, 'machine', return_value='amd64'),
                      mock.patch.object(deploy.sys, 'platform', 'freebsd14'),
                      mock.patch.object(deploy, 'socket') ):
            patch.start();self.addCleanup(patch.stop)
        self.helpers = SimpleNamespace(**{name:getattr(worker, name) for name in ('root_path','no_links','locked','digest')}, emit=lambda _:None)

    def assert_originals(self):
        for name, data in self.old.items(): self.assertEqual((self.root / 'bin' / name).read_bytes(), data)
        self.assertEqual((self.root / 'server.json').read_bytes(), self.original_config)

    def test_automatic_backup_failure_prevents_any_binary_replacement(self):
        self.helpers.backup_before_deploy=mock.Mock(side_effect=ValueError('backup failed'))
        with self.assertRaisesRegex(ValueError,'backup failed'):deploy.deploy(self.request,self.helpers)
        self.helpers.backup_before_deploy.assert_called_once_with(self.root)
        self.assert_originals()

    def result(self): return json.loads((self.folder / 'result.json').read_text())

    def test_install_four_files_and_preserve_verified_backups_and_configuration(self):
        deploy.deploy(self.request, self.helpers)
        for name in deploy.PROGRAMS:
            self.assertEqual((self.root / 'bin' / name).read_bytes(), self.new[name])
            self.assertEqual((self.folder / 'backup' / name).read_bytes(), self.old[name])
        self.assertEqual((self.root / 'server.json').read_bytes(), self.original_config)
        self.assertEqual(self.result()['status'], 'success')
        self.assertEqual(deploy.validate_programs.call_count, 2)
        self.assertFalse((self.build / '.auriga-manager/operation.lock').exists())
        self.assertFalse((self.root / '.auriga-manager/operation.lock').exists())

    def test_mid_swap_failure_restores_every_replaced_file(self):
        replace = os.replace
        def fail(source, target):
            if Path(source) == self.folder / 'staging/LoginServer': raise OSError('simulated disk error')
            return replace(source, target)
        with mock.patch.object(deploy.os, 'replace', side_effect=fail), self.assertRaises(OSError):
            deploy.deploy(self.request, self.helpers)
        self.assert_originals();self.assertEqual(self.result()['status'], 'rolled-back')

    def test_post_install_validation_failure_rolls_back_all_four(self):
        deploy.validate_programs.side_effect = [None, ValueError('post-install test failed')]
        with self.assertRaisesRegex(ValueError, 'post-install'): deploy.deploy(self.request, self.helpers)
        self.assert_originals();self.assertEqual(self.result()['status'], 'rolled-back')

    def test_validation_before_replacement_leaves_all_originals(self):
        deploy.validate_programs.side_effect = ValueError('missing shared library')
        with self.assertRaisesRegex(ValueError, 'missing shared library'): deploy.deploy(self.request, self.helpers)
        self.assert_originals();self.assertEqual(self.result()['status'], 'failed')

    def test_failed_rollback_retains_backup_and_blocks_start(self):
        import backend
        copy = shutil.copy2
        def fail(source, target):
            if Path(source) == self.folder / 'backup/GameServer': raise OSError('restore disk failure')
            return copy(source, target)
        deploy.validate_programs.side_effect = [None, ValueError('post-install failure')]
        with mock.patch.object(deploy.shutil, 'copy2', side_effect=fail), self.assertRaisesRegex(RuntimeError, 'visszaállítás nem teljes'):
            deploy.deploy(self.request, self.helpers)
        self.assertEqual(self.result()['status'], 'rollback-failed')
        self.assertEqual(self.result()['rollback_errors'], ['GameServer'])
        self.assertEqual((self.folder / 'backup/GameServer').read_bytes(), self.old['GameServer'])
        self.assertFalse(backend.describe(self.root)['can_start'])

    def test_failed_first_install_removes_new_files(self):
        for path in (self.root / 'bin').iterdir(): path.unlink()
        deploy.validate_programs.side_effect = [None, ValueError('post-install test failed')]
        with self.assertRaises(ValueError): deploy.deploy(self.request, self.helpers)
        self.assertFalse(list((self.root / 'bin').iterdir()))
        self.assertEqual(self.result()['status'], 'rolled-back')

    def test_running_unmanaged_binary_is_refused_before_copying(self):
        deploy.process_images.return_value = [(123, self.root / 'bin/GameServer')]
        with self.assertRaisesRegex(ValueError, 'Még fut'): deploy.deploy(self.request, self.helpers)
        self.assert_originals();self.assertFalse(self.folder.exists())

    def test_owned_supervisor_is_refused(self):
        manager = deploy.load_manager(self.root)
        manager.save_json(self.root / 'run/supervisor.json', dict(supervisor=manager.identity(os.getpid()), children=[]))
        with self.assertRaisesRegex(ValueError, 'állítsd le'): deploy.deploy(self.request, self.helpers)
        self.assert_originals()

    def test_used_port_is_refused(self):
        deploy.socket.socket.return_value.__enter__.return_value.bind.side_effect = OSError('busy')
        with self.assertRaisesRegex(ValueError, 'szerverport'): deploy.deploy(self.request, self.helpers)
        self.assert_originals()

    def test_build_and_runtime_locks_both_block_replacement(self):
        for root in (self.build, self.root):
            with self.subTest(root=root), worker.locked(root), self.assertRaisesRegex(ValueError, 'zárolja'):
                deploy.deploy(self.request, self.helpers)
        self.assert_originals()

    def test_wrong_config_or_platform_and_missing_target_are_refused(self):
        cache = self.build / 'CMakeCache.txt';cache.write_text('CMAKE_BUILD_TYPE:STRING=Debug\n')
        with self.assertRaisesRegex(ValueError, 'konfigurációja eltér'): deploy.deploy(self.request, self.helpers)
        cache.unlink()
        path = self.build / 'SRC/Server/GameServer/ServerPackageCheck'
        path.write_bytes(b'MZ invalid Windows binary')
        with self.assertRaisesRegex(ValueError, 'ELF'): deploy.deploy(self.request, self.helpers)
        path.unlink()
        with self.assertRaisesRegex(ValueError, 'ServerPackageCheck'): deploy.deploy(self.request, self.helpers)
        self.assert_originals()

    def test_insufficient_space_keeps_originals(self):
        with mock.patch.object(deploy.shutil, 'disk_usage', return_value=SimpleNamespace(free=1)), self.assertRaisesRegex(ValueError, 'szabad hely'):
            deploy.deploy(self.request, self.helpers)
        self.assert_originals()

    def test_interruption_rolls_back_and_restores_signal_handlers(self):
        import signal
        original = signal.getsignal(signal.SIGTERM)
        def interrupt(directory, *_):
            if directory == self.root / 'bin': signal.getsignal(signal.SIGTERM)(signal.SIGTERM, None)
        deploy.validate_programs.side_effect = interrupt
        with self.assertRaises(InterruptedError): deploy.deploy(self.request, self.helpers)
        self.assert_originals();self.assertEqual(self.result()['status'], 'rolled-back')
        self.assertIs(signal.getsignal(signal.SIGTERM), original)

    def test_unfinished_or_failed_rollback_blocks_retry_and_gui_start(self):
        import backend
        for status in ('installing', 'rollback-failed'):
            self.folder.mkdir(parents=True, exist_ok=True)
            (self.folder / 'result.json').write_text(json.dumps(dict(status=status)))
            with self.subTest(status=status), self.assertRaisesRegex(ValueError, 'helyreállítást'): deploy.deploy(self.request, self.helpers)
            self.assertFalse(backend.describe(self.root)['can_start'])
            self.assertIn('Befejezetlen', backend.describe(self.root)['phase'])
        self.assert_originals()

    def test_validation_output_redacts_sql_and_interserver_secrets(self):
        text = self.config['common']['sql']['player']['password'] + ' ' + self.config['common']['interserver_key']
        self.assertNotIn('REPLACE', deploy.redacted(text, self.config))

    def test_install_plan_separate_runtime_and_worker_payload_contains_deployer(self):
        settings = dict(jobs.defaults(), build_dir='/Auriga/Source/build', configuration='Debug')
        request = jobs.install_request(settings, '/Auriga/Server')
        self.assertEqual(request['root'], '/Auriga/Server');self.assertEqual(request['configuration'], 'Debug')
        self.assertIn(b'install_support', jobs.worker_input(request))
        for root in ('/Auriga/Source', '/Auriga/Source/build', '/Auriga/Source/build/server', '/'):
            with self.subTest(root=root), self.assertRaises(EditorError): jobs.install_request(settings, root)


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

    def test_install_destination_changes_and_verified_qprocess_success_is_required(self):
        self.page.remote_root.setText('/home/runtime')
        self.assertIn('/home/runtime/bin', self.page.work.destination.text())
        for marker, success in (('AURIGA_INSTALL_SUCCESS files=4 backup=test', True), ('missing marker', False)):
            command = [sys.executable, '-c', 'import sys;sys.stdin.read();print(' + repr(marker) + ')']
            with mock.patch.object(ssh.Session, 'ssh', return_value=(command[0], command[1:])):
                self.page.work.install()
                self.assertFalse(self.page.work.cancel_button.isEnabled())
                self.wait(lambda: not self.page.work.active())
            self.assertEqual('sikeresen' in self.page.work.status.text(), success)
        self.assertEqual(self.errors, [])

    def test_install_disabled_and_refused_when_server_running_or_disconnected(self):
        self.page.owner.remote_state = dict(running=True)
        self.page.work.controls()
        self.assertFalse(self.page.work.install_button.isEnabled())
        self.page.work.install();self.assertIn('állítsd le', self.errors.pop())
        self.page.owner.remote_state = None;self.page.owner.remote_online = False
        self.page.work.controls();self.assertFalse(self.page.work.install_button.isEnabled())
        self.page.work.install();self.assertIn('csatlakozz', self.errors.pop())

    def wait(self, predicate):
        deadline = time.monotonic() + 8
        while not predicate() and time.monotonic() < deadline:
            self.application.processEvents()
            time.sleep(.01)
        self.assertTrue(predicate(), 'Qt operation did not finish')

    def local_worker(self, target):
        program = 'import sys;source=sys.stdin.buffer.read().decode();source=source.replace("path = Path(value)",' + \
            repr('path = Path(' + repr(str(target)) + ')') + ',1);exec(compile(source,"<test-worker>","exec"))'
        return sys.executable, ['-u', '-c', program]

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
        target = self.root / 'remote'
        def transfer(_):
            snapshot = self.page.work.snapshot
            code = 'import shutil\n'
            for row in self.page.work.request['files']:
                path = target / '.auriga-manager/uploads' / self.page.work.request['id'] / 'files' / row['key']
                code += 'shutil.copy2(' + repr(str(snapshot.root / row['key'])) + ',' + repr(str(path)) + ')\n'
            return sys.executable, ['-c', code]
        with mock.patch.object(ssh.Session, 'ssh', return_value=self.local_worker(target)) as commands, \
             mock.patch.object(ssh.Session, 'sftp', side_effect=transfer):
            self.page.work.prepare()
            self.wait(lambda: not self.page.work.active())
            self.assertTrue(self.page.work.snapshot.compared)
            self.assertTrue(self.page.work.upload_button.isEnabled())
            self.assertFalse(target.exists())
            self.assertEqual(self.page.work.table.item(0, 0).text(), 'Új')
            self.page.work.upload()
            self.wait(lambda: not self.page.work.active())
            self.assertEqual(commands.call_count, 3)
        self.assertEqual((target / 'file').read_bytes(), b'upload')
        self.assertIn('sikeresen', self.page.work.status.text())
        self.assertIsNone(self.page.work.snapshot)
        self.assertEqual(self.errors, [])

    def test_deleted_only_pipeline_keeps_backup_and_does_not_start_sftp(self):
        source = self.root / 'empty'; source.mkdir()
        target = self.root / 'remote'; target.mkdir(); (target / 'removed').write_bytes(b'preserved in backup')
        self.page.work.choose([str(source)]); self.page.work.contents.setChecked(True)
        with mock.patch.object(ssh.Session, 'ssh', return_value=self.local_worker(target)), \
                mock.patch.object(ssh.Session, 'sftp') as transfer:
            self.page.work.prepare(); self.wait(lambda: not self.page.work.active())
            self.assertTrue((target / 'removed').exists())
            self.page.work.delete.setChecked(True)
            self.page.work.upload(); self.wait(lambda: not self.page.work.active())
            transfer.assert_not_called()
        self.assertFalse((target / 'removed').exists())
        saved = list((target / '.auriga-manager/uploads').glob('*/backup/deleted-00000000'))
        self.assertEqual(len(saved), 1); self.assertEqual(saved[0].read_bytes(), b'preserved in backup')
        self.assertIn('sikeresen', self.page.work.status.text())

    def test_cancelling_remote_comparison_invalidates_snapshot(self):
        source = self.root / 'file'; source.write_bytes(b'x'); self.page.work.choose([str(source)])
        command = [sys.executable, '-c', 'import sys,time;sys.stdin.buffer.read();time.sleep(5)']
        with mock.patch.object(ssh.Session, 'ssh', return_value=(command[0], command[1:])):
            self.page.work.prepare()
            self.wait(lambda: self.page.work.process is not None)
            self.page.work.cancel(); self.wait(lambda: not self.page.work.active())
        self.assertIsNone(self.page.work.snapshot)
        self.assertFalse(self.page.work.upload_button.isEnabled())

    def test_comparison_zero_changes_deleted_only_and_conflicts_control_upload(self):
        source = self.root / 'source'; source.mkdir(); (source / 'same').write_bytes(b'same')
        snapshot = jobs.UploadSnapshot([source], '/home/upload', contents=True)
        self.addCleanup(snapshot.close)
        target = self.root / 'remote'; target.mkdir(); (target / 'same').write_bytes(b'same')
        with mock.patch.object(worker, 'root_path', return_value=target): result = worker.upload_comparison(snapshot.request)
        snapshot.apply_comparison(result); self.page.work.snapshot = snapshot; self.page.work.display_comparison(); self.page.work.controls()
        self.assertEqual(self.page.work.table.rowCount(), 0)
        self.assertFalse(self.page.work.upload_button.isEnabled())
        self.assertIn('1 változatlan', self.page.work.status.text())
        self.page.work.snapshot = None
        (source / 'same').unlink()
        empty = jobs.UploadSnapshot([source], '/home/upload', contents=True); self.addCleanup(empty.close)
        with mock.patch.object(worker, 'root_path', return_value=target): result = worker.upload_comparison(empty.request)
        empty.apply_comparison(result); self.page.work.snapshot = empty; self.page.work.display_comparison(); self.page.work.controls()
        self.assertEqual(self.page.work.table.item(0, 0).text(), 'Törölt / csak távolin')
        self.assertFalse(self.page.work.upload_button.isEnabled())
        self.page.work.delete.setChecked(True)
        self.assertTrue(self.page.work.upload_button.isEnabled())

    def test_failed_or_malformed_comparison_does_not_enable_upload(self):
        source = self.root / 'file'; source.write_bytes(b'x'); self.page.work.choose([str(source)])
        for output in ('missing comparison', 'AURIGA_UPLOAD_COMPARISON {}'):
            command = [sys.executable, '-c', 'import sys;sys.stdin.buffer.read();print(' + repr(output) + ')']
            with mock.patch.object(ssh.Session, 'ssh', return_value=(command[0], command[1:])):
                self.page.work.prepare(); self.wait(lambda: not self.page.work.active())
            self.assertIsNone(self.page.work.snapshot)
            self.assertFalse(self.page.work.upload_button.isEnabled())

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
        self.page.work.snapshot = jobs.UploadSnapshot([source], '/home/upload')
        copied = self.page.work.snapshot.root
        self.page.target['host'].setText('changed.invalid')
        self.assertIsNone(self.page.work.snapshot)
        self.assertFalse(copied.exists())


if __name__ == '__main__':
    unittest.main()
