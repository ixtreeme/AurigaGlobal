"""Source-editor interactions and real native syntax checks, without services."""
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time
import unittest
from unittest import mock

os.environ.setdefault('QT_QPA_PLATFORM', 'offscreen')
REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / 'SRC/Tools/ServerManager'))
from PySide6.QtCore import Qt
from PySide6.QtGui import QTextCursor
from PySide6.QtTest import QTest
from PySide6.QtWidgets import QApplication
import backend as api
from code_editor import CodePane, utf16
from editor_checks import NativeRequest, json_issues, language_for
from generate_quest_api import render, tables
from quest_api_catalog import FUNCTIONS


class EditorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.app = QApplication.instance() or QApplication([])

    def setUp(self):
        self.pane = CodePane()
        self.pane.resize(850, 650)
        self.pane.show()
        self.app.processEvents()

    def tearDown(self):
        self.pane.shutdown()
        self.pane.close()
        self.pane.deleteLater()
        self.app.processEvents()

    def load(self, text, relative='data/example.json'):
        self.pane.edit.setPlainText(text)
        self.pane.set_context(relative)
        self.app.processEvents()

    def test_language_detection_includes_custom_quest_and_runtime_objects(self):
        for path, lang in [('data/x.json', 'json'), ('data/quest/Quest/test.lua', 'quest'),
            ('data/quest/x.quest', 'quest'), ('data/quest/object/state/test', 'lua'),
            ('data/quest/object/x/login.start', 'lua'), ('data/x.txt', 'text')]:
            self.assertEqual(language_for(path), lang)

    def test_json_location_duplicates_and_nonfinite_values(self):
        issue = json_issues('{\n"x": ]\n}')[0]
        self.assertEqual((issue.line, issue.column), (2, 6))
        for text in ('{"a": 1, "a": 2}', '{"x": NaN}', '{"x": Infinity}', '{"x": 1e999}'):
            self.assertTrue(json_issues(text))
        self.assertFalse(json_issues('{"x": [true, null, -12.5]}'))
        self.assertFalse(json_issues('{"a": 1, "child": {"a": 2}}'))
        self.assertEqual(json_issues('{"a": 1,\n"a": 2}')[0].line, 2)

    def test_error_navigation_and_clearing_after_fix(self):
        self.load('{\n"x": ]\n}')
        self.pane.validate_now()
        self.assertEqual(self.pane.issue_list.count(), 1)
        self.pane.open_issue(self.pane.issue_list.item(0))
        self.assertEqual(self.pane.edit.textCursor().blockNumber(), 1)
        self.pane.edit.setPlainText('{"x": []}')
        self.pane.validate_now()
        self.assertFalse(self.pane.edit.issues)
        self.assertEqual(self.pane.issue_list.count(), 0)

    def test_unicode_search_whole_words_replace_all_and_single_undo(self):
        self.load('😀 foo food FOO\nfoo')
        self.pane.show_find()
        self.pane.find.setText('foo')
        self.pane.whole.setChecked(True)
        self.assertEqual(len(self.pane.search_matches), 3)
        self.pane.replacement.setText('bar')
        self.pane.replace_all()
        self.assertEqual(self.pane.edit.toPlainText(), '😀 bar food bar\nbar')
        self.pane.edit.undo()
        self.assertEqual(self.pane.edit.toPlainText(), '😀 foo food FOO\nfoo')
        self.pane.case.setChecked(True)
        self.assertEqual(len(self.pane.search_matches), 2)
        self.pane.find_next()
        self.assertEqual(self.pane.edit.textCursor().selectedText(), 'foo')

    def test_format_json_preserves_types_and_is_undoable(self):
        original = '{"enabled":true,"rows":[1,null,"á😀"]}'
        self.load(original)
        self.pane.format_json()
        self.assertIn('  "enabled": true,\n', self.pane.edit.toPlainText())
        self.pane.edit.undo()
        self.assertEqual(self.pane.edit.toPlainText(), original)
        self.pane.edit.setPlainText('{"x": 1, "x": 2}')
        self.pane.format_json()
        self.assertEqual(self.pane.edit.toPlainText(), '{"x": 1, "x": 2}')

    def test_indent_selection_excludes_next_line_and_undo_groups_changes(self):
        self.load('a\nb\nc', 'data/quest/test.lua')
        cursor = self.pane.edit.textCursor()
        cursor.setPosition(0)
        cursor.setPosition(4, QTextCursor.MoveMode.KeepAnchor)
        self.pane.edit.setTextCursor(cursor)
        self.pane.edit.indent()
        self.assertEqual(self.pane.edit.toPlainText(), '    a\n    b\nc')
        self.pane.edit.undo()
        self.assertEqual(self.pane.edit.toPlainText(), 'a\nb\nc')
        self.pane.edit.selectAll()
        self.pane.edit.comment()
        self.assertEqual(self.pane.edit.toPlainText(), '-- a\n-- b\n-- c')
        self.pane.edit.comment()
        self.assertEqual(self.pane.edit.toPlainText(), 'a\nb\nc')

    def test_enter_preserves_indent_and_json_block_indent(self):
        self.load('    {')
        self.pane.edit.moveCursor(QTextCursor.MoveOperation.End)
        QTest.keyClick(self.pane.edit, Qt.Key.Key_Return)
        self.assertEqual(self.pane.edit.toPlainText(), '    {\n        ')

    def completions(self):
        model = self.pane.edit.completer.completionModel()
        return [model.index(row, 0).data() for row in range(model.rowCount())]

    def test_completion_catalog_tracks_source_and_excludes_comments_and_disabled_libraries(self):
        source = REPO / 'SRC/Tools/ServerManager/quest_api_catalog.py'
        self.assertEqual(source.read_text(encoding='utf-8'), render(REPO))
        self.assertIn('get_level', FUNCTIONS['pc'])
        self.assertIn('give_item2', FUNCTIONS['pc'])
        self.assertNotIn('mysql_password', FUNCTIONS[''])  # Commented-out registration.
        self.assertNotIn('execute', FUNCTIONS['os'])
        self.assertNotIn('io', FUNCTIONS)
        self.assertNotIn('debug', FUNCTIONS)
        self.assertIn('foreach', FUNCTIONS['table'])
        self.assertIn('loadstring', FUNCTIONS[''])
        self.assertEqual(tables('luaL_Reg a[] = {\n // {"bad", fn},\n {"yes", fn},\n {nullptr, nullptr}\n};'),
                         {'a': {'yes'}})

    def test_pc_completion_typing_filter_tab_insertion_and_undo(self):
        self.load('', 'data/quest/Quest/demo.lua')
        edit = self.pane.edit
        edit.setFocus()
        QTest.keyClicks(edit, 'pc.')
        self.assertTrue(edit.completer.popup().isVisible())
        self.assertIn('pc.get_level', self.completions())
        self.assertNotIn('npc.get_level', self.completions())
        QTest.keyClicks(edit, 'get_leve')
        self.assertEqual(self.completions(), ['pc.get_level'])
        QTest.keyClick(edit, Qt.Key.Key_Tab)
        self.assertEqual(edit.toPlainText(), 'pc.get_level()')
        self.assertEqual(edit.textCursor().position(), len('pc.get_level('))
        self.assertFalse(edit.completer.popup().isVisible())
        edit.undo()
        self.assertEqual(edit.toPlainText(), 'pc.get_leve')

    def test_completion_enter_click_and_escape_leave_normal_enter_and_tab_working(self):
        self.load('pc.get_leve', 'data/quest/demo.lua')
        edit = self.pane.edit
        edit.moveCursor(QTextCursor.MoveOperation.End)
        edit.setFocus()
        QTest.keyClick(edit, Qt.Key.Key_Space, Qt.KeyboardModifier.ControlModifier)
        self.assertTrue(edit.completer.popup().isVisible())
        QTest.keyClick(edit, Qt.Key.Key_Return)
        self.assertEqual(edit.toPlainText(), 'pc.get_level()')
        edit.setPlainText('pc.get_leve')
        edit.moveCursor(QTextCursor.MoveOperation.End)
        self.pane.complete_code()
        popup = edit.completer.popup()
        rect = popup.visualRect(popup.model().index(0, 0))
        QTest.mouseClick(popup.viewport(), Qt.MouseButton.LeftButton, pos=rect.center())
        self.assertEqual(edit.toPlainText(), 'pc.get_level()')
        edit.setPlainText('pc.')
        edit.moveCursor(QTextCursor.MoveOperation.End)
        self.pane.complete_code()
        QTest.keyClick(edit, Qt.Key.Key_Escape)
        self.assertFalse(popup.isVisible())
        QTest.keyClick(edit, Qt.Key.Key_Return)
        QTest.keyClick(edit, Qt.Key.Key_Tab)
        self.assertEqual(edit.toPlainText(), 'pc.\n    ')

    def test_completion_arrow_navigation_unknown_member_and_closed_comment(self):
        self.load('--[[pc.]] pc.', 'data/quest/Quest/demo.lua')
        edit = self.pane.edit
        edit.moveCursor(QTextCursor.MoveOperation.End)
        edit.setFocus()
        self.pane.complete_code()
        choices = self.completions()
        self.assertGreater(len(choices), 2)
        QTest.keyClick(edit, Qt.Key.Key_Down)
        self.assertEqual(edit.completer.popup().currentIndex().data(), choices[1])
        QTest.keyClick(edit, Qt.Key.Key_Return)
        self.assertEqual(edit.toPlainText(), '--[[pc.]] ' + choices[1] + '()')
        edit.setPlainText('pc.')
        edit.moveCursor(QTextCursor.MoveOperation.End)
        self.pane.complete_code()
        QTest.keyClicks(edit, 'function_that_does_not_exist')
        self.assertFalse(edit.completer.popup().isVisible())
        self.assertEqual(edit.toPlainText(), 'pc.function_that_does_not_exist')

    def test_completion_replaces_middle_of_identifier_without_duplicate_parentheses_or_unicode_damage(self):
        original = 'local text = "á😀"; pc.get_lvel (50187, 1)'
        self.load(original, 'data/quest/demo.lua')
        edit = self.pane.edit
        cursor = edit.textCursor()
        cursor.setPosition(utf16('local text = "á😀"; pc.get_l'))
        edit.setTextCursor(cursor)
        edit.request_completion()
        edit.insert_completion('pc.get_level')
        self.assertEqual(edit.toPlainText(), 'local text = "á😀"; pc.get_level (50187, 1)')
        self.assertEqual(edit.textCursor().position(), utf16('local text = "á😀"; pc.get_level ('))
        edit.undo()
        self.assertEqual(edit.toPlainText(), original)

    def test_completion_is_suppressed_in_comments_strings_json_unknown_tables_and_selections(self):
        for text in ('-- pc.', 'local x = "pc.', "local x = 'pc.",
                     '--[=[\npc.', 'local x = [=[\npc.', '--[[\n', 'foo.pc.', 'unknown.', 'pc:'):
            with self.subTest(text=text):
                self.load(text, 'data/quest/demo.lua')
                self.pane.edit.moveCursor(QTextCursor.MoveOperation.End)
                self.pane.complete_code()
                self.assertFalse(self.pane.edit.completer.popup().isVisible())
        self.load('pc.', 'data/example.json')
        self.pane.edit.moveCursor(QTextCursor.MoveOperation.End)
        self.pane.complete_code()
        self.assertFalse(self.pane.edit.completer.popup().isVisible())
        self.assertFalse(self.pane.completion_button.isVisible())
        self.load('pc.', 'data/quest/demo.lua')
        self.pane.edit.selectAll()
        self.pane.complete_code()
        self.assertFalse(self.pane.edit.completer.popup().isVisible())

    def test_namespace_completion_manual_root_keywords_backspace_and_context_switch(self):
        self.load('p', 'data/quest/demo.lua')
        edit = self.pane.edit
        edit.moveCursor(QTextCursor.MoveOperation.End)
        self.pane.complete_code()
        self.assertIn('pc', self.completions())
        edit.insert_completion('pc')
        self.assertEqual(edit.toPlainText(), 'pc.')
        self.assertIn('pc.get_level', self.completions())
        QTest.keyClicks(edit, 'get_leve')
        QTest.keyClick(edit, Qt.Key.Key_Backspace)
        self.assertIn('pc.get_level', self.completions())
        self.pane.set_context('data/example.json')
        self.assertFalse(edit.completer.popup().isVisible())
        self.load('functio', 'data/quest/demo.lua')
        edit.moveCursor(QTextCursor.MoveOperation.End)
        self.pane.complete_code()
        edit.insert_completion('function')
        self.assertEqual(edit.toPlainText(), 'function')
        self.load('', 'data/quest/Quest/demo.lua')
        self.pane.complete_code()
        self.assertIn('quest', self.completions())
        self.pane.set_context('data/quest/questlib.lua')
        self.pane.complete_code()
        self.assertNotIn('quest', self.completions())

    def test_highlighter_multiline_comments_strings_and_unicode_brackets(self):
        self.load('--[=[ comment\n() ]=]\nlocal x = "😀 ()"\nreturn (1)', 'data/quest/test.lua')
        self.pane.edit.syntax.rehighlight()
        second = self.pane.edit.document().findBlockByNumber(1)
        self.assertTrue(self.pane.edit.ignored(second.position()))
        third = self.pane.edit.document().findBlockByNumber(2)
        quote_bracket = third.position() + utf16('local x = "😀 ')
        self.assertTrue(self.pane.edit.ignored(quote_bracket))
        self.pane.edit.moveCursor(QTextCursor.MoveOperation.End)
        self.assertEqual(len(self.pane.edit.bracket_positions()), 2)

    def test_encoding_errors_are_not_reported_as_valid_syntax(self):
        self.load('return "😀"', 'data/quest/test.lua')
        self.pane.set_context('data/quest/test.lua', 'latin1', '\r\n')
        self.pane.validate_now()
        self.assertEqual(self.pane.edit.issues[0].line, 1)
        self.assertIn('LATIN1', self.pane.position.text())

    def native(self, text, language):
        request = NativeRequest(text, language, 'utf-8')
        try:
            result = subprocess.run([str(request.program), *request.arguments], cwd=request.directory,
                capture_output=True, timeout=8,
                creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)
            output = (result.stdout + result.stderr).decode('utf-8', errors='replace')
            files = {p.relative_to(request.directory).as_posix() for p in request.directory.rglob('*')}
            self.assertEqual(files, {request.path.name}, output)
            return request.result(output, result.returncode)
        finally:
            request.close()

    def test_real_lua_check_never_executes_infinite_loop_or_filesystem_code(self):
        self.assertFalse(self.native('while true do end\nos.execute("must_never_execute")', 'lua'))
        self.assertTrue(self.native('function broken(\n', 'lua'))
        issues = self.native('local a = 1\nlocal b = )', 'lua')
        self.assertEqual(issues[0].line, 2)

    def test_real_quest_check_handles_body_functions_missing_end_and_lexical_errors(self):
        valid = 'quest editor_test begin\nstate start begin\nwhen login begin\nlocal x=1\nend\nend\nend\n'
        self.assertFalse(self.native(valid, 'quest'))
        self.assertFalse(self.native('quest test begin state start begin function f(a) return a+1 end end end', 'quest'))
        for text in (valid[:-5], valid.replace('local x=1', 'local x=)'),
                     valid.replace('local x=1', 'local x="unterminated'), '',
                     'quest test begin state start begin function f() local x=) end end end'):
            with self.subTest(text=text):
                self.assertTrue(self.native(text, 'quest'))

    def test_async_lua_result_and_shutdown_cleanup(self):
        self.load('local x = )', 'data/quest/test.lua')
        self.pane.validate_now()
        directory = self.pane.request.directory
        deadline = time.monotonic() + 8
        while self.pane.process and time.monotonic() < deadline:
            QTest.qWait(20)
        self.assertIsNone(self.pane.process)
        self.assertTrue(self.pane.edit.issues)
        self.assertFalse(directory.exists())
        self.pane.edit.setPlainText('return 1')
        self.pane.validate_now()
        self.pane.shutdown()
        self.assertIsNone(self.pane.process)

    def test_invalid_json_can_be_opened_repaired_and_saved_without_losing_backup(self):
        with tempfile.TemporaryDirectory(prefix='auriga-editor-repair-') as folder:
            root = Path(folder)
            (root / '.auriga-package.json').write_text('{}')
            shutil.copy2(REPO / 'config/server.json.example', root / 'server.json')
            shutil.copy2(REPO / 'SRC/Tools/server_package/manage.py', root / 'manage.py')
            (root / 'data').mkdir()
            path = root / 'data/broken.json'
            path.write_bytes(b'{broken')
            document = api.Document(root, 'data/broken.json', allow_invalid_json=True)
            self.assertTrue(document.parse_error)
            with mock.patch.object(api, 'validate_candidate', return_value='OK'):
                api.save_document(root, document.relative, b'{"fixed": true}', document.digest)
            self.assertEqual(path.read_bytes(), b'{"fixed": true}')
            self.assertEqual(api.backup_files(root, document.relative)[0].read_bytes(), b'{broken')


if __name__ == '__main__':
    unittest.main()
