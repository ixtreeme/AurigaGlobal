"""Real Qt drag/drop delivery, item identity, cancellation and portable icons."""
import argparse,copy,json,os,sys,unittest
from pathlib import Path
from unittest import mock
import CatalogEditorTests as fixtures
from CatalogEditorTests import PageCase,create,saved
from PySide6.QtCore import QEvent,QPoint,QPointF,Qt
from PySide6.QtGui import QDragEnterEvent,QDropEvent,QMouseEvent,QStandardItem
from PySide6.QtTest import QTest
import item_catalog as catalog
from drop_editor import DropGroupDialog
from reward_editor import ContainerLootDialog
from reward_split import ITEM_MIME,ROW_MIME,mime_data
from item_icons import item_icon


class Enter(QDragEnterEvent):
    def __init__(self,source,mime,action):
        super().__init__(QPoint(30,80),action,mime,Qt.MouseButton.LeftButton,Qt.KeyboardModifier.NoModifier);self.origin=source
    def source(self):return self.origin


class Drop(QDropEvent):
    def __init__(self,source,mime,action):
        super().__init__(QPointF(30,80),action,mime,Qt.MouseButton.LeftButton,Qt.KeyboardModifier.NoModifier);self.origin=source
    def source(self):return self.origin


class RewardSplitTests(PageCase):
    def setUp(self):
        super().setUp();self.dialogs=[]
        packet,self.first=create(catalog.blank(fixtures.TOOL),6,lambda d:d['base'].update(max_stack='200',icon_asset='icon/item/00010.tga'))
        packet,self.second=create(packet,6,lambda d:d['base'].update(max_stack='200',icon_asset='icon/item/00020.tga'))
        packet,self.box=create(packet,8,lambda d:d.update(loot=dict(pick_mode='2',entries=[dict(reward_kind='1',item_category_id=str(self.first[0]),item_local_id=str(self.first[1]),amount='7',weight='1250',rare_attribute_chance_bp='2500',refinement_level='0')])))
        packet['live_drops']=dict(supported=True,containers_supported=True,release='',hash=packet['hash'],base_hash=packet['hash'],tables={key:copy.deepcopy(packet['tables'][key]) for key in ('mob_drop_group','mob_drop_entry')},container_groups=[],container_entries=[])
        import content_names
        labels=copy.deepcopy(content_names.names())
        for key in (self.first,self.second):labels['items'][catalog.Catalog(packet).item(key)['base']['name_key']]='Kianit fülbevaló'
        labels['items'][catalog.Catalog(packet).item(self.box)['base']['name_key']]='Próbaláda'
        patch=mock.patch.object(content_names,'names',return_value=labels);patch.start();self.addCleanup(patch.stop)
        self.packet=packet

    def tearDown(self):
        for dialog in self.dialogs:dialog.reject();dialog.deleteLater()
        super().tearDown()

    def chest(self):
        dialog=ContainerLootDialog(self.page,self.packet,self.box);self.dialogs.append(dialog);dialog.show();self.app.processEvents();return dialog

    def mob(self):
        dialog=DropGroupDialog(self.page,catalog.Catalog(self.packet,live_drops=True));dialog.mob.setCurrentIndex(dialog.mob.findData('103'));self.dialogs.append(dialog);dialog.show();self.app.processEvents();return dialog

    def drop(self,target,source,mime,action=Qt.DropAction.CopyAction):
        enter=Enter(source,mime,action);self.app.sendEvent(target.viewport(),enter)
        event=Drop(source,mime,action)
        if enter.isAccepted():self.app.sendEvent(target.viewport(),event)
        self.app.processEvents();return event.isAccepted()

    def add(self,dialog,keys):
        split=dialog.split;return self.drop(split.library.partner,split.library,mime_data(ITEM_MIME,split.library.session,['%s:%s'%key for key in keys]))

    def test_left_library_filters_accent_free_names_and_displays_original_icons(self):
        dialog=self.chest();split=dialog.split;split.search.setText('fulbevalo');self.app.processEvents()
        self.assertEqual(split.proxy.rowCount(),2);self.assertEqual(split.proxy.index(0,0).data(),'Kianit fülbevaló')
        self.assertFalse(split.proxy.index(0,0).data(Qt.ItemDataRole.DecorationRole).pixmap(36,36).isNull())
        self.assertEqual(split.count.text(),'2 tárgy');self.assertLess(split.widget(0).geometry().x(),split.widget(1).geometry().x())

    def test_multiple_items_drop_into_chest_without_altering_existing_settings_or_library(self):
        dialog=self.chest();before=dialog.collect();count=dialog.split.proxy.rowCount()
        self.assertTrue(self.add(dialog,[self.first,self.second]))
        after=dialog.collect();self.assertEqual(after['entries'][0],before['entries'][0]);self.assertEqual(len(after['entries']),3)
        self.assertEqual([(row['item_category_id'],row['item_local_id']) for row in after['entries'][1:]],[tuple(map(str,self.first)),tuple(map(str,self.second))])
        self.assertTrue(all(row['amount']=='1' and row['weight']=='10000' for row in after['entries'][1:]));self.assertEqual(dialog.split.proxy.rowCount(),count)

    def test_mob_drop_uses_chance_and_weight_defaults_for_the_active_roll_mode(self):
        dialog=self.mob();self.assertTrue(self.add(dialog,[self.first,self.second]));document=dialog.collect()
        self.assertTrue(all(row['chance_ppb']=='10000000' and row['weight']=='0' for row in document['entries']))
        dialog.roll.setCurrentIndex(dialog.roll.findData('2'));self.assertTrue(self.add(dialog,[self.second]));document=dialog.collect()
        self.assertTrue(all(row['chance_ppb']=='0' and row['weight']=='1' for row in document['entries']))
        catalog.validate_drops(self.packet,document,executable=fixtures.TOOL)

    def test_dragging_rows_back_removes_only_those_rows_and_keeps_library_unchanged(self):
        dialog=self.chest();self.add(dialog,[self.second,self.first]);table=dialog.loot.table;before=dialog.collect();count=dialog.split.proxy.rowCount()
        mime=mime_data(ROW_MIME,table.session,[table.token(0),table.token(2)])
        self.assertTrue(self.drop(dialog.split.library,table,mime,Qt.DropAction.MoveAction))
        self.assertEqual(dialog.collect()['entries'][0]['item_local_id'],str(self.second[1]));self.assertEqual(table.rowCount(),1)
        self.assertEqual(dialog.split.proxy.rowCount(),count);self.assertEqual(before['entries'][0]['amount'],'7')

    def test_cancelled_and_foreign_drags_never_remove_or_add_rows(self):
        dialog=self.chest();other=self.chest();table=dialog.loot.table;before=dialog.collect();token=table.token(0)
        self.assertFalse(self.drop(table,other.split.library,mime_data(ITEM_MIME,table.session,['%s:%s'%self.second])))
        self.assertFalse(self.drop(dialog.split.library,table,mime_data(ROW_MIME,table.session,[token,'stale']),Qt.DropAction.MoveAction))
        self.assertEqual(dialog.collect(),before)
        # Merely creating the drag payload has no removal side effect.
        mime_data(ROW_MIME,table.session,[token]);self.assertEqual(dialog.collect(),before)

    def test_removing_the_last_chest_row_keeps_a_valid_empty_chest(self):
        dialog=self.chest();table=dialog.loot.table
        self.assertTrue(self.drop(dialog.split.library,table,mime_data(ROW_MIME,table.session,[table.token(0)]),Qt.DropAction.MoveAction))
        self.assertEqual(table.rowCount(),0);document=dialog.collect()
        self.assertEqual(document['entries'][0]['reward_kind'],'9');self.assertEqual(document['entries'][0]['amount'],'0')
        self.assertIn('nem ad jutalmat',dialog.loot.total.text())

    def test_unknown_item_and_malformed_payload_are_ignored(self):
        dialog=self.chest();table=dialog.loot.table;before=dialog.collect()
        self.assertFalse(self.drop(table,dialog.split.library,mime_data(ITEM_MIME,table.session,['6:999999'])))
        mime=mime_data(ITEM_MIME,table.session,['6:1']);mime.setData(ITEM_MIME,b'{broken')
        self.assertFalse(self.drop(table,dialog.split.library,mime));self.assertEqual(dialog.collect(),before)

    def test_entry_limit_rejects_the_whole_batch_without_partial_changes(self):
        for dialog in (self.chest(),self.mob()):
            self.assertTrue(self.add(dialog,[self.first]));table=dialog.loot.table if hasattr(dialog,'loot') else dialog.table;before=table.rowCount()
            self.assertFalse(self.add(dialog,[self.second]*256));self.assertEqual(table.rowCount(),before)
            self.assertTrue(dialog.error.isVisible())

    def test_closing_the_editor_leaves_server_packet_and_pending_queue_unchanged(self):
        before=copy.deepcopy(self.packet);dialog=self.chest();self.add(dialog,[self.second]);dialog.reject()
        self.assertEqual(self.packet,before);self.assertIsNone(dialog.result_document)

    def test_missing_icon_has_a_visible_placeholder_and_original_icons_keep_alpha(self):
        self.assertFalse(item_icon('missing/no_icon.tga').pixmap(36,36).isNull())
        icon=item_icon('icon/item/00010.tga').pixmap(36,36).toImage();self.assertFalse(icon.isNull());self.assertTrue(icon.hasAlphaChannel())

    def test_row_header_gesture_selects_the_row_and_starts_a_removal_drag(self):
        dialog=self.chest();self.add(dialog,[self.second]);table=dialog.loot.table;header=table.verticalHeader();point=QPoint(14,header.sectionViewportPosition(1)+20)
        QTest.mousePress(header.viewport(),Qt.MouseButton.LeftButton,pos=point);self.app.processEvents()
        try:
            self.assertEqual({index.row() for index in table.selectedIndexes()},{1})
            with mock.patch.object(table,'startDrag') as started:
                move=QMouseEvent(QEvent.Type.MouseMove,QPointF(point+QPoint(15,0)),Qt.MouseButton.NoButton,Qt.MouseButton.LeftButton,Qt.KeyboardModifier.NoModifier)
                self.app.sendEvent(header.viewport(),move);started.assert_called_once_with(Qt.DropAction.MoveAction)
        finally:QTest.mouseRelease(header.viewport(),Qt.MouseButton.LeftButton,pos=point)

    def test_double_click_adds_an_item_without_editing_the_library_name(self):
        dialog=self.chest();library=dialog.split.library;index=library.model().index(0,0);name=index.data();point=library.visualRect(index).center()
        QTest.mouseClick(library.viewport(),Qt.MouseButton.LeftButton,pos=point);QTest.mouseDClick(library.viewport(),Qt.MouseButton.LeftButton,pos=point);self.app.processEvents()
        self.assertEqual(dialog.loot.table.rowCount(),2);self.assertEqual(index.data(),name)
        self.assertEqual(library.editTriggers(),library.EditTrigger.NoEditTriggers)


if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--tool',type=Path,required=True);args,rest=parser.parse_known_args()
    fixtures.TOOL=args.tool.resolve();unittest.main(argv=[sys.argv[0],*rest],verbosity=2)
