"""Bulk planning and Qt review; never writes a real server database."""
import argparse,copy,sys,unittest
from pathlib import Path
from unittest import mock
import CatalogEditorTests as fixtures
from CatalogEditorTests import PageCase,create,saved
import item_catalog as catalog
from reward_catalog import Rewards


class RewardTests(PageCase):
    def setUp(self):
        super().setUp()
        packet,self.old=create(catalog.blank(fixtures.TOOL),6,lambda d:d['base'].update(max_stack='200'))
        packet,self.item=create(packet,6,lambda d:d['base'].update(max_stack='200'))
        self.boxes=[]
        for mode in ('1','2','2'):
            def setup(d,mode=mode):
                d['loot']=dict(pick_mode=mode,entries=[dict(reward_kind='1',item_category_id=str(self.old[0]),item_local_id=str(self.old[1]),
                    amount='1',weight='1' if mode=='1' else '10000',rare_attribute_chance_bp='0',refinement_level='0')])
            packet,key=create(packet,8,setup);self.boxes.append(key)
        model=catalog.Catalog(packet);group=model.new_drop_group();group['group'].update(target_value='103',code='drop.review_fixture')
        group['entries']=[dict(category_id=str(self.old[0]),local_id=str(self.old[1]),refinement_level='0',amount='1',chance_ppb='1000000000',weight='0',
            rare_attribute_chance_bp='0',killer_level_min='1',killer_level_max='65535')]
        packet=saved(packet,model.apply_drops(group))
        packet['live_drops']=dict(supported=True,containers_supported=True,release='',hash=packet['hash'],base_hash=packet['hash'],
            tables={key:copy.deepcopy(packet['tables'][key]) for key in ('mob_drop_group','mob_drop_entry')},container_groups=[],container_entries=[])
        import content_names
        labels=copy.deepcopy(content_names.names())
        for key,name in [(self.item,'Új fejlesztőtárgy'),(self.old,'Régi alapanyag'),*zip(self.boxes,('Első láda','Második láda','Érintetlen láda'))]:
            labels['items'][catalog.Catalog(packet).item(key)['base']['name_key']]=name
        patch=mock.patch.object(content_names,'names',return_value=labels);patch.start();self.addCleanup(patch.stop)
        self.packet=packet

    def test_bulk_plan_preserves_unselected_contents_and_translates_chance_units(self):
        before=copy.deepcopy(self.packet);model=Rewards(self.packet)
        targets=[('box',self.boxes[0]),('box',self.boxes[1]),('mob','1')]
        plan=model.plan_add(self.item,targets,amount=2,box_chance='0.25',drop_chance='0.0004',weight=3)
        self.assertEqual(plan['changed'],3);self.assertEqual(self.packet,before)
        result=copy.deepcopy(self.packet);result['live_drops'].update(plan['containers']);after=Rewards(result)
        self.assertEqual(after.loot(self.boxes[0])['entries'][-1]['weight'],'3')
        self.assertEqual(after.loot(self.boxes[1])['entries'][-1]['weight'],'25')
        self.assertEqual(after.loot(self.boxes[2]),model.loot(self.boxes[2]))
        effective=copy.deepcopy(result);effective['tables']=plan['tables'];effective['live_drops']['tables']={k:plan['tables'][k] for k in ('mob_drop_group','mob_drop_entry')}
        self.assertEqual(Rewards(effective).model.drop_group('1')['entries'][-1]['chance_ppb'],'4000')
        self.assertEqual(plan['preview'][0]['chance'],'75%')

    def test_duplicate_skip_and_update_are_explicit(self):
        model=Rewards(self.packet);targets=[('box',self.boxes[0]),('mob','1')]
        self.assertEqual(model.plan_add(self.old,targets)['changed'],0)
        plan=model.plan_add(self.old,targets,amount=4,existing='update')
        self.assertEqual(plan['changed'],2)
        self.assertEqual(len(plan['containers']['container_entries']),1)
        self.assertEqual(plan['containers']['container_entries'][0][6],'4')

    def test_bad_stack_level_and_foreign_target_leave_source_unchanged(self):
        model=Rewards(self.packet);before=copy.deepcopy(self.packet)
        for settings in (dict(amount=201),dict(level=1),dict(box_chance='0'),dict(box_chance='100.1')):
            with self.assertRaises(catalog.api.EditorError):model.plan_add(self.item,[('box',self.boxes[1])],**settings)
        with self.assertRaises(catalog.api.EditorError):model.plan_add(self.item,[('box',(8,999999))])
        self.assertEqual(self.packet,before)

    def test_dialog_selection_filter_override_and_restoration_keep_exact_targets(self):
        from PySide6.QtCore import Qt
        from reward_editor import BulkRewardDialog
        dialog=BulkRewardDialog(self.page,self.packet,item=self.item)
        dialog.search.setText('elso');dialog.check_visible(True)
        self.assertEqual(dialog.selected(),[('box',self.boxes[0])])
        dialog.search.setText('masodik');dialog.check_visible(True)
        self.assertEqual(len(dialog.selected()),2,'filtered-away checks are retained')
        row=next(r for r in range(dialog.preview.rowCount()) if dialog.preview.item(r,0).text()=='Második láda')
        dialog.preview.item(row,2).setText('7');dialog.preview.item(row,3).setText('0.5');dialog.refresh()
        snapshot=dialog.snapshot();other=BulkRewardDialog(self.page,self.packet,state=snapshot)
        self.assertEqual(other.selected(),dialog.selected());plan=other.plan()
        target=next(r for r in plan['preview'] if r['name']=='Második láda')
        self.assertEqual((target['amount'],target['rate']),('7','0.5'));self.assertTrue(other.save_button.isEnabled())
        other.item.setEditText('Nem létező tárgy');other.refresh();self.assertFalse(other.save_button.isEnabled())
        dialog.deleteLater();other.deleteLater()

    def test_single_chest_editor_changes_any_item_and_rejects_unselected_text(self):
        from reward_editor import ContainerLootDialog
        from item_page import chosen_key
        dialog=ContainerLootDialog(self.page,self.packet,self.boxes[1])
        dialog.loot.append(dict(reward_kind='1',item_category_id=str(self.item[0]),item_local_id=str(self.item[1]),amount='3',weight='2500',rare_attribute_chance_bp='0',refinement_level='0'))
        wanted=dialog.collect();self.assertEqual(wanted['entries'][-1]['amount'],'3')
        combo=dialog.loot.table.cellWidget(1,1);combo.setEditText('Nem létező tárgy')
        with self.assertRaises(catalog.api.EditorError):dialog.collect()
        dialog.deleteLater()

    def test_remove_all_matching_rows_only_from_selected_destinations(self):
        before=copy.deepcopy(self.packet);model=Rewards(self.packet)
        loot=model.loot(self.boxes[0]);loot['entries']*=2;model.set_loot(self.boxes[0],loot)
        packet=copy.deepcopy(self.packet);packet['live_drops'].update(model.container_rows());model=Rewards(packet)
        plan=model.plan_remove(self.old,[('box',self.boxes[0]),('box',self.boxes[1]),('mob','1')])
        self.assertEqual(plan['changed'],3);self.assertEqual([r['removed'] for r in plan['preview']],[2,1,1])
        after=copy.deepcopy(packet);after['live_drops'].update(plan['containers']);read=Rewards(after)
        for key in self.boxes[:2]:self.assertEqual(read.loot(key)['entries'][0]['reward_kind'],'9')
        self.assertEqual(read.loot(self.boxes[2]),model.loot(self.boxes[2]))
        self.assertEqual(plan['tables']['mob_drop_group'],[]);self.assertEqual(plan['tables']['mob_drop_entry'],[])
        self.assertEqual(self.packet,before)

    def test_remove_level_scope_noop_and_unknown_target(self):
        model=Rewards(self.packet);targets=[('box',self.boxes[0]),('mob','1')]
        self.assertEqual(model.plan_remove(self.old,targets,level=1)['changed'],0)
        self.assertEqual(model.plan_remove(self.old,targets,level=0)['changed'],2)
        self.assertEqual(model.plan_remove(self.item,targets)['changed'],0)
        with self.assertRaises(catalog.api.EditorError):model.plan_remove(self.old,[('mob','999')])

    def test_remove_dialog_filter_hidden_selection_and_restore(self):
        from reward_editor import BulkRemoveDialog
        dialog=BulkRemoveDialog(self.page,self.packet,item=self.old)
        dialog.search.setText('elso');dialog.check_visible(True)
        dialog.search.setText('masodik');dialog.check_visible(True)
        self.assertEqual(set(dialog.selected()),{('box',self.boxes[0]),('box',self.boxes[1])})
        self.assertEqual(dialog.plan()['changed'],2);self.assertTrue(dialog.save_button.isEnabled())
        restored=BulkRemoveDialog(self.page,self.packet,state=dialog.snapshot())
        self.assertEqual(restored.selected(),dialog.selected())
        restored.level_mode.setCurrentIndex(1);restored.level.setValue(1);restored.refresh()
        self.assertFalse(restored.save_button.isEnabled());self.assertEqual(restored.plan()['changed'],0)
        dialog.item.setEditText('Nem létező tárgy');dialog.refresh()
        self.assertFalse(dialog.save_button.isEnabled());self.assertEqual(dialog.preview.rowCount(),0)
        dialog.deleteLater();restored.deleteLater()

    def test_cached_views_refresh_on_new_packet_without_losing_item_draft(self):
        self.page.loaded(self.packet);first=self.page.catalog_model();self.page.open_item(self.old)
        next(field for field in self.page.base_form.fields if field.column=='min_level').edit.setText('19');self.page.dirty=True
        fresh=copy.deepcopy(self.packet);fresh['live_drops']['release']='new_fixture_release'
        self.page.drops_saved(fresh)
        self.assertIsNot(first,self.page.catalog_model());self.assertTrue(self.page.dirty)
        self.assertEqual(self.page.collect()['base']['min_level'],'19')
        self.assertEqual(self.page.packet['live_drops']['release'],'new_fixture_release')

    def test_catalog_snapshots_keep_input_rows_isolated(self):
        source=copy.deepcopy(self.packet);model=catalog.Catalog(source,live_drops=True)
        encoded=model.encode();source['tables']['item_base'][0][0]='99';source['live_drops']['tables']['mob_drop_entry'][0][0]='999'
        self.assertEqual(model.encode(),encoded);self.assertNotEqual(model.packet['tables']['item_base'][0][0],'99')

    def test_zero_weight_attribute_candidates_roundtrip_and_share_choices(self):
        from attribute_editor import AttributePanel
        definition=next(d for d in self.packet['bonuses'] if int(d['sources'])&8)
        document=dict(attribute_profile=dict(allow_lock='0'),attribute_groups=[dict(pool_kind='1',capacity='1',exclude_static='0')],
            attribute_candidates=[dict(pool_kind='1',bonus_id=str(definition['id']),weight='0')],
            attribute_values=[dict(pool_kind='1',bonus_id=str(definition['id']),tier=str(tier),value_scaled='1000') for tier in (1,2)])
        panel=AttributePanel(lambda:None);panel.load(document,self.packet['bonuses']);result=copy.deepcopy(document);panel.collect(result)
        self.assertEqual(result['attribute_candidates'],document['attribute_candidates'])
        self.assertIs(panel.values.cellWidget(0,1).model(),panel.values.cellWidget(1,1).model())
        panel.deleteLater()

    def test_update_mixed_existing_rows_preserves_other_settings_and_unselected_chests(self):
        before=copy.deepcopy(self.packet);model=Rewards(self.packet)
        plan=model.plan_update(self.old,[('box',self.boxes[0]),('box',self.boxes[1]),('mob','1')],amount=3,weight=5,box_chance='25',drop_chance='0.0004')
        self.assertEqual(plan['changed'],3);self.assertEqual([r['altered'] for r in plan['preview']],[1,1,1])
        after=copy.deepcopy(self.packet);after['live_drops'].update(plan['containers']);read=Rewards(after)
        self.assertEqual(read.loot(self.boxes[0])['entries'][0]['weight'],'5')
        self.assertEqual(read.loot(self.boxes[1])['entries'][0]['weight'],'2500')
        self.assertEqual(read.loot(self.boxes[2]),model.loot(self.boxes[2]))
        mob=dict(zip(self.packet['columns']['mob_drop_entry'],plan['tables']['mob_drop_entry'][0]))
        self.assertEqual((mob['amount'],mob['chance_ppb'],mob['killer_level_min'],mob['rare_attribute_chance_bp']),('3','4000','1','0'))
        self.assertEqual(plan['preview'][1]['before_chance'],'100%');self.assertEqual(plan['preview'][1]['chance'],'25%')
        self.assertEqual(self.packet,before)

    def test_update_amount_only_chance_only_and_missing_item_never_adds_rows(self):
        model=Rewards(self.packet);targets=[('box',self.boxes[1]),('mob','1')]
        count=model.plan_update(self.old,targets,amount=4)
        box=count['containers']['container_entries'][0]
        self.assertEqual((box[6],box[7]),('4','10000'))
        chance=model.plan_update(self.old,targets,box_chance='5',drop_chance='7')
        self.assertEqual((chance['containers']['container_entries'][0][6],chance['containers']['container_entries'][0][7]),('1','500'))
        missing=model.plan_update(self.item,targets,amount=2)
        self.assertEqual(missing['changed'],0);self.assertEqual(missing['containers']['container_entries'],[])
        self.assertEqual(len(missing['tables']['mob_drop_entry']),1)
        self.assertEqual(model.plan_update(self.old,targets,amount=1)['changed'],0)

    def test_update_duplicates_level_filter_and_per_target_override(self):
        model=Rewards(self.packet);loot=model.loot(self.boxes[1]);loot['entries']+=copy.deepcopy(loot['entries'])
        loot['entries'][0]['rare_attribute_chance_bp']='1250';model.set_loot(self.boxes[1],loot)
        packet=copy.deepcopy(self.packet);packet['live_drops'].update(model.container_rows());model=Rewards(packet)
        self.assertEqual(model.plan_update(self.old,[('box',self.boxes[1])],amount=2,level=1)['changed'],0)
        plan=model.plan_update(self.old,[('box',self.boxes[1])],amount=2,box_chance='10',overrides={('box',self.boxes[1]):dict(amount='7',rate='0.25')})
        self.assertEqual(plan['preview'][0]['altered'],2)
        self.assertEqual([row[6:9] for row in plan['containers']['container_entries']],[['7','25','1250'],['7','25','0']])

    def test_update_invalid_values_are_atomic_and_both_fields_can_be_disabled(self):
        model=Rewards(self.packet);before=copy.deepcopy(self.packet);targets=[('box',self.boxes[1]),('mob','1')]
        for options in (dict(amount=201),dict(amount='2.5'),dict(box_chance='0'),dict(drop_chance='101'),dict(level=256,amount=2),dict()):
            with self.subTest(options=options),self.assertRaises(catalog.api.EditorError):model.plan_update(self.old,targets,**options)
        with self.assertRaises(catalog.api.EditorError):model.plan_update(self.old,[('mob','999')],amount=2)
        self.assertEqual(self.packet,before)

    def test_update_weighted_chance_preview_and_total_overflow(self):
        model=Rewards(self.packet);loot=model.loot(self.boxes[0]);other=copy.deepcopy(loot['entries'][0])
        other.update(item_local_id=str(self.item[1]),weight='3');loot['entries'].append(other);model.set_loot(self.boxes[0],loot)
        packet=copy.deepcopy(self.packet);packet['live_drops'].update(model.container_rows());model=Rewards(packet)
        plan=model.plan_update(self.old,[('box',self.boxes[0])],weight=3)
        self.assertEqual((plan['preview'][0]['before_chance'],plan['preview'][0]['chance']),('25%','50%'))
        self.assertEqual(plan['containers']['container_entries'][1][7],'3')
        with self.assertRaises(catalog.api.EditorError):model.plan_update(self.old,[('box',self.boxes[0])],weight=2**32-1)

    def test_update_dialog_only_existing_targets_partial_fields_and_restored_overrides(self):
        from PySide6.QtCore import Qt
        from reward_editor import BulkRewardDialog
        dialog=BulkRewardDialog(self.page,self.packet,item=self.old,operation='update')
        dialog.amount.setValue(2);dialog.change_chance.setChecked(False);dialog.refresh()
        dialog.search.setText('elso');dialog.check_visible(True)
        dialog.search.setText('masodik');dialog.check_visible(True)
        self.assertEqual(len(dialog.selected()),2);self.assertTrue(dialog.save_button.isEnabled())
        self.assertFalse(dialog.level.isEnabled());self.assertFalse(dialog.box_chance.isEnabled())
        self.assertFalse(dialog.preview.item(0,3).flags()&Qt.ItemFlag.ItemIsEditable)
        row=next(i for i in range(dialog.preview.rowCount()) if dialog.preview.item(i,0).text()=='Második láda')
        dialog.preview.item(row,2).setText('8');dialog.refresh()
        restored=BulkRewardDialog(self.page,self.packet,state=dialog.snapshot())
        self.assertEqual(restored.operation,'update');self.assertEqual(restored.selected(),dialog.selected())
        self.assertEqual(next(p for p in restored.plan()['preview'] if p['name']=='Második láda')['amount'],'8')
        restored.change_amount.setChecked(False);restored.refresh();self.assertFalse(restored.save_button.isEnabled())
        restored.change_amount.setChecked(True);restored.item.setCurrentIndex(restored.item.findData('%s:%s'%self.item));restored.refresh()
        self.assertEqual(restored.plan()['changed'],0);self.assertFalse(restored.save_button.isEnabled())
        self.assertTrue(all(restored.targets.isRowHidden(row) for row in range(restored.targets.rowCount())))
        dialog.deleteLater();restored.deleteLater()

    def drop_workspace(self):
        import window
        view=window.MainWindow();view.timer.stop();view.refresh=mock.Mock();view.root=Path('unused-local-package')
        page=view.item_page;page.target=view.target_id();page.state=dict(database_running=True,running=True,drop_reload=dict(available=True,containers_available=True))
        page.loaded(copy.deepcopy(self.packet))
        def cleanup():page.dirty=False;view.close();view.deleteLater()
        self.addCleanup(cleanup)
        return view,page

    def test_summary_covers_all_selected_targets_and_old_new_chance_units(self):
        import change_review
        model=Rewards(self.packet);plan=model.plan_update(self.old,[('box',self.boxes[0]),('box',self.boxes[1]),('mob','1')],amount=3,weight=5,box_chance='25',drop_chance='0.0004')
        rows=change_review.reward_rows(self.packet,plan['tables'],plan['containers'])
        self.assertEqual(len(rows),3)
        self.assertIn('Első láda',{row[0] for row in rows});self.assertIn('Második láda',{row[0] for row in rows})
        self.assertNotIn('Érintetlen láda',{row[0] for row in rows})
        self.assertTrue(any('1 db' in row[2] and '3 db' in row[3] and '0.0004%' in row[3] for row in rows))
        loot=model.loot(self.boxes[0]);loot.update(pick_mode='2');loot['entries'][0]['weight']='2500';model.set_loot(self.boxes[0],loot)
        rows=change_review.reward_rows(self.packet,model.model.encode(),model.container_rows())
        entry=next(row for row in rows if row[1]=='Jutalombejegyzés')
        self.assertIn('súly 1',entry[2]);self.assertIn('esély 25%',entry[3])

    def test_cancelled_sql_review_calls_no_writer_and_retains_full_plan(self):
        import change_review
        from PySide6.QtWidgets import QDialog
        view,page=self.drop_workspace();view.change_review_root=self.owner.change_review_root
        plan=Rewards(self.packet).plan_remove(self.old,[('box',self.boxes[0]),('mob','1')])
        writer=mock.Mock();done=mock.Mock();draft=mock.Mock()
        with mock.patch.object(change_review,'request',side_effect=lambda *a,**kw:self.real_review_request(*a,**kw,defer=False)), \
                mock.patch.object(change_review.ChangeReviewDialog,'exec',return_value=QDialog.DialogCode.Rejected),mock.patch.object(page,'run') as run:
            page.review_write('Tömeges eltávolítás','bulk_rewards',dict(before=self.packet,plan=plan),
                change_review.reward_rows(self.packet,plan['tables'],plan['containers']),writer,done,'Próba',cancelled=draft)
        self.app.processEvents();writer.assert_not_called();run.assert_not_called();done.assert_not_called();draft.assert_called_once()
        folders=list(view.change_review_root.iterdir());self.assertEqual(len(folders),1)
        stage=next(iter(folders));import gzip,json
        with gzip.open(stage/'snapshot.json.gz','rt',encoding='utf-8') as file:body=json.load(file)
        self.assertEqual(body['payload']['plan']['tables'],plan['tables']);self.assertGreater(len(body['rows']),1)

    def test_cancelled_mob_deletion_keeps_cached_catalog_and_never_starts_sql(self):
        import change_review
        from PySide6.QtWidgets import QDialog
        view,page=self.drop_workspace();view.change_review_root=self.owner.change_review_root
        previous=copy.deepcopy(page.catalog_model(True).encode())
        with mock.patch.object(change_review,'request',side_effect=lambda *a,**kw:self.real_review_request(*a,**kw,defer=False)), \
                mock.patch.object(change_review.ChangeReviewDialog,'exec',return_value=QDialog.DialogCode.Rejected),mock.patch.object(page,'run') as run:
            page.delete_drop_group('1')
        run.assert_not_called();self.assertEqual(previous,page.catalog_model(True).encode())

    def test_dedicated_drop_refresh_preserves_item_draft_tab_and_save_shortcut(self):
        view,page=self.drop_workspace();page.open_item(self.old)
        next(field for field in page.base_form.fields if field.column=='min_level').edit.setText('23');page.dirty=True
        view.navigation.setCurrentRow(10);view.drop_page.tabs.setCurrentIndex(1)
        fresh=copy.deepcopy(self.packet);fresh['live_drops']['release']='drop_page_refresh'
        page.accept_reward_refresh(fresh)
        self.assertIs(view.pages.currentWidget(),view.drop_page);self.assertIs(view.drop_page.tabs.currentWidget(),page.containers_page)
        self.assertTrue(page.dirty);self.assertEqual(page.collect()['base']['min_level'],'23')
        with mock.patch.object(page,'save') as save:view.save_current();save.assert_not_called()
        view.navigation.setCurrentRow(8);self.assertEqual(page.collect()['base']['min_level'],'23')

    def test_dedicated_drop_refresh_cannot_rebase_a_dirty_item_onto_foreign_definitions(self):
        view,page=self.drop_workspace();page.open_item(self.old);page.dirty=True
        before=page.packet;fresh=copy.deepcopy(before);fresh['hash']='different_static_catalog'
        with self.assertRaisesRegex(catalog.api.EditorError,'piszkozat'):page.accept_reward_refresh(fresh)
        self.assertIs(page.packet,before);self.assertTrue(page.dirty)

    def test_drop_page_loads_first_without_visiting_catalog_and_clears_old_target(self):
        view,page=self.drop_workspace();page.packet=None;page.document=None;page.original_key=None;page.attempted=None
        view.navigation.setCurrentRow(10);view.show();self.app.processEvents()
        with mock.patch.object(page,'reload_rewards') as load:
            page.tick(dict(database_running=True,running=False));load.assert_called_once()
        page.loaded(copy.deepcopy(self.packet));self.assertEqual(page.drop_list.rowCount(),1);self.assertEqual(page.container_list.rowCount(),3)
        page.target='previous-target';page.tick(dict(database_running=False,running=False));view.drop_page.sync()
        self.assertEqual(page.drop_list.rowCount(),0);self.assertEqual(page.container_list.rowCount(),0);self.assertIsNone(page.packet)

    def test_drop_page_keeps_bulk_actions_and_chest_link_routes_to_drop_workspace(self):
        view,page=self.drop_workspace();page.open_item(self.boxes[0])
        with mock.patch.object(page,'edit_container') as edit:page.open_container_drop();edit.assert_called_once_with(key=self.boxes[0])
        self.assertIs(view.pages.currentWidget(),view.drop_page);self.assertIs(view.drop_page.tabs.currentWidget(),page.containers_page)
        self.assertEqual(len(page.container_bulk_button.menu().actions()),3)
        with mock.patch.object(page,'open_bulk_rewards') as bulk:
            page.container_bulk_button.menu().actions()[0].trigger();bulk.assert_called_once_with(item=None,operation='update')

    def test_new_chest_has_no_hidden_reward_editor_and_starts_without_a_reward(self):
        view,page=self.drop_workspace();document=page.catalog_model().new(8);page.open_draft(document)
        self.assertTrue(page.loot_table.isHidden());self.assertTrue(page.edit_live_contents_button.isHidden())
        self.assertEqual(page.collect()['loot']['entries'][0]['reward_kind'],'9')
        self.assertEqual(page.collect()['loot']['entries'][0]['amount'],'0')
        catalog.validate(self.packet,page.collect(),executable=fixtures.TOOL)
        page.dirty=False;page.open_item(self.boxes[0])
        self.assertEqual(page.edit_live_contents_button.text(),'Megnyitás a Dropok oldalon…')


if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--tool',type=Path,required=True);args,rest=parser.parse_known_args()
    fixtures.TOOL=args.tool.resolve();unittest.main(argv=[sys.argv[0],*rest],verbosity=2)
