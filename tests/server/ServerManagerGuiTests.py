"""Qt6 interaction tests on a disposable copy; requires requirements.txt."""
import argparse
import json
import os
from pathlib import Path
import shutil
import sys
import tempfile
import time
import unittest
from unittest import mock

os.environ.setdefault('QT_QPA_PLATFORM', 'offscreen')
REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / 'SRC/Tools/ServerManager'))
from PySide6.QtCore import QItemSelectionModel, QPoint, QSettings, Qt
from PySide6.QtGui import QFontDatabase
from PySide6.QtTest import QTest
from PySide6.QtWidgets import QApplication, QComboBox, QLineEdit
import backend as api
import window as ui
PACKAGE = None


class ServerManagerGuiTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not PACKAGE:
            raise unittest.SkipTest('Pass --package for disposable GUI integration tests')
        cls.temp = tempfile.TemporaryDirectory(prefix='auriga-manager-gui-tests-')
        cls.base = Path(cls.temp.name).resolve()
        assert cls.base.parent == Path(tempfile.gettempdir()).resolve()
        assert cls.base.name.startswith('auriga-manager-gui-tests-')
        cls.root = cls.base / 'package'
        shutil.copytree(PACKAGE, cls.root, ignore=shutil.ignore_patterns('run', 'manager', '__pycache__'))
        QSettings.setDefaultFormat(QSettings.Format.IniFormat)
        QSettings.setPath(QSettings.Format.IniFormat, QSettings.Scope.UserScope, str(cls.base / 'settings'))
        cls.app = QApplication.instance() or QApplication([])
        if os.name == 'nt':
            for font in ('segoeui.ttf', 'consola.ttf'):
                QFontDatabase.addApplicationFont(str(Path(os.environ.get('WINDIR', 'C:/Windows')) / 'Fonts' / font))
        cls.app.setStyleSheet(ui.STYLE)

    @classmethod
    def tearDownClass(cls):
        assert cls.base.parent == Path(tempfile.gettempdir()).resolve()
        assert cls.base.name.startswith('auriga-manager-gui-tests-')
        cls.temp.cleanup()

    def setUp(self):
        self.window = ui.MainWindow(self.root)
        self.errors = []
        self.window.error = self.errors.append
        self.window.show()
        self.app.processEvents()

    def tearDown(self):
        self.assertFalse(self.window.busy())
        for editor in self.window.editors:
            editor.dirty = False
        self.window.close()
        self.window.deleteLater()
        self.app.processEvents()
        self.assertFalse((self.root / 'run/mariadb').exists())

    def wait_job(self):
        deadline = time.monotonic() + 25
        while self.window.busy() and time.monotonic() < deadline:
            QTest.qWait(25)
        self.app.processEvents()
        self.assertFalse(self.window.busy(), 'Worker did not finish')

    def item_at(self, fields, path):
        def search(item):
            if item.data(0, ui.PATH_ROLE) == path:
                return item
            for i in range(item.childCount()):
                found = search(item.child(i))
                if found:
                    return found
        return search(fields.tree.topLevelItem(0))

    def form_page(self, scope, category):
        form = self.window.config_editor.config_form
        self.window.navigation.setCurrentRow(1)
        index = form.scopes.findData('/'.join(scope))
        self.assertGreaterEqual(index, 0)
        form.scopes.setCurrentIndex(index)
        index = next(i for i in range(form.categories.count())
                     if form.categories.item(i).data(Qt.ItemDataRole.UserRole) == category)
        form.categories.setCurrentRow(index)
        self.app.processEvents()
        return form

    def test_open_masks_credentials_and_leaves_database_untouched(self):
        self.assertEqual(self.window.core_table.rowCount(), 15)
        self.assertEqual(len(self.window.channel_boxes), 6)
        self.assertTrue(self.window.channel_boxes[0].isChecked())
        self.assertFalse(self.window.start_button.isEnabled())
        form = self.form_page(('common',), 'sql.player')
        password = form.controls[('common', 'sql', 'player', 'password')]
        self.assertEqual(password.echoMode(), QLineEdit.EchoMode.Password)
        self.assertEqual(form.mode.currentIndex(), 0)
        self.assertFalse(self.window.console_panel.isVisible())
        self.assertFalse(self.window.config_editor.tabs.isTabVisible(1))

    def test_cores_start_and_stop_one_by_one_from_the_table(self):
        state = api.describe(self.root)
        state = dict(state, running=True, phase='Fut', core_control=True)
        state['rows'] = [dict(row, running=row['name'] in ('database', 'login', 'ch1-core1')) for row in state['rows']]
        names = [row['name'] for row in state['rows']]
        jobs = []
        def select(*rows):
            # Several rows, as a Ctrl-click would select them (selectRow replaces the selection).
            table = self.window.core_table
            table.clearSelection()
            for name in rows:
                table.selectionModel().select(table.model().index(names.index(name), 0),
                    QItemSelectionModel.SelectionFlag.Select | QItemSelectionModel.SelectionFlag.Rows)
            self.app.processEvents()
        def selected():
            return sorted(index.row() for index in self.window.core_table.selectionModel().selectedRows())
        click = lambda widget: QTest.mouseClick(widget, Qt.MouseButton.LeftButton)
        with mock.patch.object(self.window, 'state', return_value=state), \
                mock.patch.object(self.window, 'job', side_effect=lambda arguments, callback=None: jobs.append(arguments)):
            self.window.refresh()
            self.assertFalse(self.window.start_cores_button.isEnabled(), 'nothing selected')
            self.assertFalse(self.window.stop_cores_button.isEnabled())
            select('ch2-core1')
            self.assertTrue(self.window.start_cores_button.isEnabled())
            self.assertFalse(self.window.stop_cores_button.isEnabled(), 'a stopped core cannot be stopped')
            self.window.refresh()
            self.assertEqual(selected(), [names.index('ch2-core1')], 'a status poll keeps the selection')
            click(self.window.start_cores_button)
            self.assertEqual(jobs[-1], ['start-cores', '--instance', 'ch2-core1'])
            select('ch1-core1', 'ch2-core1')
            with mock.patch.object(ui.QMessageBox, 'question', return_value=ui.QMessageBox.StandardButton.No):
                click(self.window.stop_cores_button)
            self.assertEqual(len(jobs), 1, 'declining the confirmation stops nothing')
            with mock.patch.object(ui.QMessageBox, 'question', return_value=ui.QMessageBox.StandardButton.Yes):
                click(self.window.stop_cores_button)
            self.assertEqual(jobs[-1], ['stop-cores', '--instance', 'ch1-core1'], 'only the running one of the selection')
            select('database', 'login')
            self.assertFalse(self.window.start_cores_button.isEnabled())
            self.assertFalse(self.window.stop_cores_button.isEnabled(), 'Database and LoginServer stay with the stack')
            self.assertTrue(self.window.start_button.isEnabled(), 'a running stack can take more channels')
            click(self.window.start_button)
            self.assertEqual(jobs[-1], ['start-cores', '--channel', '1'])
            state['core_control'] = False
            state['core_control_supported'] = True
            select('ch2-core1')
            self.window.refresh()
            self.assertFalse(self.window.start_cores_button.isEnabled())
            self.assertFalse(self.window.start_button.isEnabled())
            self.assertIn('indítsd újra', self.window.start_cores_button.toolTip())
            state['core_control_supported'] = False
            self.window.refresh()
            self.assertIn('manage.py', self.window.start_cores_button.toolTip())

    def test_real_field_edit_and_async_native_save(self):
        editor = self.window.config_editor
        self.window.navigation.setCurrentRow(1)
        old = editor.document.original
        core = next(iter(editor.config_form.value['game_servers']))
        form = self.form_page(('game_servers', core), 'server')
        path = ('game_servers', core, 'server', 'max_level')
        control = form.controls[path]
        control.selectAll()
        QTest.keyClicks(control, '161')
        QTest.keyClick(control, Qt.Key.Key_Return)
        self.app.processEvents()
        self.assertEqual(api.value_at(form.value, path), 161)
        self.assertTrue(editor.dirty)
        QTest.mouseClick(editor.save_button, Qt.MouseButton.LeftButton)
        self.wait_job()
        self.assertEqual(self.errors, [])
        self.assertFalse(editor.dirty)
        self.assertEqual(api.value_at(api.strict_json((self.root / 'server.json').read_text()), path), 161)
        self.assertIn(old, [p.read_bytes() for p in api.backup_files(self.root, 'server.json')])
        self.assertEqual(list((self.root / 'run/editor-pending').iterdir()), [])
        self.assertTrue(self.window.console_panel.isVisible())

    def test_port_collision_is_rejected_without_replacing_config(self):
        editor = self.window.config_editor
        before = editor.document.original
        form = self.form_page(('login',), 'general')
        control = form.controls[('login', 'port')]
        control.selectAll()
        QTest.keyClicks(control, str(form.value['database']['network']['bind_port']))
        editor.save()
        self.wait_job()
        self.assertEqual((self.root / 'server.json').read_bytes(), before)
        self.assertTrue(editor.dirty)
        self.assertIn('overlapping', self.window.console.toPlainText())

    def test_json_views_and_row_duplicate_remove(self):
        path = self.root / 'data/gui-fixture.json'
        path.write_bytes(b'{"rows": [{"vnum": 42, "enabled": true}]}\n')
        editor = self.window.data_page.editor
        self.window.navigation.setCurrentRow(2)
        self.assertTrue(editor.load('data/gui-fixture.json'))
        fields = editor.fields
        fields.table.selectRow(0)
        fields.duplicate()
        self.assertEqual(len(fields.value['rows']), 2)
        fields.table.selectRow(1)
        fields.remove()
        self.assertEqual(len(fields.value['rows']), 1)
        editor.tabs.setCurrentIndex(1)
        self.assertIn('42', editor.source.toPlainText())
        editor.source.setPlainText('{broken')
        editor.tabs.setCurrentIndex(0)
        self.assertEqual(editor.tabs.currentIndex(), 1)
        self.assertTrue(self.errors)
        editor.save()
        self.assertFalse(self.window.busy())
        self.assertEqual(path.read_bytes(), b'{"rows": [{"vnum": 42, "enabled": true}]}\n')

    def test_table_edits_preserve_numbers_and_booleans(self):
        path = self.root / 'data/table-fixture.json'
        path.write_bytes(b'{"rows": [{"vnum": 42, "enabled": true}]}\n')
        editor = self.window.data_page.editor
        self.window.navigation.setCurrentRow(2)
        editor.load('data/table-fixture.json')
        fields = editor.fields
        fields.table.editItem(fields.table.item(0, 0))
        self.app.processEvents()
        control = fields.table.findChild(QLineEdit)
        self.assertIsNotNone(control)
        control.selectAll()
        QTest.keyClicks(control, '43')
        QTest.keyClick(control, Qt.Key.Key_Return)
        self.app.processEvents()
        fields.table.editItem(fields.table.item(0, 1))
        self.app.processEvents()
        control = fields.table.findChild(QComboBox)
        self.assertIsNotNone(control)
        control.setCurrentIndex(0)
        QTest.keyClick(control, Qt.Key.Key_Return)
        self.app.processEvents()
        self.assertEqual(fields.value['rows'], [{'vnum': 43, 'enabled': False}])
        self.assertIs(type(fields.value['rows'][0]['vnum']), int)

    def test_inherited_fields_can_be_overridden_and_reset_without_expansion(self):
        form = self.window.config_editor.config_form
        core = next(iter(form.value['game_servers']))
        form = self.form_page(('game_servers', core), 'network')
        path = ('game_servers', core, 'network', 'bind_ip')
        self.assertNotIn('bind_ip', form.value['game_servers'][core]['network'])
        defaults = json.dumps(form.value['game_defaults'], sort_keys=True)
        control = form.controls[path]
        control.selectAll()
        QTest.keyClicks(control, '127.0.0.2')
        self.assertEqual(api.value_at(form.value, path), '127.0.0.2')
        self.assertEqual(form.origin_labels[path].text(), 'Saját beállítás')
        form.reset_buttons[path].click()
        self.assertNotIn('bind_ip', form.value['game_servers'][core]['network'])
        self.assertEqual(json.dumps(form.value['game_defaults'], sort_keys=True), defaults)
        self.assertEqual(form.controls[path].text(), '127.0.0.1')

    def test_saving_while_typing_in_a_table_cell_uses_the_new_value(self):
        path = self.root / 'data/pending-cell.json'
        path.write_bytes(b'{"rows": [{"vnum": 42}]}\n')
        editor = self.window.data_page.editor
        self.window.navigation.setCurrentRow(2)
        editor.load('data/pending-cell.json')
        editor.fields.table.editItem(editor.fields.table.item(0, 0))
        self.app.processEvents()
        control = editor.fields.table.findChild(QLineEdit)
        control.selectAll()
        QTest.keyClicks(control, '77')
        self.assertTrue(editor.dirty)
        self.assertTrue(editor.save_button.isEnabled())
        # Save without Enter or leaving the active cell (the Ctrl+S path).
        editor.save()
        self.wait_job()
        self.assertEqual(self.errors, [])
        self.assertEqual(json.loads(path.read_bytes())['rows'][0]['vnum'], 77)

    def test_invalid_active_table_cell_cannot_silently_save_the_old_value(self):
        path = self.root / 'data/invalid-cell.json'
        original = b'{"rows": [{"vnum": 42}]}\n'
        path.write_bytes(original)
        editor = self.window.data_page.editor
        self.window.navigation.setCurrentRow(2)
        editor.load('data/invalid-cell.json')
        editor.fields.table.editItem(editor.fields.table.item(0, 0))
        self.app.processEvents()
        control = editor.fields.table.findChild(QLineEdit)
        control.selectAll()
        QTest.keyClicks(control, 'invalid')
        editor.save()
        self.assertTrue(self.errors)
        self.assertFalse(self.window.busy())
        self.assertEqual(path.read_bytes(), original)
        QTest.keyClick(control, Qt.Key.Key_Escape)
        self.app.processEvents()

    def test_invalid_numeric_draft_blocks_save_and_survives_navigation(self):
        editor = self.window.config_editor
        core = next(iter(editor.config_form.value['game_servers']))
        form = self.form_page(('game_servers', core), 'server')
        path = ('game_servers', core, 'server', 'max_level')
        before = (self.root / 'server.json').read_bytes()
        control = form.controls[path]
        control.selectAll()
        QTest.keyClick(control, Qt.Key.Key_Backspace)
        editor.save()
        self.assertFalse(self.window.busy())
        self.assertTrue(self.errors)
        self.assertEqual((self.root / 'server.json').read_bytes(), before)
        self.form_page(('common',), 'general')
        self.form_page(('game_servers', core), 'server')
        self.assertEqual(form.controls[path].text(), '')
        QTest.keyClicks(form.controls[path], '160')
        self.assertFalse(form.errors)

    def test_save_toolbar_stays_visible_with_long_paths_at_small_window_size(self):
        self.window.resize(1100, 760)
        self.window.navigation.setCurrentRow(2)
        editor = self.window.data_page.editor
        path = self.root / 'data/layout-fixture.json'
        path.write_text('{}')
        editor.load('data/layout-fixture.json')
        editor.filename.setText('folder/' * 70 + 'very-long-file-name.json')
        self.app.processEvents()
        last = editor.save_button.mapTo(self.window, QPoint(editor.save_button.width(), editor.save_button.height()))
        self.assertTrue(self.window.rect().contains(last))
        self.assertLessEqual(self.window.width(), 1100)

    def test_runtime_lua_save_preserves_latin1_crlf(self):
        path = self.root / 'data/quest/gui-example.lua'
        path.write_bytes(b'-- \xe1\xe9\r\nreturn 1\r\n')
        editor = self.window.quest_page.editor
        self.window.navigation.setCurrentRow(3)
        self.assertTrue(editor.load('data/quest/gui-example.lua'))
        editor.source.setPlainText('-- \xe1\xe9\nreturn 2\n')
        editor.save()
        self.wait_job()
        self.assertEqual(self.errors, [])
        self.assertEqual(path.read_bytes(), b'-- \xe1\xe9\r\nreturn 2\r\n')

    def test_invalid_json_opens_as_source_and_can_be_repaired(self):
        path = self.root / 'data/repair-fixture.json'
        path.write_bytes(b'{broken')
        editor = self.window.data_page.editor
        self.window.navigation.setCurrentRow(2)
        self.assertTrue(editor.load('data/repair-fixture.json'))
        self.assertEqual(editor.tabs.currentIndex(), 1)
        editor.code.validate_now()
        self.assertTrue(editor.source.issues)
        editor.source.setPlainText('{"fixed": true}')
        editor.save()
        self.wait_job()
        self.assertEqual(self.errors, [])
        self.assertEqual(path.read_bytes(), b'{"fixed": true}')
        self.assertEqual(api.backup_files(self.root, editor.document.relative)[0].read_bytes(), b'{broken')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--package', type=Path)
    args, rest = parser.parse_known_args()
    PACKAGE = args.package.resolve() if args.package else None
    unittest.main(argv=[sys.argv[0], *rest])
