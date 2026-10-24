"""Qt6 source editor: syntax, diagnostics and familiar text-editing tools."""
import json
import re

from PySide6.QtCore import QProcess, QRect, QSize, Qt, QTimer, Signal
from PySide6.QtGui import (QColor, QFont, QKeySequence, QPainter, QShortcut, QStandardItem, QStandardItemModel,
                          QSyntaxHighlighter, QTextCharFormat, QTextCursor,
                          QTextFormat, QTextBlockUserData)
from PySide6.QtWidgets import (QApplication, QCheckBox, QCompleter, QHBoxLayout, QInputDialog, QLabel,
    QLineEdit, QListWidget, QListWidgetItem, QPlainTextEdit, QPushButton,
    QTextEdit, QToolButton, QVBoxLayout, QWidget)

import backend as api
from editor_checks import Issue, NativeRequest, json_issues, language_for
from quest_api_catalog import FUNCTIONS, ORIGINS


def utf16(text):
    return len(text.encode('utf-16-le')) // 2


class BlockData(QTextBlockUserData):
    def __init__(self, ignored):
        super().__init__()
        self.ignored = ignored


class Syntax(QSyntaxHighlighter):
    KEYWORDS = set(('and break do else elseif end false for function goto if in local nil not or '
                    'repeat return then true until while quest state when begin with').split())
    TOKEN = re.compile(r'\b[A-Za-z_][\w]*\b|\b(?:0[xX][\da-fA-F]+|\d+(?:\.\d+)?(?:[eE][+-]?\d+)?)\b')
    LONG = re.compile(r'\[(=*)\[')

    def __init__(self, document):
        super().__init__(document)
        self.language = 'text'
        self.styles = {}
        for name, color in dict(keyword='#cba6f7', number='#f5bf88', string='#a6dca0',
                                comment='#7d95a3', key='#8ed9e8', call='#e9d68e').items():
            style = QTextCharFormat()
            style.setForeground(QColor(color))
            self.styles[name] = style

    def highlightBlock(self, text):
        self.setCurrentBlockState(0)
        ignored = []
        def paint(start, end, style, ignore=False):
            self.setFormat(utf16(text[:start]), utf16(text[start:end]), self.styles[style])
            if ignore:
                ignored.append((utf16(text[:start]), utf16(text[:end])))
        if self.language == 'text':
            self.setCurrentBlockUserData(BlockData([]))
            return
        i, limit = 0, len(text)
        previous = self.previousBlockState()
        if self.language != 'json' and previous > 0:
            comment, equals = previous % 2 == 1, (previous - 1) // 2
            stop = text.find(']' + '=' * equals + ']')
            end = limit if stop < 0 else stop + equals + 2
            paint(0, end, 'comment' if comment else 'string', True)
            if stop < 0:
                self.setCurrentBlockState(previous)
            i = end
        while i < limit:
            # Keep pathological single-line data from monopolising repaint.
            if i > 24000:
                break
            if self.language != 'json' and text.startswith('--', i):
                long = self.LONG.match(text, i + 2)
                if not long:
                    paint(i, limit, 'comment', True)
                    break
                equals = len(long[1])
                stop = text.find(']' + '=' * equals + ']', long.end())
                end = limit if stop < 0 else stop + equals + 2
                paint(i, end, 'comment', True)
                if stop < 0:
                    self.setCurrentBlockState(1 + equals * 2)
                i = end
                continue
            long = self.LONG.match(text, i) if self.language != 'json' else None
            if long:
                equals = len(long[1])
                stop = text.find(']' + '=' * equals + ']', long.end())
                end = limit if stop < 0 else stop + equals + 2
                paint(i, end, 'string', True)
                if stop < 0:
                    self.setCurrentBlockState(2 + equals * 2)
                i = end
                continue
            if text[i] == '"' or (self.language != 'json' and text[i] == "'"):
                quote, end = text[i], i + 1
                while end < limit:
                    if text[end] == '\\':
                        end += 2
                    elif text[end] == quote:
                        end += 1
                        break
                    else:
                        end += 1
                style = 'key' if self.language == 'json' and text[end:].lstrip().startswith(':') else 'string'
                paint(i, min(end, limit), style, True)
                i = end
                continue
            token = self.TOKEN.match(text, i)
            if token:
                word = token[0]
                if word[0].isdigit():
                    paint(i, token.end(), 'number')
                elif word in (('true', 'false', 'null') if self.language == 'json' else self.KEYWORDS):
                    paint(i, token.end(), 'keyword')
                elif self.language != 'json' and text[token.end():].lstrip().startswith('('):
                    paint(i, token.end(), 'call')
                i = token.end()
            else:
                i += 1
        self.setCurrentBlockUserData(BlockData(ignored))


class Gutter(QWidget):
    def __init__(self, editor):
        super().__init__(editor)
        self.editor = editor

    def sizeHint(self):
        return QSize(self.editor.gutter_width(), 0)

    def paintEvent(self, event):
        self.editor.paint_gutter(event)


class CodeEdit(QPlainTextEdit):
    positionChanged = Signal()

    def __init__(self):
        super().__init__()
        self.setObjectName('SourceCode')
        self.setStyleSheet('QPlainTextEdit#SourceCode { font-family: "Consolas"; font-size: 14px; }')
        self.setFont(QFont('Consolas', 11))
        self.setLineWrapMode(QPlainTextEdit.LineWrapMode.NoWrap)
        self.setTabStopDistance(self.fontMetrics().horizontalAdvance(' ') * 4)
        self.syntax = Syntax(self.document())
        self.completion_model = QStandardItemModel(self)
        self.completer = QCompleter(self.completion_model, self)
        self.completer.setWidget(self)
        self.completer.setCompletionMode(QCompleter.CompletionMode.PopupCompletion)
        self.completer.setCaseSensitivity(Qt.CaseSensitivity.CaseSensitive)
        self.completer.setMaxVisibleItems(10)
        self.completer.popup().setFont(self.font())
        self.completer.popup().setAccessibleName('Lua- és quest-kiegészítések')
        self.completer.activated[str].connect(self.insert_completion)
        self.completion_namespace = None
        self.textChanged.connect(self.hide_completion)
        self.cursorPositionChanged.connect(self.hide_completion)
        self.gutter = Gutter(self)
        self.issues, self.matches = [], []
        self.blockCountChanged.connect(self.update_margin)
        self.updateRequest.connect(self.update_gutter)
        self.cursorPositionChanged.connect(self.decorate)
        self.cursorPositionChanged.connect(self.positionChanged)
        self.update_margin()
        self.decorate()

    def gutter_width(self):
        return 22 + self.fontMetrics().horizontalAdvance('9') * max(2, len(str(self.blockCount())))

    def update_margin(self, *args):
        self.setViewportMargins(self.gutter_width(), 0, 0, 0)
        self.gutter.setGeometry(QRect(0, 0, self.gutter_width(), self.height()))

    def update_gutter(self, rect, dy):
        if dy:
            self.gutter.scroll(0, dy)
        else:
            self.gutter.update(0, rect.y(), self.gutter.width(), rect.height())
        if rect.contains(self.viewport().rect()):
            self.update_margin()

    def resizeEvent(self, event):
        super().resizeEvent(event)
        self.update_margin()

    def paint_gutter(self, event):
        painter = QPainter(self.gutter)
        painter.setFont(self.font())
        painter.fillRect(event.rect(), QColor('#121d2a'))
        block = self.firstVisibleBlock()
        top = round(self.blockBoundingGeometry(block).translated(self.contentOffset()).top())
        lines = {issue.line for issue in self.issues}
        while block.isValid() and top <= event.rect().bottom():
            height = round(self.blockBoundingRect(block).height())
            number = block.blockNumber() + 1
            if block.isVisible() and top + height >= event.rect().top():
                painter.setPen(QColor('#ef8c92' if number in lines else '#7e93aa'))
                painter.drawText(0, top, self.gutter.width() - 7, self.fontMetrics().height(),
                                 Qt.AlignmentFlag.AlignRight, str(number))
                if number in lines:
                    painter.setBrush(QColor('#ef8c92'))
                    painter.drawEllipse(3, top + 5, 5, 5)
            block = block.next()
            top += height

    def goto(self, line, column=1):
        block = self.document().findBlockByNumber(max(0, min(line - 1, self.blockCount() - 1)))
        cursor = QTextCursor(block)
        # Diagnostics use Unicode character columns; Qt cursors use UTF-16.
        cursor.setPosition(block.position() + utf16(block.text()[:max(0, column - 1)]))
        self.setTextCursor(cursor)
        self.centerCursor()
        self.setFocus()

    def bracket_positions(self):
        cursor = self.textCursor()
        for position in (cursor.position() - 1, cursor.position()):
            if position < 0:
                continue
            char = self.document().characterAt(position)
            if char not in '()[]{}' or self.ignored(position):
                continue
            opening = char in '([{'
            opposite = dict(zip('()[]{}', ')(][}{'))[char]
            step, depth = (1 if opening else -1), 1
            for offset in range(position + step, max(-1, position - 12000) if step < 0 else
                                min(self.document().characterCount() - 1, position + 12000), step):
                other = self.document().characterAt(offset)
                if self.ignored(offset):
                    continue
                if other == char:
                    depth += 1
                elif other == opposite:
                    depth -= 1
                    if not depth:
                        return [position, offset]
            return [position]
        return []

    def ignored(self, position):
        block = self.document().findBlock(position)
        data = block.userData()
        return data and any(start <= position - block.position() < end for start, end in data.ignored)

    def decorate(self):
        selections = []
        current = QTextEdit.ExtraSelection()
        current.cursor = self.textCursor()
        current.cursor.clearSelection()
        current.format.setBackground(QColor('#1d2e42'))
        current.format.setProperty(QTextFormat.Property.FullWidthSelection, True)
        selections.append(current)
        for position in self.bracket_positions():
            item = QTextEdit.ExtraSelection()
            item.cursor = QTextCursor(self.document())
            item.cursor.setPosition(position)
            item.cursor.movePosition(QTextCursor.MoveOperation.NextCharacter, QTextCursor.MoveMode.KeepAnchor)
            item.format.setBackground(QColor('#49645f'))
            selections.append(item)
        for start, end in self.matches[:2000]:
            item = QTextEdit.ExtraSelection()
            item.cursor = QTextCursor(self.document())
            item.cursor.setPosition(start)
            item.cursor.setPosition(end, QTextCursor.MoveMode.KeepAnchor)
            item.format.setBackground(QColor('#5d4d28'))
            selections.append(item)
        for issue in self.issues:
            if issue.line <= 0:
                continue
            block = self.document().findBlockByNumber(issue.line - 1)
            if not block.isValid():
                continue
            item = QTextEdit.ExtraSelection()
            item.cursor = QTextCursor(block)
            item.cursor.movePosition(QTextCursor.MoveOperation.EndOfBlock, QTextCursor.MoveMode.KeepAnchor)
            item.format.setUnderlineStyle(QTextCharFormat.UnderlineStyle.WaveUnderline)
            item.format.setUnderlineColor(QColor('#ef8c92'))
            item.format.setToolTip(issue.message)
            selections.append(item)
        self.setExtraSelections(selections)
        self.gutter.update()

    def indent(self, backwards=False):
        cursor = self.textCursor()
        if not backwards and not cursor.hasSelection():
            cursor.insertText(' ' * (4 - cursor.positionInBlock() % 4))
            return
        start, end = cursor.selectionStart(), cursor.selectionEnd()
        first = self.document().findBlock(start).blockNumber()
        last = self.document().findBlock(end)
        last_number = last.blockNumber() - int(end == last.position() and end > start)
        cursor.beginEditBlock()
        for number in range(last_number, first - 1, -1):
            block = self.document().findBlockByNumber(number)
            edit = QTextCursor(block)
            if backwards:
                text = block.text()
                count = 1 if text.startswith('\t') else min(4, len(text) - len(text.lstrip(' ')))
                for _ in range(count):
                    edit.deleteChar()
            else:
                edit.insertText('    ')
        cursor.endEditBlock()
        self.setTextCursor(cursor)

    def comment(self):
        if self.syntax.language not in ('lua', 'quest'):
            return
        cursor = self.textCursor()
        first = self.document().findBlock(cursor.selectionStart()).blockNumber()
        last = self.document().findBlock(cursor.selectionEnd())
        end = last.blockNumber() - int(cursor.selectionEnd() == last.position() and cursor.hasSelection())
        blocks = [self.document().findBlockByNumber(i) for i in range(first, end + 1)]
        uncomment = all(not b.text().strip() or b.text().lstrip().startswith('--') for b in blocks)
        cursor.beginEditBlock()
        for block in reversed(blocks):
            if not block.text().strip():
                continue
            edit = QTextCursor(block)
            indent = len(block.text()) - len(block.text().lstrip())
            edit.setPosition(block.position() + indent)
            if uncomment:
                count = 3 if block.text()[indent:].startswith('-- ') else 2
                for _ in range(count):
                    edit.deleteChar()
            else:
                edit.insertText('-- ')
        cursor.endEditBlock()

    def hide_completion(self):
        self.completer.popup().hide()

    def completion_context(self):
        """Return the qualified token and its UTF-16 replacement range at the caret."""
        cursor = self.textCursor()
        if self.syntax.language not in ('lua', 'quest') or cursor.hasSelection():
            return None
        block = cursor.block()
        self.syntax.rehighlightBlock(block)
        position = cursor.positionInBlock()
        before = block.text().encode('utf-16-le')[:position * 2].decode('utf-16-le')
        # A continued long comment/string also covers an empty line.
        if (position == 0 and block.previous().isValid() and block.previous().userState() > 0
                or position > 0 and self.ignored(cursor.position() - 1)):
            return None
        match = re.search(r'(?<![\w.:\]])([A-Za-z_][A-Za-z_0-9]*(?:\.[A-Za-z_][A-Za-z_0-9]*)*\.?)$', before)
        if match:
            prefix = match[1]
        elif not before or before[-1].isspace() or before[-1] in '(=,+-*/%<>~{;':
            prefix = ''
        else:
            return None
        # Include the rest of the current identifier when completing in its middle.
        after = block.text().encode('utf-16-le')[position * 2:].decode('utf-16-le')
        suffix = re.match(r'[A-Za-z_0-9]*', after)[0]
        return prefix, cursor.position() - utf16(prefix), cursor.position() + utf16(suffix)

    def request_completion(self, manual=True):
        context = self.completion_context()
        if context is None:
            self.hide_completion()
            return
        prefix = context[0]
        namespace = prefix.rsplit('.', 1)[0] if '.' in prefix else ''
        if namespace not in FUNCTIONS or not manual and '.' not in prefix and len(prefix) < 2:
            self.hide_completion()
            return
        if self.completion_namespace != (namespace, self.syntax.language):
            self.completion_namespace = (namespace, self.syntax.language)
            self.completion_model.clear()
            names = {((namespace + '.') if namespace else '') + name: 'function'
                     for name in FUNCTIONS[namespace]}
            if not namespace:
                names.update({name: 'namespace' for name in FUNCTIONS if name})
                keywords = self.syntax.KEYWORDS
                if self.syntax.language == 'lua':
                    keywords = keywords - {'quest', 'state', 'when', 'begin', 'with'}
                names.update({name: 'keyword' for name in keywords})
            for name, kind in sorted(names.items()):
                item = QStandardItem(name)
                item.setData(kind, Qt.ItemDataRole.UserRole)
                if kind == 'function':
                    item.setToolTip(name + '()\n' + '\n'.join(ORIGINS[namespace])
                        + '\nAz opcionális szerverfunkciók elérhetősége a build beállításaitól függ.')
                else:
                    item.setToolTip('Névtér' if kind == 'namespace' else 'Kulcsszó')
                self.completion_model.appendRow(item)
        self.completer.setCompletionPrefix(prefix)
        if not self.completer.completionCount():
            self.hide_completion()
            return
        self.completer.popup().setCurrentIndex(self.completer.completionModel().index(0, 0))
        rect = self.cursorRect()
        rect.setWidth(min(600, max(320, self.completer.popup().sizeHintForColumn(0) + 30)))
        self.completer.complete(rect)

    def insert_completion(self, name):
        context = self.completion_context()
        if context is None:
            self.hide_completion()
            return
        prefix, start, end = context
        namespace = prefix.rsplit('.', 1)[0] if '.' in prefix else ''
        if namespace not in FUNCTIONS or not name.startswith(prefix):
            return
        if name in FUNCTIONS and name:
            kind = 'namespace'
        elif name.removeprefix(namespace + '.' if namespace else '') in FUNCTIONS[namespace]:
            kind = 'function'
        elif '.' not in name and name in self.syntax.KEYWORDS:
            kind = 'keyword'
        else:
            return
        self.hide_completion()
        cursor = self.textCursor()
        cursor.setPosition(start)
        cursor.setPosition(end, QTextCursor.MoveMode.KeepAnchor)
        tail = cursor.block().text().encode('utf-16-le')[(end - cursor.block().position()) * 2:].decode('utf-16-le')
        insert, advance = name, 0
        if kind == 'function':
            existing = re.match(r'[ \t]*\(', tail)
            if existing:
                advance = utf16(existing[0])
            else:
                insert += '()'
                advance = -1
        elif kind == 'namespace':
            if tail.startswith('.'):
                advance = 1
            else:
                insert += '.'
        cursor.beginEditBlock()
        cursor.insertText(insert)
        cursor.setPosition(cursor.position() + advance)
        cursor.endEditBlock()
        self.setTextCursor(cursor)
        self.ensureCursorVisible()
        if kind == 'namespace':
            self.request_completion()

    def keyPressEvent(self, event):
        if event.key() == Qt.Key.Key_Space and event.modifiers() == Qt.KeyboardModifier.ControlModifier:
            self.request_completion()
            return
        popup = self.completer.popup()
        was_visible = popup.isVisible()
        if was_visible and event.key() in (Qt.Key.Key_Up, Qt.Key.Key_Down, Qt.Key.Key_PageUp, Qt.Key.Key_PageDown):
            QApplication.sendEvent(popup, event)
            return
        if was_visible and event.key() in (Qt.Key.Key_Return, Qt.Key.Key_Enter, Qt.Key.Key_Tab):
            name = popup.currentIndex().data()
            if name:
                self.insert_completion(name)
            else:
                self.hide_completion()
            return
        if was_visible and event.key() in (Qt.Key.Key_Escape, Qt.Key.Key_Backtab):
            self.hide_completion()
            return
        if event.key() in (Qt.Key.Key_Tab, Qt.Key.Key_Backtab) and not event.modifiers() & Qt.KeyboardModifier.ControlModifier:
            self.indent(event.key() == Qt.Key.Key_Backtab or bool(event.modifiers() & Qt.KeyboardModifier.ShiftModifier))
            return
        if event.key() in (Qt.Key.Key_Return, Qt.Key.Key_Enter) and not self.textCursor().hasSelection():
            cursor = self.textCursor()
            before = cursor.block().text().encode('utf-16-le')[:cursor.positionInBlock()*2].decode('utf-16-le')
            indent = re.match(r'[ \t]*', before)[0]
            if re.search(r'(?:\b(?:then|do|begin|else|repeat)|[\{\[])\s*$', before):
                indent += '    '
            cursor.beginEditBlock()
            cursor.insertText('\n' + indent)
            cursor.endEditBlock()
            return
        super().keyPressEvent(event)
        if (not event.modifiers() & (Qt.KeyboardModifier.ControlModifier | Qt.KeyboardModifier.AltModifier)
                and (event.text() and re.fullmatch(r'[A-Za-z_0-9.]', event.text())
                     or was_visible and event.key() == Qt.Key.Key_Backspace)):
            self.request_completion(manual=False)


class CodePane(QWidget):
    def __init__(self):
        super().__init__()
        self.setStyleSheet('''
            QCheckBox::indicator { width: 13px; height: 13px; border: 1px solid #6b829d;
                border-radius: 2px; background: #172231; }
            QCheckBox::indicator:checked { background: #37967c; border: 2px solid #8bdbc0; }
            QToolButton { padding: 6px 10px; }
        ''')
        self.relative, self.encoding, self.newline = '', 'utf-8', '\n'
        self.request, self.process, self.revision = None, None, 0
        self.layout = QVBoxLayout(self)
        self.layout.setContentsMargins(0, 0, 0, 0)
        tools = QHBoxLayout()
        def tool(title, callback):
            result = QToolButton()
            result.setText(title)
            result.clicked.connect(callback)
            tools.addWidget(result)
            return result
        tool('Keresés / csere', self.show_find)
        tool('Sorhoz…', self.goto_dialog)
        self.format_button = tool('JSON rendezése', self.format_json)
        tool('Ellenőrzés · F7', self.validate_now)
        self.completion_button = tool('Kiegészítés', lambda: self.complete_code())
        self.completion_button.setToolTip('Lua- és quest-függvények · Ctrl+Space\n'
            'A pc. után automatikusan megjelennek a lehetőségek.\nEnter / Tab: beszúrás; Esc: bezárás.')
        self.wrap = QCheckBox('Sortördelés')
        self.wrap.toggled.connect(lambda on: self.edit.setLineWrapMode(
            QPlainTextEdit.LineWrapMode.WidgetWidth if on else QPlainTextEdit.LineWrapMode.NoWrap))
        tools.addWidget(self.wrap)
        tools.addStretch()
        self.layout.addLayout(tools)
        self.find_panel = QWidget()
        find_layout = QVBoxLayout(self.find_panel)
        find_layout.setContentsMargins(0, 0, 0, 0)
        row = QHBoxLayout()
        self.find = QLineEdit()
        self.find.setPlaceholderText('Keresett szöveg')
        self.case = QCheckBox('Aa')
        self.case.setToolTip('Kis- és nagybetűk megkülönböztetése')
        self.whole = QCheckBox('Teljes szó')
        self.find_count = QLabel()
        for item in (self.find, self.case, self.whole, self.find_count):
            row.addWidget(item)
        for text, callback in [('↑', lambda: self.find_next(True)), ('↓', self.find_next), ('Bezár', self.hide_find)]:
            b = QPushButton(text)
            b.clicked.connect(callback)
            row.addWidget(b)
        find_layout.addLayout(row)
        row = QHBoxLayout()
        self.replacement = QLineEdit()
        self.replacement.setPlaceholderText('Csere erre (üresen: törlés)')
        row.addWidget(self.replacement)
        for text, callback in [('Csere', self.replace_one), ('Összes cseréje', self.replace_all)]:
            b = QPushButton(text)
            b.clicked.connect(callback)
            row.addWidget(b)
        find_layout.addLayout(row)
        self.layout.addWidget(self.find_panel)
        self.find_panel.hide()
        self.edit = CodeEdit()
        self.edit.setAccessibleName('Forráskód szerkesztő')
        self.edit.setToolTip('Lua / quest: pc., npc., item.… · Ctrl+Space: kiegészítés\n'
            'Enter / Tab: beszúrás · Esc: lista bezárása')
        self.layout.addWidget(self.edit, 1)
        self.issue_list = QListWidget()
        self.issue_list.setMaximumHeight(85)
        self.issue_list.itemClicked.connect(self.open_issue)
        self.layout.addWidget(self.issue_list)
        self.issue_list.hide()
        status = QHBoxLayout()
        self.validation = QLabel('Nincs megnyitott fájl.')
        self.validation.setWordWrap(True)
        self.position = QLabel()
        status.addWidget(self.validation, 1)
        status.addWidget(self.position)
        self.layout.addLayout(status)
        self.timer = QTimer(self)
        self.timer.setSingleShot(True)
        self.timer.setInterval(700)
        self.timer.timeout.connect(self.validate_now)
        self.timeout = QTimer(self)
        self.timeout.setSingleShot(True)
        self.timeout.setInterval(6000)
        self.timeout.timeout.connect(self.timed_out)
        self.edit.textChanged.connect(self.changed)
        self.edit.positionChanged.connect(self.update_position)
        for widget in (self.find, self.case, self.whole):
            (widget.textChanged if isinstance(widget, QLineEdit) else widget.toggled).connect(self.refresh_matches)
        self.find.returnPressed.connect(self.find_next)
        for keys, callback in [('Ctrl+f', self.show_find), ('Ctrl+h', self.show_find), ('F3', self.find_next),
                               ('Shift+F3', lambda: self.find_next(True)), ('Ctrl+g', self.goto_dialog),
                               ('Ctrl+/', self.edit.comment), ('Ctrl+Shift+f', self.format_json),
                               ('F7', self.validate_now), ('Escape', self.escape)]:
            shortcut = QShortcut(QKeySequence(keys), self)
            shortcut.setContext(Qt.ShortcutContext.WidgetWithChildrenShortcut)
            shortcut.activated.connect(callback)

    def set_context(self, relative, encoding='utf-8', newline='\n'):
        self.stop_check()
        self.edit.hide_completion()
        self.relative, self.encoding, self.newline = relative, encoding, newline
        self.edit.syntax.language = language_for(relative)
        self.edit.syntax.rehighlight()
        self.format_button.setVisible(self.edit.syntax.language == 'json')
        self.completion_button.setVisible(self.edit.syntax.language in ('lua', 'quest'))
        self.changed()

    def complete_code(self):
        self.edit.setFocus()
        self.edit.request_completion()

    def escape(self):
        if self.edit.completer.popup().isVisible():
            self.edit.hide_completion()
        else:
            self.hide_find()

    def update_position(self):
        cursor = self.edit.textCursor()
        before = cursor.block().text().encode('utf-16-le')[:cursor.positionInBlock()*2].decode('utf-16-le')
        self.position.setText('%d:%d · %s · %s' % (cursor.blockNumber() + 1,
            len(before) + 1, self.encoding.upper(), 'CRLF' if self.newline == '\r\n' else 'LF'))

    def changed(self):
        self.revision += 1
        self.timer.stop()
        self.stop_check()
        self.set_issues([], 'Módosítva · ellenőrzésre vár…')
        self.refresh_matches()
        self.update_position()
        if self.relative and len(self.edit.toPlainText()) <= 1024 * 1024:
            self.timer.start()
        elif self.relative:
            self.validation.setText('Nagy fájl · kézi ellenőrzés: F7.')

    def set_issues(self, issues, message):
        self.edit.issues = issues
        self.edit.decorate()
        self.issue_list.clear()
        for issue in issues:
            location = '%d:%d · ' % (issue.line, issue.column) if issue.line else ''
            item = QListWidgetItem(location + issue.message.replace('\n', ' '))
            item.setToolTip(issue.message)
            item.setData(Qt.ItemDataRole.UserRole, issue)
            self.issue_list.addItem(item)
        self.issue_list.setVisible(bool(issues))
        self.validation.setText(message)
        self.validation.setStyleSheet('color: %s;' % ('#ef8c92' if issues else '#9bb0c8'))

    def open_issue(self, item):
        issue = item.data(Qt.ItemDataRole.UserRole)
        if issue.line:
            self.edit.goto(issue.line, issue.column or 1)

    def validate_now(self):
        self.timer.stop()
        self.stop_check()
        language = self.edit.syntax.language
        text = self.edit.toPlainText()
        try:
            text.encode(self.encoding)
        except UnicodeEncodeError as error:
            line = text.count('\n', 0, error.start) + 1
            self.set_issues([Issue('Ez a karakter nem menthető %s kódolással.' % self.encoding, line, 1)], 'Kódolási hiba')
            return
        if language == 'json':
            issues = json_issues(text)
            self.set_issues(issues, 'Hibás JSON · kattints a hibára.' if issues else 'JSON szintaxis rendben')
            return
        if language == 'text' or not self.relative:
            self.set_issues([], 'Szöveg · ehhez a fájltípushoz nincs szintaxisellenőrző.')
            return
        try:
            request = NativeRequest(text, language, self.encoding)
        except (api.EditorError, OSError, UnicodeError) as error:
            self.set_issues([Issue(str(error))], 'Nem sikerült ellenőrizni')
            return
        self.request, self.process = request, QProcess(self)
        process, revision = self.process, self.revision
        process.setProcessChannelMode(QProcess.ProcessChannelMode.MergedChannels)
        process.setWorkingDirectory(str(request.directory))
        process.finished.connect(lambda code, status: self.finished(process, request, revision, code, status))
        process.errorOccurred.connect(lambda error: self.start_error(process, error))
        process.start(str(request.program), request.arguments)
        self.validation.setText('Natív szintaxisellenőrzés…')
        self.timeout.start()

    def start_error(self, process, error):
        if process is self.process and error == QProcess.ProcessError.FailedToStart:
            self.stop_check()
            self.set_issues([Issue('A natív ellenőrző nem indult el.')], 'Nem sikerült ellenőrizni')

    def finished(self, process, request, revision, code, status):
        if process is not self.process:
            return
        output = bytes(process.readAllStandardOutput()).decode('utf-8', errors='replace')
        issues = request.result(output, code if status == QProcess.ExitStatus.NormalExit else -1)
        self.stop_check()
        if revision == self.revision:
            self.set_issues(issues, 'Szintaktikai hiba · kattints a hibára.' if issues else
                            ('Quest' if self.edit.syntax.language == 'quest' else 'Lua') + ' szintaxis rendben')

    def timed_out(self):
        self.stop_check()
        self.set_issues([Issue('Az ellenőrzés túllépte a 6 másodperces időkorlátot.')], 'Nem sikerült ellenőrizni')

    def stop_check(self):
        self.timeout.stop()
        process, self.process = self.process, None
        if process:
            process.kill()
            process.waitForFinished(1000)
            process.deleteLater()
        if self.request:
            self.request.close()
            self.request = None

    def shutdown(self):
        self.timer.stop()
        self.edit.hide_completion()
        self.stop_check()

    def show_find(self):
        selected = self.edit.textCursor().selectedText()
        if selected and '\u2029' not in selected:
            self.find.setText(selected)
        self.find_panel.show()
        self.find.setFocus()
        self.find.selectAll()
        self.refresh_matches()

    def hide_find(self):
        self.find_panel.hide()
        self.edit.matches = []
        self.edit.decorate()
        self.edit.setFocus()

    def refresh_matches(self, *args):
        text, needle = self.edit.toPlainText(), self.find.text()
        self.search_matches = []
        if needle and self.find_panel.isVisible():
            pattern = re.escape(needle)
            if self.whole.isChecked():
                pattern = r'(?<!\w)' + pattern + r'(?!\w)'
            found = re.finditer(pattern, text, 0 if self.case.isChecked() else re.IGNORECASE)
            # Each mapping pass is linear, including non-BMP Unicode text.
            offsets, offset = [0], 0
            for char in text:
                offset += 2 if ord(char) > 0xffff else 1
                offsets.append(offset)
            self.search_matches = [(offsets[m.start()], offsets[m.end()]) for m in found]
        self.edit.matches = self.search_matches
        self.find_count.setText('%d találat' % len(self.search_matches))
        self.edit.decorate()

    def find_next(self, backwards=False):
        if not self.find_panel.isVisible():
            self.show_find()
        self.refresh_matches()
        if not self.search_matches:
            return
        cursor = self.edit.textCursor()
        if backwards:
            eligible = [m for m in self.search_matches if m[0] < cursor.selectionStart()]
            chosen = (eligible or self.search_matches)[-1]
        else:
            eligible = [m for m in self.search_matches if m[0] >= cursor.selectionEnd()]
            chosen = (eligible or self.search_matches)[0]
        cursor.setPosition(chosen[0])
        cursor.setPosition(chosen[1], QTextCursor.MoveMode.KeepAnchor)
        self.edit.setTextCursor(cursor)
        self.edit.ensureCursorVisible()

    def replace_one(self):
        self.refresh_matches()
        cursor = self.edit.textCursor()
        if (cursor.selectionStart(), cursor.selectionEnd()) not in self.search_matches:
            self.find_next()
            return
        cursor.insertText(self.replacement.text())
        self.find_next()

    def replace_all(self):
        self.refresh_matches()
        matches = list(self.search_matches)
        cursor = QTextCursor(self.edit.document())
        cursor.beginEditBlock()
        for start, end in reversed(matches):
            cursor.setPosition(start)
            cursor.setPosition(end, QTextCursor.MoveMode.KeepAnchor)
            cursor.insertText(self.replacement.text())
        cursor.endEditBlock()
        self.refresh_matches()

    def goto_dialog(self):
        line, ok = QInputDialog.getInt(self, 'Ugrás sorra', 'Sor:', self.edit.textCursor().blockNumber() + 1,
                                       1, self.edit.blockCount())
        if ok:
            self.edit.goto(line)

    def format_json(self):
        if self.edit.syntax.language != 'json':
            return
        text = self.edit.toPlainText()
        issues = json_issues(text)
        if issues:
            self.set_issues(issues, 'Hibás JSON · a szöveg változatlan maradt.')
            return
        formatted = json.dumps(api.strict_json(text), ensure_ascii=False, indent=2, allow_nan=False) + '\n'
        cursor = QTextCursor(self.edit.document())
        cursor.select(QTextCursor.SelectionType.Document)
        cursor.insertText(formatted)
