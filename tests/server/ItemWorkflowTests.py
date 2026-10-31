"""Guided creation, browsing, review and draft protection; no database writes."""
import argparse
import copy
from pathlib import Path
import sys
import unittest
from unittest import mock

import CatalogEditorTests as fixtures
from CatalogEditorTests import PageCase, create, key_of
import item_catalog as catalog


class ItemWorkflowTests(PageCase):
    def test_wizard_builds_valid_unsaved_items_in_every_category(self):
        from item_workflows import NewItemDialog
        packet = catalog.blank(fixtures.TOOL)
        before = copy.deepcopy(packet)
        for category in catalog.CATEGORIES:
            with self.subTest(category=category):
                dialog = NewItemDialog(self.page, packet, category)
                dialog.name.setText('Árvíztűrő próbatárgy')
                dialog.level.setValue(20)
                dialog.create()
                self.assertIsNotNone(dialog.result_document)
                document = dialog.result_document
                self.assertEqual(document['base']['content_code'], catalog.CODE_PREFIX[category] + '.arvizturo_probatargy')
                self.assertEqual(document['base']['min_level'], '20')
                catalog.validate(packet, document, executable=fixtures.TOOL)
                dialog.deleteLater()
        self.assertEqual(packet, before, 'the wizard must not write or consume an item ID')

    def test_wizard_rejects_collision_and_preserves_manually_edited_code(self):
        from item_workflows import NewItemDialog
        packet, keys = self.fixture()
        dialog = NewItemDialog(self.page, packet)
        dialog.name.setText('Egyedi kard')
        dialog.code.setText('weapon.sword_iron'); dialog.code.textEdited.emit('weapon.sword_iron')
        dialog.name.setText('Másik belső elnevezés')
        self.assertEqual(dialog.code.text(), 'weapon.sword_iron')
        dialog.create()
        self.assertIsNone(dialog.result_document)
        self.assertIn('foglalt', dialog.error.text())
        dialog.code.setText('weapon.second_sword'); dialog.create()
        self.assertIsNotNone(dialog.result_document)
        dialog.deleteLater()

    def test_bow_creation_supplies_valid_ammunition_policy(self):
        from item_workflows import NewItemDialog
        packet = catalog.blank(fixtures.TOOL)
        dialog = NewItemDialog(self.page, packet)
        dialog.name.setText('Vadászíj')
        dialog.subtype.setCurrentIndex(dialog.subtype.findData('3'))
        dialog.create()
        self.assertEqual(dialog.result_document['ammo']['units_per_hit'], '1')
        catalog.validate(packet, dialog.result_document, executable=fixtures.TOOL)
        dialog.deleteLater()

    def test_template_creation_copies_saved_properties_without_changing_source(self):
        from item_workflows import NewItemDialog
        packet, keys = self.fixture()
        source = catalog.Catalog(packet).item(keys[1]); before = copy.deepcopy(packet)
        dialog = NewItemDialog(self.page, packet, source=source)
        dialog.create()
        result = dialog.result_document
        self.assertNotEqual(key_of(result), keys[1])
        self.assertEqual(result['child']['physical_attack_base_raw'], source['child']['physical_attack_base_raw'])
        self.assertEqual(result['base']['name_key'], source['base']['name_key'])
        self.assertEqual(result['appearance'], source['appearance'])
        catalog.validate(packet, result, executable=fixtures.TOOL)
        self.assertEqual(packet, before)
        dialog.deleteLater()

    def test_browser_selection_is_separate_from_opening_and_draft_survives_filters(self):
        packet, keys = self.fixture(); p = self.page
        p.loaded(packet)
        p.list.selectRow(0)
        self.assertIsNone(p.document)
        self.assertEqual(p.workspace.currentWidget(), p.catalog_page)
        p.edit_selected()
        self.assertEqual(p.workspace.currentWidget(), p.editor_page)
        field = next(f for f in p.base_form.fields if f.column == 'min_level')
        field.edit.setText('37'); field.edit.textEdited.emit('37')
        p.back_to_catalog()
        p.search.setText('no_such_item_zzz')
        self.assertEqual(p.list.rowCount(), 0)
        self.assertTrue(p.dirty)
        self.assertIn('Nincs találat', p.empty_list.text())
        p.resume_editing()
        self.assertEqual(p.collect()['base']['min_level'], '37')
        p.clear_filters(); p.dirty = False

    def test_review_shows_old_and_new_display_values(self):
        packet, keys = self.fixture(); p = self.page
        p.loaded(packet, keys[1])
        field = next(f for f in p.category_forms[1].fields if f.column == 'physical_attack_base_raw')
        field.edit.setText('20,5'); field.edit.textEdited.emit('20,5')
        p.tabs.setCurrentWidget(p.review_page)
        self.assertEqual(p.changes_table.rowCount(), 1)
        self.assertIn('Fizikai támadás', p.changes_table.item(0, 0).text())
        self.assertEqual(p.changes_table.item(0, 1).text(), '15.5 pont')
        self.assertEqual(p.changes_table.item(0, 2).text(), '20.5 pont')
        p.dirty = False

    def test_invalid_field_review_navigates_to_the_correct_section(self):
        packet, keys = self.fixture(); p = self.page
        p.loaded(packet, keys[1])
        field = next(f for f in p.base_form.fields if f.column == 'min_level')
        field.edit.setText('hibás'); field.edit.textEdited.emit('hibás')
        p.tabs.setCurrentWidget(p.review_page)
        self.assertEqual(p.tabs.currentWidget(), p.base_page)
        self.assertTrue(field.edit.property('invalid'))
        self.assertIn('Szükséges karakterszint', p.inline_error.text())
        p.dirty = False

    def test_hidden_draft_and_running_server_cannot_save_via_shortcut(self):
        packet, keys = self.fixture(); p = self.page
        p.loaded(packet, keys[1]); p.dirty = True
        p.back_to_catalog()
        with mock.patch.object(p, 'run') as run:
            p.save()
        run.assert_not_called()
        p.resume_editing()
        with mock.patch.object(p, 'local', return_value=True), mock.patch.object(p, 'run') as run:
            p.state = dict(database_running=True, running=True)
            p.save()
        run.assert_not_called()
        self.assertIn('állítsd le', p.inline_error.text())
        p.state = {}; p.dirty = False

    def test_section_navigation_keeps_the_document_identical(self):
        packet, keys = self.fixture(); p = self.page
        p.loaded(packet, keys[1]); before = p.collect()
        for index in range(p.section_nav.count()):
            p.section_nav.setCurrentRow(index)
        self.assertEqual(p.collect(), before)
        self.assertFalse(p.dirty)
        self.assertEqual(p.section_nav.currentRow(), p.tabs.currentIndex())

    def test_overview_follows_consumable_effect_changes(self):
        packet, keys = self.fixture(); p = self.page
        p.loaded(packet, keys[5])
        p.consumable_kind.setCurrentIndex(p.consumable_kind.findData('3'))
        p.tabs.setCurrentWidget(p.overview_page)
        p.refresh_item_overview()
        self.assertIn('Kapcsolható harmat', p.overview_details.text())
        self.assertIsNotNone(p.collect()['toggle'])
        p.dirty = False

    def test_exhausted_identity_range_is_an_inline_creation_error(self):
        from item_workflows import NewItemDialog
        packet = catalog.blank(fixtures.TOOL)
        packet['sequences']['1'] = '4294967296'
        dialog = NewItemDialog(self.page, packet)
        dialog.name.setText('Új kard'); dialog.create()
        self.assertIsNone(dialog.result_document)
        self.assertIn('elfogyott', dialog.error.text())
        dialog.deleteLater()

    def test_destructive_confirmation_defaults_to_cancel(self):
        from PySide6.QtCore import QTimer
        from PySide6.QtWidgets import QMessageBox
        from manager_ui import confirm_action
        seen = {}
        def dismiss():
            dialog = next(widget for widget in self.app.topLevelWidgets() if isinstance(widget, QMessageBox) and widget.isVisible())
            seen['default'] = dialog.defaultButton().text()
            next(button for button in dialog.buttons() if dialog.buttonRole(button) == QMessageBox.ButtonRole.RejectRole).click()
        QTimer.singleShot(30, dismiss)
        self.assertFalse(confirm_action(self.page, 'Példány törlése', 'Ellenőrzési párbeszéd.', 'Példány törlése', destructive=True))
        self.assertEqual(seen['default'], 'Mégse')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--tool', type=Path, default=Path(__file__).resolve().parents[2] / 'build/SRC/ItemDomain/RelWithDebInfo/CatalogEditorTool.exe')
    args, rest = parser.parse_known_args()
    fixtures.TOOL = args.tool.resolve()
    unittest.main(argv=[sys.argv[0], *rest], verbosity=2)
