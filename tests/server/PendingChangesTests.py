"""Several drafts, restart, overlays, one approval and no implicit writes."""
import argparse,base64,copy,json,os,sys,tempfile,time,unittest
from pathlib import Path
from unittest import mock
os.environ.setdefault('QT_QPA_PLATFORM','offscreen')
ROOT=Path(__file__).resolve().parents[2];sys.path[:0]=[str(ROOT/'SRC/Tools/ServerManager'),str(Path(__file__).parent)]
from PySide6.QtCore import QSettings, Qt
from PySide6.QtWidgets import QApplication,QDialog
import backend as api,change_review as review,item_catalog as catalog
from pending_changes import PendingChanges,compile_catalog
from reward_catalog import Rewards
import CatalogEditorTests as fixtures
from CatalogEditorTests import create,saved
import window


class PendingTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.app=QApplication.instance() or QApplication([])
        cls.settings=tempfile.TemporaryDirectory(prefix='auriga-pending-settings-')
        QSettings.setDefaultFormat(QSettings.Format.IniFormat);QSettings.setPath(QSettings.Format.IniFormat,QSettings.Scope.UserScope,cls.settings.name)
        packet,cls.material=create(catalog.blank(fixtures.TOOL),6,lambda d:d['base'].update(max_stack='200'))
        packet,cls.other=create(packet,6,lambda d:d['base'].update(max_stack='200'))
        cls.boxes=[]
        for mode in ('1','2'):
            def setup(d,mode=mode):d['loot']=dict(pick_mode=mode,entries=[dict(reward_kind='1',item_category_id=str(cls.material[0]),item_local_id=str(cls.material[1]),amount='1',weight='1' if mode=='1' else '10000',rare_attribute_chance_bp='0',refinement_level='0')])
            packet,key=create(packet,8,setup);cls.boxes.append(key)
        model=catalog.Catalog(packet);doc=model.new_drop_group();doc['group'].update(target_value='103',code='drop.pending_test')
        doc['entries']=[dict(category_id=str(cls.material[0]),local_id=str(cls.material[1]),refinement_level='0',amount='1',chance_ppb='1000000000',weight='0',rare_attribute_chance_bp='0',killer_level_min='1',killer_level_max='65535')]
        packet=saved(packet,model.apply_drops(doc));packet['live_drops']=dict(supported=True,containers_supported=True,release='',base_hash=packet['hash'],hash=packet['hash'],
            tables={k:copy.deepcopy(packet['tables'][k]) for k in ('mob_drop_group','mob_drop_entry')},container_groups=[],container_entries=[])
        cls.packet=packet

    @classmethod
    def tearDownClass(cls):cls.settings.cleanup()

    def setUp(self):
        self.temp=tempfile.TemporaryDirectory(prefix='auriga-pending-tests-');self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name)/'package';self.root.mkdir();(self.root/'.auriga-package.json').write_text('{}');(self.root/'manage.py').write_text('')
        (self.root/'server.json').write_text('{"schema_version":1,"game_servers":{}}');(self.root/'data').mkdir()
        self.view=window.MainWindow();self.view.timer.stop();self.view.root=self.root;self.view.local_root=self.root;self.view.change_review_root=Path(self.temp.name)/'stages'
        self.view.refresh=mock.Mock();self.view.refresh_pending_views=mock.Mock();self.view.state=mock.Mock(return_value=dict(database_running=True,running=False))
        self.errors=[];self.view.error=self.errors.append;self.page=self.view.item_page;self.page.state=dict(database_running=True,running=False,drop_reload=dict(available=True,containers_available=True))
        self.page.loaded(copy.deepcopy(self.packet));self.queue=self.view.pending_changes
        self.signal_errors=[];previous=sys.excepthook;sys.excepthook=lambda kind,error,tb:self.signal_errors.append(str(error));self.addCleanup(setattr,sys,'excepthook',previous)

    def tearDown(self):
        self.assertFalse(self.queue.active);self.assertFalse(self.signal_errors,self.signal_errors)
        self.page.dirty=False;self.view.remote_page.dirty=False
        for editor in self.view.editors:editor.dirty=False
        self.view.close();self.view.deleteLater();self.app.processEvents()

    def wait(self):
        deadline=time.monotonic()+12
        while self.queue.active and time.monotonic()<deadline:self.app.processEvents();time.sleep(.01)
        self.app.processEvents();self.assertFalse(self.queue.active,'Publication did not finish')

    def update_item(self,key,value):
        self.page.open_item(key);next(f for f in self.page.base_form.fields if f.column=='min_level').edit.setText(str(value));self.page.dirty=True;self.page.save()

    def bulk(self,packet,operation='add',amount=2):
        model=Rewards(packet);targets=[('box',k) for k in self.boxes]+[('mob','1')]
        plan=model.plan_add(self.other,targets,amount=amount,box_chance='25',drop_chance='1',weight=5) if operation=='add' else model.plan_update(self.other,targets,amount=amount)
        self.queue.enqueue('Tömeges szerkesztés','bulk_rewards',dict(before=packet,plan=plan),review.reward_rows(packet,plan['tables'],plan['containers']))
        return self.queue.overlay_packet(packet)

    def file(self,name,value):
        path=self.root/'data'/name
        if not path.exists():path.write_text('{"value":1}')
        editor=self.view.data_page.editor;editor.load('data/'+name,force=True);editor.tabs.setCurrentIndex(1);editor.source.setPlainText(json.dumps(dict(value=value)));editor.save()
        return editor

    def test_two_item_saves_accumulate_without_sql_and_remain_visible(self):
        with mock.patch.object(catalog,'save') as write,mock.patch.object(review.ChangeReviewDialog,'exec') as dialog:
            self.update_item(self.material,11);self.update_item(self.other,22)
        write.assert_not_called();dialog.assert_not_called();self.assertEqual(len(self.queue.entries()),2);self.assertFalse(self.page.dirty)
        model=catalog.Catalog(self.page.packet);self.assertEqual(model.item(self.material)['base']['min_level'],'11');self.assertEqual(model.item(self.other)['base']['min_level'],'22')
        self.assertEqual(catalog.Catalog(self.packet).item(self.material)['base']['min_level'],'0')

    def test_resave_replaces_one_draft_and_keeps_original_comparison(self):
        self.update_item(self.material,11);self.update_item(self.material,22)
        entries=self.queue.entries();self.assertEqual(len(entries),1)
        row=next(row for row in entries[0]['body']['rows'] if 'Minimum szint' in row[1] or row[2]=='0' and row[3]=='22')
        self.assertEqual((row[2],row[3]),('0','22'));self.update_item(self.material,0);self.assertEqual(self.queue.entries(),[])
        self.assertEqual(self.page.collect()['base']['min_level'],'0')

    def test_bulk_add_then_update_keeps_all_destinations_and_one_shared_baseline(self):
        first=self.bulk(copy.deepcopy(self.packet));second=self.bulk(first,'update',3)
        self.assertEqual(len(self.queue.entries()),3);read=Rewards(second)
        self.assertEqual(read.loot(self.boxes[0])['entries'][-1]['amount'],'3');self.assertEqual(read.model.drop_group('1')['entries'][-1]['amount'],'3')
        self.assertEqual(len(list((self.queue.root/'baselines').iterdir())),1)
        self.assertTrue(all('before' not in e['body']['payload'] for e in self.queue.entries()))

    def test_group_tab_stages_whole_group_for_mixed_targets_after_filtering(self):
        from reward_editor import GroupDistributionDialog
        from reward_groups import GroupStore, from_keys
        from manager_navigation import DROP_PAGE

        # A drop can exist only in the live overlay, outside the base catalog.
        packet = copy.deepcopy(self.packet)
        packet['tables']['mob_drop_group'] = []
        packet['tables']['mob_drop_entry'] = []
        self.page.loaded(packet)
        entries = from_keys(Rewards(packet).model, ['%s:%s' % key for key in (self.other, self.material)])
        entries[0].update(amount='13', weight='7', chance_ppb='125000000',
                          rare_attribute_chance_bp='3500', killer_level_min='80', killer_level_max='100')
        store = GroupStore(self.view)
        group = store.save('Fejlesztő anyagok', entries)
        self.view.navigation.setCurrentRow(DROP_PAGE)
        page = self.view.drop_page
        page.tabs.setCurrentWidget(page.groups_page)
        panel = page.groups_panel
        self.assertEqual(panel.preview.rowCount(), 2)

        def select(dialog):
            dialog.kind.setCurrentIndex(1)
            dialog.check_targets(True)
            self.assertEqual(len(dialog.selected()), 2)
            dialog.kind.setCurrentIndex(2)
            dialog.check_targets(True)
            self.assertEqual(len(dialog.selected()), 3)
            dialog.accept_plan()
            return dialog.result()

        with mock.patch.object(GroupDistributionDialog, 'exec', select), \
             mock.patch('reward_catalog.save_bulk') as publish, \
             mock.patch.object(review.ChangeReviewDialog, 'exec') as confirm:
            panel.add_button.click()
        publish.assert_not_called()
        confirm.assert_not_called()
        self.assertEqual(self.errors, [])
        pending = self.queue.entries()
        self.assertEqual([edit['state'] for edit in pending], ['staged'] * 3)
        self.assertEqual(len(self.queue.summary(pending)), 6)
        rewards = Rewards(self.queue.overlay_packet(packet))
        weighted, chance = (rewards.loot(key)['entries'] for key in self.boxes)
        mob = rewards.model.drop_group('1')['entries']
        self.assertEqual([len(rows) for rows in (weighted, chance, mob)], [3, 3, 3])
        self.assertEqual([rows[1]['amount'] for rows in (weighted, chance, mob)], ['13'] * 3)
        self.assertEqual((weighted[1]['weight'], chance[1]['weight'], mob[1]['chance_ppb']),
                         ('7', '1250', '125000000'))
        self.assertEqual((mob[1]['killer_level_min'], mob[1]['killer_level_max']), ('80', '100'))
        self.assertEqual(store.get(group['id'])['entries'], entries)

    def test_group_distribution_rejects_every_target_when_one_cannot_use_chance(self):
        from reward_groups import from_keys

        rewards = Rewards(self.packet)
        entries = from_keys(rewards.model, ['%s:%s' % self.other])
        entries[0]['chance_ppb'] = '125000001'
        with self.assertRaisesRegex(api.EditorError, 'két tizedesjegyű'):
            rewards.plan_add_group(entries, [('box', self.boxes[0]), ('mob', '1'), ('box', self.boxes[1])])
        self.assertEqual(rewards.container_rows(), dict(container_groups=[], container_entries=[]))
        self.assertEqual(len(rewards.model.drop_group('1')['entries']), 1)
        self.assertEqual(self.queue.entries(), [])

    def test_group_tab_stages_removal_of_matching_rows_from_mixed_targets(self):
        from manager_navigation import DROP_PAGE
        from reward_editor import GroupDistributionDialog
        from reward_groups import GroupStore, from_keys

        packet, weapon = create(self.packet, 1, lambda document: fixtures.curve(document, 1))
        # Preserve an unselected target with matches to catch accidental cross-target removals.
        reward = dict(reward_kind='1', item_category_id=str(self.material[0]), item_local_id=str(self.material[1]),
                      amount='1', weight='10000', rare_attribute_chance_bp='0', refinement_level='0')
        packet, untouched_box = create(packet, 8, lambda doc: doc.update(loot=dict(pick_mode='2', entries=[reward])))
        rewards = Rewards(packet)
        entries = from_keys(rewards.model, ['%s:%s' % key for key in (weapon, self.material)])
        weighted_box, chance_box = self.boxes
        targets = [('box', weighted_box), ('box', chance_box), ('mob', '1')]
        plan = rewards.plan_add_group(entries, targets)
        packet['live_drops'].update(plan['containers'])
        packet['live_drops']['tables'] = {name: plan['tables'][name] for name in ('mob_drop_group', 'mob_drop_entry')}
        rewards = Rewards(packet)
        weighted = rewards.loot(weighted_box)
        weighted['entries'][-1]['amount'] = '13'
        rewards.set_loot(weighted_box, weighted)
        chance = rewards.loot(chance_box)
        keep_weapon = dict(chance['entries'][1], refinement_level='1')
        keep_yang = dict(reward_kind='2', amount='10', weight='10000', rare_attribute_chance_bp='0',
                         refinement_level='0', item_category_id=None, item_local_id=None)
        chance['entries'].extend([keep_weapon, keep_yang])
        rewards.set_loot(chance_box, chance)
        packet['live_drops'].update(rewards.container_rows())
        self.page.loaded(packet)

        store = GroupStore(self.view)
        group = store.save('Fejlesztő csoport', entries)
        self.view.navigation.setCurrentRow(DROP_PAGE)
        drops = self.view.drop_page
        drops.tabs.setCurrentWidget(drops.groups_page)

        def select(dialog):
            self.assertTrue(dialog.removing)
            self.assertEqual([dialog.match_counts[target] for target in targets], [3, 3, 3])
            dialog.kind.setCurrentIndex(1)
            dialog.check_targets(True)
            for row, target in enumerate(dialog.target_rows):
                if target['key'] == ('box', untouched_box):
                    dialog.targets.item(row, 0).setCheckState(Qt.CheckState.Unchecked)
            dialog.kind.setCurrentIndex(2)
            dialog.check_targets(True)
            self.assertEqual(set(dialog.selected()), set(targets))
            self.assertIn('9 kikerülő sor', dialog.summary.text())
            dialog.save_button.click()
            return dialog.result()

        with mock.patch.object(GroupDistributionDialog, 'exec', select), \
             mock.patch('reward_catalog.save_bulk') as publish, \
             mock.patch.object(review.ChangeReviewDialog, 'exec') as confirm:
            drops.groups_panel.remove_button.click()
        publish.assert_not_called()
        confirm.assert_not_called()
        self.assertEqual(self.errors, [])
        pending = self.queue.entries()
        self.assertEqual([edit['state'] for edit in pending], ['staged'] * 3)
        result = Rewards(self.queue.overlay_packet(packet))
        self.assertEqual([row['reward_kind'] for row in result.loot(weighted_box)['entries']], ['9'])
        self.assertEqual([row['reward_kind'] for row in result.loot(chance_box)['entries']], ['1', '2'])
        self.assertEqual(result.loot(chance_box)['entries'][0]['refinement_level'], '1')
        self.assertEqual(result.loot(untouched_box), rewards.loot(untouched_box))
        self.assertEqual(result.model.drop_groups(), [])
        self.assertEqual(len(rewards.model.drop_group('1')['entries']), 3)
        self.assertEqual(store.get(group['id']), group)

    def test_group_tab_stages_per_item_values_for_mixed_targets(self):
        from manager_navigation import DROP_PAGE
        from reward_editor import GroupDistributionDialog
        from reward_groups import GroupStore, from_keys

        packet = copy.deepcopy(self.packet)
        packet['tables']['mob_drop_group'] = []
        packet['tables']['mob_drop_entry'] = []
        rewards = Rewards(packet)
        entries = from_keys(rewards.model, ['%s:%s' % key for key in (self.material, self.other)])
        entries[0].update(rare_attribute_chance_bp='2500', killer_level_min='75', killer_level_max='105')
        selected = [('box', key) for key in self.boxes] + [('mob', '1')]
        # Only the first box and the mob contain the second template item.
        added = rewards.plan_add_group(entries[1:], [selected[0], selected[2]])
        packet['live_drops'].update(added['containers'])
        packet['live_drops']['tables'] = {name: added['tables'][name] for name in ('mob_drop_group', 'mob_drop_entry')}
        self.page.loaded(packet)
        store = GroupStore(self.view)
        group = store.save('Fejlesztő csoport', entries)
        self.view.navigation.setCurrentRow(DROP_PAGE)
        drops = self.view.drop_page
        drops.tabs.setCurrentWidget(drops.groups_page)

        def edit(dialog):
            self.assertTrue(dialog.updating)
            self.assertEqual([dialog.match_counts[key] for key in selected], [2, 1, 2])
            for row, values in enumerate((('13', '12,50', '7'), ('5', '25', '3'))):
                for column, value in enumerate(values, 2):
                    dialog.values.item(row, column).setText(value)
            dialog.check_targets(True)
            dialog.rate_enabled.setChecked(False)
            dialog.amount_enabled.setChecked(False)
            self.assertFalse(dialog.save_button.isEnabled())
            dialog.amount_enabled.setChecked(True)
            dialog.rate_enabled.setChecked(True)
            dialog.save_button.click()
            return dialog.result()

        with mock.patch.object(GroupDistributionDialog, 'exec', edit), \
             mock.patch('reward_catalog.save_bulk') as publish, \
             mock.patch.object(review.ChangeReviewDialog, 'exec') as confirm:
            drops.groups_panel.update_button.click()
        publish.assert_not_called()
        confirm.assert_not_called()
        self.assertEqual(self.errors, [])
        pending = self.queue.entries()
        self.assertEqual([edit['state'] for edit in pending], ['staged'] * 3)
        self.assertEqual(len(self.queue.summary(pending)), 5)
        result = Rewards(self.queue.overlay_packet(packet))
        first, second = (result.loot(key)['entries'] for key in self.boxes)
        mob = result.model.drop_group('1')['entries']
        self.assertEqual([(row['amount'], row['weight']) for row in first], [('13', '7'), ('5', '3')])
        self.assertEqual([(row['amount'], row['weight']) for row in second], [('13', '1250')])
        self.assertEqual([(row['amount'], row['chance_ppb']) for row in mob], [('13', '125000000'), ('5', '250000000')])
        original_mob = Rewards(packet).model.drop_group('1')['entries']
        for before, after in zip(original_mob, mob):
            self.assertEqual(dict(after, amount=before['amount'], chance_ppb=before['chance_ppb']), before)
        self.assertEqual(store.get(group['id']), group)

    def test_group_update_checks_target_precision_and_duplicate_template_values(self):
        from reward_groups import from_keys

        rewards = Rewards(self.packet)
        entries = from_keys(rewards.model, ['%s:%s' % self.material])
        selected = [('mob', '1'), ('box', self.boxes[1])]
        with self.assertRaisesRegex(api.EditorError, 'tizedesjegy'):
            rewards.plan_update_group(entries, selected, chances=['0.0000001'])
        with self.assertRaisesRegex(api.EditorError, 'eltérő darabszám'):
            rewards.plan_update_group(entries * 2, selected, amounts=['2', '3'])
        self.assertEqual(rewards.loot(self.boxes[1])['entries'][0]['weight'], '10000')
        self.assertEqual(rewards.model.drop_group('1')['entries'][0]['chance_ppb'], '1000000000')

        plan = rewards.plan_update_group(entries * 2, selected, amounts=['2', '2'])
        updated = dict(self.packet, live_drops=dict(self.packet['live_drops'], **plan['containers']))
        updated['live_drops']['tables'] = {name: plan['tables'][name] for name in ('mob_drop_group', 'mob_drop_entry')}
        result = Rewards(updated)
        self.assertEqual(result.loot(self.boxes[1])['entries'][0]['weight'], '10000')
        self.assertEqual(result.model.drop_group('1')['entries'][0]['amount'], '2')
        self.assertEqual(result.plan_update_group(entries, selected, amounts=['2'])['changed'], 0)

    def test_discard_one_destination_preserves_other_drafts(self):
        effective=self.bulk(copy.deepcopy(self.packet));entry=next(e for e in self.queue.entries() if e['body']['payload']['operation']['slot']=='container:%d:%d'%self.boxes[0])
        self.queue.discard({entry['id']});result=Rewards(self.queue.overlay_packet(self.packet))
        self.assertEqual(result.loot(self.boxes[0]),Rewards(self.packet).loot(self.boxes[0]));self.assertEqual(len(result.loot(self.boxes[1])['entries']),2)
        self.assertEqual(len(result.model.drop_group('1')['entries']),2)

    def test_restart_restores_edits_and_epoch_does_not_change_target_identity(self):
        self.bulk(copy.deepcopy(self.packet));self.view.target_epoch+=5
        queue=PendingChanges(self.view);self.view.pending_changes=queue
        self.assertEqual(len(queue.entries()),3);self.assertEqual(len(Rewards(queue.overlay_packet(self.packet)).loot(self.boxes[0])['entries']),2)
        stage=queue.entries()[0]['stage'];stage.approve(self.view);stage.begin(self.view)

    def test_other_package_is_visible_only_in_all_targets_and_cannot_be_approved(self):
        self.update_item(self.material,11);entry=self.queue.entries()[0];self.view.root=self.root/'another'
        self.assertEqual(self.queue.entries(),[]);self.assertEqual(len(self.queue.entries(current=False)),1)
        with self.assertRaises(api.EditorError):entry['stage'].approve(self.view)
        self.view.pending_page.scope_filter.setCurrentIndex(1);self.assertEqual(self.view.pending_page.table.rowCount(),1)

    def test_files_reopen_overlay_and_resave_keeps_original_hash(self):
        with mock.patch.object(self.view,'job') as job:
            editor=self.file('a.json',2);self.file('b.json',3);self.file('a.json',4)
        job.assert_not_called();self.assertEqual(len(self.queue.entries()),2);self.assertEqual((self.root/'data/a.json').read_text(),'{"value":1}')
        entry=next(e for e in self.queue.entries() if e['slot']=='file:data/a.json');self.assertEqual(entry['body']['payload']['expected_hash'],api.sha(b'{"value":1}'))
        editor.load('data/a.json',force=True);self.assertEqual(editor.document.value,dict(value=4));self.assertFalse(editor.dirty)

    def test_cancel_combined_review_leaves_every_draft_and_starts_no_worker(self):
        self.update_item(self.material,11);self.file('a.json',2)
        with mock.patch.object(review.ChangeReviewDialog,'exec',return_value=QDialog.DialogCode.Rejected),mock.patch.object(self.view.pending_page.publisher,'background') as worker:
            self.view.pending_page.publisher.start(self.queue.entries())
        worker.assert_not_called();self.assertEqual(len(self.queue.entries()),2);self.assertTrue(all(e['state']=='staged' for e in self.queue.entries()))

    def test_one_approval_publishes_multiple_live_destinations_in_one_transaction(self):
        self.bulk(copy.deepcopy(self.packet));publisher=self.view.pending_page.publisher
        with mock.patch.object(review.ChangeReviewDialog,'exec',return_value=QDialog.DialogCode.Accepted) as dialog, \
                mock.patch.object(publisher,'preflight',return_value=True),mock.patch.object(catalog,'request',return_value=copy.deepcopy(self.packet)), \
                mock.patch.object(catalog,'commit_drop_tables',return_value=copy.deepcopy(self.packet)) as writer:
            publisher.start(self.queue.entries());self.wait()
        self.assertEqual(dialog.call_count,1);writer.assert_called_once();self.assertEqual(self.queue.entries(),[]);self.assertEqual(self.errors,[])
        self.assertEqual(len(writer.call_args.kwargs['containers']['container_groups']),2)

    def test_preflight_conflict_blocks_all_writes_and_keeps_drafts(self):
        self.bulk(copy.deepcopy(self.packet));changed=copy.deepcopy(self.packet);changed['hash']='changed'
        with mock.patch.object(review.ChangeReviewDialog,'exec',return_value=QDialog.DialogCode.Accepted),mock.patch.object(catalog,'request',return_value=changed),mock.patch.object(catalog,'commit_drop_tables') as write:
            self.view.pending_page.publisher.start(self.queue.entries());self.wait()
        write.assert_not_called();self.assertEqual(len(self.queue.entries()),3);self.assertTrue(all(e['state']=='staged' for e in self.queue.entries()));self.assertTrue(self.errors)

    def test_failed_write_stops_once_and_is_not_automatically_retried(self):
        self.bulk(copy.deepcopy(self.packet));publisher=self.view.pending_page.publisher
        with mock.patch.object(review.ChangeReviewDialog,'exec',return_value=QDialog.DialogCode.Accepted),mock.patch.object(publisher,'preflight',return_value=True), \
                mock.patch.object(catalog,'request',return_value=copy.deepcopy(self.packet)),mock.patch.object(catalog,'commit_drop_tables',side_effect=api.EditorError('uncertain write')) as writer:
            publisher.start(self.queue.entries());self.wait()
        self.assertEqual(writer.call_count,1);self.assertTrue(all(e['state']=='unconfirmed' for e in self.queue.entries()))

    def test_new_items_reserve_distinct_keys_before_publication(self):
        keys=[]
        for _ in range(2):
            document=self.page.catalog_model().new(6);keys.append(document['base']['local_id'])
            self.queue.enqueue('Új tárgy','item',dict(before=self.page.packet,document=document,key=None),[('Új tárgy','Létrehozás','Nincs','Új')])
            self.page.loaded(self.queue.overlay_packet(self.packet))
        self.assertNotEqual(keys[0],keys[1]);self.assertEqual(len(self.queue.entries()),2)
        catalog.run_tool(dict(action='validate',tables=self.page.packet['tables']),self.page.packet['columns'],fixtures.TOOL)

    def test_bulk_disk_error_does_not_leave_half_a_bulk_edit_in_index(self):
        original=self.queue.put;calls=[]
        def fail(*args,**kwargs):
            calls.append(1)
            if len(calls)==2:raise OSError('disk full')
            return original(*args,**kwargs)
        with mock.patch.object(self.queue,'put',side_effect=fail),self.assertRaises(OSError):self.bulk(copy.deepcopy(self.packet))
        self.assertEqual(self.queue.entries(),[])

    def test_multiple_file_writes_use_one_approval_and_exact_durable_contents(self):
        self.file('a.json',2);self.file('b.json',3);self.file('a.json',4);jobs=[]
        def job(args,callback,**kwargs):
            self.assertTrue(kwargs['_pending']);jobs.append(args);(self.root/args[1]).write_bytes(Path(args[2]).read_bytes());callback(True)
        with mock.patch.object(review.ChangeReviewDialog,'exec',return_value=QDialog.DialogCode.Accepted) as dialog,mock.patch.object(self.view,'job',side_effect=job):
            self.view.pending_page.publisher.start(self.queue.entries());self.wait()
        self.assertEqual(dialog.call_count,1);self.assertEqual(len(jobs),2);self.assertEqual(self.queue.entries(),[])
        self.assertEqual(json.loads((self.root/'data/a.json').read_text()),dict(value=4));self.assertEqual(self.errors,[])

    def test_conflicting_second_file_blocks_entire_batch_before_first_file_write(self):
        self.file('a.json',2);self.file('b.json',3);(self.root/'data/b.json').write_text('{"external":true}')
        with mock.patch.object(review.ChangeReviewDialog,'exec',return_value=QDialog.DialogCode.Accepted),mock.patch.object(self.view,'job') as writer:
            self.view.pending_page.publisher.start(self.queue.entries());self.wait()
        writer.assert_not_called();self.assertEqual((self.root/'data/a.json').read_text(),'{"value":1}');self.assertEqual(len(self.queue.entries()),2)

    def test_real_native_preflight_validates_combined_reward_drafts(self):
        self.bulk(copy.deepcopy(self.packet));publisher=self.view.pending_page.publisher;publisher.entries=self.queue.entries()
        with mock.patch.object(catalog,'request',return_value=copy.deepcopy(self.packet)),mock.patch.object(catalog,'helper',return_value=fixtures.TOOL):
            self.assertTrue(publisher.preflight())

    def test_new_static_item_and_reward_reference_share_the_same_draft(self):
        doc=self.page.catalog_model().new(6);key=(6,int(doc['base']['local_id']))
        self.queue.enqueue('Új tárgy','item',dict(before=self.packet,document=doc,key=None),[('Új tárgy','Létrehozás','Nincs','Új')])
        packet=self.queue.overlay_packet(self.packet);model=Rewards(packet);plan=model.plan_add(key,[('box',self.boxes[0]),('mob','1')],amount=1)
        self.queue.enqueue('Új tárgy dropja','bulk_rewards',dict(before=packet,plan=plan),review.reward_rows(packet,plan['tables'],plan['containers']))
        final=self.queue.overlay_packet(self.packet);self.assertEqual(len(self.queue.entries()),3)
        self.assertEqual(Rewards(final).loot(self.boxes[0])['entries'][-1]['item_local_id'],str(key[1]))
        publisher=self.view.pending_page.publisher;publisher.entries=self.queue.entries()
        with mock.patch.object(catalog,'request',return_value=copy.deepcopy(self.packet)),mock.patch.object(catalog,'helper',return_value=fixtures.TOOL):self.assertTrue(publisher.preflight())

    def test_another_window_cannot_overwrite_a_newer_queue_index(self):
        stale=PendingChanges(self.view);stale.entries();self.update_item(self.material,11)
        with self.assertRaisesRegex(api.EditorError,'Másik kezelőablak'):
            stale.enqueue('Fájl','file',dict(relative='data/new.json',data='e30=',expected_hash='old'),[('Fájl','Érték','1','2')])
        self.assertEqual(len(self.queue.entries()),1)

    def test_two_windows_cannot_publish_the_same_batch(self):
        self.update_item(self.material,11)
        with self.queue.write_guard(),mock.patch.object(review.ChangeReviewDialog,'exec') as dialog:
            self.view.pending_page.publisher.start(self.queue.entries())
        dialog.assert_not_called();self.assertTrue(self.errors);self.assertEqual(self.queue.entries()[0]['state'],'staged')

    def test_search_does_not_remove_hidden_edits_from_approve_all(self):
        self.file('a.json',2);self.file('b.json',3);page=self.view.pending_page;page.fill();page.search.setText('a.json')
        self.assertEqual(page.table.rowCount(),1)
        with mock.patch.object(page.publisher,'start') as start:page.approve(False)
        self.assertEqual(len(start.call_args.args[0]),2)

    def test_resaved_reward_summary_keeps_both_existing_change_and_new_reward(self):
        model=Rewards(self.packet);loot=model.loot(self.boxes[0]);loot['entries'][0]['amount']='4'
        self.queue.enqueue('Láda','container',dict(before=self.packet,key=self.boxes[0],document=loot),[('Láda','Darab','1','4')])
        effective=self.queue.overlay_packet(self.packet);plan=Rewards(effective).plan_add(self.other,[('box',self.boxes[0])],amount=2)
        self.queue.enqueue('Láda bővítése','bulk_rewards',dict(before=effective,plan=plan),review.reward_rows(effective,plan['tables'],plan['containers']))
        rows=self.queue.entries()[0]['body']['rows'];self.assertEqual(len(rows),2)
        self.assertTrue(any('1 db' in r[2] and '4 db' in r[3] for r in rows));self.assertTrue(any(r[2]=='Nincs' and '2 db' in r[3] for r in rows))

    def test_repeated_quest_save_preserves_latin1_and_crlf(self):
        path=self.root/'data/example.lua';path.write_bytes(b'-- caf\xe9\r\nreturn 1\r\n')
        editor=self.view.quest_page.editor;editor.load('data/example.lua',force=True);editor.source.setPlainText('-- ascii\nreturn 2\n');editor.save()
        editor.load('data/example.lua',force=True);self.assertEqual(editor.document.encoding,'latin1');self.assertEqual(editor.document.newline,'\r\n')
        editor.source.setPlainText('-- caf\xe9\nreturn 3\n');editor.save();data=self.queue.entries()[0]['body']['payload']['data']
        self.assertEqual(base64.b64decode(data),b'-- caf\xe9\r\nreturn 3\r\n');self.assertEqual(path.read_bytes(),b'-- caf\xe9\r\nreturn 1\r\n')

    def test_old_snapshot_can_be_explicitly_adopted_without_uploading(self):
        from pending_page import LegacyDrafts
        original=b'{"value":1}';data=dict(relative='data/old.json',data=base64.b64encode(b'{"value":2}').decode(),expected_hash=api.sha(original))
        stage=review.Stage.create(self.view,'Régi tervezet','file',data,[('Régi fájl','Érték','1','2')])
        dialog=LegacyDrafts(self.view,self.queue);dialog.table.selectRow(0);dialog.adopt();dialog.deleteLater()
        self.assertEqual(len(self.queue.entries()),1);self.assertEqual(stage.read()[1]['state'],'superseded');self.assertFalse((self.root/'data/old.json').exists())


if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--tool',type=Path,required=True);args,rest=parser.parse_known_args();fixtures.TOOL=args.tool.resolve()
    unittest.main(argv=[sys.argv[0],*rest],verbosity=2)
