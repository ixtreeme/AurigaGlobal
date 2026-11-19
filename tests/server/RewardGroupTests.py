"""Reusable local item templates, real Qt copies and atomic validation."""
import argparse,copy,sys,unittest
from pathlib import Path
from types import SimpleNamespace
from unittest import mock
import CatalogEditorTests as fixtures
from CatalogEditorTests import PageCase,create
import RewardSplitTests as split_tests
from PySide6.QtCore import Qt,QTimer
from PySide6.QtWidgets import QDialog
import backend as api
import item_catalog as catalog
import reward_groups as groups
from reward_group_ui import ItemGroupDialog
from reward_split import GROUP_MIME,ROW_MIME,mime_data
from drop_editor import CHANCE,WEIGHT,AMOUNT,RARE,LOW,HIGH


class RewardGroupTests(PageCase):
    chest=split_tests.RewardSplitTests.chest
    mob=split_tests.RewardSplitTests.mob
    drop=split_tests.RewardSplitTests.drop
    add=split_tests.RewardSplitTests.add

    def setUp(self):
        super().setUp();self.dialogs=[]
        packet,self.first=create(catalog.blank(fixtures.TOOL),6,lambda d:d['base'].update(max_stack='200',icon_asset='icon/item/00010.tga'))
        packet,self.second=create(packet,6,lambda d:d['base'].update(max_stack='200'))
        packet,self.box=create(packet,8,lambda d:d.update(loot=dict(pick_mode='2',entries=[dict(reward_kind='1',item_category_id=str(self.first[0]),item_local_id=str(self.first[1]),amount='7',weight='1250',rare_attribute_chance_bp='2500',refinement_level='0')])))
        packet['live_drops']=dict(supported=True,containers_supported=True,release='',hash=packet['hash'],base_hash=packet['hash'],tables={key:copy.deepcopy(packet['tables'][key]) for key in ('mob_drop_group','mob_drop_entry')},container_groups=[],container_entries=[])
        self.packet=packet;self.model=catalog.Catalog(packet,live_drops=True);self.store=groups.GroupStore(self.owner)
        self.entries=groups.from_keys(self.model,['%s:%s'%self.first,'%s:%s'%self.second])
        self.entries[0].update(amount='13',chance_ppb='125000000',weight='7',rare_attribute_chance_bp='3500',killer_level_min='80',killer_level_max='100')

    def tearDown(self):
        for dialog in self.dialogs:dialog.reject();dialog.deleteLater()
        super().tearDown()

    def test_local_store_survives_reopen_edit_rename_delete_and_preserves_history(self):
        saved=self.store.save('Fejlesztő anyagok',self.entries);self.entries[0]['amount']='1'
        reopened=groups.GroupStore(self.owner);self.assertEqual(reopened.get(saved['id'])['entries'][0]['amount'],'13')
        updated=reopened.save('Új alapanyagok',saved['entries'],identity=saved['id'],expected=groups.revision(saved))
        self.assertEqual(updated['name'],'Új alapanyagok');self.assertEqual(len(list((self.store.root/'history').glob('*.json'))),1)
        reopened.delete(updated['id'],groups.revision(updated));self.assertEqual(self.store.groups(),[])
        self.assertEqual(len(list((self.store.root/'history').glob('*.json'))),2)

    def test_concurrent_changes_names_scope_and_corrupt_files_are_guarded(self):
        saved=self.store.save('Fülbevalók',self.entries)
        with self.assertRaises(api.EditorError):self.store.save('fulbevalok',self.entries)
        changed=self.store.save('Új név',self.entries,identity=saved['id'],expected=groups.revision(saved))
        with self.assertRaises(api.EditorError):self.store.delete(saved['id'],groups.revision(saved))
        self.assertEqual(self.store.get(saved['id']),changed)
        other=groups.GroupStore(SimpleNamespace(root='other-server',change_review_root=self.owner.change_review_root));self.assertEqual(other.groups(),[])
        self.store.path.write_bytes(b'{broken')
        with self.assertRaises(api.EditorError):self.store.save('Egy másik',self.entries)
        self.assertEqual(self.store.path.read_bytes(),b'{broken')

    def test_mob_capture_works_before_target_selection_and_retains_selected_rows(self):
        dialog=self.mob();dialog.mob.setCurrentIndex(-1);self.add(dialog,[self.first,self.second])
        for column,text in ((CHANCE,'12.5'),(RARE,'35')):dialog.table.cellWidget(0,column).setText(text)
        for column,value in ((AMOUNT,13),(LOW,80),(HIGH,100)):dialog.table.cellWidget(0,column).setValue(value)
        dialog.table.selectRow(0);rows=dialog.group_rows(True)
        self.assertEqual(len(rows),1)
        for field in ('amount','chance_ppb','rare_attribute_chance_bp','killer_level_min','killer_level_max'):self.assertEqual(rows[0][field],self.entries[0][field])
        self.assertEqual(len(dialog.group_rows()),2)
        dialog.target.setCurrentIndex(dialog.target.findData('1'));dialog.roll.setCurrentIndex(dialog.roll.findData('2'));dialog.table.cellWidget(0,WEIGHT).setText('37')
        self.assertEqual(dialog.group_rows(True)[0]['weight'],'37');self.assertEqual(dialog.group_rows(True)[0]['chance_ppb'],groups.DEFAULTS['chance_ppb'])

    def test_chest_capture_maps_exact_chance_and_weight_and_ignores_other_rewards(self):
        dialog=self.chest();self.add(dialog,[self.second]);dialog.loot.table.selectRow(0);rows=dialog.group_rows(True)
        self.assertEqual(len(rows),1);self.assertEqual(rows[0]['amount'],'7');self.assertEqual(rows[0]['chance_ppb'],'125000000');self.assertEqual(rows[0]['rare_attribute_chance_bp'],'2500')
        dialog.mode.setCurrentIndex(dialog.mode.findData('1'));dialog.loot.table.cellWidget(0,3).setText('37')
        self.assertEqual(dialog.group_rows(True)[0]['weight'],'37')
        dialog.loot.table.cellWidget(0,0).setCurrentIndex(dialog.loot.table.cellWidget(0,0).findData('2'))
        self.assertEqual(len(dialog.group_rows()),1)
        with self.assertRaises(api.EditorError):dialog.group_rows(True)

    def test_template_editor_keeps_both_rates_and_saves_only_local_template(self):
        before=copy.deepcopy(self.packet);dialog=ItemGroupDialog(self.page,self.model,entries=self.entries);self.dialogs.append(dialog);dialog.show();self.app.processEvents()
        self.assertFalse(dialog.table.isColumnHidden(CHANCE));self.assertFalse(dialog.table.isColumnHidden(WEIGHT));self.assertFalse(dialog.target.isVisible())
        dialog.group_name.setText('Fejlesztő csomag');dialog.table.cellWidget(0,WEIGHT).setText('23');dialog.accept_group()
        self.assertEqual(dialog.result(),QDialog.DialogCode.Accepted);self.assertEqual(dialog.result_document['entries'][0]['weight'],'23')
        self.assertEqual(dialog.result_document['entries'][0]['chance_ppb'],'125000000')
        self.assertEqual(self.packet,before)

    def test_panel_save_uses_real_modal_editor_and_local_persistence_without_staging(self):
        dialog=self.mob();panel=dialog.group_panel
        def fill():
            editor=next(w for w in self.app.topLevelWidgets() if isinstance(w,ItemGroupDialog) and w.isVisible())
            editor.group_name.setText('Fejlesztő csomag');editor.save_button.click()
        with mock.patch('change_review.Stage.create',side_effect=AssertionError('Templates must not publish or stage server edits')):
            QTimer.singleShot(0,fill);panel.edit(entries=self.entries)
        self.assertEqual(len(self.store.groups()),1);self.assertEqual(panel.list.count(),1);self.assertEqual(dialog.table.rowCount(),0)

    def test_one_template_applies_to_multiple_destinations_as_independent_copies(self):
        first=self.mob();second=self.chest();saved=self.store.save('Alapanyagok',self.entries)
        for dialog in (first,second):dialog.group_panel.refresh();self.assertTrue(dialog.group_panel.apply_current())
        self.assertEqual(first.collect()['entries'][0]['chance_ppb'],'125000000');self.assertEqual(second.collect()['entries'][1]['weight'],'1250')
        first.table.cellWidget(0,AMOUNT).setValue(2)
        self.assertEqual(second.collect()['entries'][1]['amount'],'13');self.assertEqual(self.store.get(saved['id'])['entries'][0]['amount'],'13')
        edited=copy.deepcopy(saved['entries']);edited[0]['amount']='99';self.store.save(saved['name'],edited,identity=saved['id'],expected=groups.revision(saved))
        self.assertEqual(first.collect()['entries'][0]['amount'],'2');self.assertEqual(second.collect()['entries'][1]['amount'],'13')
        catalog.validate_drops(self.packet,first.collect(),executable=fixtures.TOOL)

    def test_duplicate_name_keeps_the_group_editor_and_its_unsaved_changes(self):
        self.store.save('Csomag',self.entries);dialog=self.mob();seen=[]
        def retry():
            editor=next(w for w in self.app.topLevelWidgets() if isinstance(w,ItemGroupDialog) and w.isVisible())
            seen.append((editor.group_name.text(),editor.table.cellWidget(0,AMOUNT).value(),editor.error.text()))
            editor.group_name.setText('Másik csomag');editor.save_button.click()
        def fill():
            editor=next(w for w in self.app.topLevelWidgets() if isinstance(w,ItemGroupDialog) and w.isVisible())
            editor.group_name.setText('Csomag');editor.table.cellWidget(0,AMOUNT).setValue(17);editor.save_button.click();QTimer.singleShot(0,retry)
        QTimer.singleShot(0,fill);dialog.group_panel.edit(entries=self.entries)
        self.assertEqual(seen[0][:2],('Csomag',17));self.assertIn('már létezik',seen[0][2])
        self.assertEqual(next(g for g in self.store.groups() if g['name']=='Másik csomag')['entries'][0]['amount'],'17')

    def test_weighted_targets_use_weights_instead_of_independent_chances(self):
        mob=self.mob();mob.roll.setCurrentIndex(mob.roll.findData('2'));chest=self.chest();chest.mode.setCurrentIndex(chest.mode.findData('1'))
        self.assertTrue(mob.add_group(self.entries));self.assertTrue(chest.add_group(self.entries))
        self.assertEqual(mob.collect()['entries'][0]['weight'],'7');self.assertEqual(mob.collect()['entries'][0]['chance_ppb'],'0');self.assertEqual(chest.collect()['entries'][1]['weight'],'7')

    def test_invalid_group_is_rejected_as_a_whole_without_partial_additions(self):
        for dialog in (self.mob(),self.chest()):
            table=dialog.loot.table if hasattr(dialog,'loot') else dialog.table;before=table.rowCount()
            for changes in (dict(content_code='changed.code'),dict(amount='201'),dict(refinement_level='1'),dict(local_id='99999')):
                bad=copy.deepcopy(self.entries);bad[1].update(changes)
                self.assertFalse(dialog.add_group(bad));self.assertEqual(table.rowCount(),before)
            self.assertFalse(dialog.add_group([self.entries[0]]*257));self.assertEqual(table.rowCount(),before)
        chest=self.chest();bad=copy.deepcopy(self.entries);bad[1]['chance_ppb']='12345678'
        self.assertFalse(chest.add_group(bad));self.assertEqual(chest.loot.table.rowCount(),1)
        mob=self.mob();mob.roll.setCurrentIndex(mob.roll.findData('2'));mob.add_items(['%s:%s'%self.first]);mob.table.cellWidget(0,WEIGHT).setText(str(groups.MAX_WEIGHT))
        self.assertFalse(mob.add_group(self.entries));self.assertEqual(mob.table.rowCount(),1)

    def test_group_drag_and_drag_back_use_exact_session_and_reject_stale_templates(self):
        dialog=self.mob();saved=self.store.save('Csomag',self.entries);dialog.group_panel.refresh();panel=dialog.group_panel;table=dialog.table;dialog.split.tabs.setCurrentIndex(1)
        mime=mime_data(GROUP_MIME,table.session,[saved['id'],groups.revision(saved)])
        self.assertTrue(self.drop(table,panel.list,mime));self.assertEqual(table.rowCount(),2)
        self.assertTrue(self.drop(panel.list,table,mime_data(ROW_MIME,table.session,[table.token(0)]),Qt.DropAction.MoveAction));self.assertEqual(table.rowCount(),1)
        self.store.save('Másik név',saved['entries'],identity=saved['id'],expected=groups.revision(saved))
        self.assertFalse(self.drop(table,panel.list,mime));self.assertEqual(table.rowCount(),1)
        other=self.mob();self.assertFalse(self.drop(table,other.group_panel.list,mime));self.assertEqual(table.rowCount(),1)

    def test_group_name_and_contents_search_are_accent_insensitive(self):
        self.store.save('Fülbevalók',self.entries);dialog=self.mob();panel=dialog.group_panel
        panel.search.setText('fulbevalok');self.assertEqual(panel.list.count(),1)
        self.assertTrue(panel.preview.item(0,0).text());self.assertFalse(panel.preview.item(0,0).icon().isNull())
        panel.search.setText('no-such-group');self.assertEqual(panel.list.count(),0);self.assertFalse(panel.add_button.isEnabled())


if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--tool',type=Path,required=True);args,rest=parser.parse_known_args()
    fixtures.TOOL=args.tool.resolve();unittest.main(argv=[sys.argv[0],*rest],verbosity=2)
