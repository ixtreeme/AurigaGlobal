"""Real Qt typing, scrolling, keyboard/mouse choice and duplicate identities."""
import os,sys,unittest
from pathlib import Path
os.environ.setdefault('QT_QPA_PLATFORM','offscreen')
ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'SRC/Tools/ServerManager'))
from PySide6.QtCore import QEvent,Qt
from PySide6.QtGui import QKeyEvent,QStandardItem,QStandardItemModel
from PySide6.QtTest import QTest
from PySide6.QtWidgets import QApplication,QDialog,QDialogButtonBox,QVBoxLayout
from item_page import item_combo,chosen_key,LootTable
from manager_ui import STYLE


class NamePickerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.app=QApplication.instance() or QApplication([]);cls.app.setStyle('Fusion');cls.app.setStyleSheet(STYLE)

    def setUp(self):
        self.dialog=QDialog();layout=QVBoxLayout(self.dialog)
        self.model=QStandardItemModel();self.model.setHorizontalHeaderLabels(['Tárgy neve','Kategória','Szinttől'])
        for i in range(40):
            item=QStandardItem('Kianit fülbevaló' if i in (1,2) else 'Kianit tárgy %02d'%i);item.setData('6:%d'%(i+1),Qt.ItemDataRole.UserRole)
            self.model.appendRow([item,QStandardItem('Alapanyag' if i!=2 else 'Kiegészítő'),QStandardItem(str(i))])
        other=QStandardItem('Acélkard');other.setData('1:1',Qt.ItemDataRole.UserRole);self.model.appendRow([other,QStandardItem('Fegyver'),QStandardItem('1')])
        self.combo=item_combo(self.model,None);layout.addWidget(self.combo)
        buttons=QDialogButtonBox(QDialogButtonBox.StandardButton.Save|QDialogButtonBox.StandardButton.Cancel);buttons.accepted.connect(self.dialog.accept);buttons.rejected.connect(self.dialog.reject);layout.addWidget(buttons)
        self.dialog.resize(650,140);self.dialog.show();self.combo.setFocus();self.app.processEvents()

    def tearDown(self):
        self.combo.completer().popup().hide();self.dialog.close();self.dialog.deleteLater();self.app.processEvents()

    def type(self,text):
        self.combo.lineEdit().selectAll();QTest.keyClicks(self.combo.lineEdit(),text);QTest.qWait(20)

    def test_typing_opens_scrollable_all_matches_without_inline_overwrite(self):
        self.type('kianit');completion=self.combo.completer();popup=completion.popup()
        self.assertTrue(popup.isVisible());self.assertEqual(self.combo.currentText(),'kianit')
        self.assertEqual(completion.completionCount(),40);self.assertEqual(popup.model().rowCount(),40)
        self.assertGreater(popup.verticalScrollBar().maximum(),0);self.assertIsNone(chosen_key(self.combo))

    def test_contains_search_is_case_insensitive(self):
        self.type('IANIT')
        self.assertEqual(self.combo.completer().completionCount(),40)
        self.assertTrue(self.combo.completer().popup().isVisible())

    def test_keyboard_chooses_a_result_beyond_first_visible_rows(self):
        self.type('kianit');popup=self.combo.completer().popup();index=popup.model().index(25,0)
        popup.setCurrentIndex(index);popup.scrollTo(index);QTest.keyClick(popup,Qt.Key.Key_Return);self.app.processEvents()
        self.assertEqual(chosen_key(self.combo),(6,26));self.assertFalse(popup.isVisible());self.assertTrue(self.dialog.isVisible())

    def test_mouse_chooses_exact_duplicate_name_with_its_own_item_key(self):
        self.type('kianit');popup=self.combo.completer().popup();index=popup.model().index(2,0)
        popup.scrollTo(index);self.app.processEvents();QTest.mouseClick(popup.viewport(),Qt.MouseButton.LeftButton,pos=popup.visualRect(index).center());self.app.processEvents()
        self.assertEqual(self.combo.currentText(),'Kianit fülbevaló');self.assertEqual(chosen_key(self.combo),(6,3))

    def test_clicking_level_column_selects_the_item_name_and_identity(self):
        self.type('kianit');popup=self.combo.completer().popup();index=popup.model().index(2,2)
        QTest.mouseClick(popup.viewport(),Qt.MouseButton.LeftButton,pos=popup.visualRect(index).center());self.app.processEvents()
        self.assertEqual(self.combo.currentText(),'Kianit fülbevaló');self.assertEqual(chosen_key(self.combo),(6,3))

    def test_escape_closes_results_without_saving_or_selecting(self):
        self.type('kianit');QTest.keyClick(self.combo.completer().popup(),Qt.Key.Key_Escape);self.app.processEvents()
        self.assertFalse(self.combo.completer().popup().isVisible());self.assertTrue(self.dialog.isVisible());self.assertIsNone(chosen_key(self.combo))

    def test_no_match_hides_popup_and_cannot_be_used_as_an_item(self):
        self.type('kianit');self.type('nincs ilyen nev')
        self.assertEqual(self.combo.completer().completionCount(),0);self.assertFalse(self.combo.completer().popup().isVisible());self.assertIsNone(chosen_key(self.combo))

    def test_programmatic_loading_never_opens_results(self):
        self.combo.setCurrentIndex(1);self.app.processEvents()
        self.assertFalse(self.combo.completer().popup().isVisible());self.assertEqual(chosen_key(self.combo),(6,2))

    def test_accented_hungarian_fragment_keeps_full_names_in_results(self):
        event=QKeyEvent(QEvent.Type.KeyPress,Qt.Key.Key_unknown,Qt.KeyboardModifier.NoModifier,'FÜL')
        self.app.sendEvent(self.combo.lineEdit(),event);self.app.processEvents();popup=self.combo.completer().popup()
        self.assertTrue(popup.isVisible());self.assertEqual(self.combo.completer().completionCount(),2)
        self.assertEqual(popup.model().index(0,0).data(),'Kianit fülbevaló')

    def test_unaccented_full_name_matches_and_selects_original_name_and_key(self):
        self.type('FULBEVALO');completion=self.combo.completer();popup=completion.popup()
        self.assertTrue(popup.isVisible());self.assertEqual(completion.completionCount(),2)
        self.assertEqual(self.combo.currentText(),'FULBEVALO')
        self.assertEqual(popup.model().index(1,0).data(),'Kianit fülbevaló')
        self.assertTrue(popup.viewport().rect().contains(popup.visualRect(popup.model().index(1,0))))
        QTest.mouseClick(popup.viewport(),Qt.MouseButton.LeftButton,pos=popup.visualRect(popup.model().index(1,2)).center());self.app.processEvents()
        self.assertEqual(self.combo.currentText(),'Kianit fülbevaló');self.assertEqual(chosen_key(self.combo),(6,3))

    def test_accent_normalization_handles_long_vowels_and_combining_marks(self):
        self.model.item(40,0).setText('Árvíztűrő Tükörfúrógép')
        self.type('arvizturo tukorfurogep');self.assertEqual(self.combo.completer().completionCount(),1)
        self.combo.lineEdit().selectAll()
        self.app.sendEvent(self.combo.lineEdit(),QKeyEvent(QEvent.Type.KeyPress,Qt.Key.Key_unknown,Qt.KeyboardModifier.NoModifier,'tu\u0308ko\u0308r'))
        self.app.processEvents();popup=self.combo.completer().popup();self.assertEqual(popup.model().rowCount(),1)
        QTest.keyClick(popup,Qt.Key.Key_Down);QTest.keyClick(popup,Qt.Key.Key_Return);self.app.processEvents()
        self.assertEqual(chosen_key(self.combo),(1,1));self.assertEqual(self.combo.currentText(),'Árvíztűrő Tükörfúrógép')

    def test_changed_and_appended_names_are_searchable_without_stale_cache(self):
        self.type('fulbevalo');self.assertEqual(self.combo.completer().completionCount(),2)
        self.combo.completer().popup().hide();self.model.item(2,0).setText('Kianit karkötő')
        added=QStandardItem('Új fülbevaló');added.setData('6:99',Qt.ItemDataRole.UserRole);self.model.appendRow([added,QStandardItem('Ékszer'),QStandardItem('10')])
        self.type('fulbevalo');self.assertEqual(self.combo.completer().completionCount(),2)
        popup=self.combo.completer().popup();popup.setCurrentIndex(popup.model().index(1,0));QTest.keyClick(popup,Qt.Key.Key_Return);self.app.processEvents()
        self.assertEqual(chosen_key(self.combo),(6,99));self.assertEqual(self.combo.currentText(),'Új fülbevaló')


if __name__=='__main__':unittest.main(verbosity=2)
