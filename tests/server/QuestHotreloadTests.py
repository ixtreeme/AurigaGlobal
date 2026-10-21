"""Real multi-process quest publication, using production native runtime/control."""
import argparse
import contextlib
import hashlib
import json
import os
from pathlib import Path
import secrets
import shutil
import subprocess
import sys
import tempfile
import time
import unittest
from unittest import mock
from types import SimpleNamespace

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / 'SRC/Tools'))
from server_package import manage, quest_reload as reload
CORE = CHECKER = None


class QuestHotreloadTests(unittest.TestCase):
    def setUp(self):
        if not CORE or not CHECKER:
            self.skipTest('Pass --core and --checker for native integration')
        self.temp = tempfile.TemporaryDirectory(prefix='auriga-hotreload-test-')
        self.root = Path(self.temp.name).resolve()
        self.old_root, manage.ROOT = manage.ROOT, self.root
        self.processes, self.logs = {}, []
        self.api = SimpleNamespace(**vars(manage))
        self.api.ROOT = self.root
        self.api.executable = lambda name: CHECKER
        self.config = dict(schema_version=1, game_defaults=dict(server=dict(data_path='data')),
                           game_servers={name: dict(working_directory='run/' + name) for name in ('ch1', 'ch99')})
        self.write('.auriga-package.json', '{}')
        self.write('server.json', json.dumps(self.config))
        self.write('data/settings.lua', '')
        self.write('data/translate.lua', '')
        self.write('data/restart_city_pos.lua', '')
        self.write('data/quest/locale.lua', '')
        self.write('data/quest/questlib.lua', "helper='old'; select=function(...) return coroutine.yield('select',{...}) end")
        self.write('data/quest/questnpc.json', '{"npcs":[]}')
        self.write('data/quest/questcategory.json', '{"categories":[]}')
        self.write('data/quest/quest_list', 'main.quest\n')
        self.write('data/quest/main.quest', 'quest main begin state start begin when login begin end end end')
        children = [self.start(name) for name in self.config['game_servers']]
        manage.save_json(self.root / 'run/supervisor.json', dict(phase='listening', children=children))
        self.wait_for(lambda: reload.status(self.api)['available'])

    def write(self, relative, text):
        path = self.root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding='utf-8')

    def start(self, name):
        directory = self.root / 'run' / name
        directory.mkdir(parents=True, exist_ok=True)
        token = secrets.token_hex(24)
        log = (self.root / ('test-' + name + '-' + secrets.token_hex(3) + '.log')).open('wb')
        self.logs.append(log)
        process = subprocess.Popen([str(CORE), '--reload-core', str(self.root), name], cwd=directory,
            env=dict(os.environ, AURIGA_SHUTDOWN_TOKEN=token), stdout=log, stderr=log,
            creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)
        self.processes[name] = process
        self.wait_for(lambda: manage.identity(process.pid))
        return dict(name=name, directory=str(directory), token=token, identity=manage.identity(process.pid))

    def tearDown(self):
        if not hasattr(self, 'root'):
            return
        self.write('run/test-stop', '')
        for process in self.processes.values():
            try: process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill(); process.wait(timeout=5)
        for handle in self.logs: handle.close()
        manage.ROOT = self.old_root
        self.temp.cleanup()

    def wait_for(self, callback):
        deadline = time.monotonic() + 12
        while time.monotonic() < deadline:
            value = callback()
            if value: return value
            time.sleep(.02)
        logs = '\n'.join(p.read_text(errors='replace')[-4000:] for p in self.root.glob('test-*.log'))
        self.fail('Native test cores did not reach expected state: ' + logs)

    def children(self):
        return reload.participants(self.api)[1]

    def change(self, value='new'):
        self.write('data/quest/questlib.lua', "helper='" + value + "'; select=function(...) return coroutine.yield('select',{...}) end")

    def test_two_cores_activate_same_snapshot_and_restart_from_commit(self):
        old = [reload.core_status(c)['revision'] for c in self.children()]
        self.change()
        publication = reload.reload(self.api, timeout=5)
        self.assertTrue(all(reload.core_status(c)['revision'] == publication['revision'] for c in self.children()))
        self.assertNotEqual(old[0], publication['revision'])
        self.assertFalse((self.root / 'run/quest-reloads/pending.json').exists())
        # The authoring tree can be broken after activation: restart still uses
        # the sealed, committed generation and its stable ID map.
        self.write('data/quest/main.quest', 'broken pending edit')
        self.write('run/test-stop', '')
        self.processes['ch1'].wait(timeout=5)
        self.processes['ch99'].wait(timeout=5)
        (self.root / 'run/test-stop').unlink()
        child = self.start('ch1')
        self.wait_for(lambda: self._status(child, publication))
        result = reload.core_status(dict(child, directory=Path(child['directory']), token_hash=reload.digest(child['token'].encode())))
        self.assertEqual(result['id_hash'], reload.digest(json.dumps(publication['quest_ids'], sort_keys=True, separators=(',', ':')).encode()))

    def _status(self, child, publication):
        try:
            value = reload.core_status(dict(child, directory=Path(child['directory']), token_hash=reload.digest(child['token'].encode())))
            return value['transaction'] == publication['id'] and value['revision'] == publication['revision']
        except (OSError, ValueError, RuntimeError): return False

    def test_one_reject_aborts_every_core_without_commit(self):
        old = [reload.core_status(c)['revision'] for c in self.children()]
        self.change(); self.write('run/reject-ch99', '')
        with self.assertRaisesRegex(RuntimeError, 'injected core preparation failure'):
            reload.reload(self.api, timeout=5)
        self.assertIsNone(reload.current(self.api))
        self.assertEqual(old, [reload.core_status(c)['revision'] for c in self.children()])
        self.assertTrue(all(not reload.core_status(c)['prepared'] for c in self.children()))

    def test_syntax_failure_leaves_running_generation_untouched(self):
        self.write('data/quest/main.quest', 'quest bad begin state start begin when login begin if then end end end end')
        with self.assertRaisesRegex(RuntimeError, 'validation failed'):
            reload.reload(self.api, timeout=5)
        self.assertIsNone(reload.current(self.api))

    def test_postcommit_timeout_keeps_decision_and_rerun_recovers(self):
        self.change(); save = self.api.save_json
        def injected(path, value):
            if path.name == 'current.json': self.write('run/stall-ch99', '')
            save(path, value)
        with mock.patch.object(self.api, 'save_json', side_effect=injected):
            with self.assertRaisesRegex(RuntimeError, 'awaiting cores'):
                reload.reload(self.api, timeout=.8)
        decision = reload.current(self.api)
        self.assertTrue((self.root / 'run/quest-reloads/pending.json').is_file())
        (self.root / 'run/stall-ch99').unlink()
        recovered = reload.reload(self.api, timeout=5)
        self.assertEqual(decision['id'], recovered['id'])
        self.assertFalse((self.root / 'run/quest-reloads/pending.json').exists())

    def test_stale_token_status_is_unavailable(self):
        child = self.children()[0]
        status = reload.core_status(child)
        status['token_hash'] = '0' * 64
        manage.save_json(child['directory'] / '.auriga-quest-status.json', status)
        self.assertFalse(reload.status(self.api)['available'])

    def test_state_removal_is_rejected_by_native_core(self):
        self.write('data/quest/main.quest', 'quest main begin state start begin when login begin end end state persisted begin when login begin end end end')
        reload.reload(self.api, timeout=5)
        committed = reload.current(self.api)
        self.write('data/quest/main.quest', 'quest main begin state start begin when login begin end end end')
        with self.assertRaisesRegex(RuntimeError, 'persisted state'):
            reload.reload(self.api, timeout=5)
        self.assertEqual(reload.current(self.api)['id'], committed['id'])

    def test_library_registration_change_requires_restart(self):
        self.write('data/quest/questlib.lua', "add_goto_info('new',0,1,20,30); select=function(...) return coroutine.yield('select',{...}) end")
        with self.assertRaisesRegex(RuntimeError, 'bootstrap registrations changed'):
            reload.reload(self.api, timeout=5)
        self.assertIsNone(reload.current(self.api))

    def test_new_quest_ids_stay_stable_when_inserted_before_existing(self):
        self.write('data/quest/extra.quest', 'quest extra begin state start begin when login begin end end end')
        self.write('data/quest/quest_list', 'extra.quest\nmain.quest\n')
        publication = reload.reload(self.api, timeout=5)
        self.assertEqual(publication['quest_ids'], dict(main=1, extra=2))

    def test_offline_publication_requires_stop_then_applies_shared_setup(self):
        with self.assertRaisesRegex(RuntimeError, 'Stop all managed'):
            reload.apply_offline(self.api)
        previous = reload.reload(self.api, timeout=5)
        self.write('run/test-stop', '')
        for process in self.processes.values(): process.wait(timeout=5)
        self.write('data/settings.lua', 'new_shared_setting=true')
        publication = reload.apply_offline(self.api)
        self.assertNotEqual(publication['revision'], previous['revision'])
        self.assertEqual(publication['quest_ids'], previous['quest_ids'])
        self.assertTrue(publication['offline'])
        (self.root / 'run/test-stop').unlink()
        child = self.start('ch1')
        self.wait_for(lambda: self._status(child, publication))


class PortableIdentityTests(unittest.TestCase):
    def test_freebsd_procfs_fallback(self):
        if os.name == 'nt':
            # Exercise the portable branch without changing the process-wide os module.
            namespace = dict(vars(manage)); namespace['os'] = SimpleNamespace(name='posix', environ=os.environ)
            exec(__import__('inspect').getsource(manage.identity), namespace)
            identity = namespace['identity']
        else: identity = manage.identity
        with mock.patch.object(Path, 'read_text', side_effect=FileNotFoundError()), mock.patch.object(sys, 'platform', 'freebsd15'), \
                mock.patch.object(subprocess, 'run', return_value=SimpleNamespace(returncode=0, stdout='Wed Oct 21 10:00:00 2026 S /usr/local/bin/GameServer\n')):
            result = identity(123)
        self.assertEqual(result, dict(pid=123, created='Wed Oct 21 10:00:00 2026', image='/usr/local/bin/GameServer'))


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--core', type=Path)
    parser.add_argument('--checker', type=Path)
    args, remaining = parser.parse_known_args()
    CORE, CHECKER = args.core.resolve() if args.core else None, args.checker.resolve() if args.checker else None
    unittest.main(argv=[sys.argv[0], *remaining])
