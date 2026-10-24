"""Backup integrity, restore transactions, SQL tooling and target-bound UI."""
import io
import json
import os
from pathlib import Path
import shutil
import sys
import tarfile
import tempfile
import subprocess
import time
import types
import unittest
from unittest import mock

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'SRC/Tools/ServerManager'))
import backup_tools as backups
import backend as api


def fixture(root):
    root.mkdir(parents=True,exist_ok=True)
    (root/'.auriga-package.json').write_text('{}')
    shutil.copy2(ROOT/'config/server.json.example',root/'server.json')
    shutil.copy2(ROOT/'SRC/Tools/server_package/manage.py',root/'manage.py')
    (root/'data/quest').mkdir(parents=True)
    (root/'data/quest/probe.lua').write_text('return 1')
    (root/'tls').mkdir();(root/'tls/server.key').write_bytes(b'private-test-key')
    (root/'bin').mkdir()


class BackupTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory(prefix='auriga-backup-center-');self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name).resolve();fixture(self.root)

    def test_create_verify_restore_and_safety_backup_exact_files(self):
        saved=backups.create(self.root,api,['config','data','tls'])
        self.assertNotIn('private-test-key',json.dumps(saved))
        with backups.extracted(self.root,saved['id']) as (meta,stage):
            self.assertEqual((stage/'files/data/quest/probe.lua').read_text(),'return 1')
        (self.root/'data/quest/probe.lua').write_text('return 2')
        (self.root/'data/quest/new.lua').write_text('new file')
        plan=backups.restore_plan(self.root,api,saved['id'],saved['scopes'])
        self.assertEqual(plan['remove'],['data/quest/new.lua'])
        result=backups.restore(self.root,api,saved['id'],saved['scopes'],plan['fingerprint'])
        self.assertEqual(result['status'],'success')
        self.assertEqual((self.root/'data/quest/probe.lua').read_text(),'return 1')
        self.assertFalse((self.root/'data/quest/new.lua').exists())
        self.assertIsNone(backups.pending(self.root))
        with backups.extracted(self.root,result['safety_backup']) as (_,stage):
            self.assertEqual((stage/'files/data/quest/new.lua').read_text(),'new file')

    def test_stale_plan_and_corrupt_archive_refused_before_restore(self):
        saved=backups.create(self.root,api,['data'])
        plan=backups.restore_plan(self.root,api,saved['id'],['data'])
        (self.root/'data/quest/probe.lua').write_text('changed')
        with self.assertRaisesRegex(ValueError,'megváltozott'):
            backups.restore(self.root,api,saved['id'],['data'],plan['fingerprint'])
        folder,_=backups.backup(self.root,saved['id']);(folder/'backup.tar.gz').write_bytes(b'corrupt')
        with self.assertRaisesRegex(ValueError,'sérült'):backups.restore_plan(self.root,api,saved['id'],['data'])

    def test_structurally_invalid_configuration_is_rejected_before_mutation(self):
        saved=backups.create(self.root,api,['config']);folder,meta=backups.backup(self.root,saved['id'])
        cfg=json.loads((self.root/'server.json').read_text());cfg['database']=None;raw=json.dumps(cfg).encode()
        row=meta['files']['files/server.json'];row['size']=len(raw)
        import hashlib
        row['sha256']=hashlib.sha256(raw).hexdigest()
        manifest={key:value for key,value in meta.items() if key not in ('archive_size','archive_sha256')}
        with tarfile.open(folder/'backup.tar.gz','w:gz') as tar:
            for name,payload in (('manifest.json',json.dumps(manifest).encode()),('files/server.json',raw)):
                info=tarfile.TarInfo(name);info.size=len(payload);tar.addfile(info,io.BytesIO(payload))
        meta['archive_size']=(folder/'backup.tar.gz').stat().st_size;meta['archive_sha256']=backups.digest(folder/'backup.tar.gz')
        backups.atomic_json(folder/'meta.json',meta)
        before=(self.root/'server.json').read_bytes()
        with self.assertRaisesRegex(ValueError,'szerkezete hibás'):backups.restore_plan(self.root,api,saved['id'],['config'])
        self.assertEqual((self.root/'server.json').read_bytes(),before)

    def test_failure_rolls_back_and_retains_journal(self):
        saved=backups.create(self.root,api,['data'])
        (self.root/'data/quest/probe.lua').write_text('current')
        plan=backups.restore_plan(self.root,api,saved['id'],['data'])
        actual=backups.apply_snapshot
        def fail_after_apply(*args,**kwargs):
            actual(*args,**kwargs)
            if args[2]['id']==saved['id']:raise OSError('injected disk failure')
        with mock.patch.object(backups,'apply_snapshot',side_effect=fail_after_apply),self.assertRaisesRegex(ValueError,'rolled-back'):
            backups.restore(self.root,api,saved['id'],['data'],plan['fingerprint'])
        self.assertEqual((self.root/'data/quest/probe.lua').read_text(),'current')
        self.assertIsNone(backups.pending(self.root))

    def test_preserve_current_sql_endpoints_on_config_restore(self):
        saved=backups.create(self.root,api,['config'])
        cfg=json.loads((self.root/'server.json').read_text());cfg['common']['sql']['player']['port']=54321
        (self.root/'server.json').write_text(json.dumps(cfg))
        plan=backups.restore_plan(self.root,api,saved['id'],['config'])
        backups.restore(self.root,api,saved['id'],['config'],plan['fingerprint'])
        cfg=json.loads((self.root/'server.json').read_text())
        self.assertEqual(cfg['common']['sql']['player']['port'],54321)

    def test_archive_import_and_path_guards(self):
        saved=backups.create(self.root,api,['data'])
        folder,_=backups.backup(self.root,saved['id'])
        archive=self.root/'offsite.tar.gz';shutil.copy2(folder/'backup.tar.gz',archive)
        for name in ('../outside','C:/Windows/file','data/../file','data\\file','/absolute'):
            with self.subTest(name=name),self.assertRaises(ValueError):backups.valid_relative(name)
        target=self.root/'second';fixture(target)
        imported=backups.import_archive(target,api,archive,saved['archive_sha256'])
        self.assertEqual(imported['id'],saved['id'])
        self.assertEqual(len(backups.inventory(target,api)['backups']),1)

    def test_policy_validation_and_scheduler_does_not_run_when_disabled(self):
        self.assertEqual(backups.run_schedule(self.root,api),dict(skipped='disabled'))
        bad=dict(backups.defaults(),minute=60)
        with self.assertRaises(ValueError):backups.validate_policy(bad)
        with self.assertRaises(ValueError):backups.scopes([])
        cfg=json.loads((self.root/'server.json').read_text());cfg['common']['sql']['player']['database']='mysql'
        with self.assertRaises(ValueError):backups.sql_groups(cfg,api)

    def test_interrupted_restore_blocks_start_and_can_be_recovered(self):
        saved=backups.create(self.root,api,['data'])
        (self.root/'data/quest/probe.lua').write_text('current')
        plan=backups.restore_plan(self.root,api,saved['id'],['data'])
        with mock.patch.object(backups,'apply_snapshot',side_effect=OSError('disk failure')),self.assertRaisesRegex(ValueError,'rollback-failed'):
            backups.restore(self.root,api,saved['id'],['data'],plan['fingerprint'])
        self.assertIsNotNone(backups.pending(self.root))
        self.assertFalse(api.describe(self.root)['can_start'])
        manager=api.load_manager(self.root)
        with self.assertRaisesRegex(RuntimeError,'Unfinished backup restore'):manager.supervise_owned([1])
        plan=backups.restore_plan(self.root,api,saved['id'],['data'])
        backups.restore(self.root,api,saved['id'],['data'],plan['fingerprint'])
        self.assertIsNone(backups.pending(self.root))

    def test_job_lock_and_retention_preserve_manual_backups(self):
        with backups.job_lock(self.root,api,'test'):
            with self.assertRaisesRegex(ValueError,'Már fut'),backups.job_lock(self.root,api,'other'):pass
        manual=backups.create(self.root,api,['data'])
        policy=dict(backups.defaults(),enabled=True,scopes=['data'],keep=1)
        backups.atomic_json(backups.store(self.root)/'policy.json',policy)
        first=backups.run_schedule(self.root,api)
        self.assertEqual(backups.run_schedule(self.root,api)['skipped'],'already-completed')
        backups.atomic_json(backups.store(self.root)/'schedule-last.json',dict(date='yesterday',status='success'))
        second=backups.run_schedule(self.root,api)
        ids={row['id'] for row in backups.inventory(self.root,api)['backups']}
        self.assertEqual(ids,{manual['id'],second['backup']});self.assertNotIn(first['backup'],ids)

    def test_scheduler_preserves_unrelated_cron_lines_and_uses_target_runner(self):
        calls=[]
        def command(argv,**kwargs):
            calls.append((argv,kwargs))
            return types.SimpleNamespace(returncode=0,stdout='15 4 * * * /usr/local/bin/existing-job\n' if argv[-1]=='-l' else '',stderr='')
        settings=dict(backups.defaults(),enabled=True,scopes=['data'])
        with mock.patch.object(backups,'windows',return_value=False),mock.patch.object(backups.subprocess,'run',side_effect=command):
            backups.save_policy(self.root,api,settings,'print("scheduled worker")')
        written=next(kwargs['input'] for _,kwargs in calls if 'input' in kwargs)
        self.assertIn('/usr/local/bin/existing-job',written);self.assertIn('scheduled-worker.py',written)
        self.assertIn('0 3 * * *',written);self.assertIn('# AurigaBackup-',written)

    def test_before_deploy_policy_requires_successful_backup(self):
        self.assertIsNone(backups.before_deploy(self.root,api))
        backups.atomic_json(backups.store(self.root)/'policy.json',dict(backups.defaults(),auto_before_deploy=True,scopes=['data']))
        identity=backups.before_deploy(self.root,api)
        self.assertEqual(backups.backup(self.root,identity)[1]['reason'],'before-deploy')
        with mock.patch.object(backups,'_create',side_effect=OSError('backup failed')),self.assertRaises(OSError):
            backups.before_deploy(self.root,api)

    def test_symlink_archive_member_and_windows_drive_path_are_rejected(self):
        saved=backups.create(self.root,api,['data']);folder,meta=backups.backup(self.root,saved['id'])
        with tarfile.open(folder/'backup.tar.gz','w:gz') as tar:
            manifest={key:value for key,value in meta.items() if key not in ('archive_size','archive_sha256')}
            raw=json.dumps(manifest).encode();info=tarfile.TarInfo('manifest.json');info.size=len(raw);tar.addfile(info,io.BytesIO(raw))
            info=tarfile.TarInfo('files/data/quest/probe.lua');info.type=tarfile.SYMTYPE;info.linkname='/outside';tar.addfile(info)
        meta['archive_sha256']=backups.digest(folder/'backup.tar.gz');meta['archive_size']=(folder/'backup.tar.gz').stat().st_size
        backups.atomic_json(folder/'meta.json',meta)
        with self.assertRaisesRegex(ValueError,'archívumtag'),backups.extracted(self.root,saved['id']):pass


try:
    import PySide6
except ImportError:PySide6=None


@unittest.skipUnless(PySide6,'Qt environment required')
class BackupGuiTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        os.environ.setdefault('QT_QPA_PLATFORM','offscreen')
        from PySide6.QtWidgets import QApplication
        from PySide6.QtCore import QSettings
        cls.app=QApplication.instance() or QApplication([])
        cls.settings=tempfile.TemporaryDirectory(prefix='auriga-backup-settings-')
        QSettings.setDefaultFormat(QSettings.Format.IniFormat)
        QSettings.setPath(QSettings.Format.IniFormat,QSettings.Scope.UserScope,cls.settings.name)

    @classmethod
    def tearDownClass(cls):cls.settings.cleanup()

    def setUp(self):
        import window
        self.temp=tempfile.TemporaryDirectory(prefix='auriga-backup-ui-');self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name).resolve();fixture(self.root)
        self.view=window.MainWindow(self.root);self.addCleanup(self.view.close)
        self.errors=[];self.view.error=self.errors.append
        self.page=self.view.backup_page

    def wait(self):
        end=time.monotonic()+8
        while self.page.active() and time.monotonic()<end:self.app.processEvents();time.sleep(.01)
        self.app.processEvents()
        self.assertFalse(self.page.active())

    def test_async_create_review_restore_and_nested_download_copy(self):
        from PySide6.QtWidgets import QFileDialog
        self.page.checks['sql'].setChecked(False)
        self.page.create();self.wait()
        self.page.refresh_list();self.wait()
        self.assertEqual(self.page.table.rowCount(),1);self.page.table.selectRow(0)
        (self.root/'data/quest/probe.lua').write_text('changed')
        self.page.prepare_restore();self.wait();self.assertIsNotNone(self.page.plan,self.page.status.text())
        self.assertFalse(self.page.restore_button.isEnabled())
        self.page.confirm.setChecked(True);self.assertTrue(self.page.restore_button.isEnabled())
        self.page.restore();self.wait()
        self.assertEqual((self.root/'data/quest/probe.lua').read_text(),'return 1')
        self.page.refresh_list();self.wait();self.page.table.selectRow(0)
        destination=self.root/'downloaded.tar.gz'
        with mock.patch.object(QFileDialog,'getSaveFileName',return_value=(str(destination),'')):
            self.page.download();self.wait()
        self.assertTrue(destination.is_file(),self.page.status.text());self.assertEqual(self.errors,[])

    def test_production_rpc_inventory_and_download_manifest(self):
        import server_targets as targets
        saved=backups.create(self.root,api,['data'])
        for operation,args in (('backup-list',{}),('backup-info',dict(id=saved['id']))):
            request=dict(schema_version=1,id='a'*24,target='b'*24,root=str(self.root),operation=operation,arguments=args)
            result=subprocess.run([sys.executable,'-B','-'],input=targets.worker_input(request),capture_output=True,timeout=15)
            self.assertEqual(result.returncode,0,result.stderr)
            packet=json.loads(next(line[len('AURIGA_TARGET_RESULT '):] for line in result.stdout.decode().splitlines() if line.startswith('AURIGA_TARGET_RESULT ')))
            self.assertTrue(packet['ok'],packet)
            if operation=='backup-info':self.assertEqual(packet['result']['sha256'],saved['archive_sha256'])


if __name__=='__main__':unittest.main()
