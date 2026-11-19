"""Temporary snapshots, explicit approval, exact-payload binding and no-write cancellation."""
import gzip,json,os,sys,tempfile,unittest
from pathlib import Path
from unittest import mock
os.environ.setdefault('QT_QPA_PLATFORM','offscreen')
ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'SRC/Tools/ServerManager'))
from PySide6.QtCore import Qt,QTimer
from PySide6.QtTest import QTest
from PySide6.QtWidgets import QApplication,QMainWindow
import change_review as review
import backend as api


class ReviewTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):cls.app=QApplication.instance() or QApplication([])
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        class Owner(QMainWindow):
            epoch=1
            def target_id(self):return self.epoch,'local','owned-test-package'
        self.owner=Owner();self.owner.change_review_root=Path(self.temp.name)/'stages'
        self.addCleanup(self.owner.deleteLater)
        self.rows=[('Láda neve','Darabszám','1','3')];self.payload=dict(amount=3)
        self.original=Path(self.temp.name)/'original.txt';self.original.write_text('unchanged')
    def stage(self):return review.Stage.create(self.owner,'Próba','fixture',self.payload,self.rows)

    def test_stage_is_durable_before_approval_and_does_not_touch_target(self):
        stage=self.stage();self.assertTrue((stage.path/'snapshot.json.gz').is_file())
        body,status=stage.read();self.assertEqual(status['state'],'staged');self.assertEqual(body['payload'],self.payload)
        with self.assertRaises(api.EditorError):stage.begin(self.owner)
        self.assertEqual(self.original.read_text(),'unchanged')

    def test_publish_uses_exact_approved_snapshot_and_consumes_approval_once(self):
        stage=self.stage();self.payload['amount']=99;stage.approve(self.owner);seen=[]
        result=review.publish(stage,self.owner,lambda payload:(seen.append(payload),dict(ok=True))[1])
        self.assertEqual(seen,[dict(amount=3)]);self.assertEqual(result,dict(ok=True));self.assertEqual(stage.read()[1]['state'],'applied')
        with self.assertRaises(api.EditorError):review.publish(stage,self.owner,lambda payload:self.fail('approval reused'))

    def test_target_change_blocks_both_approval_and_publication(self):
        stage=self.stage();self.owner.epoch=2
        with self.assertRaises(api.EditorError):stage.approve(self.owner)
        self.owner.epoch=1;stage.approve(self.owner);self.owner.epoch=3
        with self.assertRaises(api.EditorError):stage.begin(self.owner)

    def test_modified_payload_or_checksum_cannot_replace_reviewed_contents(self):
        stage=self.stage();body,_=stage.read();body['payload']['amount']=100
        raw=review.encoded(body);(stage.path/'snapshot.json.gz').write_bytes(gzip.compress(raw))
        status=json.loads((stage.path/'status.json').read_text());status['checksum']=review.digest(raw)
        (stage.path/'status.json').write_text(json.dumps(status))
        with self.assertRaises(api.EditorError):stage.approve(self.owner)

    def test_cancel_keeps_staged_snapshot_and_calls_no_publisher(self):
        with mock.patch.object(review.ChangeReviewDialog,'exec',return_value=review.QDialog.DialogCode.Rejected):
            result=review.request(self.owner,'Próba','fixture',self.payload,self.rows)
        self.assertIsNone(result);folders=list(self.owner.change_review_root.iterdir());self.assertEqual(len(folders),1)
        self.assertEqual(json.loads((folders[0]/'status.json').read_text())['state'],'staged')
        self.assertEqual(self.original.read_text(),'unchanged')

    def test_dialog_observes_snapshot_before_user_approves(self):
        def accept(dialog):
            body,status=dialog.stage.read();self.assertEqual(status['state'],'staged');self.assertEqual(body['rows'],[list(self.rows[0])])
            self.assertEqual(self.original.read_text(),'unchanged');return review.QDialog.DialogCode.Accepted
        with mock.patch.object(review.ChangeReviewDialog,'exec',new=accept):stage=review.request(self.owner,'Próba','fixture',self.payload,self.rows)
        self.assertEqual(stage.read()[1]['state'],'approved')

    def test_failed_publication_is_unconfirmed_and_never_automatically_retried(self):
        stage=self.stage();stage.approve(self.owner);writer=mock.Mock(side_effect=api.EditorError('unknown outcome'))
        with self.assertRaises(api.EditorError):review.publish(stage,self.owner,writer)
        self.assertEqual(writer.call_count,1);self.assertEqual(stage.read()[1]['state'],'unconfirmed')
        with self.assertRaises(api.EditorError):review.publish(stage,self.owner,writer)
        self.assertEqual(writer.call_count,1)

    def test_secret_values_are_masked_in_json_and_source_summary(self):
        rows=review.dict_rows('Beállítás',dict(sql=dict(password='old-secret')),dict(sql=dict(password='new-secret')))
        self.assertEqual(len(rows),1);self.assertNotIn('old-secret',str(rows));self.assertNotIn('new-secret',str(rows))
        rows=review.text_rows('Script','password = old-secret','password = new-secret')
        self.assertNotIn('old-secret',str(rows));self.assertNotIn('new-secret',str(rows))

    def test_enter_defaults_to_return_without_approving_upload(self):
        stage=self.stage();dialog=review.ChangeReviewDialog(self.owner,stage)
        QTimer.singleShot(0,lambda:QTest.keyClick(dialog,Qt.Key.Key_Return))
        self.assertEqual(dialog.exec(),review.QDialog.DialogCode.Rejected)
        self.assertEqual(stage.read()[1]['state'],'staged');dialog.deleteLater()

    def test_filtered_review_still_identifies_total_approval_scope(self):
        stage=review.Stage.create(self.owner,'Próba','fixture',self.payload,[*self.rows,('Mob neve','Darab','2','3')])
        dialog=review.ChangeReviewDialog(self.owner,stage);dialog.search.setText('Láda')
        self.assertEqual(dialog.model.rowCount(),1);self.assertIn('2 módosítás',dialog.count.text());dialog.deleteLater()

    def test_attachment_is_durable_and_verified_before_publication(self):
        source=Path(self.temp.name)/'source';source.write_bytes(b'approved bytes')
        stage=review.Stage.create(self.owner,'Feltöltés','upload',{},self.rows,{'00000000':dict(path=str(source),sha256=review.digest(source.read_bytes()))})
        source.write_bytes(b'changed source');stage.approve(self.owner)
        self.assertEqual((stage.path/'files/00000000').read_bytes(),b'approved bytes')
        (stage.path/'files/00000000').write_bytes(b'tampered')
        with self.assertRaises(api.EditorError):stage.begin(self.owner)

    def test_attachment_changed_before_staging_never_reaches_review(self):
        source=Path(self.temp.name)/'source';source.write_bytes(b'changed')
        with mock.patch.object(review.ChangeReviewDialog,'exec') as dialog:
            with self.assertRaises(api.EditorError):review.request(self.owner,'Feltöltés','upload',{},self.rows,{'file':dict(path=str(source),sha256=review.digest(b'original'))})
        dialog.assert_not_called()


class FileGateTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):cls.app=QApplication.instance() or QApplication([])
    def setUp(self):
        import window
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        class Owner(QMainWindow):
            remote=False
            def target_id(self):return 1,'remote' if self.remote else 'local','fixture'
            def busy(self):return False
            def is_remote(self):return self.remote
            def refresh(self):pass
            def state(self):return {}
        self.owner=Owner();self.owner.root=Path(self.temp.name);self.owner.change_review_root=self.owner.root/'stages'
        self.owner.error=mock.Mock();self.owner.job=mock.Mock();self.owner.remote_request=mock.Mock(return_value=True)
        self.editor=window.Editor(self.owner);self.addCleanup(self.editor.code.shutdown);self.addCleanup(self.owner.deleteLater)
        self.editor.document=api.Document.from_bytes('data/example.json',b'{"value":1}')
        self.editor.document.target_id=self.owner.target_id();self.editor.filename.setText('Példa')
        self.editor.text=lambda:'{"value":3}';self.editor.dirty=True

    def test_cancel_retains_local_draft_and_writes_no_pending_target_file(self):
        with mock.patch.object(review.ChangeReviewDialog,'exec',return_value=review.QDialog.DialogCode.Rejected):self.editor.save()
        self.owner.job.assert_not_called();self.owner.remote_request.assert_not_called()
        self.assertFalse((self.owner.root/'run').exists());self.assertTrue(self.editor.dirty)
        self.assertEqual(len(list(self.owner.change_review_root.iterdir())),1)

    def test_cancel_never_starts_remote_save(self):
        self.owner.remote=True;self.editor.document.target_id=self.owner.target_id()
        with mock.patch.object(review.ChangeReviewDialog,'exec',return_value=review.QDialog.DialogCode.Rejected):self.editor.save()
        self.owner.remote_request.assert_not_called();self.assertTrue(self.editor.dirty)

    def test_approved_local_save_uses_reviewed_bytes_even_if_form_changes(self):
        def approve(dialog):
            self.editor.text=lambda:'{"value":99}'
            return review.QDialog.DialogCode.Accepted
        with mock.patch.object(review.ChangeReviewDialog,'exec',new=approve):self.editor.save()
        args=self.owner.job.call_args.args[0];self.assertEqual(args[0],'save')
        self.assertEqual(Path(args[2]).read_bytes(),b'{"value":3}')
        self.owner.error.assert_not_called()

    def test_approved_remote_save_uses_exact_snapshot_and_hash(self):
        import base64
        self.owner.remote=True;self.editor.document.target_id=self.owner.target_id()
        with mock.patch.object(review.ChangeReviewDialog,'exec',return_value=review.QDialog.DialogCode.Accepted):self.editor.save()
        operation,payload,*_=self.owner.remote_request.call_args.args
        self.assertEqual(operation,'save');self.assertEqual(base64.b64decode(payload['data']),b'{"value":3}')
        self.assertEqual(payload['expected_hash'],self.editor.document.digest);self.owner.error.assert_not_called()


class OtherWriteGateTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):cls.app=QApplication.instance() or QApplication([])
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        class Owner(QMainWindow):
            remote_profile=None;remote_state={};editors=[]
            def target_id(self):return 1,'local','fixture'
            def is_remote(self):return False
            def busy(self):return False
            def refresh(self):pass
            def state(self):return {}
        self.owner=Owner();self.owner.root=Path(self.temp.name);self.owner.change_review_root=self.owner.root/'stages';self.owner.error=mock.Mock()
        self.addCleanup(self.owner.deleteLater)
        self.reject=mock.patch.object(review.ChangeReviewDialog,'exec',return_value=review.QDialog.DialogCode.Rejected)
        self.reject.start();self.addCleanup(self.reject.stop)

    def remote_page(self):
        import ssh_connection as ssh
        from remote_page import RemotePage
        page=RemotePage(self.owner,self.owner.root/'profiles.json');self.addCleanup(page.shutdown);self.addCleanup(page.deleteLater)
        profile=ssh.new_profile();profile.update(name='Teszt cél',remote_root='/srv/auriga',mode='direct');profile['target'].update(host='example.invalid',user='auriga')
        profile['work'].update(source_dir='/home/source',build_dir='/home/build',upload_dir='/home/upload')
        page.load(profile);page.operation_profile=mock.Mock(return_value=profile)
        return page

    def test_cancelled_ssh_profile_never_writes_profile_or_credentials(self):
        page=self.remote_page();page.name.setText('Módosított név')
        with mock.patch.object(page.store,'save') as save:
            page.save();save.assert_not_called()
        self.assertTrue(page.dirty);self.assertFalse((self.owner.root/'profiles.json').exists());self.owner.error.assert_not_called()

    def test_cancelled_remote_upload_never_opens_session_or_starts_worker(self):
        import remote_jobs,ssh_connection as ssh
        page=self.remote_page();source=self.owner.root/'source';source.write_bytes(b'upload bytes')
        snapshot=remote_jobs.UploadSnapshot([source],'/srv/upload');snapshot.compared=True
        snapshot.request['deletions']=[]
        for row in snapshot.request['files']:row['previous']=None
        page.work.snapshot=snapshot
        with mock.patch.object(ssh,'Session') as session,mock.patch.object(page.work,'start_worker') as worker:
            page.work.upload();session.assert_not_called();worker.assert_not_called()
        self.assertIs(page.work.snapshot,snapshot);self.owner.error.assert_not_called()
        self.assertTrue(any((folder/'files/00000000').is_file() for folder in self.owner.change_review_root.iterdir()))

    def test_cancelled_build_and_install_never_start_remote_worker(self):
        import ssh_connection as ssh
        page=self.remote_page()
        with mock.patch.object(ssh,'Session') as session,mock.patch.object(page.work,'start_worker') as worker:
            page.work.start_build('build');page.work.install();session.assert_not_called();worker.assert_not_called()
        self.owner.error.assert_not_called()

    def test_cancelled_remote_quest_activation_never_opens_ssh_session(self):
        import ssh_connection as ssh
        page=self.remote_page()
        with mock.patch.object(ssh,'Session') as session,mock.patch.object(page,'launch') as launch:
            page.reload_quests();page.reload_quests(offline=True);session.assert_not_called();launch.assert_not_called()
        self.owner.error.assert_not_called()

    def test_cancelled_backup_policy_and_restore_do_not_start_writer_and_keep_plan(self):
        from backup_page import BackupPage
        page=BackupPage(self.owner);self.addCleanup(page.deleteLater)
        page.plan=dict(id='fixture',scopes=['data'],fingerprint='fixed',files=1,replace=['data/file.json'],remove=[],sql=[])
        page.confirm.setChecked(True);plan=page.plan
        with mock.patch.object(page,'run_local') as writer:
            page.run('backup-policy',dict(policy=dict(enabled=True)),mock.Mock())
            page.restore();writer.assert_not_called()
        self.assertIs(page.plan,plan);self.owner.error.assert_not_called()


if __name__=='__main__':unittest.main(verbosity=2)
