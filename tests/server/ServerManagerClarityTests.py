"""Navigation and visible editor state; never starts a server or writes game data."""
import os
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock
os.environ.setdefault('QT_QPA_PLATFORM', 'offscreen')
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'SRC/Tools/ServerManager'))
from PySide6.QtCore import QSettings, Qt
from PySide6.QtGui import QFontDatabase
from PySide6.QtTest import QTest
from PySide6.QtWidgets import QApplication
import window as ui
from manager_navigation import PAGES


class ClarityTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temporary = tempfile.TemporaryDirectory(prefix='auriga-clarity-settings-')
        QSettings.setDefaultFormat(QSettings.Format.IniFormat)
        QSettings.setPath(QSettings.Format.IniFormat, QSettings.Scope.UserScope, cls.temporary.name)
        cls.app = QApplication.instance() or QApplication([])
        cls.app.setStyle('Fusion')
        if os.name == 'nt': QFontDatabase.addApplicationFont('C:/Windows/Fonts/segoeui.ttf')
        cls.app.setStyleSheet(ui.STYLE)

    @classmethod
    def tearDownClass(cls): cls.temporary.cleanup()

    def setUp(self):
        self.view = ui.MainWindow()
        self.view.timer.stop()
        self.view.show()
        self.app.processEvents()

    def tearDown(self):
        for editor in self.view.editors: editor.dirty = False
        self.view.item_page.dirty = self.view.remote_page.dirty = False
        self.view.close(); self.view.deleteLater(); self.app.processEvents()

    def test_grouped_destinations_keep_routes_and_crash_indicator(self):
        v = self.view
        self.assertEqual(v.navigation.count(), 12)
        self.assertEqual(list(v.navigation.groups), ['Szerver', 'Játéktartalom', 'Üzemeltetés', 'Kapcsolatok'])
        for page in PAGES:
            v.navigation.setCurrentRow(page)
            self.assertEqual(v.pages.currentIndex(), page)
            self.assertEqual(v.page_title.text(), PAGES[page][1])
        v.navigation.item(6).setText('Összeomlások ●')
        self.assertEqual(v.navigation.item(6).text(), 'Összeomlások ●')

    def test_search_is_accent_insensitive_and_hidden_routes_can_open(self):
        v = self.view
        v.navigation_search.setText('bonusz')
        self.assertFalse(v.navigation.item(8).isHidden())
        self.assertTrue(v.navigation.item(5).isHidden())
        QTest.keyClick(v.navigation_search, Qt.Key.Key_Return)
        self.assertEqual(v.pages.currentWidget(), v.item_page)
        v.navigation.setCurrentRow(5)
        self.assertEqual(v.navigation_search.text(), '')
        self.assertEqual(v.pages.currentWidget(), v.remote_page)

    def test_drop_navigation_is_independent_from_the_item_catalog(self):
        v=self.view
        v.navigation_search.setText('drop')
        self.assertFalse(v.navigation.item(10).isHidden());self.assertTrue(v.navigation.item(8).isHidden())
        QTest.keyClick(v.navigation_search,Qt.Key.Key_Return)
        self.assertIs(v.pages.currentWidget(),v.drop_page)
        self.assertEqual(v.page_title.text(),'Dropok')
        self.assertEqual([v.drop_page.tabs.tabText(i) for i in range(v.drop_page.tabs.count())],
                         ['Mobdropok', 'Ládák tartalma', 'Tárgycsoportok'])
        v.drop_page.tabs.setCurrentWidget(v.drop_page.groups_page)
        self.assertIs(v.drop_page.tabs.currentWidget(), v.drop_page.groups_page)
        self.assertEqual(v.item_page.workspace.indexOf(v.item_page.drops_page),-1)
        self.assertEqual(v.item_page.workspace.indexOf(v.item_page.containers_page),-1)
        self.assertFalse(v.item_page.isAncestorOf(v.item_page.drop_list))
        self.assertFalse(v.item_page.isAncestorOf(v.item_page.container_list))
        self.assertEqual(v.drop_page.tabs.indexOf(v.item_page.drops_page),0)
        self.assertEqual(v.drop_page.tabs.indexOf(v.item_page.containers_page),1)
        v.navigation_search.setText('unknown-no-page')
        self.assertTrue(v.navigation_empty.isVisible())
        self.assertEqual(v.pages.currentWidget(), v.drop_page)

    def test_unsaved_menu_preserves_draft_and_opens_its_original_page(self):
        v = self.view
        v.data_page.editor.document = SimpleNamespace(relative='data/example.json')
        v.data_page.editor.dirty = True
        v.item_page.dirty = True
        v.update_unsaved_indicator()
        self.assertIn('2', v.unsaved_button.text())
        v.navigation_search.setText('ssh')
        v.populate_unsaved_menu()
        v.unsaved_menu.actions()[0].trigger()
        self.assertEqual(v.pages.currentWidget(), v.data_page)
        self.assertTrue(v.data_page.editor.dirty)
        self.assertEqual(v.data_page.editor.document.relative, 'data/example.json')

    def test_log_filters_combine_and_target_reset_cannot_restore_old_lines(self):
        v = self.view
        v.set_log_text('[info] alpha\n[error] alpha failed\n[warning] beta\n[error] beta failed')
        v.log_level.setCurrentIndex(1)
        v.log_search.setText('ALPHA')
        self.assertEqual(v.log_view.toPlainText(), '[error] alpha failed')
        self.assertEqual(v.log_count.text(), '1 / 4 sor')
        v.log_level.setCurrentIndex(2)
        self.assertEqual(v.log_view.toPlainText(), '')
        v.clear_target_pages()
        v.log_search.clear();v.log_level.setCurrentIndex(0)
        self.assertEqual(v.log_full_text, '')
        self.assertEqual(v.log_view.toPlainText(), '')

    def test_quick_file_picker_only_opens_existing_file_on_user_action(self):
        v = self.view
        with mock.patch.object(v, 'file_paths', return_value=['data/typed_shops.json', 'data/typed_crafting.json']), mock.patch.object(v.data_page.editor, 'load') as load:
            v.data_page.refresh()
            load.assert_not_called()
            picker = v.data_page.quick_files
            self.assertEqual(picker.count(), 3)
            index = picker.findData('data/typed_crafting.json')
            picker.activated.emit(index)
            load.assert_called_once_with('data/typed_crafting.json')

    def test_small_window_keeps_dashboard_inside_viewport(self):
        v = self.view
        v.resize(1024, 720); self.app.processEvents()
        self.assertEqual(v.width(), 1024)
        self.assertLessEqual(v.overview.widget().width(), v.overview.viewport().width())
        self.assertEqual(v.server_control_row.direction(), ui.QBoxLayout.Direction.TopToBottom)
        v.resize(1320, 860); self.app.processEvents()
        self.assertEqual(v.server_control_row.direction(), ui.QBoxLayout.Direction.LeftToRight)


if __name__ == '__main__': unittest.main()
