"""Qt Widgets frontend. No services are started by opening the application."""
import argparse
import base64
import copy
import json
import os
from pathlib import Path
import secrets
import sys
import time

from PySide6.QtCore import QProcess, QSettings, Qt, QTimer, QUrl, Signal
from PySide6.QtGui import QColor, QDesktopServices, QFont, QFontDatabase, QKeySequence, QShortcut
from PySide6.QtWidgets import (QAbstractItemView, QApplication, QCheckBox, QComboBox,
    QFileDialog, QFrame, QHBoxLayout, QHeaderView, QInputDialog, QLabel, QLineEdit,
    QListWidget, QListWidgetItem, QMainWindow, QMessageBox, QPlainTextEdit,
    QPushButton, QSizePolicy, QSplitter, QStackedWidget, QStyledItemDelegate, QTabWidget,
    QTableWidget, QTableWidgetItem, QToolButton, QMenu, QTreeWidget, QTreeWidgetItem, QVBoxLayout, QWidget)

import backend as api
from config_form import ConfigForm
from remote_page import RemotePage
from code_editor import CodePane
import server_targets as targets
import ssh_connection as ssh
from crash_page import CrashPage

PATH_ROLE = int(Qt.ItemDataRole.UserRole)
VALUE_ROLE = PATH_ROLE + 1
LABELS = {
    'common': 'Közös beállítások', 'database': 'Database szerver', 'login': 'Beléptető szerver',
    'game_defaults': 'Game core alapértékek', 'game_servers': 'Csatornák és core-ok',
    'network': 'Hálózat', 'server': 'Szerver', 'sql': 'SQL-kapcsolatok', 'tls': 'TLS-tanúsítvány',
    'gameplay': 'Játékszabályok', 'quest': 'Quest útvonalak', 'version': 'Kliensverzió',
    'guild': 'Céhbeállítások', 'logs': 'Naplózás', 'events': 'Időzítések',
    'working_directory': 'Futási mappa', 'locale': 'Adatbázis-karakterkódolás',
    'hostname': 'Szerver neve', 'channel': 'Csatorna', 'server_id': 'Core azonosító',
    'max_level': 'Maximális szint', 'user_limit': 'Játékoslimit', 'data_path': 'Játékadatok mappája',
    'port': 'Port', 'p2p_port': 'P2P-port', 'bind_port': 'Figyelőport', 'bind_ip': 'Figyelő IP-cím',
    'db_port': 'Database port', 'db_addr': 'Database címe', 'host': 'Szervercím',
    'user': 'Felhasználónév', 'password': 'Jelszó', 'interserver_key': 'Szerverek közötti kulcs',
    'cert_file': 'Tanúsítványfájl', 'key_file': 'Privátkulcs fájlja', 'map_allow': 'Engedélyezett mapazonosítók',
    'item_id_range': 'Kiosztható tárgyazonosítók', 'schema_version': 'Konfiguráció formátumverziója',
    'cache': 'Adatbázis-cache', 'proto': 'Prototípusok', 'from_db': 'Prototípusok SQL-ből',
    'mirror2db': 'Prototípusok tükrözése SQL-be', 'table_postfix': 'SQL-táblák utótagja',
    'vnum': 'VNUM', 'npcs': 'NPC-k', 'items': 'Hozzávalók', 'rewards': 'Jutalmak',
    'percent': 'Esély (%)', 'chance': 'Esély', 'category': 'Kategória',
    'count': 'Darabszám', 'name': 'Név', 'gold': 'Yang', 'map_index': 'Mapazonosító',
}

STYLE = '''
QWidget { background: #111925; color: #e4eaf3; font-family: "Segoe UI"; font-size: 13px; }
QLabel, QCheckBox { background: transparent; }
QMainWindow, QStackedWidget { background: #111925; }
QLabel#Title { font-size: 25px; font-weight: 650; }
QLabel#Subtle { color: #96a6bd; }
QLabel#SectionTitle { font-size: 20px; font-weight: 600; }
QLabel#FieldHint { color: #8498b1; font-size: 11px; }
QLabel#SavedBadge { background: #213b38; color: #8bdbc0; border-radius: 4px; padding: 5px 9px; }
QLabel#SavedBadge[dirty="true"] { background: #4a3b25; color: #ffda95; }
QWidget#FormCard { background: #182331; border-radius: 7px; }
QWidget#FormField { background: transparent; }
QTableWidget { alternate-background-color: #1d2b3b; }
QListWidget#Categories { border: none; background: #141f2d; }
QListWidget#Categories::item { padding: 12px 10px; border-radius: 4px; margin-bottom: 3px; }
QListWidget#Categories::item:selected { background: #2c4c64; color: #ffffff; }
QLineEdit[invalid="true"], QPlainTextEdit[invalid="true"] { border: 1px solid #e98383; }
QLabel#Metric { font-size: 23px; font-weight: 650; color: #83d6c0; padding: 16px; background: #1b2939; border-radius: 8px; }
QPushButton { background: #26374d; padding: 8px 14px; border: 1px solid #364960; border-radius: 5px; }
QPushButton:hover { background: #314963; }
QPushButton:disabled { color: #637188; background: #1c2735; border-color: #263345; }
QPushButton#Primary { background: #287c67; border-color: #37967c; font-weight: 600; }
QPushButton#Stop { background: #73434e; border-color: #945061; }
QPushButton#Primary:disabled, QPushButton#Stop:disabled { background: #202d3b; border-color: #2e3e50; color: #8591a5; }
QLineEdit, QPlainTextEdit, QTreeWidget, QListWidget, QTableWidget, QComboBox {
 background: #172231; border: 1px solid #2c3e54; border-radius: 5px; padding: 5px; selection-background-color: #305d70;
}
QTreeWidget::item, QListWidget::item { padding: 4px; }
QTreeWidget::item:selected, QListWidget::item:selected, QTableWidget::item:selected { background: #30576d; color: #ffffff; }
QHeaderView::section { background: #213147; padding: 7px; border: none; color: #acbdd2; }
QTabBar::tab { background: #1a2738; padding: 8px 18px; }
QTabBar::tab:selected { background: #305064; }
QCheckBox { padding: 4px; }
QToolButton { background: #24364a; border: 1px solid #364960; border-radius: 5px; padding: 8px 12px; }
QToolButton:checked { background: #305064; }
QMenu { background: #1b2939; border: 1px solid #364960; padding: 5px; }
QMenu::item { padding: 8px 18px; }
QMenu::item:selected { background: #305064; }
QSplitter::handle { background: #233347; }
QScrollBar:vertical { background: #15202c; width: 11px; }
QScrollBar::handle:vertical { background: #3a4d64; min-height: 25px; }
'''


def button(text, callback, name=None):
    result = QPushButton(text)
    if name:
        result.setObjectName(name)
    result.clicked.connect(callback)
    return result


def note(text):
    label = QLabel(text)
    label.setObjectName('Subtle')
    label.setWordWrap(True)
    label.setSizePolicy(QSizePolicy.Policy.Preferred, QSizePolicy.Policy.Minimum)
    return label


class ElidedLabel(QLabel):
    """Long paths never push the save toolbar beyond the window edge."""
    def __init__(self, text=''):
        super().__init__()
        self.full_text = ''
        self.setMinimumWidth(60)
        self.setSizePolicy(QSizePolicy.Policy.Ignored, QSizePolicy.Policy.Preferred)
        self.setTextFormat(Qt.TextFormat.PlainText)
        self.setText(text)

    def setText(self, text):
        self.full_text = text
        self.setToolTip(text)
        super().setText(self.fontMetrics().elidedText(text, Qt.TextElideMode.ElideMiddle, max(40, self.width())))

    def resizeEvent(self, event):
        super().resizeEvent(event)
        self.setText(self.full_text)


class ValueDelegate(QStyledItemDelegate):
    def createEditor(self, parent, option, index):
        if index.model() == self.parent().tree.model() and index.column() != 1:
            return None
        if not index.flags() & Qt.ItemFlag.ItemIsEditable:
            return None
        value = index.data(VALUE_ROLE)
        if type(value) is bool:
            editor = QComboBox(parent)
            editor.addItems(['Kikapcsolva', 'Bekapcsolva'])
        else:
            editor = QLineEdit(parent)
            if any(api.secret_field(part) for part in index.data(PATH_ROLE) or []):
                editor.setEchoMode(QLineEdit.EchoMode.Password)
        owner = self.parent()
        owner.active_editor = (editor, index, self)
        if isinstance(editor, QLineEdit):
            editor.textEdited.connect(lambda text: owner.changed.emit())
        else:
            editor.activated.connect(lambda index: owner.changed.emit())
        def destroyed():
            active = owner.active_editor
            if active and active[0] is editor:
                owner.active_editor = None
        editor.destroyed.connect(destroyed)
        return editor

    def setEditorData(self, editor, index):
        value = index.data(VALUE_ROLE)
        if isinstance(editor, QComboBox):
            editor.setCurrentIndex(int(value))
        else:
            editor.setText(value if isinstance(value, str) else json.dumps(value))

    def editor_value(self, editor, index):
        old = index.data(VALUE_ROLE)
        if isinstance(editor, QComboBox):
            value = bool(editor.currentIndex())
        elif isinstance(old, str):
            value = editor.text()
        else:
            value = api.strict_json(editor.text())
            if type(value) is not type(old):
                raise api.EditorError('Az érték típusának változatlannak kell maradnia.')
        return value

    def setModelData(self, editor, model, index):
        try:
            value = self.editor_value(editor, index)
            self.parent().change_value(index, value)
        except api.EditorError as error:
            QMessageBox.warning(self.parent(), 'Érvénytelen érték', str(error))


class JsonFields(QWidget):
    changed = Signal()

    def __init__(self):
        super().__init__()
        self.value = None
        self.active_editor = None
        self.table_path, self.table_rows = [], []
        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        tools = QHBoxLayout()
        self.search = QLineEdit()
        self.search.setPlaceholderText('Mező vagy érték keresése…')
        self.search.textChanged.connect(self.filter)
        tools.addWidget(self.search, 1)
        self.add_button = button('Hozzáadás', self.add)
        self.copy_button = button('Sor másolása', self.duplicate)
        self.remove_button = button('Törlés', self.remove)
        for control in (self.add_button, self.copy_button, self.remove_button):
            tools.addWidget(control)
        layout.addLayout(tools)
        navigation = QHBoxLayout()
        self.up_button = button('← Vissza', self.up)
        navigation.addWidget(self.up_button)
        self.breadcrumb = ElidedLabel('Dokumentum')
        navigation.addWidget(self.breadcrumb, 1)
        self.details_button = button('Sor részletei', self.open_selected)
        navigation.addWidget(self.details_button)
        self.mode = QComboBox()
        self.mode.addItems(['Táblázat', 'Szerkezeti fa'])
        self.mode.currentIndexChanged.connect(self.change_mode)
        navigation.addWidget(self.mode)
        layout.addLayout(navigation)
        self.stack = QStackedWidget()
        self.table = QTableWidget()
        self.table.setAlternatingRowColors(True)
        self.table.setSelectionBehavior(QAbstractItemView.SelectionBehavior.SelectRows)
        self.table.setItemDelegate(ValueDelegate(self))
        self.table.setWordWrap(False)
        self.table.verticalHeader().setDefaultSectionSize(33)
        self.table.horizontalHeader().setStretchLastSection(True)
        self.table.itemDoubleClicked.connect(self.open_cell)
        self.stack.addWidget(self.table)
        self.tree = QTreeWidget()
        self.tree.setColumnCount(3)
        self.tree.setHeaderLabels(['Mező / sor', 'Érték', 'Típus'])
        self.tree.header().setSectionResizeMode(0, QHeaderView.ResizeMode.Interactive)
        self.tree.setColumnWidth(0, 310)
        self.tree.header().setSectionResizeMode(1, QHeaderView.ResizeMode.Stretch)
        self.tree.setColumnWidth(2, 90)
        self.tree.setItemDelegate(ValueDelegate(self))
        self.tree.setEditTriggers(QAbstractItemView.EditTrigger.DoubleClicked | QAbstractItemView.EditTrigger.EditKeyPressed)
        self.tree.itemDoubleClicked.connect(self.edit_cell)
        self.stack.addWidget(self.tree)
        layout.addWidget(self.stack)
        layout.addWidget(note('Dupla kattintás: érték szerkesztése vagy adatcsoport megnyitása. A sorok külön is megnyithatók.'))

    def edit_cell(self, item, column):
        if column == 1 and not isinstance(item.data(1, VALUE_ROLE), (dict, list)):
            self.tree.editItem(item, 1)

    def set_value(self, value):
        self.value = copy.deepcopy(value)
        self.table_path = []
        if isinstance(value, dict) and len(value) == 1:
            key = next(iter(value))
            if isinstance(value[key], list):
                self.table_path = [key]
        self.rebuild()

    def flush(self):
        # Ctrl+S can arrive while the user is still typing into a table cell.
        # Validate and publish that edit before taking the JSON snapshot.
        if self.active_editor:
            editor, index, delegate = self.active_editor
            if index.isValid():
                self.change_value(index, delegate.editor_value(editor, index))

    def display(self, item, value, path):
        secret = any(api.secret_field(part) for part in path)
        text = ('••••••' if secret and value else '') if secret else value if isinstance(value, str) else json.dumps(value, ensure_ascii=False)
        item.setText(1, text)
        item.setData(1, VALUE_ROLE, value)
        item.setData(1, PATH_ROLE, list(path))

    def rebuild(self):
        self.tree.clear()
        count = 0
        def append(parent, key, value, path):
            nonlocal count
            count += 1
            label = LABELS.get(str(key), str(key))
            item = QTreeWidgetItem(parent, [label, '', type(value).__name__])
            item.setToolTip(0, '.'.join(map(str, path)) or '$')
            item.setData(0, PATH_ROLE, path)
            item.setData(1, PATH_ROLE, path)
            item.setData(1, VALUE_ROLE, value)
            if isinstance(value, (dict, list)):
                item.setText(1, '%d elem' % len(value))
                values = value.items() if isinstance(value, dict) else enumerate(value)
                for child_key, child in values:
                    append(item, child_key, child, path + [child_key])
            else:
                item.setFlags(item.flags() | Qt.ItemFlag.ItemIsEditable)
                self.display(item, value, path)
        append(self.tree, 'Dokumentum', self.value, [])
        self.tree.expandToDepth(0)
        self.build_table()
        self.filter()

    def change_value(self, index, value):
        path = index.data(PATH_ROLE)
        if path:
            api.value_at(self.value, path[:-1])[path[-1]] = value
        else:
            self.value = value
        if index.model() == self.table.model():
            self.table_cell(self.table.itemFromIndex(index), value, path)
        else:
            self.display(self.tree.itemFromIndex(index), value, path)
        self.changed.emit()

    def selected(self):
        if self.mode.currentIndex() == 0:
            row = self.table.currentRow()
            return self.table_rows[row] if 0 <= row < len(self.table_rows) else self.table_path
        item = self.tree.currentItem()
        return item.data(0, PATH_ROLE) if item else []

    def filter(self):
        text = self.search.text().casefold()
        if self.mode.currentIndex() == 0:
            for row in range(self.table.rowCount()):
                self.table.setRowHidden(row, bool(text) and not any(text in self.table.item(row, column).text().casefold()
                    for column in range(self.table.columnCount()) if self.table.item(row, column)))
            return
        def visit(item):
            own = any(text in item.text(column).casefold() for column in (0, 1)) or text in item.toolTip(0).casefold()
            matches = [visit(item.child(i)) for i in range(item.childCount())]
            visible = own or any(matches)
            item.setHidden(not visible)
            if text and any(matches):
                item.setExpanded(True)
            return visible
        if self.tree.topLevelItemCount():
            visit(self.tree.topLevelItem(0))

    def add(self):
        path = self.table_path if self.mode.currentIndex() == 0 else self.selected()
        container = api.value_at(self.value, path)
        if not isinstance(container, (dict, list)):
            QMessageBox.information(self, 'Hozzáadás', 'Válassz egy objektumot vagy listát.')
            return
        key = len(container)
        if isinstance(container, list) and container and isinstance(container[0], dict):
            def blank(value):
                if isinstance(value, dict): return {key: blank(child) for key, child in value.items()}
                if isinstance(value, list): return []
                if isinstance(value, bool): return False
                if isinstance(value, (int, float)): return type(value)(0)
                return '' if isinstance(value, str) else None
            container.append(blank(container[0]))
            self.rebuild()
            self.changed.emit()
            return
        if isinstance(container, dict):
            key, accepted = QInputDialog.getText(self, 'Új mező', 'Mező neve (a forrásban használt kulcs):')
            if not accepted or not key:
                return
            if key in container:
                QMessageBox.warning(self, 'Új mező', 'Ez a mező már létezik.')
                return
        kind, accepted = QInputDialog.getItem(self, 'Új érték', 'Típus:', ['Szöveg', 'Egész szám', 'Tizedes szám', 'Logikai', 'Objektum', 'Lista'], 0, False)
        if not accepted:
            return
        value = {'Szöveg': '', 'Egész szám': 0, 'Tizedes szám': 0.0, 'Logikai': False, 'Objektum': {}, 'Lista': []}[kind]
        if isinstance(container, list):
            container.append(value)
        else:
            container[key] = value
        self.rebuild()
        self.changed.emit()

    def change_mode(self, index):
        self.stack.setCurrentIndex(index)
        self.up_button.setVisible(index == 0)
        self.breadcrumb.setVisible(index == 0)
        self.details_button.setVisible(index == 0)
        if index:
            self.copy_button.setEnabled(True)
            self.add_button.setText('Új elem')
            self.search.setPlaceholderText('Keresés a szerkezeti fában…')
        else:
            self.build_table()
        self.filter()

    def table_cell(self, item, value, path):
        item.setData(PATH_ROLE, list(path))
        item.setData(VALUE_ROLE, value)
        secret = any(api.secret_field(part) for part in path)
        text = '••••••' if secret and value else value if isinstance(value, str) else ('Bekapcsolva' if value else 'Kikapcsolva') if isinstance(value, bool) else json.dumps(value, ensure_ascii=False)
        item.setText(text)
        item.setToolTip('.'.join(map(str, path)))
        item.setFlags(item.flags() | Qt.ItemFlag.ItemIsEditable)
        if isinstance(value, (dict, list)):
            item.setText('%d elem  ›' % len(value))
            item.setFlags(item.flags() & ~Qt.ItemFlag.ItemIsEditable)

    def build_table(self):
        try:
            node = api.value_at(self.value, self.table_path)
        except (KeyError, IndexError, TypeError):
            self.table_path = []
            node = self.value
        self.table.clear()
        self.table_rows = []
        records = isinstance(node, list) and bool(node) and all(isinstance(row, dict) for row in node)
        if records:
            columns = list(dict.fromkeys(key for row in node for key in row)) or ['']
            rows = list(enumerate(node))
        elif isinstance(node, dict):
            columns, rows = ['Mező', 'Érték'], list(node.items())
        elif isinstance(node, list):
            columns, rows = ['Érték'], list(enumerate(node))
        else:
            columns, rows = ['Érték'], [(None, node)]
        self.table.setColumnCount(len(columns))
        self.table.setHorizontalHeaderLabels([LABELS.get(key, key.replace('_', ' ').capitalize()) for key in columns])
        self.table.setRowCount(len(rows))
        for row_index, (key, value) in enumerate(rows):
            path = self.table_path + ([] if key is None else [key])
            self.table_rows.append(path)
            for column, name in enumerate(columns):
                item = QTableWidgetItem()
                self.table.setItem(row_index, column, item)
                if records:
                    if name in value:
                        self.table_cell(item, value[name], path + [name])
                    else:
                        item.setText('—')
                        item.setFlags(item.flags() & ~Qt.ItemFlag.ItemIsEditable)
                elif isinstance(node, dict) and column == 0:
                    item.setText(LABELS.get(str(key), str(key)))
                    item.setToolTip(str(key))
                    item.setFlags(item.flags() & ~Qt.ItemFlag.ItemIsEditable)
                else:
                    self.table_cell(item, value, path)
        for column in range(len(columns)):
            self.table.setColumnWidth(column, 165 if records else 230)
        self.breadcrumb.setText('Dokumentum' + ''.join('  ›  ' + str(part) for part in self.table_path))
        self.up_button.setEnabled(bool(self.table_path))
        self.add_button.setText('Új sor' if isinstance(node, list) else 'Új mező')
        self.copy_button.setEnabled(isinstance(node, list) or self.mode.currentIndex() == 1)
        self.search.setPlaceholderText('Keresés a táblázatban…' if self.mode.currentIndex() == 0 else 'Keresés a szerkezeti fában…')
        self.filter()

    def navigate(self, path):
        self.table_path = list(path)
        self.search.clear()
        self.build_table()

    def up(self):
        self.navigate(self.table_path[:-1])

    def open_selected(self):
        path = self.selected()
        if path != self.table_path:
            self.navigate(path)

    def open_cell(self, item):
        path = item.data(PATH_ROLE)
        if path is not None and isinstance(api.value_at(self.value, path), (dict, list)):
            self.navigate(path)

    def duplicate(self):
        path = self.selected()
        if not path or not isinstance(api.value_at(self.value, path[:-1]), list):
            QMessageBox.information(self, 'Sor másolása', 'Válassz egy listában található sort.')
            return
        rows = api.value_at(self.value, path[:-1])
        rows.insert(path[-1] + 1, copy.deepcopy(rows[path[-1]]))
        self.rebuild()
        self.changed.emit()

    def remove(self):
        path = self.selected()
        if not path:
            return
        del api.value_at(self.value, path[:-1])[path[-1]]
        self.rebuild()
        self.changed.emit()


class Editor(QWidget):
    def __init__(self, window):
        super().__init__()
        self.window, self.document, self.dirty, self.syncing = window, None, False, False
        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        bar = QHBoxLayout()
        self.filename = ElidedLabel('Válassz egy állományt')
        self.filename.setObjectName('SectionTitle')
        bar.addWidget(self.filename, 1)
        self.saved_badge = QLabel('Mentett')
        self.saved_badge.setObjectName('SavedBadge')
        bar.addWidget(self.saved_badge)
        self.save_button = button('Ellenőrzés és mentés', self.save, 'Primary')
        self.save_button.setEnabled(False)
        bar.addWidget(self.save_button)
        more = QToolButton()
        more.setText('Továbbiak')
        more.setPopupMode(QToolButton.ToolButtonPopupMode.InstantPopup)
        menu = QMenu(more)
        menu.addAction('Fájl újratöltése', self.reload)
        menu.addAction('Korábbi mentés visszaállítása', self.restore)
        more.setMenu(menu)
        bar.addWidget(more)
        layout.addLayout(bar)
        self.tabs = QTabWidget()
        self.fields = JsonFields()
        self.config_form = ConfigForm(JsonFields)
        self.config_form.error.connect(lambda text: window.error(text))
        self.config_form.changed.connect(self.changed)
        self.field_pages = QStackedWidget()
        self.field_pages.addWidget(self.config_form)
        self.field_pages.addWidget(self.fields)
        self.code = CodePane()
        self.source = self.code.edit
        self.tabs.addTab(self.field_pages, 'Szerkesztés')
        self.tabs.addTab(self.code, 'Forráskód')
        self.fields.changed.connect(self.changed)
        self.source.textChanged.connect(self.changed)
        self.tabs.currentChanged.connect(self.switch_tab)
        layout.addWidget(self.tabs, 1)
        self.empty = QLabel('Válassz egy állományt a bal oldali listából.\nA tartalma itt jelenik meg szerkeszthető formában.')
        self.empty.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.empty.setObjectName('Subtle')
        layout.addWidget(self.empty, 1)
        self.tabs.hide()
        self.saved_badge.hide()
        self.info = note('Mentéskor az eredeti fájl másolata a run/editor-backups mappába kerül.')
        layout.addWidget(self.info)

    def clear(self):
        self.document, self.dirty = None, False
        self.filename.setText('Válassz egy állományt')
        self.source.clear()
        self.code.set_context('')
        self.fields.set_value({})
        self.tabs.hide()
        self.empty.show()
        self.saved_badge.hide()
        self.save_button.setEnabled(False)

    def changed(self):
        if not self.syncing and self.document:
            self.dirty = True
            self.saved_badge.setText('Nincs mentve')
            self.saved_badge.setProperty('dirty', True)
            self.saved_badge.style().unpolish(self.saved_badge)
            self.saved_badge.style().polish(self.saved_badge)
            self.window.refresh()

    def active_fields(self):
        return self.config_form if self.document and self.document.relative == 'server.json' else self.fields

    def may_discard(self):
        if not self.dirty:
            return True
        return QMessageBox.question(self, 'Nem mentett módosítások',
            'Elveted a nem mentett módosításokat? A megtartásukhoz válaszd a Nem lehetőséget, majd mentsd a fájlt.',
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No,
            QMessageBox.StandardButton.No) == QMessageBox.StandardButton.Yes

    def load(self, relative, force=False):
        if not force and not self.may_discard():
            return False
        if self.window.is_remote():
            return self.window.remote_request('read', dict(relative=relative),
                lambda packet: self.display(targets.document(packet)))
        try:
            return self.display(api.Document(self.window.root, relative, allow_invalid_json=True))
        except (api.EditorError, OSError) as error:
            self.window.error(str(error))
            return False

    def display(self, document):
        document.target_id = self.window.target_id()
        try:
            self.syncing = True
            self.document = document
            self.empty.hide()
            self.tabs.show()
            self.saved_badge.show()
            self.source.setPlainText(document.text)
            self.code.set_context(document.relative, document.encoding, document.newline)
            self.tabs.setTabVisible(0, document.is_json)
            # Config secrets stay masked in the field editor; all fields remain editable.
            self.tabs.setTabVisible(1, document.relative != 'server.json')
            self.tabs.tabBar().setVisible(document.is_json and document.relative != 'server.json')
            if document.is_json and not document.parse_error:
                active = self.active_fields()
                self.field_pages.setCurrentWidget(active)
                active.set_value(document.value)
            self.tabs.setCurrentIndex(0 if document.is_json and not document.parse_error else 1)
            self.filename.setText('Szerver beállításai' if document.relative == 'server.json' else Path(document.relative).name)
            self.filename.setToolTip(document.relative)
            self.saved_badge.setText('Mentett')
            self.saved_badge.setProperty('dirty', False)
            self.saved_badge.style().unpolish(self.saved_badge)
            self.saved_badge.style().polish(self.saved_badge)
            self.info.setText('A mentés ellenőrzés után készül el. A korábbi változat a Továbbiak menüből állítható vissza.'
                              if document.is_json else 'Kódolás: %s. A mentett questváltozások a Questek frissítése gombbal aktiválhatók.' % document.encoding)
            self.dirty = False
            self.save_button.setEnabled(False)
            return True
        except (api.EditorError, OSError) as error:
            self.window.error(str(error))
            return False
        finally:
            self.syncing = False

    def switch_tab(self, current):
        if self.syncing or not self.document or not self.document.is_json:
            return
        try:
            self.syncing = True
            if current == 1:
                self.source.setPlainText(json.dumps(self.active_fields().value, ensure_ascii=False, indent=2) + '\n')
            else:
                self.active_fields().set_value(api.strict_json(self.source.toPlainText()))
        except api.EditorError as error:
            self.tabs.setCurrentIndex(1)
            self.window.error(str(error))
        finally:
            self.syncing = False

    def text(self):
        if self.document.is_json and self.tabs.currentIndex() == 0:
            active = self.active_fields()
            if isinstance(active, ConfigForm):
                value = active.checked_value()
            else:
                active.flush()
                value = active.value
            return json.dumps(value, ensure_ascii=False, indent=2, allow_nan=False) + '\n'
        return self.source.toPlainText()

    def save(self):
        if not self.document or self.window.busy():
            return
        try:
            payload = self.document.encoded(self.text())
            if getattr(self.document, 'target_id', None) != self.window.target_id():
                raise api.EditorError('A megnyitott fájl másik szerverhez tartozik. Töltsd újra a kiválasztott célgépről.')
            if self.window.is_remote():
                relative = self.document.relative
                def saved(result):
                    self.window.show_remote_output(result.get('output', 'Mentve.'))
                    self.display(targets.document(result['document']))
                    self.window.poll_remote(force=True)
                self.window.remote_request('save', dict(relative=relative,
                    data=base64.b64encode(payload).decode('ascii'), expected_hash=self.document.digest), saved)
                return
            directory = self.window.root / 'run/editor-pending'
            directory.mkdir(parents=True, exist_ok=True)
            pending = directory / ('payload-' + secrets.token_hex(8))
            descriptor = os.open(pending, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
            with os.fdopen(descriptor, 'wb') as handle:
                handle.write(payload)
            relative = self.document.relative
            def finished(ok):
                pending.unlink(missing_ok=True)
                if ok:
                    self.load(relative, force=True)
                    if relative == 'server.json':
                        self.window.update_channels()
            self.window.job(['save', relative, str(pending), self.document.digest], finished)
        except (api.EditorError, OSError, ValueError) as error:
            self.window.error(str(error))

    def reload(self):
        if self.document:
            self.load(self.document.relative)

    def restore(self):
        if not self.document or self.window.busy():
            return
        if self.window.is_remote():
            relative = self.document.relative
            def available(files):
                if not files:
                    QMessageBox.information(self, 'Korábbi mentés', 'Ehhez a távoli fájlhoz még nincs korábbi mentés.')
                    return
                choices = [name.removeprefix('run/editor-backups/') for name in files]
                choice, accepted = QInputDialog.getItem(self, 'Távoli korábbi mentés',
                    'Betöltés a szerkesztőbe; visszaállítás a Mentés gombbal:', choices, 0, False)
                if accepted and self.may_discard():
                    self.window.remote_request('restore', dict(relative=relative, backup=files[choices.index(choice)]),
                        lambda packet: self.restore_text(base64.b64decode(packet['data'], validate=True)))
            self.window.remote_request('backups', dict(relative=relative), available)
            return
        files = api.backup_files(self.window.root, self.document.relative)
        if not files:
            QMessageBox.information(self, 'Korábbi mentés', 'Ehhez a fájlhoz még nincs korábbi mentés.')
            return
        choices = [p.relative_to(self.window.root / 'run/editor-backups').as_posix() for p in files]
        choice, accepted = QInputDialog.getItem(self, 'Korábbi mentés', 'Betöltés a szerkesztőbe (a visszaállítást ezután a Mentés végzi):', choices, 0, False)
        if not accepted or not self.may_discard():
            return
        try:
            payload = api.restore_payload(self.window.root, self.document.relative, files[choices.index(choice)])
            text = payload.decode(self.document.encoding).replace('\r\n', '\n')
            self.syncing = True
            self.source.setPlainText(text)
            if self.document.is_json:
                self.active_fields().set_value(api.strict_json(text))
            self.syncing = False
            self.changed()
        except (api.EditorError, OSError, UnicodeError) as error:
            self.window.error(str(error))
        finally:
            self.syncing = False

    def restore_text(self, payload):
        try:
            text = payload.decode(self.document.encoding).replace('\r\n', '\n')
            self.syncing = True
            self.source.setPlainText(text)
            if self.document.is_json:
                self.active_fields().set_value(api.strict_json(text))
        finally:
            self.syncing = False
        self.changed()


class FilePage(QWidget):
    def __init__(self, window, quests=False):
        super().__init__()
        self.window, self.quests = window, quests
        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        splitter = QSplitter()
        browser = QWidget()
        browser.setMinimumWidth(180)
        browser.setMaximumWidth(310)
        left = QVBoxLayout(browser)
        left.setContentsMargins(0, 0, 0, 0)
        self.search = QLineEdit()
        self.search.setPlaceholderText('Fájlnév keresése…')
        self.search.textChanged.connect(self.filter)
        self.count = note('Állományok')
        left.addWidget(self.count)
        left.addWidget(self.search)
        left.addWidget(button('Lista frissítése', window.refresh_inventory))
        self.files = QTreeWidget()
        self.files.setHeaderHidden(True)
        self.files.setIndentation(15)
        self.files.setHorizontalScrollBarPolicy(Qt.ScrollBarPolicy.ScrollBarAlwaysOff)
        self.files.itemClicked.connect(self.open_file)
        left.addWidget(self.files)
        splitter.addWidget(browser)
        self.editor = Editor(window)
        splitter.addWidget(self.editor)
        splitter.setSizes([235, 850])
        splitter.setStretchFactor(0, 0)
        splitter.setStretchFactor(1, 1)
        layout.addWidget(splitter)

    def refresh(self):
        paths = self.window.file_paths(self.quests)
        self.files.clear()
        folders = {}
        prefix = 'data/quest' if self.quests else 'data'
        for relative in paths:
            path = Path(relative)
            local = path.relative_to(prefix)
            parent = self.files.invisibleRootItem()
            parts = local.parts
            for depth, folder in enumerate(parts[:-1]):
                key = parts[:depth + 1]
                if key not in folders:
                    folders[key] = QTreeWidgetItem(parent, [folder])
                parent = folders[key]
            item = QTreeWidgetItem(parent, [path.name])
            item.setToolTip(0, local.as_posix())
            item.setData(0, PATH_ROLE, relative)
        self.count.setText('%d állomány · %s' % (len(paths), 'Questek és Lua' if self.quests else 'Játékadatok'))
        self.files.expandToDepth(0)
        self.filter()

    def filter(self):
        text = self.search.text().casefold()
        def visit(item):
            children = [visit(item.child(i)) for i in range(item.childCount())]
            own = text in (item.toolTip(0) or item.text(0)).casefold()
            visible = own or any(children)
            item.setHidden(not visible)
            if text and any(children):
                item.setExpanded(True)
            return visible
        for i in range(self.files.topLevelItemCount()):
            visit(self.files.topLevelItem(i))

    def open_file(self, item, column=0):
        relative = item.data(0, PATH_ROLE)
        if relative:
            self.editor.load(relative)


class MainWindow(QMainWindow):
    def __init__(self, root=None):
        super().__init__()
        self.root, self.process, self.detached = None, None, None
        self.local_root = None
        self.remote_profile = self.remote_client = self.remote_state = None
        self.remote_inventory = dict(data=[], quests=[], logs=[])
        self.remote_online = False
        self.remote_next_poll = 0
        self.remote_next_log = 0
        self.target_epoch = 0
        self.action_log, self.log_text, self.on_finished = None, '', None
        self.settings = QSettings('Auriga', 'ServerManager')
        self.setWindowTitle('Auriga • Szerverkezelő')
        self.setMinimumSize(1100, 760)
        self.resize(1320, 860)
        center = QWidget()
        self.setCentralWidget(center)
        layout = QVBoxLayout(center)
        layout.setContentsMargins(20, 18, 20, 14)
        header = QHBoxLayout()
        title = QLabel('Auriga szerverkezelő')
        title.setObjectName('Title')
        header.addWidget(title, 1)
        self.choose_root_button = button('Helyi szervermappa…', self.choose_root)
        self.open_root_button = button('Megnyitás Intézőben', self.open_root)
        header.addWidget(self.choose_root_button)
        header.addWidget(self.open_root_button)
        layout.addLayout(header)
        target_bar = QHBoxLayout()
        target_bar.addWidget(QLabel('Kezelt szerver:'))
        self.target_choice = QComboBox()
        self.target_choice.setMinimumWidth(280)
        self.target_choice.setSizeAdjustPolicy(QComboBox.SizeAdjustPolicy.AdjustToMinimumContentsLengthWithIcon)
        self.target_choice.setMinimumContentsLength(22)
        target_bar.addWidget(self.target_choice, 1)
        self.target_badge = QLabel('HELYI')
        self.target_badge.setObjectName('Metric')
        target_bar.addWidget(self.target_badge)
        self.connect_button = button('Frissítés', self.reconnect_target)
        target_bar.addWidget(self.connect_button)
        target_bar.addWidget(button('SSH-profilok és eszközök', lambda: self.navigation.setCurrentRow(5)))
        layout.addLayout(target_bar)
        self.location = ElidedLabel('Válassz szervercsomagot a kezdéshez.')
        self.location.setObjectName('Subtle')
        self.location.setTextInteractionFlags(Qt.TextInteractionFlag.TextSelectableByMouse)
        layout.addWidget(self.location)
        self.crash_alert = QLabel()
        self.crash_alert.setWordWrap(True)
        self.crash_alert.setStyleSheet('background: #592d30; color: #ffd2cb; padding: 8px;')
        self.crash_alert.hide()
        layout.addWidget(self.crash_alert)
        main = QSplitter(Qt.Orientation.Vertical)
        self.main_splitter = main
        body = QSplitter()
        self.navigation = QListWidget()
        self.navigation.addItems(['Áttekintés', 'Beállítások', 'Játékadatok', 'Questek és Lua', 'Naplók', 'Kapcsolatok és eszközök', 'Összeomlások'])
        self.navigation.setFixedWidth(176)
        self.pages = QStackedWidget()
        self.overview = self.dashboard()
        self.config_editor = Editor(self)
        self.data_page, self.quest_page = FilePage(self), FilePage(self, True)
        self.log_page = self.logs_page()
        self.remote_page = RemotePage(self)
        self.crash_page = CrashPage(self)
        self.profiles_changed()
        self.target_choice.currentIndexChanged.connect(self.target_selected)
        for page in (self.overview, self.config_editor, self.data_page, self.quest_page, self.log_page, self.remote_page, self.crash_page):
            self.pages.addWidget(page)
        body.addWidget(self.navigation)
        body.addWidget(self.pages)
        body.setSizes([190, 1100])
        body.setStretchFactor(0, 0)
        body.setStretchFactor(1, 1)
        self.navigation.currentRowChanged.connect(self.pages.setCurrentIndex)
        self.navigation.setCurrentRow(0)
        main.addWidget(body)
        console_panel = QWidget()
        self.console_panel = console_panel
        console_layout = QVBoxLayout(console_panel)
        console_layout.setContentsMargins(0, 5, 0, 0)
        console_layout.addWidget(note('Műveletek és ellenőrzési eredmények'))
        self.console = QPlainTextEdit()
        self.console.setReadOnly(True)
        self.console.setFont(QFont('Cascadia Mono', 10))
        self.console.document().setMaximumBlockCount(2500)
        console_layout.addWidget(self.console)
        main.addWidget(console_panel)
        main.setSizes([580, 170])
        console_panel.hide()
        layout.addWidget(main, 1)
        logbar = QHBoxLayout()
        self.console_toggle = QToolButton()
        self.console_toggle.setText('Műveleti napló')
        self.console_toggle.setCheckable(True)
        self.console_toggle.toggled.connect(self.toggle_console)
        logbar.addWidget(self.console_toggle)
        logbar.addWidget(note('Az ellenőrzések és mentések eredménye itt követhető.'), 1)
        layout.addLayout(logbar)
        self.statusBar().showMessage('A megnyitás nem indít szervert vagy adatbázist.')
        self.timer = QTimer(self)
        self.timer.timeout.connect(self.refresh)
        self.timer.start(1200)
        QShortcut(QKeySequence.StandardKey.Save, self, activated=self.save_current)
        geometry = self.settings.value('geometry')
        if geometry:
            self.restoreGeometry(geometry)
        if root:
            self.set_root(root)
        else:
            self.refresh()

    def toggle_console(self, shown):
        self.console_panel.setVisible(shown)
        if shown:
            self.main_splitter.setSizes([max(350, self.height() - 320), 165])

    @property
    def editors(self):
        return [self.config_editor, self.data_page.editor, self.quest_page.editor]

    def dashboard(self):
        page = QWidget()
        layout = QVBoxLayout(page)
        layout.setContentsMargins(0, 0, 0, 0)
        metrics = QHBoxLayout()
        self.stack_metric, self.core_metric, self.db_metric = QLabel('Leállítva'), QLabel('0 core'), QLabel('SQL: később')
        for metric in (self.stack_metric, self.core_metric, self.db_metric):
            metric.setObjectName('Metric')
            metrics.addWidget(metric)
        layout.addLayout(metrics)
        self.database_tools_panel = QWidget()
        database_tools = QHBoxLayout(self.database_tools_panel)
        database_tools.setContentsMargins(0, 0, 0, 0)
        self.setup_db_button = button('Adatbázis előkészítése', lambda: self.job(['setup-db']))
        database_tools.addWidget(self.setup_db_button)
        database_tools.addWidget(note('Az első betöltés létrehozza a saját helyi SQL-példányt és betölti az ellenőrzött mentést. Meglévő adatokat megőriz.'), 1)
        layout.addWidget(self.database_tools_panel)
        tools = QHBoxLayout()
        self.prepare_button = button('Előkészítés', lambda: self.job(['prepare']))
        self.check_button = button('Fájlok ellenőrzése', lambda: self.job(['check']))
        self.start_button = button('Kijelölt csatornák indítása', self.start_stack, 'Primary')
        self.stop_button = button('Szerver leállítása', lambda: self.job(['stop']), 'Stop')
        self.stop_db_button = button('MariaDB leállítása', lambda: self.job(['stop-db']))
        for item in (self.prepare_button, self.check_button, self.start_button, self.stop_button, self.stop_db_button):
            tools.addWidget(item)
        layout.addLayout(tools)
        quest_tools = QHBoxLayout()
        self.quest_reload_button = button('Questek frissítése', self.reload_quests)
        quest_tools.addWidget(self.quest_reload_button)
        self.db_check_button = button('SQL-kapcsolatok ellenőrzése', lambda: self.job(['database-check']))
        quest_tools.addWidget(self.db_check_button)
        quest_tools.addWidget(note('A mentett questek és Lua-könyvtárak frissítése minden futó core-on, a játékosok kiléptetése nélkül.'), 1)
        layout.addLayout(quest_tools)
        layout.addWidget(note('A kijelölt csatornák mellett a közös CH99 is elindul. Az indítás előbb ellenőrzi a fájlokat és az adatbázist.'))
        channels = QWidget()
        self.channel_layout = QHBoxLayout(channels)
        self.channel_layout.setContentsMargins(0, 0, 0, 0)
        self.channel_boxes = []
        layout.addWidget(channels)
        self.core_table = QTableWidget(0, 4)
        self.core_table.setHorizontalHeaderLabels(['Példány', 'Csatorna', 'Port', 'Állapot'])
        self.core_table.horizontalHeader().setSectionResizeMode(QHeaderView.ResizeMode.Stretch)
        self.core_table.setEditTriggers(QAbstractItemView.EditTrigger.NoEditTriggers)
        self.core_table.setSelectionBehavior(QAbstractItemView.SelectionBehavior.SelectRows)
        layout.addWidget(self.core_table, 1)
        self.target_hint = note('A beállítások és játékadatok módosításához leállítás szükséges. A questek külön, futás közben is frissíthetők.')
        layout.addWidget(self.target_hint)
        return page

    def logs_page(self):
        page = QWidget()
        layout = QVBoxLayout(page)
        layout.setContentsMargins(0, 0, 0, 0)
        bar = QHBoxLayout()
        self.log_choice = QComboBox()
        self.log_choice.currentIndexChanged.connect(self.refresh_log)
        bar.addWidget(self.log_choice, 1)
        bar.addWidget(button('Lista frissítése', self.update_logs))
        layout.addLayout(bar)
        self.log_view = QPlainTextEdit()
        self.log_view.setReadOnly(True)
        self.log_view.setFont(QFont('Cascadia Mono', 10))
        layout.addWidget(self.log_view)
        layout.addWidget(note('Az utolsó 160 kB látható. A szerverkonfiguráció ismert jelszavait a nézet kitakarja.'))
        return page

    def error(self, text):
        QMessageBox.warning(self, 'Auriga szerverkezelő', text)

    def busy(self):
        return (self.process is not None and self.process.state() != QProcess.ProcessState.NotRunning) or \
            bool(hasattr(self, 'crash_page') and self.crash_page.active()) or \
            bool(self.remote_client and self.remote_client.busy()) or \
            bool(hasattr(self, 'remote_page') and (self.remote_page.work.active() or
                 (self.remote_page.probe_process and getattr(self.remote_page, 'probe_action', '') in ('reload-quests', 'apply-quests'))))

    def is_remote(self):
        return self.remote_profile is not None

    def target_id(self):
        return (self.target_epoch, 'remote', self.remote_profile['id']) if self.is_remote() else (self.target_epoch, 'local', str(self.root))

    def state(self):
        if self.is_remote():
            return self.remote_state if self.remote_online else None
        return api.describe(self.root) if self.root else None

    def profiles_changed(self):
        if not hasattr(self, 'remote_page'):
            return
        selected = 'remote:' + self.remote_profile['id'] if self.is_remote() else 'local'
        self.target_choice.blockSignals(True)
        self.target_choice.clear()
        self.target_choice.addItem('Helyi szerver', 'local')
        for profile in self.remote_page.profiles:
            self.target_choice.addItem('Távoli • ' + profile['name'], 'remote:' + profile['id'])
        self.target_choice.addItem('SSH-profil hozzáadása / szerkesztése…', 'profiles')
        index = self.target_choice.findData(selected)
        self.target_choice.setCurrentIndex(max(0, index))
        self.target_choice.blockSignals(False)
        if self.is_remote():
            profile = next((p for p in self.remote_page.profiles if p['id'] == self.remote_profile['id']), None)
            if profile is not None and profile != self.remote_profile:
                if ssh.connection_route(profile) != ssh.connection_route(self.remote_profile):
                    if not self.set_remote(profile, connect=False):
                        self.remote_online = False
                        if self.remote_client and not self.remote_client.mutating: self.remote_client.cancel()
                else:
                    self.remote_profile = copy.deepcopy(profile)
                    if self.remote_client: self.remote_client.profile = copy.deepcopy(profile)
                    self.target_badge.setToolTip(profile['name'])
                    self.setWindowTitle('Auriga • Távoli szerver: ' + profile['name'])
            elif profile is None:
                if not self.set_local():
                    # A profile file can also change outside this window. Keep
                    # the actual target visible when unsaved work blocks exit.
                    self.remote_online = False
                    if self.remote_client and not self.remote_client.mutating: self.remote_client.cancel()
                    self.target_choice.blockSignals(True)
                    self.target_choice.insertItem(1, 'Távoli • ' + self.remote_profile['name'] + ' (törölt profil)', selected)
                    self.target_choice.setCurrentIndex(1)
                    self.target_choice.blockSignals(False)
                    self.refresh()

    def restore_target_choice(self):
        value = 'remote:' + self.remote_profile['id'] if self.is_remote() else 'local'
        self.target_choice.blockSignals(True)
        self.target_choice.setCurrentIndex(self.target_choice.findData(value))
        self.target_choice.blockSignals(False)

    def target_selected(self, index):
        value = self.target_choice.itemData(index)
        if value == 'profiles':
            self.navigation.setCurrentRow(5)
        elif value == 'local':
            if self.is_remote(): self.set_local()
        elif value:
            profile = next((p for p in self.remote_page.profiles if 'remote:' + p['id'] == value), None)
            if profile and (not self.is_remote() or profile != self.remote_profile): self.set_remote(profile)
        self.restore_target_choice()

    def may_switch_target(self):
        local_busy = self.process is not None and self.process.state() != QProcess.ProcessState.NotRunning
        if local_busy or (self.remote_client and self.remote_client.mutating) or self.remote_page.work.active() or self.remote_page.probe_process:
            self.error('Célgépváltás előtt várd meg a folyamatban lévő művelet végét.')
            return False
        return all(editor.may_discard() for editor in self.editors) and self.remote_page.may_discard()

    def release_remote(self):
        if self.remote_client:
            self.remote_client.changed.disconnect(self.refresh)
            self.remote_client.cancel()
            self.remote_client.deleteLater()
        self.remote_client = self.remote_profile = self.remote_state = None
        self.remote_online = False
        self.remote_inventory = dict(data=[], quests=[], logs=[])

    def clear_target_pages(self):
        for editor in self.editors: editor.clear()
        for page in (self.data_page, self.quest_page): page.files.clear()
        self.core_table.setRowCount(0)
        while self.channel_layout.count():
            item = self.channel_layout.takeAt(0)
            if item.widget(): item.widget().deleteLater()
        self.channel_boxes = []
        self.log_choice.blockSignals(True); self.log_choice.clear(); self.log_choice.blockSignals(False)
        self.log_view.clear(); self.console.clear()
        self.action_log, self.detached = None, None

    def choose_root(self):
        directory = QFileDialog.getExistingDirectory(self, 'Helyi Auriga szervercsomag mappája', str(self.local_root or Path.cwd()))
        if directory: self.set_root(directory)

    def set_local(self):
        if self.local_root:
            return self.set_root(self.local_root)
        if not self.may_switch_target(): return False
        self.target_epoch += 1
        self.release_remote(); self.root = None
        self.clear_target_pages()
        self.target_badge.setText('HELYI')
        self.target_badge.setToolTip('A saját gépen található szervercsomag.')
        self.setWindowTitle('Auriga • Helyi szerver')
        self.location.setText('Helyi szerver • válassz szervercsomagot.')
        self.restore_target_choice(); self.refresh()
        return True

    def set_root(self, path):
        if not self.may_switch_target(): return False
        try:
            root = api.package_root(path)
            self.target_epoch += 1
            self.release_remote()
            self.root = self.local_root = root
            self.clear_target_pages()
            self.location.setText('Helyi szerver • ' + str(root))
            self.target_badge.setText('HELYI')
            self.target_badge.setToolTip('A saját gépen található szervercsomag.')
            self.setWindowTitle('Auriga • Helyi szerver')
            self.settings.setValue('package', str(root))
            self.config_editor.load('server.json', force=True)
            for page in (self.data_page, self.quest_page): page.refresh()
            self.update_channels(); self.update_logs()
            self.restore_target_choice(); self.refresh()
            return True
        except (api.EditorError, OSError, ValueError) as error:
            self.error(str(error)); return False

    def set_remote(self, profile, *, connect=True):
        if not self.may_switch_target(): return False
        if self.remote_page.tunnel_session and self.remote_page.tunnel_session.profile['id'] != profile['id']:
            self.error('Másik SSH-profil kiválasztása előtt bontsd a jelenlegi portalagutat.')
            return False
        self.target_epoch += 1
        self.release_remote()
        self.root = None
        self.remote_profile = copy.deepcopy(profile)
        self.remote_client = targets.RemoteClient(profile, self)
        self.remote_client.changed.connect(self.refresh)
        self.clear_target_pages()
        self.remote_page.load(profile)
        endpoint = profile['target']
        self.target_badge.setText('TÁVOLI')
        self.setWindowTitle('Auriga • Távoli szerver: ' + profile['name'])
        self.target_badge.setToolTip(profile['name'])
        self.location.setText('Távoli szerver • %s@%s:%s • %s' % (endpoint['user'], endpoint['host'], endpoint['port'], profile['remote_root']))
        self.restore_target_choice()
        if connect: self.reconnect_target()
        else: self.refresh()
        return True

    def reconnect_target(self):
        if not self.is_remote():
            if self.root: self.set_root(self.root)
            else: self.choose_root()
            return
        if self.remote_client and self.remote_client.busy(): return
        saved = next((p for p in self.remote_page.profiles if p['id'] == self.remote_profile['id']), None)
        if saved is None:
            self.remote_online = False
            self.error('A kiválasztott SSH-profil már nincs a mentett profilok között. Válassz másik célgépet, vagy mentsd újra a profilt.')
            self.refresh()
            return
        if saved and saved != self.remote_profile:
            self.set_remote(saved)
            return
        def connected(value):
            self.remote_state, self.remote_inventory = value['state'], value['inventory']
            self.remote_online = True
            self.remote_next_poll = time.monotonic() + 5
            if not self.config_editor.document:
                self.config_editor.display(targets.document(value['document']))
            for page in (self.data_page, self.quest_page): page.refresh()
            self.update_channels(); self.update_logs(); self.refresh()
            self.statusBar().showMessage('Csatlakozva: ' + self.remote_profile['name'])
        self.remote_request('connect', {}, connected)

    def remote_request(self, operation, arguments, callback, *, poll=False, error_callback=None):
        client, epoch = self.remote_client, self.target_epoch
        if not client or not self.is_remote():
            return False
        if not self.remote_online and operation != 'connect':
            self.error('Nincs ellenőrzött távoli kapcsolat. Használd az Újracsatlakozás gombot.')
            return False
        def success(value):
            if epoch != self.target_epoch or client is not self.remote_client: return
            try:
                packet = value.get('document') if operation in ('connect', 'save') else value if operation == 'read' else None
                expected = 'server.json' if operation == 'connect' else arguments.get('relative')
                if packet is not None and packet.get('relative') != expected:
                    raise api.EditorError('A válasz nem a kért távoli fájlhoz tartozik.')
                callback(value)
            except (api.EditorError, OSError, ValueError, KeyError, TypeError, UnicodeError) as error:
                self.error(str(error))
            self.refresh()
        def failed(message):
            if epoch != self.target_epoch or client is not self.remote_client: return
            if operation in ('connect', 'status') or client.last_transport_error:
                self.remote_online = False
            self.statusBar().showMessage('Távoli szerver: ' + message[:180])
            if error_callback:error_callback(message)
            if not poll:
                self.show_remote_output(message)
                self.error(message)
            self.refresh()
        return client.request(operation, arguments, success, failed, poll=poll)

    def poll_remote(self, force=False):
        if not self.is_remote() or not self.remote_online or not self.remote_client or self.remote_client.process:
            return
        if not force and time.monotonic() < self.remote_next_poll: return
        self.remote_next_poll = time.monotonic() + 5
        def received(value):
            self.remote_state = value['state']
            self.remote_online = True
            self.update_channels()
        self.remote_request('status', dict(force_sql=force), received, poll=True)

    def show_remote_output(self, text):
        self.console_toggle.setChecked(True)
        self.console.appendPlainText(text)

    def file_paths(self, quests=False):
        if self.is_remote(): return self.remote_inventory['quests' if quests else 'data']
        return api.file_list(self.root, quests) if self.root else []

    def refresh_inventory(self):
        if self.is_remote():
            def received(value):
                self.remote_inventory = value
                for page in (self.data_page, self.quest_page): page.refresh()
                self.update_logs()
            self.remote_request('inventory', {}, received)
        else:
            for page in (self.data_page, self.quest_page): page.refresh()
            self.update_logs()

    def open_root(self):
        if self.root:
            QDesktopServices.openUrl(QUrl.fromLocalFile(str(self.root)))

    def update_channels(self):
        state = self.state()
        if not state: return
        config = state['config']
        channels = sorted({api.merge(config.get('game_defaults', {}), core).get('server', {}).get('channel', 0)
                           for core in config['game_servers'].values()} - {0, 99})
        selected = {box.property('channel'): box.isChecked() for box in self.channel_boxes}
        if list(selected) == channels: return
        while self.channel_layout.count():
            item = self.channel_layout.takeAt(0)
            if item.widget(): item.widget().deleteLater()
        self.channel_boxes = []
        self.channel_layout.addWidget(QLabel('Indítandó csatornák:'))
        for channel in channels:
            box = QCheckBox('CH%d' % channel)
            box.setProperty('channel', channel)
            box.setChecked(selected.get(channel, channel == channels[0]))
            self.channel_layout.addWidget(box)
            self.channel_boxes.append(box)
        self.channel_layout.addStretch()

    def command(self, arguments, logfile):
        prefix = [] if getattr(sys, 'frozen', False) else ['-B', str(Path(__file__).with_name('app.py'))]
        return sys.executable, [*prefix, '--worker', str(self.root), str(logfile), *arguments]

    def new_log(self):
        folder = self.root / 'run/manager-jobs'
        folder.mkdir(parents=True, exist_ok=True)
        return folder / ('job-' + secrets.token_hex(8) + '.log')

    def job(self, arguments, callback=None):
        if self.is_remote():
            if self.busy(): return
            self.console_toggle.setChecked(True)
            self.console.clear()
            self.console.appendPlainText('Távoli szerver: %s\nMűvelet: %s' % (self.remote_profile['name'], arguments[0]))
            def finished(value):
                self.show_remote_output(value.get('output', 'Sikeres.'))
                if value.get('detached'):
                    self.remote_state['starting'] = True
                    self.remote_state['phase'] = 'Indítás…'
                self.remote_next_poll = 0
                self.refresh_inventory()
                if callback: callback(True)
            self.remote_request('action', dict(argv=list(arguments)), finished)
            return
        if not self.root or self.busy():
            return
        self.action_log, self.log_text = self.new_log(), ''
        self.console_toggle.setChecked(True)
        self.console.clear()
        self.console.appendPlainText('Művelet: ' + arguments[0])
        self.on_finished = callback
        process = QProcess(self)
        self.process = process
        program, argv = self.command(arguments, self.action_log)
        process.setWorkingDirectory(str(self.root))
        process.finished.connect(self.job_finished)
        process.errorOccurred.connect(self.job_error)
        process.start(program, argv)
        self.statusBar().showMessage('Folyamatban: ' + arguments[0])
        self.refresh()

    def job_error(self, error):
        if error == QProcess.ProcessError.FailedToStart:
            self.console.appendPlainText('A kezelő alfolyamatát nem sikerült elindítani.')
            self.job_finished(1, QProcess.ExitStatus.CrashExit)

    def job_finished(self, code, exit_status):
        self.read_action_log()
        ok = code == 0 and exit_status == QProcess.ExitStatus.NormalExit
        self.console.appendPlainText('Sikeres.' if ok else 'A művelet sikertelen. Az ok a fenti naplóban látható.')
        process, self.process = self.process, None
        if process:
            process.deleteLater()
        callback, self.on_finished = self.on_finished, None
        if callback:
            callback(ok)
        self.statusBar().showMessage('Kész.' if ok else 'A művelet nem fejeződött be sikeresen.')
        self.update_logs()
        self.refresh()

    def start_stack(self):
        if (not self.root and not self.is_remote()) or self.busy():
            return
        if any(editor.dirty for editor in self.editors):
            self.error('Indítás előtt mentsd vagy töltsd újra a módosított állományokat.')
            return
        selected = [box.property('channel') for box in self.channel_boxes if box.isChecked()]
        if not selected:
            self.error('Jelölj ki legalább egy normál csatornát.')
            return
        arguments = ['start']
        for channel in selected:
            arguments += ['--channel', str(channel)]
        if self.is_remote():
            self.job(arguments)
            return
        self.action_log, self.log_text = self.new_log(), ''
        self.console_toggle.setChecked(True)
        self.console.clear()
        self.console.appendPlainText('A szerver felügyelt indítása…')
        program, argv = self.command(arguments, self.action_log)
        process = QProcess(self)
        process.setProgram(program)
        process.setArguments(argv)
        process.setWorkingDirectory(str(self.root))
        started, pid = process.startDetached()
        process.deleteLater()
        if not started:
            self.error('A szerver felügyelője nem indult el.')
            return
        self.detached = api.load_manager(self.root).identity(pid)
        self.statusBar().showMessage('A felügyelő az alkalmazás bezárása után is fut; leállítás a Leállítás gombbal.')
        self.refresh()

    def reload_quests(self):
        if any(editor.dirty for editor in self.editors):
            self.error('Questfrissítés előtt mentsd vagy töltsd újra a módosított fájlokat.')
            return
        state = self.state()
        if state: self.job(['reload-quests' if state['running'] else 'apply-quests'])

    def read_action_log(self):
        if self.is_remote() or not self.root or not self.action_log or not self.action_log.is_file():
            return
        text = api.redact(api.tail(self.action_log), self.root)
        if text != self.log_text:
            self.log_text = text
            self.console.setPlainText(text)
            bar = self.console.verticalScrollBar()
            bar.setValue(bar.maximum())

    def refresh(self):
        if not hasattr(self, 'console'): return
        try:
            busy = self.busy()
            switching_blocked = bool(self.process or (self.remote_client and self.remote_client.mutating) or self.remote_page.work.active() or self.remote_page.probe_process or self.crash_page.active())
            self.target_choice.setEnabled(not switching_blocked)
            self.choose_root_button.setEnabled(not switching_blocked)
            self.open_root_button.setEnabled(not self.is_remote() and bool(self.root))
            self.connect_button.setEnabled(not busy)
            self.connect_button.setText('Állapot frissítése' if self.is_remote() and self.remote_online else 'Újracsatlakozás' if self.is_remote() else 'Frissítés')
            self.target_badge.setStyleSheet('background: #594124; color: #ffe0a5;' if self.is_remote() else 'background: #203f38; color: #a4e1ce;')
            self.remote_page.controls()
            state = self.state()
            self.crash_page.tick(state)
            actions = (self.prepare_button, self.check_button, self.start_button, self.stop_button, self.stop_db_button, self.setup_db_button, self.quest_reload_button, self.db_check_button)
            if not state:
                for item in actions: item.setEnabled(False)
                self.core_table.setRowCount(0)
                self.stack_metric.setText('Kapcsolódás…' if self.is_remote() and self.remote_client and self.remote_client.operation == 'connect' else 'Nincs kapcsolat' if self.is_remote() else 'Nincs szervercsomag')
                self.core_metric.setText('Core-állapot: —'); self.db_metric.setText('SQL: —')
                for editor in self.editors: editor.save_button.setEnabled(False)
                return
            if not self.is_remote(): self.read_action_log()
            starting = bool(state.get('starting')) if self.is_remote() else bool(self.detached and api.load_manager(self.root).owned(self.detached))
            running = state['running'] or starting
            external = state.get('database_kind') == 'external'
            self.database_tools_panel.setVisible(not external)
            self.stop_db_button.setVisible(not external)
            self.stack_metric.setText('Befejezetlen buildtelepítés' if state.get('deployment_pending') else
                                     'Indítás…' if starting else state['phase'])
            self.core_metric.setText('%d / %d core fut' % (sum(row['running'] for row in state['rows'] if row['name'] not in ('database', 'login')), len(state['config']['game_servers'])))
            sql_status = state.get('database_status', {})
            if external:
                self.db_metric.setText({'available':'SQL: elérhető', 'partial':'SQL: részben elérhető',
                    'unavailable':'SQL: nem elérhető', 'error':'SQL: kapcsolati hiba',
                    'unknown':'SQL: nem ellenőrizhető'}.get(sql_status.get('status'), 'SQL: ellenőrzés…'))
                self.db_metric.setStyleSheet('color: ' + ('#83d6c0' if sql_status.get('status') == 'available' else
                    '#ffb4a7' if sql_status.get('status') in ('unavailable','error') else '#edcf8e') + ';')
                details = [sql_status['message']] if sql_status.get('message') else []
                details += ['%s:%s / %s — %s' % (row['host'],row['port'],row['database'],row['message'])
                            for row in sql_status.get('connections', [])]
                self.db_metric.setToolTip('\n'.join(details))
            else:
                self.db_metric.setStyleSheet('')
                self.db_metric.setText('MariaDB: fut' if state['database_running'] else 'MariaDB: leállítva' if state['database_initialized'] else 'SQL: még nincs betöltve')
                self.db_metric.setToolTip('A helyi csomag saját MariaDB-folyamatának állapota.')
            self.target_hint.setText(('A célgép külső SQL-szolgáltatását használjuk. A kapcsolatok a Beállítások oldalon szerkeszthetők; az indítás ellenőrzi őket. ' if external else '') +
                'A beállítások és játékadatok mentéséhez leállítás szükséges. A questek külön, futás közben is frissíthetők.')
            if external and sql_status.get('status') not in (None, 'available'):
                failed = next((row for row in sql_status.get('connections', []) if row['status'] != 'available'), None)
                reason = '%s:%s / %s — %s' % (failed['host'],failed['port'],failed['database'],failed['message']) if failed else sql_status.get('message','')
                self.target_hint.setText('SQL: ' + reason + ' A kapcsolatot a Beállítások oldalon javíthatod; az Állapot frissítése újra ellenőrzi.')
            if state.get('deployment_pending'):
                self.target_hint.setText('Befejezetlen buildtelepítés miatt az indítás tiltott. Ellenőrizd a mentést és az eredményt: ' + state['deployment_pending'])
            self.core_table.setRowCount(len(state['rows']))
            for i, row in enumerate(state['rows']):
                for j, value in enumerate((row['name'], row['channel'], row['port'], 'Fut' if row['running'] else 'Leállítva')):
                    item = QTableWidgetItem(str(value))
                    if j == 3: item.setForeground(QColor('#83d6c0' if row['running'] else '#96a6bd'))
                    self.core_table.setItem(i, j, item)
            self.prepare_button.setEnabled(not busy and not running)
            self.setup_db_button.setEnabled(not busy and not running and not state['database_initialized'] and bool(state.get('can_setup')))
            self.check_button.setEnabled(not busy)
            self.db_check_button.setEnabled(not busy and (external or bool(state.get('can_start', state['database_initialized']))))
            self.start_button.setEnabled(not busy and not running and bool(state.get('can_start', state['database_initialized'])))
            self.start_button.setToolTip('Kijelölt csatornák és közös CH99 indítása a kiválasztott szerveren.')
            self.stop_button.setEnabled(not busy and state['running'])
            self.stop_db_button.setEnabled(not busy and not running and state['database_running'])
            self.quest_reload_button.setEnabled(not busy and (bool(state.get('hotreload')) or (not running and bool(state.get('reload_supported')))))
            self.quest_reload_button.setText('Questek frissítése' if running else 'Questek előkészítése induláshoz')
            for page in (self.data_page, self.quest_page): page.files.setEnabled(not busy)
            for editor in self.editors:
                editor.tabs.setEnabled(not busy)
                can_save = not running or (state.get('hotreload') and editor.document and api.hotreload_file(editor.document.relative))
                editor.save_button.setEnabled(bool(editor.document and editor.dirty and not busy and can_save))
            if self.pages.currentWidget() == self.log_page and (not self.is_remote() or (self.remote_client and not self.remote_client.process and time.monotonic() >= self.remote_next_log)):
                self.refresh_log()
            self.poll_remote()
        except (api.EditorError, OSError, ValueError, KeyError, TypeError) as error:
            self.statusBar().showMessage('Állapot nem olvasható: ' + type(error).__name__)

    def update_logs(self):
        selected = self.log_choice.currentData()
        paths = self.remote_inventory['logs'] if self.is_remote() else api.log_files(self.root) if self.root else []
        self.log_choice.blockSignals(True)
        self.log_choice.clear()
        for relative in paths: self.log_choice.addItem(relative, relative)
        match = self.log_choice.findData(selected)
        if match >= 0: self.log_choice.setCurrentIndex(match)
        self.log_choice.blockSignals(False)
        self.refresh_log()

    def set_log_text(self, text):
        if self.log_view.toPlainText() != text:
            bottom = self.log_view.verticalScrollBar().value() == self.log_view.verticalScrollBar().maximum()
            self.log_view.setPlainText(text)
            if bottom: self.log_view.verticalScrollBar().setValue(self.log_view.verticalScrollBar().maximum())

    def refresh_log(self):
        relative = self.log_choice.currentData()
        if not relative:
            self.log_view.clear(); return
        if self.is_remote():
            if not self.remote_online or not self.remote_client or self.remote_client.process: return
            self.remote_next_log = time.monotonic() + 5
            self.remote_request('log', dict(relative=relative), lambda value: self.set_log_text(value['text']))
            return
        if not self.root: return
        try: self.set_log_text(api.redact(api.tail(self.root / relative), self.root))
        except OSError: self.log_view.setPlainText('A napló jelenleg nem olvasható.')

    def save_current(self):
        current = self.pages.currentWidget()
        if isinstance(current, Editor):
            current.save()
        elif isinstance(current, FilePage):
            current.editor.save()
        elif isinstance(current, RemotePage):
            current.save()

    def closeEvent(self, event):
        if self.remote_client and self.remote_client.mutating:
            self.error('Várd meg a távoli mentés vagy szerverművelet eredményét.')
            event.ignore(); return
        if self.remote_client and self.remote_client.process:
            self.remote_client.cancel()
        if self.remote_page.work.active():
            self.error('Várd meg a távoli művelet végét, vagy kérd a megszakítását a Kapcsolatok és eszközök oldalon. '
                       'Fájlcsere és csomagtelepítés közben a befejezést meg kell várni.')
            event.ignore()
            return
        if self.busy():
            self.error('Várd meg a folyamatban levő ellenőrzés vagy mentés befejezését.')
            event.ignore()
            return
        if not all(editor.may_discard() for editor in self.editors):
            event.ignore()
            return
        if not self.remote_page.may_discard():
            event.ignore()
            return
        self.release_remote()
        self.remote_page.shutdown()
        for editor in self.editors:
            editor.code.shutdown()
        self.settings.setValue('geometry', self.saveGeometry())
        event.accept()


def default_root():
    settings = QSettings('Auriga', 'ServerManager')
    candidates = [settings.value('package', '')]
    location = Path(sys.executable if getattr(sys, 'frozen', False) else __file__).resolve().parent
    candidates += [location, *list(location.parents)[:3], Path('D:/AurigaGlobal/Server2-Local')]
    for candidate in candidates:
        try:
            return api.package_root(candidate)
        except (api.EditorError, OSError, ValueError):
            pass
    return None


def run(arguments):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--package', type=Path)
    parser.add_argument('--screenshot', type=Path, help=argparse.SUPPRESS)
    parser.add_argument('--screenshot-page', choices=['overview', 'settings', 'data', 'quests', 'remote', 'crashes'], default='overview', help=argparse.SUPPRESS)
    parser.add_argument('--screenshot-file', help=argparse.SUPPRESS)
    args = parser.parse_args(arguments)
    application = QApplication.instance() or QApplication([sys.argv[0]])
    if os.name == 'nt' and 'Segoe UI' not in QFontDatabase.families():
        # The offscreen Windows plugin has no system-font database. Use local
        # installed fonts for deterministic screenshots and headless UI tests.
        for font in ('segoeui.ttf', 'consola.ttf'):
            QFontDatabase.addApplicationFont(str(Path(os.environ.get('WINDIR', 'C:/Windows')) / 'Fonts' / font))
    application.setStyle('Fusion')
    application.setStyleSheet(STYLE)
    window = MainWindow(args.package or default_root())
    if args.screenshot:
        window.resize(1320, 860)
        window.navigation.setCurrentRow({'overview': 0, 'settings': 1, 'data': 2, 'quests': 3, 'remote': 5, 'crashes': 6}[args.screenshot_page])
        if args.screenshot_file and args.screenshot_page in ('data', 'quests'):
            page = window.data_page if args.screenshot_page == 'data' else window.quest_page
            page.editor.load(args.screenshot_file)
    window.show()
    if args.screenshot:
        def capture():
            window.grab().save(str(args.screenshot))
            application.quit()
        QTimer.singleShot(900, capture)
    elif not window.root:
        QTimer.singleShot(0, window.choose_root)
    return application.exec()
