"""Item catalog editor page for the local package's protos database.

Reads, validates and saves through item_catalog (native CatalogEditorTool).
The page never builds SQL and never edits more than one item per save.
"""
import copy
import re

from PySide6.QtCore import QThread, Qt, Signal
from PySide6.QtGui import QStandardItem, QStandardItemModel
from PySide6.QtWidgets import (QAbstractItemView, QCheckBox, QComboBox, QDialog, QDialogButtonBox, QFormLayout,
    QGridLayout, QHBoxLayout, QHeaderView, QLabel, QLineEdit, QMenu, QMessageBox, QPushButton, QScrollArea,
    QSpinBox, QSplitter, QStackedWidget, QTabWidget, QTableWidget, QTableWidgetItem, QToolButton, QVBoxLayout, QWidget)

import backend as api
import item_catalog as catalog

KEY_ROLE = int(Qt.ItemDataRole.UserRole)
BONUS_LABELS = {
    'physical_attack.flat': 'Fizikai támadás', 'magical_attack.flat': 'Mágikus támadás',
    'physical_defense.flat': 'Fizikai védekezés', 'magical_defense.flat': 'Mágikus védekezés',
    'physical_attack.percent': 'Fizikai támadás %', 'magical_attack.percent': 'Mágikus támadás %',
    'normal.outgoing': 'Sebzés normál támadással', 'skill.outgoing': 'Képességsebzés',
    'pve.outgoing': 'Sebzés szörnyek ellen', 'pvp.outgoing': 'Sebzés játékosok ellen',
    'normal.incoming': 'Normál támadás elleni védelem', 'skill.incoming': 'Képesség elleni védelem',
    'pve.incoming': 'Védelem szörnyek ellen', 'pvp.incoming': 'Védelem játékosok ellen',
    'physical.penetration.flat': 'Fizikai átütés', 'magical.penetration.flat': 'Mágikus átütés',
    'physical.penetration.percent': 'Fizikai átütés %', 'magical.penetration.percent': 'Mágikus átütés %',
    'physical.resistance': 'Fizikai ellenállás', 'magical.resistance': 'Mágikus ellenállás',
    'critical.chance': 'Kritikus esély', 'critical.resistance': 'Kritikus elleni védelem',
    'critical.multiplier_extra': 'Kritikus szorzó', 'block.reduction': 'Blokkolás', 'accuracy.evasion': 'Kitérés',
}
# Display scale per bonus unit: raw values are 1000/point, bp is 1/100 percent.
UNITS = {1: (1000, 'pont'), 2: (100, '%'), 3: (1, 'db')}
RESISTANCE_LABELS = ['Fizikai', 'Mágikus', 'Kard', 'Tőr', 'Íj', 'Kétkezes', 'Csengő', 'Legyező', 'Lovas lándzsa',
                     'Tűz', 'Villám', 'Jég', 'Szél', 'Föld', 'Sötétség']
EFFECT_DEFAULTS = {
    1: dict(hp_raw='1000', sp_raw='0', max_hp_bp='0', max_sp_bp='0', flat_bonus_cap_bp='10000'),
    2: dict(hp_raw='1000', sp_raw='0', flat_bonus_cap_bp='10000', tick_interval_ms='1000', max_pool_per_tick_bp='1000'),
    3: dict(hp_per_tick_raw='100', sp_per_tick_raw='0', tick_interval_ms='1000', duration_ms='10000'),
}


class Task(QThread):
    ready = Signal(object); failed = Signal(str)

    def __init__(self, callback, parent):
        super().__init__(parent); self.callback = callback

    def run(self):
        try: self.ready.emit(self.callback())
        except Exception as error: self.failed.emit(str(error))


class Field:
    """One control bound to document[section][column]; values stay strings."""

    def __init__(self, section, column, label, kind='int', changed=None, **options):
        self.section, self.column, self.label, self.kind, self.options = section, column, label, kind, options
        if kind == 'bool':
            self.widget = QCheckBox(options.get('text', '')); self.widget.toggled.connect(lambda *_: changed and changed())
        elif kind == 'choice':
            self.widget = QComboBox()
            for value, text in options['choices']: self.widget.addItem(text, str(value))
            self.widget.currentIndexChanged.connect(lambda *_: changed and changed())
        elif kind == 'bits':
            self.widget = QWidget(); row = QHBoxLayout(self.widget); row.setContentsMargins(0, 0, 0, 0); self.boxes = []
            for text in options['names']:
                box = QCheckBox(text); box.toggled.connect(lambda *_: changed and changed()); row.addWidget(box); self.boxes.append(box)
            row.addStretch()
        else:
            self.edit = QLineEdit(); self.edit.textEdited.connect(lambda *_: changed and changed())
            if options.get('placeholder'): self.edit.setPlaceholderText(options['placeholder'])
            unit = options.get('unit')
            if unit:
                self.widget = QWidget(); row = QHBoxLayout(self.widget); row.setContentsMargins(0, 0, 0, 0)
                row.addWidget(self.edit, 1); suffix = QLabel(unit); suffix.setObjectName('Subtle'); row.addWidget(suffix)
            else:
                self.widget = self.edit
        if options.get('hint'): self.widget.setToolTip(options['hint'])

    def load(self, value):
        widgets = self.boxes if self.kind == 'bits' else [self.widget]
        for widget in widgets: widget.blockSignals(True)
        try:
            if self.kind == 'bool': self.widget.setChecked(str(value) == '1')
            elif self.kind == 'choice':
                index = self.widget.findData(str(value)); self.widget.setCurrentIndex(max(0, index))
            elif self.kind == 'bits':
                for bit, box in enumerate(self.boxes): box.setChecked(bool(int(value or 0) & (1 << bit)))
            else:
                self.edit.blockSignals(True)
                if value is None: self.edit.setText('')
                elif self.kind == 'scaled': self.edit.setText(catalog.display(value, self.options['scale']))
                else: self.edit.setText(str(value))
                self.edit.blockSignals(False)
        finally:
            for widget in widgets: widget.blockSignals(False)

    def value(self):
        if self.kind == 'bool': return '1' if self.widget.isChecked() else '0'
        if self.kind == 'choice': return self.widget.currentData()
        if self.kind == 'bits': return str(sum(1 << bit for bit, box in enumerate(self.boxes) if box.isChecked()))
        text = self.edit.text().strip()
        if self.kind == 'text':
            if not text and not self.options.get('optional'): raise api.EditorError(self.label + ': kötelező mező.')
            return text or None
        try:
            if self.kind != 'scaled' and not re.fullmatch(r'-?[0-9]+', text): raise ValueError()
            number = catalog.scaled(text, self.options['scale']) if self.kind == 'scaled' else int(text)
        except (ValueError, api.EditorError):
            raise api.EditorError(self.label + ': érvénytelen szám (' + (text or 'üres') + ').') from None
        low, high = self.options.get('range', (0, 2 ** 63 - 1))
        if not low <= number <= high:
            raise api.EditorError('%s: %s és %s között legyen.' % (self.label, self.show(low), self.show(high)))
        return str(number)

    def show(self, number):
        return catalog.display(number, self.options['scale']) if self.kind == 'scaled' else str(number)


class Form(QWidget):
    """A titled set of Fields writing into one or more document sections."""

    def __init__(self, changed, columns=1):
        super().__init__(); self.changed = changed; self.fields = []
        self.grid = QGridLayout(self); self.grid.setColumnStretch(1, 1); self.columns = columns; self.count = 0
        if columns > 1: self.grid.setColumnStretch(3, 1)

    def add(self, section, column, label, kind='int', **options):
        field = Field(section, column, label, kind, self.changed, **options); self.fields.append(field)
        row, column_index = divmod(self.count, self.columns); self.count += 1
        caption = QLabel(label); caption.setToolTip(options.get('hint', ''))
        self.grid.addWidget(caption, row, column_index * 2); self.grid.addWidget(field.widget, row, column_index * 2 + 1)
        return field

    def note(self, text):
        row = (self.count + self.columns - 1) // self.columns; self.count = (row + 1) * self.columns
        label = QLabel(text); label.setObjectName('FieldHint'); label.setWordWrap(True)
        self.grid.addWidget(label, row, 0, 1, self.columns * 2)

    def finish(self):
        self.grid.setRowStretch((self.count + self.columns - 1) // self.columns + 1, 1)

    def load(self, document):
        for field in self.fields:
            section = document.get(field.section) or {}
            field.load(section.get(field.column))

    def collect(self, document):
        for field in self.fields:
            if document.get(field.section) is None: document[field.section] = {}
            document[field.section][field.column] = field.value()


def scroll(widget):
    area = QScrollArea(); area.setWidgetResizable(True); area.setWidget(widget); area.setFrameShape(QScrollArea.Shape.NoFrame)
    return area


class BonusTable(QWidget):
    """Bonus entries of one profile. Values are shown in the definition's unit."""

    def __init__(self, changed, source):
        super().__init__(); self.changed = changed; self.source = source; self.definitions = []
        layout = QVBoxLayout(self); layout.setContentsMargins(0, 0, 0, 0)
        self.table = QTableWidget(0, 3); self.table.setHorizontalHeaderLabels(['Bónusz', 'Érték', 'Megengedett tartomány'])
        self.table.horizontalHeader().setSectionResizeMode(0, QHeaderView.ResizeMode.Stretch)
        self.table.setColumnWidth(1, 120); self.table.setColumnWidth(2, 210)
        self.table.verticalHeader().hide(); self.table.setSelectionBehavior(QAbstractItemView.SelectionBehavior.SelectRows)
        layout.addWidget(self.table, 1)
        row = QHBoxLayout()
        add, remove = QPushButton('Bónusz hozzáadása'), QPushButton('Kijelölt törlése')
        add.clicked.connect(lambda: (self.append(None, None), self.changed()))
        remove.clicked.connect(self.remove)
        row.addWidget(add); row.addWidget(remove); row.addStretch(); layout.addLayout(row)

    def set_definitions(self, definitions):
        bit = 1 if self.source == '1' else 2
        self.definitions = [d for d in definitions if int(d['sources']) & bit]

    def definition(self, bonus_id):
        return next((d for d in self.definitions if str(d['id']) == str(bonus_id)), None)

    def append(self, bonus_id, value):
        row = self.table.rowCount(); self.table.insertRow(row)
        choice = QComboBox()
        for d in self.definitions: choice.addItem(BONUS_LABELS.get(d['code'], d['code']) + '  (' + d['code'] + ')', str(d['id']))
        if bonus_id is not None:
            index = choice.findData(str(bonus_id))
            if index < 0: choice.addItem('Ismeretlen bónusz #' + str(bonus_id), str(bonus_id)); index = choice.count() - 1
            choice.setCurrentIndex(index)
        edit = QLineEdit()
        self.table.setCellWidget(row, 0, choice); self.table.setCellWidget(row, 1, edit)
        self.table.setItem(row, 2, QTableWidgetItem(''))
        def refresh(*_):
            d = self.definition(choice.currentData())
            scale, unit = UNITS.get(int(d['unit']) if d else 3, (1, ''))
            self.table.item(row_of(), 2).setText('%s … %s %s' % (catalog.display(d['minimum'], scale), catalog.display(d['maximum'], scale), unit) if d else '')
            edit.setPlaceholderText(unit)
        def row_of():
            for index in range(self.table.rowCount()):
                if self.table.cellWidget(index, 0) is choice: return index
            return row
        if value is not None:
            d = self.definition(bonus_id)
            edit.setText(catalog.display(value, UNITS.get(int(d['unit']), (1, ''))[0]) if d else str(value))
        refresh()
        choice.currentIndexChanged.connect(lambda *_: (refresh(), self.changed()))
        edit.textEdited.connect(lambda *_: self.changed())

    def remove(self):
        rows = sorted({index.row() for index in self.table.selectedIndexes()}, reverse=True)
        for row in rows: self.table.removeRow(row)
        if rows: self.changed()

    def load(self, entries):
        self.table.setRowCount(0)
        for entry in entries or []: self.append(entry['bonus_id'], entry['value_scaled'])

    def entries(self):
        result, seen = [], set()
        for row in range(self.table.rowCount()):
            bonus_id = self.table.cellWidget(row, 0).currentData()
            text = self.table.cellWidget(row, 1).text().strip()
            d = self.definition(bonus_id)
            if d is None: raise api.EditorError('Ismeretlen bónusz a listában.')
            label = BONUS_LABELS.get(d['code'], d['code'])
            if bonus_id in seen: raise api.EditorError(label + ': ez a bónusz kétszer szerepel.')
            seen.add(bonus_id)
            scale = UNITS.get(int(d['unit']), (1, ''))[0]
            try: value = catalog.scaled(text, scale)
            except api.EditorError: raise api.EditorError(label + ': érvénytelen érték (' + (text or 'üres') + ').') from None
            if not int(d['minimum']) <= value <= int(d['maximum']):
                raise api.EditorError(label + ': az érték kívül esik a megengedett tartományon.')
            result.append(dict(bonus_id=str(bonus_id), value_scaled=str(value)))
        return result


class BonusDialog(QDialog):
    def __init__(self, parent, title, definitions, entries):
        super().__init__(parent); self.setWindowTitle(title); self.resize(640, 360)
        layout = QVBoxLayout(self)
        hint = QLabel('Ezek a bónuszok csak ezen a fejlesztési szinten érvényesek (nem adódnak össze a korábbi szintekkel).')
        hint.setWordWrap(True); hint.setObjectName('Subtle'); layout.addWidget(hint)
        self.table = BonusTable(lambda: None, '2'); self.table.set_definitions(definitions); self.table.load(entries)
        layout.addWidget(self.table, 1)
        buttons = QDialogButtonBox(QDialogButtonBox.StandardButton.Ok | QDialogButtonBox.StandardButton.Cancel)
        buttons.button(QDialogButtonBox.StandardButton.Ok).setText('Alkalmaz')
        buttons.button(QDialogButtonBox.StandardButton.Cancel).setText('Mégse')
        buttons.accepted.connect(self.accept_entries); buttons.rejected.connect(self.reject); layout.addWidget(buttons)
        self.result_entries = None

    def accept_entries(self):
        try: self.result_entries = self.table.entries()
        except api.EditorError as error:
            QMessageBox.warning(self, 'Szintbónuszok', str(error)); return
        self.accept()


class FillDialog(QDialog):
    """Linear fill: value(L) = start value + step * (L - start level), up to the maximum.

    `preview` receives (target, values), or None for invalid input, on every
    change, so the table behind the dialog shows the result while it is open.
    The table is the edit: closing (Kész, X, Esc) keeps the last valid result;
    only Visszavonás restores the original values.
    """

    def __init__(self, parent, targets, maximum, current, preview=None, steps=None):
        super().__init__(parent); self.setWindowTitle('Egyenletes kitöltés'); self.resize(520, 300)
        self.targets, self.maximum, self.current, self.on_preview = targets, maximum, current, preview
        self.steps = steps or (lambda target: None)
        layout = QVBoxLayout(self); form = QFormLayout()
        self.target = QComboBox()
        for target in targets: self.target.addItem(target['label'])
        self.start = QSpinBox(); self.start.setRange(1, maximum); self.start.setPrefix('+')
        self.value, self.step = QLineEdit(), QLineEdit('1')
        self.value_unit, self.step_unit = QLabel(), QLabel()
        def with_unit(edit, unit):
            box = QWidget(); row = QHBoxLayout(box); row.setContentsMargins(0, 0, 0, 0); row.addWidget(edit, 1); row.addWidget(unit); return box
        form.addRow('Mit töltsön ki', self.target)
        form.addRow('Kezdőszint', self.start)
        form.addRow('Érték a kezdőszinten', with_unit(self.value, self.value_unit))
        form.addRow('Növekmény szintenként', with_unit(self.step, self.step_unit))
        layout.addLayout(form)
        self.preview = QLabel(); self.preview.setWordWrap(True); self.preview.setObjectName('Subtle'); layout.addWidget(self.preview)
        hint = QLabel('A változás azonnal a táblázatba kerül, és bezáráskor is megmarad; a „Visszavonás” visszaállítja az '
                      'eredeti értékeket. A növekményt a szerkesztő megjegyzi: a „+” gombbal hozzáadott új szintek ezzel '
                      'folytatják a sort. A kezdőszint alatti szintek nem változnak.')
        hint.setWordWrap(True); hint.setObjectName('FieldHint'); layout.addWidget(hint); layout.addStretch()
        buttons = QDialogButtonBox(QDialogButtonBox.StandardButton.Ok)
        buttons.button(QDialogButtonBox.StandardButton.Ok).setText('Kész')
        self.revert_button = buttons.addButton('Visszavonás', QDialogButtonBox.ButtonRole.ResetRole)
        self.revert_button.clicked.connect(self.revert)
        buttons.accepted.connect(self.accept_values); layout.addWidget(buttons)
        self.target.currentIndexChanged.connect(self.reset_target); self.start.valueChanged.connect(self.reset_value)
        self.value.textEdited.connect(self.show_preview); self.step.textEdited.connect(self.show_preview)
        self.last = None; self.last_step = None; self.reverted = False
        # Start at the first level that already carries the chosen bonus.
        first = next((level for level in range(1, maximum + 1) if current(targets[0], level) is not None), 1)
        self.start.blockSignals(True); self.start.setValue(first); self.start.blockSignals(False)
        self.reset_target()

    def chosen(self):
        return self.targets[self.target.currentIndex()]

    def reset_target(self, *_):
        target = self.chosen()
        self.value_unit.setText(target['unit']); self.step_unit.setText(target['unit'])
        step = self.steps(target)
        self.step.setText(catalog.display(step, target['scale']) if step is not None else '1')
        self.reset_value()

    def reset_value(self, *_):
        target = self.chosen()
        value = self.current(target, self.start.value())
        self.value.setText(catalog.display(value, target['scale']) if value is not None else '0')
        self.show_preview()

    def values(self):
        target = self.chosen()
        try: start, step = catalog.scaled(self.value.text(), target['scale']), catalog.scaled(self.step.text(), target['scale'])
        except api.EditorError: raise api.EditorError('Érvénytelen kezdőérték vagy növekmény.') from None
        result = {level: start + step * (level - self.start.value()) for level in range(self.start.value(), self.maximum + 1)}
        low, high = target['range']
        bad = next((level for level, value in result.items() if not low <= value <= high), None)
        if bad is not None:
            raise api.EditorError('+%d szinten az érték (%s %s) kívül esik a megengedett tartományon.' % (
                bad, catalog.display(result[bad], target['scale']), target['unit']))
        self.computed_step = step
        return target, result

    def show_preview(self, *_):
        try:
            target, result = self.values()
            shown = list(result.items())
            text = ', '.join('+%d: %s' % (level, catalog.display(value, target['scale'])) for level, value in shown[:8])
            if len(shown) > 8: text += ' … +%d: %s' % (shown[-1][0], catalog.display(shown[-1][1], target['scale']))
            self.preview.setText('Eredmény: ' + text + ' ' + target['unit'])
            self.last, self.last_step = (target, result), self.computed_step
        except api.EditorError as error:
            self.preview.setText(str(error))
            self.last = self.last_step = None
        if self.on_preview: self.on_preview(self.last)

    def accept_values(self):
        try: self.values()
        except api.EditorError as error:
            QMessageBox.warning(self, 'Egyenletes kitöltés', str(error)); return
        self.accept()

    def revert(self):
        self.reverted = True; self.reject()


REWARD_KINDS = [(1, 'Tárgy'), (2, 'Yang'), (3, 'Tapasztalat'), (4, 'Szörny idézése'), (5, 'Szörnycsoport idézése'),
                (6, 'Lassítás'), (7, 'HP-szívás'), (8, 'Mérgezés')]
# Display scale and unit of the amount per reward kind; drain HP is bp of max HP.
AMOUNT_UNITS = {1: (1, 'db'), 2: (1, 'Yang'), 3: (1, 'TP'), 4: (1, 'szörny vnum'), 5: (1, 'csoport-ID'), 6: (1, 'pont'),
                7: (100, '% max HP'), 8: (1, '')}


class LootTable(QWidget):
    """Rewards of one container. Weighted mode shows relative weights and the
    resulting chance; independent mode shows each row's own chance in %."""
    COLUMNS = ['Jutalom', 'Tárgy', 'Mennyiség', 'Súly', 'Ritka bónusz %', 'Esély']

    def __init__(self, changed):
        super().__init__(); self.changed = changed; self.mode = '1'; self.items = QStandardItemModel(self)
        layout = QVBoxLayout(self); layout.setContentsMargins(0, 0, 0, 0)
        self.table = QTableWidget(0, len(self.COLUMNS)); self.table.setHorizontalHeaderLabels(self.COLUMNS)
        header = self.table.horizontalHeader()
        header.setSectionResizeMode(1, QHeaderView.ResizeMode.Stretch)
        for column, width in ((0, 170), (2, 110), (3, 90), (4, 110), (5, 80)): self.table.setColumnWidth(column, width)
        self.table.verticalHeader().hide(); self.table.setSelectionBehavior(QAbstractItemView.SelectionBehavior.SelectRows)
        layout.addWidget(self.table, 1)
        row = QHBoxLayout()
        add, remove = QPushButton('Jutalom hozzáadása'), QPushButton('Kijelölt törlése')
        add.clicked.connect(lambda: (self.append(dict(reward_kind='1', amount='1', weight='1' if self.mode == '1' else '10000')), self.changed()))
        remove.clicked.connect(self.remove)
        self.total = QLabel(); self.total.setObjectName('Subtle')
        row.addWidget(add); row.addWidget(remove); row.addWidget(self.total, 1); layout.addLayout(row)

    def set_items(self, rows):
        self.items.clear()
        for row in rows:
            key = '%s:%s' % (row['category_id'], row['local_id'])
            entry = QStandardItem('%s  %s%s' % (key, row['content_code'], '  (kivezetve)' if row['retired'] == '1' else ''))
            entry.setData(key, Qt.ItemDataRole.UserRole); self.items.appendRow(entry)

    def row_of(self, widget, column):
        for index in range(self.table.rowCount()):
            if self.table.cellWidget(index, column) is widget: return index
        return -1

    def append(self, entry):
        row = self.table.rowCount(); self.table.insertRow(row)
        kind = QComboBox()
        for value, text in REWARD_KINDS: kind.addItem(text, str(value))
        kind.setCurrentIndex(max(0, kind.findData(str(entry.get('reward_kind') or '1'))))
        item = QComboBox(); item.setModel(self.items); item.setEditable(True); item.setInsertPolicy(QComboBox.InsertPolicy.NoInsert)
        item.completer().setFilterMode(Qt.MatchFlag.MatchContains); item.completer().setCaseSensitivity(Qt.CaseSensitivity.CaseInsensitive)
        if entry.get('item_category_id') is not None:
            key = '%s:%s' % (entry['item_category_id'], entry['item_local_id'])
            index = item.findData(key)
            if index < 0:
                missing = QStandardItem(key + '  (nem létező tárgy)'); missing.setData(key, Qt.ItemDataRole.UserRole); self.items.appendRow(missing)
                index = item.findData(key)
            item.setCurrentIndex(index)
        else:
            item.setCurrentIndex(-1)
        amount, weight, rare = QLineEdit(), QLineEdit(), QLineEdit()
        chance = QTableWidgetItem(''); chance.setFlags(Qt.ItemFlag.ItemIsEnabled)
        for column, widget in ((0, kind), (1, item), (2, amount), (3, weight), (4, rare)): self.table.setCellWidget(row, column, widget)
        self.table.setItem(row, 5, chance)
        reward = int(kind.currentData()); scale = AMOUNT_UNITS[reward][0]
        amount.setText(catalog.display(entry.get('amount') or 0, scale) if scale > 1 else str(entry.get('amount') or '0'))
        weight.setText(self.show_weight(int(entry.get('weight') or 0)))
        rare.setText(catalog.display(entry.get('rare_attribute_chance_bp') or 0, 100))
        def kind_changed(*_):
            self.kind_state(self.row_of(kind, 0)); self.changed()
        kind.currentIndexChanged.connect(kind_changed)
        item.currentIndexChanged.connect(lambda *_: self.changed())
        for edit in (amount, rare): edit.textEdited.connect(lambda *_: self.changed())
        weight.textEdited.connect(lambda *_: (self.refresh(), self.changed()))
        self.kind_state(row); self.refresh()

    def kind_state(self, row):
        kind = int(self.table.cellWidget(row, 0).currentData())
        item, amount, rare = (self.table.cellWidget(row, column) for column in (1, 2, 4))
        item.setEnabled(kind == 1)
        if kind != 1: item.setCurrentIndex(-1); rare.setText('0')
        rare.setEnabled(kind == 1)
        amount.setEnabled(kind != 8)
        if kind == 8: amount.setText('0')
        amount.setPlaceholderText(AMOUNT_UNITS[kind][1]); amount.setToolTip(AMOUNT_UNITS[kind][1])

    def show_weight(self, raw):
        return str(raw) if self.mode == '1' else catalog.display(raw, 100)

    def raw_weight(self, row, mode=None):
        text = self.table.cellWidget(row, 3).text().strip()
        try: return catalog.scaled(text, 1 if (mode or self.mode) == '1' else 100)
        except api.EditorError: return None

    def set_mode(self, mode):
        raws = [self.raw_weight(row) for row in range(self.table.rowCount())]
        self.mode = str(mode)
        self.table.horizontalHeaderItem(3).setText('Súly' if self.mode == '1' else 'Esély %')
        for row, raw in enumerate(raws):
            if raw is not None: self.table.cellWidget(row, 3).setText(self.show_weight(raw))
        self.refresh()

    def refresh(self):
        raws = [self.raw_weight(row) for row in range(self.table.rowCount())]
        total = sum(raw for raw in raws if raw)
        for row, raw in enumerate(raws):
            if raw is None: text = '?'
            elif self.mode == '1': text = '%.2f%%' % (100.0 * raw / total) if total else '—'
            else: text = catalog.display(raw, 100) + '%'
            self.table.item(row, 5).setText(text)
        self.total.setText(('Súlyok összege: %d — pontosan egy jutalom nyer.' % total) if self.mode == '1'
                           else 'Minden sor a saját esélyével, egymástól függetlenül nyer (lehet, hogy egy sem).')

    def remove(self):
        rows = sorted({index.row() for index in self.table.selectedIndexes()}, reverse=True)
        for row in rows: self.table.removeRow(row)
        if rows: self.refresh(); self.changed()

    def load(self, entries, mode):
        self.table.setRowCount(0); self.mode = str(mode)
        self.table.horizontalHeaderItem(3).setText('Súly' if self.mode == '1' else 'Esély %')
        for entry in entries or []: self.append(entry)
        self.refresh()

    def entries(self):
        result = []
        for row in range(self.table.rowCount()):
            where = '%d. jutalomsor' % (row + 1)
            kind = int(self.table.cellWidget(row, 0).currentData())
            entry = dict(reward_kind=str(kind), item_category_id=None, item_local_id=None)
            if kind == 1:
                key = self.table.cellWidget(row, 1).currentData()
                if not key: raise api.EditorError(where + ': válassz tárgyat.')
                entry['item_category_id'], entry['item_local_id'] = key.split(':')
            scale = AMOUNT_UNITS[kind][0]
            text = self.table.cellWidget(row, 2).text().strip()
            try: entry['amount'] = str(catalog.scaled(text, scale) if scale > 1 else int(text if re.fullmatch(r'[0-9]+', text) else 'x'))
            except (ValueError, api.EditorError): raise api.EditorError(where + ', mennyiség: érvénytelen szám (' + (text or 'üres') + ').') from None
            raw = self.raw_weight(row)
            if raw is None: raise api.EditorError(where + ', ' + ('súly' if self.mode == '1' else 'esély') + ': érvénytelen szám.')
            entry['weight'] = str(raw)
            try: entry['rare_attribute_chance_bp'] = str(catalog.scaled(self.table.cellWidget(row, 4).text() or '0', 100)) if kind == 1 else '0'
            except api.EditorError: raise api.EditorError(where + ', ritka bónusz: érvénytelen szám.') from None
            result.append(entry)
        return result


class RefinementPanel(QWidget):
    """Kind, maximum and the full cumulative +0..+max curve of one item.

    Stat gains live in the table cells until collect(); level bonuses live in
    self.levels. Rows are added/removed in place so typed values survive.
    """
    STAT_COLUMNS = {1: [('physical_attack_gain_raw', 'Fizikai támadás +'), ('magical_attack_gain_raw', 'Mágikus támadás +')],
                    2: [('physical_defense_gain_raw', 'Fizikai védekezés +'), ('magical_defense_gain_raw', 'Mágikus védekezés +')],
                    3: []}

    def __init__(self, changed):
        super().__init__(); self.changed = changed; self.category = 1; self.levels = []; self.definitions = []; self.loading = False
        self.rules = {}
        layout = QVBoxLayout(self)
        top = QHBoxLayout()
        self.kind = QComboBox(); self.kind.currentIndexChanged.connect(self.kind_changed)
        self.maximum = QSpinBox(); self.maximum.setRange(0, 255); self.maximum.setPrefix('+'); self.maximum.valueChanged.connect(self.resize_levels)
        self.step_button = QPushButton('Egyenletes kitöltés…'); self.step_button.clicked.connect(self.fill)
        top.addWidget(QLabel('Fejlesztés:')); top.addWidget(self.kind); top.addSpacing(16)
        top.addWidget(QLabel('Legmagasabb szint:')); top.addWidget(self.maximum); top.addSpacing(16); top.addWidget(self.step_button); top.addStretch()
        layout.addLayout(top)
        hint = QLabel('Egy tárgy, egy prototípus: a +0…+N szint a tárgypéldány állapota. Az értékek halmozottak a +0-hoz képest '
                      '(pontban), és szintről szintre nem csökkenhetnek. Új szint a legmagasabb szintet folytatja: a kitöltésnél megadott '
                      'növekménnyel, ennek hiányában az utolsó két szint különbségével (egyforma értéknél másol). Ékszer fejlesztése '
                      'csak szintbónuszt adhat.')
        hint.setWordWrap(True); hint.setObjectName('FieldHint'); layout.addWidget(hint)
        self.table = QTableWidget(0, 0); self.table.verticalHeader().hide(); self.table.itemChanged.connect(self.cell_changed)
        layout.addWidget(self.table, 1)

    def stats(self):
        return self.STAT_COLUMNS[self.category]

    def set_category(self, category, definitions):
        self.category = category; self.definitions = definitions
        self.kind.blockSignals(True); self.kind.clear()
        self.kind.addItem('Nincs fejlesztés', '1')
        self.kind.addItem({1: 'Fegyverfejlesztés', 2: 'Páncélfejlesztés', 3: 'Ékszerfejlesztés'}[category], str(catalog.REFINEMENT_KIND[category]))
        self.kind.blockSignals(False)

    def load(self, refinement, levels):
        self.loading = True
        self.rules = {}  # step per ('stat', column) / ('bonus', id), from the fill dialog
        self.kind.setCurrentIndex(max(0, self.kind.findData(str(refinement['profile_kind']))))
        self.levels = [copy.deepcopy(level) for level in levels]
        self.maximum.blockSignals(True); self.maximum.setValue(int(refinement['max_level'])); self.maximum.blockSignals(False)
        self.render()
        self.kind_changed()
        self.loading = False

    def kind_changed(self, *_):
        none = self.kind.currentData() == '1'
        if none and self.maximum.value():
            self.maximum.setValue(0)
        self.maximum.setEnabled(not none); self.step_button.setEnabled(not none)
        if not self.loading: self.changed()

    def render(self):
        stats = self.stats()
        self.table.blockSignals(True)
        self.table.clear(); self.table.setColumnCount(2 + len(stats)); self.table.setRowCount(0)
        self.table.setHorizontalHeaderLabels(['Szint', *[label + ' (pont)' for _, label in stats], 'Szintbónuszok'])
        self.table.horizontalHeader().setSectionResizeMode(QHeaderView.ResizeMode.Stretch)
        self.table.horizontalHeader().setSectionResizeMode(0, QHeaderView.ResizeMode.ResizeToContents)
        for row, level in enumerate(self.levels):
            self.add_row(row, [catalog.display(level.get(name, '0')) for name, _ in stats])
        self.table.blockSignals(False)

    def add_row(self, row, texts):
        stats = self.stats()
        self.table.insertRow(row)
        item = QTableWidgetItem('+' + str(row)); item.setFlags(Qt.ItemFlag.ItemIsEnabled); self.table.setItem(row, 0, item)
        for column, text in enumerate(texts, 1):
            cell = QTableWidgetItem(text)
            if row == 0: cell.setFlags(Qt.ItemFlag.ItemIsEnabled)
            self.table.setItem(row, column, cell)
        action = QPushButton(); action.setEnabled(row > 0)
        action.clicked.connect(lambda _=False, index=row: self.edit_bonuses(index))
        self.table.setCellWidget(row, len(stats) + 1, action)
        self.update_button(row)

    def update_button(self, row):
        action = self.table.cellWidget(row, len(self.stats()) + 1)
        entries = self.levels[row].get('bonuses') or []
        # Show the values themselves, so a filled curve is readable at a glance.
        text = self.describe(entries[0]) + (' (+%d további)' % (len(entries) - 1) if len(entries) > 1 else '') if entries else 'Hozzáadás…'
        action.setText('—' if row == 0 else text)
        action.setToolTip('\n'.join(self.describe(entry) for entry in entries))

    def describe(self, entry):
        d = next((d for d in self.definitions if str(d['id']) == str(entry['bonus_id'])), None)
        if d is None: return 'Ismeretlen bónusz #%s: %s' % (entry['bonus_id'], entry['value_scaled'])
        scale, unit = UNITS.get(int(d['unit']), (1, ''))
        return '%s: %s %s' % (BONUS_LABELS.get(d['code'], d['code']), catalog.display(entry['value_scaled'], scale), unit)

    def resize_levels(self, maximum):
        stats = self.stats()
        self.table.blockSignals(True)
        while len(self.levels) > maximum + 1:
            self.levels.pop(); self.table.removeRow(self.table.rowCount() - 1)
        while len(self.levels) < maximum + 1:
            # A new level continues the current top level: its typed gains and
            # its level bonuses, advanced by the remembered or observed step
            # (same bonus profile while the values stay equal).
            if self.levels:
                previous = copy.deepcopy(self.levels[-1]); top = len(self.levels) - 1
                texts = [self.continued_text(top, column) for column in range(1, len(stats) + 1)]
                for entry in previous.get('bonuses') or []:
                    entry['value_scaled'] = str(self.continued_bonus(top, entry))
            else:
                previous = dict(**{g: '0' for g in catalog.GAINS}, extra_bonus_profile_id=None, bonuses=[]); texts = ['0'] * len(stats)
            previous['level'] = str(len(self.levels))
            self.levels.append(previous)
            self.add_row(len(self.levels) - 1, texts)
        self.table.blockSignals(False)
        if not self.loading: self.changed()

    def cell_changed(self, item):
        if not self.loading: self.changed()

    # --- continuation of the top level ---------------------------------------
    def cell_value(self, row, column):
        try: return catalog.scaled(self.table.item(row, column).text())
        except api.EditorError: return None

    def bonus_value(self, row, bonus_id):
        entry = next((e for e in self.levels[row].get('bonuses') or [] if str(e['bonus_id']) == str(bonus_id)), None)
        return int(entry['value_scaled']) if entry else None

    def step_for(self, key, value, below):
        """Remembered fill step, else the step between the two top levels
        (+0 is fixed at zero and never counts), else 0, which copies."""
        if key in self.rules: return self.rules[key]
        return value - below if below is not None else 0

    def continued_text(self, top, column):
        value = self.cell_value(top, column)
        if value is None: return self.table.item(top, column).text()
        below = self.cell_value(top - 1, column) if top >= 2 else None
        step = self.step_for(('stat', column), value, below)
        return catalog.display(min(max(value + step, 0), 10 ** 15))

    def continued_bonus(self, top, entry):
        value = int(entry['value_scaled'])
        below = self.bonus_value(top - 1, entry['bonus_id']) if top >= 2 else None
        step = self.step_for(('bonus', str(entry['bonus_id'])), value, below)
        d = next((d for d in self.definitions if str(d['id']) == str(entry['bonus_id'])), None)
        low, high = (int(d['minimum']), int(d['maximum'])) if d else (value + step, value + step)
        return min(max(value + step, low), high)

    @staticmethod
    def rule_key(target):
        return ('stat', target['column']) if target['kind'] == 'stat' else ('bonus', target['id'])

    def edit_bonuses(self, index):
        dialog = BonusDialog(self, 'Szintbónuszok: +%d' % index, self.definitions, self.levels[index].get('bonuses'))
        if dialog.exec() == QDialog.DialogCode.Accepted and dialog.result_entries is not None:
            self.levels[index]['bonuses'] = dialog.result_entries
            self.update_button(index); self.changed()

    # --- linear fill ---------------------------------------------------------
    def fill_targets(self):
        targets = [dict(label=label + ' (halmozott)', kind='stat', column=column, scale=1000, unit='pont', range=(0, 10 ** 15))
                   for column, (_, label) in enumerate(self.stats(), 1)]
        seen = []
        for level in self.levels:
            for entry in level.get('bonuses') or []:
                if entry['bonus_id'] not in seen: seen.append(entry['bonus_id'])
        for d in self.definitions:
            if int(d['sources']) & 2 and str(d['id']) in seen:
                scale, unit = UNITS.get(int(d['unit']), (1, ''))
                targets.append(dict(label='Szintbónusz: ' + BONUS_LABELS.get(d['code'], d['code']), kind='bonus', id=str(d['id']),
                                    scale=scale, unit=unit, range=(int(d['minimum']), int(d['maximum']))))
        # Bonuses first: they are what a filled curve most often needs.
        return [t for t in targets if t['kind'] == 'bonus'] + [t for t in targets if t['kind'] == 'stat']

    def snapshot(self):
        columns = range(1, len(self.stats()) + 1)
        return dict(levels=copy.deepcopy(self.levels),
                    texts=[[self.table.item(row, column).text() for column in columns] for row in range(len(self.levels))])

    def restore(self, state):
        self.levels = copy.deepcopy(state['levels'])
        self.table.blockSignals(True)
        for row, texts in enumerate(state['texts']):
            for column, text in enumerate(texts, 1): self.table.item(row, column).setText(text)
            self.update_button(row)
        self.table.blockSignals(False)

    def current_value(self, target, level, state=None):
        state = state or self.snapshot()
        if target['kind'] == 'stat':
            try: return catalog.scaled(state['texts'][level][target['column'] - 1])
            except api.EditorError: return None
        entry = next((e for e in state['levels'][level].get('bonuses') or [] if str(e['bonus_id']) == target['id']), None)
        return int(entry['value_scaled']) if entry else None

    def apply_fill(self, target, values, notify=True):
        self.table.blockSignals(True)
        for level, value in values.items():
            if target['kind'] == 'stat':
                self.table.item(level, target['column']).setText(catalog.display(value, target['scale']))
                continue
            entries = self.levels[level].setdefault('bonuses', [])
            entry = next((e for e in entries if str(e['bonus_id']) == target['id']), None)
            if entry is None: entries.append(dict(bonus_id=target['id'], value_scaled=str(value)))
            else: entry['value_scaled'] = str(value)
            self.update_button(level)
        self.table.blockSignals(False)
        if notify: self.changed()

    def fill(self):
        if len(self.levels) < 2:
            QMessageBox.information(self, 'Egyenletes kitöltés', 'Előbb állítsd be a legmagasabb szintet (legalább +1).'); return
        targets = self.fill_targets()
        if not targets:
            QMessageBox.information(self, 'Egyenletes kitöltés', 'Nincs kitölthető érték: adj meg legalább egy szintbónuszt.'); return
        state = self.snapshot()
        def preview(result):
            self.restore(state)
            if result: self.apply_fill(*result, notify=False)
        dialog = FillDialog(self, targets, len(self.levels) - 1, lambda target, level: self.current_value(target, level, state), preview,
                            lambda target: self.rules.get(self.rule_key(target)))
        dialog.exec()
        # Whatever closed the dialog, keep its last valid result unless the
        # user explicitly reverted. The return code is not relied upon. The
        # step is remembered even when only +1 exists yet: "+" continues it.
        self.restore(state)
        if not dialog.reverted and dialog.last is not None:
            self.rules[self.rule_key(dialog.last[0])] = dialog.last_step
            self.apply_fill(*dialog.last)

    def collect(self, document):
        stats = self.stats()
        levels = []
        for row, level in enumerate(self.levels):
            level = copy.deepcopy(level); level['level'] = str(row)
            for gain in catalog.GAINS: level.setdefault(gain, '0')
            for column, (name, label) in enumerate(stats, 1):
                text = self.table.item(row, column).text().strip()
                try: level[name] = str(catalog.scaled(text))
                except api.EditorError: raise api.EditorError('+%d szint, %s: érvénytelen szám (%s).' % (row, label, text or 'üres')) from None
            levels.append(level)
        document['refinement'] = dict(document.get('refinement') or {}, profile_kind=self.kind.currentData(), max_level=str(self.maximum.value()))
        document['levels'] = levels


class ItemPage(QWidget):
    def __init__(self, owner):
        super().__init__(); self.owner = owner
        self.packet = self.document = self.original_key = self.task = None
        self.dirty = False; self.target = None; self.attempted = None; self.loading = False
        layout = QVBoxLayout(self); layout.setContentsMargins(0, 0, 0, 0)
        bar = QHBoxLayout(); title = QLabel('Tárgyak'); title.setObjectName('SectionTitle'); bar.addWidget(title, 1)
        self.start_db_button = QPushButton('Adatbázis indítása'); self.start_db_button.clicked.connect(self.start_database)
        self.reload_button = QPushButton('Lista frissítése'); self.reload_button.clicked.connect(self.reload)
        self.new_button = QToolButton(); self.new_button.setText('Új tárgy'); self.new_button.setPopupMode(QToolButton.ToolButtonPopupMode.InstantPopup)
        menu = QMenu(self.new_button)
        for category, name in catalog.CATEGORIES.items(): menu.addAction(name, lambda c=category: self.create(c))
        self.new_button.setMenu(menu)
        for widget in (self.start_db_button, self.reload_button, self.new_button): bar.addWidget(widget)
        layout.addLayout(bar)
        self.status = QLabel('Válassz helyi szervercsomagot.'); self.status.setWordWrap(True); self.status.setObjectName('Subtle')
        layout.addWidget(self.status)
        split = QSplitter(); self.split = split
        left = QWidget(); side = QVBoxLayout(left); side.setContentsMargins(0, 0, 0, 0)
        filters = QHBoxLayout()
        self.category_filter = QComboBox(); self.category_filter.addItem('Minden kategória', 0)
        for category, name in catalog.CATEGORIES.items(): self.category_filter.addItem(name, category)
        self.search = QLineEdit(); self.search.setPlaceholderText('Keresés kódra, névkulcsra, kulcsra…')
        filters.addWidget(self.category_filter); filters.addWidget(self.search, 1); side.addLayout(filters)
        self.show_retired = QCheckBox('Kivezetett tárgyak is'); side.addWidget(self.show_retired)
        self.list = QTableWidget(0, 4); self.list.setHorizontalHeaderLabels(['Kulcs', 'Kategória', 'Tartalomkód', 'Állapot'])
        self.list.horizontalHeader().setSectionResizeMode(2, QHeaderView.ResizeMode.Stretch)
        self.list.verticalHeader().hide(); self.list.setEditTriggers(QAbstractItemView.EditTrigger.NoEditTriggers)
        self.list.setSelectionBehavior(QAbstractItemView.SelectionBehavior.SelectRows)
        self.list.setSelectionMode(QAbstractItemView.SelectionMode.SingleSelection)
        self.list.itemSelectionChanged.connect(self.selected)
        side.addWidget(self.list, 1)
        for signal in (self.category_filter.currentIndexChanged, self.search.textChanged, self.show_retired.toggled):
            signal.connect(lambda *_: self.fill_list())
        split.addWidget(left)
        right = QWidget(); body = QVBoxLayout(right); body.setContentsMargins(8, 0, 0, 0)
        head = QHBoxLayout(); self.heading = QLabel('Nincs kijelölt tárgy'); self.heading.setObjectName('SectionTitle')
        self.badge = QLabel(''); self.badge.setObjectName('SavedBadge')
        head.addWidget(self.heading, 1); head.addWidget(self.badge); body.addLayout(head)
        self.tabs = QTabWidget(); body.addWidget(self.tabs, 1)
        self.build_forms()
        actions = QHBoxLayout()
        self.check_button = QPushButton('Ellenőrzés'); self.check_button.clicked.connect(self.check)
        self.save_button = QPushButton('Mentés'); self.save_button.setObjectName('Primary'); self.save_button.clicked.connect(self.save)
        self.discard_button = QPushButton('Elvetés'); self.discard_button.clicked.connect(self.discard)
        self.save_note = QLabel(''); self.save_note.setObjectName('Subtle'); self.save_note.setWordWrap(True)
        for widget in (self.check_button, self.save_button, self.discard_button): actions.addWidget(widget)
        actions.addWidget(self.save_note, 1); body.addLayout(actions)
        split.addWidget(right); split.setSizes([380, 720]); split.setStretchFactor(1, 1)
        layout.addWidget(split, 1)
        self.show_document(None)

    # --- forms -------------------------------------------------------------------
    def build_forms(self):
        changed = self.mark_dirty
        base = Form(changed)
        base.add('base', 'content_code', 'Tartalomkód', 'text', hint='Egyedi, állandó azonosító szöveg, pl. weapon.sword_iron. Kisbetű, szám, _ és .')
        base.add('base', 'name_key', 'Névkulcs', 'text', hint='Fordítási kulcs, amelyből a kliens a megjelenő nevet veszi.')
        base.add('base', 'description_key', 'Leíráskulcs', 'text')
        base.add('base', 'icon_asset', 'Ikon', 'text', hint='Kliensoldali útvonal, pl. icons/weapon/sword_iron.png')
        base.add('base', 'inventory_width', 'Szélesség (mező)', range=(1, 255))
        base.add('base', 'inventory_height', 'Magasság (mező)', range=(1, 255))
        self.stack_field = base.add('base', 'max_stack', 'Halmozás', range=(1, 4294967295), hint='Felszerelés nem halmozható (1).')
        base.add('base', 'weight_grams', 'Súly (gramm)', range=(0, 4294967295))
        base.add('base', 'min_level', 'Minimális szint', range=(0, 65535))
        self.binding_field = base.add('base', 'binding_policy', 'Kötés', 'choice', choices=[(1, 'Nincs'), (2, 'Megszerzéskor'), (3, 'Felszereléskor')])
        base.add('base', 'buy_amount', 'Vételár (Yang)', range=(0, 2 ** 63 - 1))
        base.add('base', 'sell_amount', 'Eladási ár (Yang)', range=(0, 2 ** 63 - 1))
        base.add('base', 'retired', 'Kivezetés', 'bool', text='Kivezetve — a kulcs megmarad, új példány nem készül')
        base.note('A kategória és a helyi azonosító létrehozás után nem változik, és soha nem kerül újrahasznosításra.')
        base.finish(); self.base_form = base
        use = Form(changed)
        use.add('use', 'allowed_classes', 'Viselheti', 'bits', names=['Harcos', 'Nindzsa', 'Sura', 'Sámán'])
        use.add('use', 'allowed_sexes', 'Nem', 'bits', names=['Férfi', 'Nő'])
        use.add('use', 'use_during_combat', 'Harc közben', 'bool', text='Harc közben is használható')
        use.add('trade', 'allow_trade', 'Kereskedés', 'bool', text='Játékosok között cserélhető')
        use.add('trade', 'allow_drop', 'Eldobás', 'bool', text='Földre dobható')
        use.add('trade', 'allow_sell', 'Eladás', 'bool', text='NPC-nek eladható')
        self.appearance_field = use.add('appearance', 'model_asset', 'Megjelenési modell', 'text', optional=True,
                                        placeholder='Üres = nincs külön modell', hint='Felszerelés kliensoldali modellje.')
        use.finish(); self.use_form = use
        weapon = Form(changed)
        weapon.add('child', 'weapon_subtype', 'Fegyvertípus', 'choice',
                   choices=[(1, 'Kard'), (2, 'Tőr'), (3, 'Íj'), (4, 'Kétkezes'), (5, 'Csengő'), (6, 'Legyező'), (7, 'Lovas lándzsa')])
        weapon.add('child', 'physical_attack_base_raw', 'Fizikai támadás', 'scaled', scale=1000, unit='pont', range=(0, 10 ** 15))
        weapon.add('child', 'magical_attack_base_raw', 'Mágikus támadás', 'scaled', scale=1000, unit='pont', range=(0, 10 ** 15))
        weapon.add('attack', 'interval_ticks', 'Támadási időköz', unit='tick', range=(1, 4294967295))
        weapon.add('attack', 'windup_ticks', 'Felkészülés', unit='tick', range=(0, 4294967295), hint='Nem lehet hosszabb a támadási időköznél.')
        weapon.add('attack', 'reach_millimeters', 'Hatótáv', 'scaled', scale=1000, unit='méter', range=(1, 4294967295))
        weapon.add('attack', 'max_targets', 'Célpontok száma', range=(1, 1024))
        weapon.add('ammo', 'ammo_kind', 'Lőszer', 'choice', choices=[(1, 'Nem használ lőszert'), (2, 'Lőszert fogyaszt a halomból')])
        weapon.add('ammo', 'units_per_hit', 'Lőszer találatonként', range=(0, 65535), hint='Lőszer nélkül 0, lőszerrel legalább 1.')
        weapon.finish()
        armor = Form(changed, 2)
        armor.add('child', 'equip_slot', 'Hely', 'choice', choices=[(2, 'Test'), (3, 'Fej'), (4, 'Pajzs'), (5, 'Láb')])
        armor.add('child', 'physical_defense_base_raw', 'Fizikai védekezés', 'scaled', scale=1000, unit='pont', range=(0, 10 ** 15))
        armor.add('child', 'magical_defense_base_raw', 'Mágikus védekezés', 'scaled', scale=1000, unit='pont', range=(0, 10 ** 15))
        self.add_resistances(armor)
        armor.finish()
        accessory = Form(changed, 2)
        accessory.add('child', 'equip_slot', 'Hely', 'choice', choices=[(6, 'Csukló'), (7, 'Nyak'), (8, 'Fül')])
        self.socket_field = accessory.add('child', 'socket_profile_id', 'Foglalatprofil', 'choice', choices=[('', 'Nincs foglalat')])
        self.add_resistances(accessory)
        accessory.note('Ékszer alapértéket nem kap: a statokat bónuszok és szintbónuszok adják. A foglalatprofilok külön tartalmak.')
        accessory.finish()
        material = Form(changed)
        material.add('child', 'material_kind', 'Alapanyag típusa', 'choice', choices=[(1, 'Bőr'), (2, 'Vér'), (3, 'Gyökér'), (4, 'Tű'), (5, 'Ékkő')])
        material.note('Az alapanyag nem fejleszthető, nem köthető felszereléskor, és nem kap bónuszt vagy megjelenést.')
        material.finish()
        container = QWidget(); container_layout = QVBoxLayout(container)
        container_form = Form(changed)
        self.container_kind_field = container_form.add('child', 'container_kind', 'Nyitás', 'choice',
            choices=[(1, 'Használatkor nyílik'), (2, 'Kulccsal nyílik'), (3, 'Quest nyitja (beépített tartalom nélkül)')])
        self.lock_field = container_form.add('child', 'lock_id', 'Zárazonosító', range=(0, 65535),
            hint='Kulccsal nyíló ládánál legalább 1; az a kulcs nyitja, amelynek ugyanez a zárazonosítója.')
        container_form.finish(); container_layout.addWidget(container_form)
        mode_row = QHBoxLayout(); mode_row.addWidget(QLabel('Sorsolás:'))
        self.loot_mode = QComboBox(); self.loot_mode.addItem('Pontosan egy jutalom, súly szerint', '1')
        self.loot_mode.addItem('Minden sor külön, saját esélyével', '2')
        self.loot_mode.currentIndexChanged.connect(lambda *_: (self.loot_table.set_mode(self.loot_mode.currentData()), changed()))
        mode_row.addWidget(self.loot_mode); mode_row.addStretch(); container_layout.addLayout(mode_row)
        self.loot_table = LootTable(changed); container_layout.addWidget(self.loot_table, 1)
        self.loot_hint = QLabel('A questdoboz tartalmát a quest adja; itt nincs beépített jutalom.')
        self.loot_hint.setObjectName('FieldHint'); self.loot_hint.setWordWrap(True); container_layout.addWidget(self.loot_hint)
        self.container_kind_field.widget.currentIndexChanged.connect(lambda *_: self.container_kind_changed())
        key_form = Form(changed)
        key_form.add('child', 'lock_id', 'Zárazonosító', range=(1, 65535), hint='Az azonos zárazonosítójú, kulccsal nyíló ládákat nyitja.')
        key_form.note('A kulcs a láda kinyitásakor egy darabot fogy, a ládával együtt.')
        key_form.finish()
        token_form = Form(changed)
        token_form.add('child', 'wallet_currency', 'Pénznem', 'choice', choices=[(1, 'Yang'), (2, 'Sárkányérme')])
        token_form.add('child', 'amount_per_unit', 'Érték darabonként', unit='egység', range=(1, 2 ** 63 - 1),
                       hint='Ennyit ír jóvá a pénztárcában egy beváltott darab.')
        token_form.note('Használatkor a halomból annyi darab váltódik be, amennyi értéke még belefér a pénztárcába; '
                        'a többi darab megmarad. Egy darab sem fogy el úgy, hogy az értéke elveszne.')
        token_form.finish()
        potion = QWidget(); potion_layout = QVBoxLayout(potion)
        recovery = Form(changed)
        self.delivery_field = recovery.add('recovery', 'delivery_kind', 'Hatás módja', 'choice',
                                           choices=[(1, 'Azonnali'), (2, 'Tartalék (adagolva tölt)'), (3, 'Időszakos')])
        recovery.add('recovery', 'units_per_use', 'Egység használatonként', range=(1, 65535))
        recovery.add('recovery', 'allowed_contexts', 'Használható', 'bits', names=['Világ', 'Párbaj', 'Aréna', 'Céhháború'])
        recovery.add('recovery', 'allow_during_pvp', 'PvP', 'bool', text='PvP közben is használható')
        self.cooldown_check = QCheckBox('Várakozási idő használat után'); self.cooldown_check.toggled.connect(lambda *_: (self.cooldown_toggled(), changed()))
        self.cooldown_edit = QLineEdit(); self.cooldown_edit.textEdited.connect(lambda *_: changed())
        cooldown = QHBoxLayout(); cooldown.addWidget(self.cooldown_check); cooldown.addWidget(self.cooldown_edit); cooldown.addWidget(QLabel('mp')); cooldown.addStretch()
        potion_layout.addWidget(recovery); potion_layout.addLayout(cooldown)
        self.effects = QStackedWidget(); self.effect_forms = {}
        instant = Form(changed, 2)
        instant.add('effect', 'hp_raw', 'HP', 'scaled', scale=1000, unit='pont', range=(0, 10 ** 15))
        instant.add('effect', 'sp_raw', 'SP', 'scaled', scale=1000, unit='pont', range=(0, 10 ** 15))
        instant.add('effect', 'max_hp_bp', 'HP a maximumból', 'scaled', scale=100, unit='%', range=(0, 10000))
        instant.add('effect', 'max_sp_bp', 'SP a maximumból', 'scaled', scale=100, unit='%', range=(0, 10000))
        instant.add('effect', 'flat_bonus_cap_bp', 'Bónusz-szorzó plafon', 'scaled', scale=100, unit='%', range=(0, 100000))
        instant.finish()
        reserve = Form(changed, 2)
        reserve.add('effect', 'hp_raw', 'HP-tartalék', 'scaled', scale=1000, unit='pont', range=(0, 10 ** 15))
        reserve.add('effect', 'sp_raw', 'SP-tartalék', 'scaled', scale=1000, unit='pont', range=(0, 10 ** 15))
        reserve.add('effect', 'flat_bonus_cap_bp', 'Bónusz-szorzó plafon', 'scaled', scale=100, unit='%', range=(0, 100000))
        reserve.add('effect', 'tick_interval_ms', 'Töltési időköz', 'scaled', scale=1000, unit='mp', range=(1, 2 ** 63 - 1))
        reserve.add('effect', 'max_pool_per_tick_bp', 'Töltés tickenként (max.)', 'scaled', scale=100, unit='%', range=(1, 10000))
        reserve.finish()
        periodic = Form(changed, 2)
        periodic.add('effect', 'hp_per_tick_raw', 'HP tickenként', 'scaled', scale=1000, unit='pont', range=(0, 10 ** 15))
        periodic.add('effect', 'sp_per_tick_raw', 'SP tickenként', 'scaled', scale=1000, unit='pont', range=(0, 10 ** 15))
        periodic.add('effect', 'tick_interval_ms', 'Időköz', 'scaled', scale=1000, unit='mp', range=(1, 2 ** 63 - 1))
        periodic.add('effect', 'duration_ms', 'Időtartam', 'scaled', scale=1000, unit='mp', range=(1, 2 ** 63 - 1))
        periodic.note('Időszakos hatás vagy HP-t, vagy SP-t tölt. Az időtartam az időköz egész számú többszöröse.')
        periodic.finish()
        for kind, form in ((1, instant), (2, reserve), (3, periodic)):
            self.effect_forms[kind] = form; self.effects.addWidget(form)
        self.delivery_field.widget.currentIndexChanged.connect(lambda *_: self.effects.setCurrentWidget(self.effect_forms[int(self.delivery_field.value())]))
        potion_layout.addWidget(self.effects); potion_layout.addStretch()
        self.category_forms = {1: weapon, 2: armor, 3: accessory, 5: recovery, 6: material, 8: container_form,
                               16: token_form, 18: key_form}
        self.category_pages = {1: scroll(weapon), 2: scroll(armor), 3: scroll(accessory), 5: scroll(potion), 6: scroll(material),
                               8: container, 16: scroll(token_form), 18: scroll(key_form)}
        self.refinement = RefinementPanel(changed)
        self.bonuses = BonusTable(changed, '1')
        bonus_page = QWidget(); bonus_layout = QVBoxLayout(bonus_page)
        hint = QLabel('Az alap bónuszok minden fejlesztési szinten érvényesek. Szintenkénti bónuszt a Fejlesztés lapon adhatsz meg.')
        hint.setWordWrap(True); hint.setObjectName('FieldHint'); bonus_layout.addWidget(hint); bonus_layout.addWidget(self.bonuses, 1)
        self.base_page, self.use_page, self.bonus_page = scroll(base), scroll(use), bonus_page

    def add_resistances(self, form):
        for column, label in zip(catalog.RESISTANCES, RESISTANCE_LABELS):
            form.add('resistance', column, label + ' ellenállás', 'scaled', scale=100, unit='%', range=(-10000, 10000))

    def container_kind_changed(self):
        kind = self.container_kind_field.value()
        self.lock_field.widget.setEnabled(kind == '2')
        if kind != '2' and not self.loading: self.lock_field.edit.setText('0')
        for widget in (self.loot_mode, self.loot_table): widget.setEnabled(kind != '3')
        self.loot_hint.setVisible(kind == '3')

    def cooldown_toggled(self):
        self.cooldown_edit.setEnabled(self.cooldown_check.isChecked())

    # --- state ---------------------------------------------------------------------
    def active(self):
        return self.task is not None

    def local(self):
        return not self.owner.is_remote() and bool(self.owner.root)

    def mark_dirty(self):
        if self.loading or self.document is None: return
        self.dirty = True; self.update_badge(); self.controls()

    def update_badge(self):
        self.badge.setProperty('dirty', 'true' if self.dirty else 'false')
        self.badge.setText('Nem mentett módosítás' if self.dirty else ('Új, még nem mentett tárgy' if self.document and self.original_key is None else 'Mentve' if self.document else ''))
        self.badge.setVisible(bool(self.document)); self.badge.style().unpolish(self.badge); self.badge.style().polish(self.badge)

    def may_discard(self):
        if not self.dirty and not (self.document and self.original_key is None): return True
        answer = QMessageBox.question(self, 'Tárgyszerkesztő', 'A tárgy módosításai nincsenek mentve. Elveted őket?')
        if answer == QMessageBox.StandardButton.Yes:
            self.dirty = False; return True
        return False

    def tick(self, state):
        if self.target != self.owner.target_id():
            self.target = self.owner.target_id(); self.attempted = None
            self.packet = None; self.dirty = False; self.show_document(None); self.list.setRowCount(0)
        if not self.local():
            self.status.setText('A tárgyszerkesztő csak helyi szervercsomagon érhető el; a távoli gép protos adatbázisa nem szerkeszthető innen.')
        elif state and not state.get('database_running') and not self.active():
            self.status.setText('A helyi adatbázis nem fut. Indítsd el az „Adatbázis indítása” gombbal (a játékszerver ettől nem indul el).')
        elif self.isVisible() and state and self.packet is None and self.attempted != self.target and not self.active() and not self.owner.busy():
            self.attempted = self.target; self.reload()
        self.state = state; self.controls()

    def controls(self):
        state = getattr(self, 'state', None) or {}
        local = self.local(); busy = self.active() or self.owner.busy()
        db = bool(state.get('database_running'))
        self.start_db_button.setVisible(local and not db and state.get('database_kind') != 'external')
        self.start_db_button.setEnabled(local and not busy and bool(state.get('database_initialized')))
        self.reload_button.setEnabled(local and db and not busy)
        self.new_button.setEnabled(local and db and not busy and self.packet is not None)
        self.list.setEnabled(not busy and self.packet is not None)
        has = self.document is not None
        self.check_button.setEnabled(has and not busy and self.packet is not None)
        running = bool(state.get('running'))
        can_save = has and not busy and db and not running and (self.dirty or self.original_key is None)
        self.save_button.setEnabled(can_save)
        self.discard_button.setEnabled(has and not busy and (self.dirty or self.original_key is None))
        self.save_note.setText('A mentéshez állítsd le a helyi játékszervert.' if has and running else
                               'Mentés előtt a játék saját katalógusellenőrzője vizsgálja meg a teljes katalógust.' if has else '')

    # --- background work ---------------------------------------------------------
    def run(self, function, done, message):
        if self.active(): return
        task = Task(function, self); self.task = task; outcome = {}
        self.status.setText(message)
        task.ready.connect(lambda value: outcome.update(value=value)); task.failed.connect(lambda text: outcome.update(error=text))
        def finished():
            if self.task is task: self.task = None
            task.deleteLater()
            if 'error' in outcome:
                self.status.setText(outcome['error']); self.owner.error(outcome['error'])
            elif 'value' in outcome:
                try: done(outcome['value'])
                except Exception as error: self.status.setText(str(error)); self.owner.error(str(error))
            self.controls(); self.owner.refresh()
        task.finished.connect(finished); task.start(); self.controls()

    def start_database(self):
        if not self.local() or self.owner.busy(): return
        self.owner.job(['start-db'], lambda ok: (setattr(self, 'attempted', None), self.reload()) if ok else None)

    def reload(self):
        if not self.local() or self.active(): return
        if not self.may_discard(): return
        root = self.owner.root
        keep = self.original_key
        self.run(lambda: catalog.request(root), lambda packet: self.loaded(packet, keep), 'Katalógus olvasása a helyi protos adatbázisból…')

    def loaded(self, packet, select=None):
        self.packet = packet
        model = catalog.Catalog(packet)
        self.bonuses.set_definitions(packet['bonuses'])
        self.socket_field.widget.blockSignals(True); self.socket_field.widget.clear()
        self.socket_field.widget.addItem('Nincs foglalat', '')
        for pid, code in model.choices('accessory_socket_profile'): self.socket_field.widget.addItem(code + '  (#' + pid + ')', pid)
        self.socket_field.widget.blockSignals(False)
        self.loot_table.set_items(model.items())
        items = model.items()
        retired = sum(row['retired'] == '1' for row in items)
        self.status.setText('%d tárgy (%d kivezetve) • katalógus-hash %s… • helyi protos adatbázis' % (len(items), retired, packet['hash'][:12]))
        self.dirty = False; self.document = None
        select = tuple(select) if select is not None else None
        if select is not None and any((int(r['category_id']), int(r['local_id'])) == select for r in items): self.open_item(select)
        else: self.original_key = None; self.show_document(None)
        self.fill_list()

    def fill_list(self):
        if self.packet is None: return
        selected = self.original_key if self.document is not None else None
        category = self.category_filter.currentData(); needle = self.search.text().strip().lower()
        rows = [row for row in catalog.Catalog(self.packet).items()
                if (not category or int(row['category_id']) == category) and (self.show_retired.isChecked() or row['retired'] != '1')
                and (not needle or needle in (row['content_code'] + ' ' + row['name_key'] + ' ' + row['category_id'] + ':' + row['local_id']).lower())]
        self.list.blockSignals(True); self.list.setRowCount(len(rows)); target = -1
        for index, row in enumerate(rows):
            key = (int(row['category_id']), int(row['local_id']))
            cells = ['%d:%d' % key, catalog.CATEGORIES.get(key[0], str(key[0])), row['content_code'], 'Kivezetve' if row['retired'] == '1' else 'Aktív']
            for column, value in enumerate(cells):
                item = QTableWidgetItem(value); item.setData(KEY_ROLE, key); self.list.setItem(index, column, item)
            if selected is not None and key == tuple(selected): target = index
        self.list.clearSelection()
        if target >= 0: self.list.selectRow(target)
        self.list.blockSignals(False)
        self.list.resizeColumnToContents(0); self.list.resizeColumnToContents(1)

    def selected(self):
        items = self.list.selectedItems()
        if not items: return
        key = items[0].data(KEY_ROLE)
        if self.document is not None and self.original_key == tuple(key): return
        if not self.may_discard():
            self.fill_list(); return
        self.open_item(tuple(key))

    def open_item(self, key):
        try:
            document = catalog.Catalog(self.packet).item(key)
        except (api.EditorError, StopIteration, KeyError) as error:
            self.owner.error(str(error) or 'A tárgy adatai hiányosak.'); return
        self.original_key = tuple(key); self.show_document(document)
        self.fill_list()

    def create(self, category):
        if self.packet is None or not self.may_discard(): return
        try: document = catalog.Catalog(self.packet).new(category)
        except api.EditorError as error: self.owner.error(str(error)); return
        self.original_key = None
        self.list.blockSignals(True); self.list.clearSelection(); self.list.blockSignals(False)
        self.show_document(document)
        self.tabs.setCurrentIndex(0)

    # --- document <-> forms ------------------------------------------------------
    def show_document(self, document):
        self.loading = True; self.document = copy.deepcopy(document); self.dirty = False
        current = self.tabs.currentIndex()
        while self.tabs.count(): self.tabs.removeTab(0)
        if document is None:
            self.heading.setText('Nincs kijelölt tárgy'); self.loading = False; self.update_badge(); self.controls(); return
        base = document['base']; category = int(base['category_id'])
        self.heading.setText('%s %s:%s — %s' % (catalog.CATEGORIES[category], base['category_id'], base['local_id'], base['content_code']))
        equipment = category in (1, 2, 3)
        self.tabs.addTab(self.base_page, 'Alapadatok')
        self.tabs.addTab(self.use_page, 'Használat és kereskedelem')
        self.tabs.addTab(self.category_pages[category], catalog.CATEGORIES[category])
        if equipment:
            self.tabs.addTab(self.refinement, 'Fejlesztés'); self.tabs.addTab(self.bonus_page, 'Bónuszok')
        self.stack_field.edit.setEnabled(not equipment)
        self.appearance_field.widget.setEnabled(equipment)
        self.base_form.load(document); self.use_form.load(document)
        if category == 5:
            self.category_forms[5].load(document)
            kind = int(document['recovery']['delivery_kind'])
            for value, form in self.effect_forms.items():
                form.load(dict(effect=document['effect'] if value == kind else EFFECT_DEFAULTS[value]))
            self.effects.setCurrentWidget(self.effect_forms[kind])
            cooldown = document.get('cooldown')
            self.cooldown_check.setChecked(bool(cooldown))
            self.cooldown_edit.setText(catalog.display(cooldown['duration_ms']) if cooldown else '1')
            self.cooldown_toggled()
        else:
            document = dict(document, child=dict(document['child'], socket_profile_id=document['child'].get('socket_profile_id') or ''))
            self.category_forms[category].load(document)
        if category == 8:
            loot = document.get('loot') or dict(pick_mode='1', entries=[])
            self.loot_mode.blockSignals(True); self.loot_mode.setCurrentIndex(max(0, self.loot_mode.findData(str(loot['pick_mode'])))); self.loot_mode.blockSignals(False)
            self.loot_table.load(loot['entries'], loot['pick_mode'])
            self.container_kind_changed()
        if equipment:
            self.refinement.set_category(category, self.packet['bonuses'] if self.packet else [])
            self.refinement.load(document['refinement'], document['levels'])
            self.bonuses.load(document.get('bonuses'))
        self.tabs.setCurrentIndex(min(max(current, 0), self.tabs.count() - 1))
        self.loading = False; self.update_badge(); self.controls()

    def collect(self):
        document = copy.deepcopy(self.document); category = int(document['base']['category_id'])
        self.base_form.collect(document); self.use_form.collect(document)
        if not document['appearance'].get('model_asset'): document['appearance'] = None
        if category == 5:
            self.category_forms[5].collect(document)
            kind = int(document['recovery']['delivery_kind'])
            holder = {}; self.effect_forms[kind].collect(holder)
            document['effect'] = dict(holder['effect'], delivery_kind=str(kind))
            if self.cooldown_check.isChecked():
                try: duration = catalog.scaled(self.cooldown_edit.text())
                except api.EditorError: raise api.EditorError('Várakozási idő: érvénytelen szám.') from None
                if duration < 1: raise api.EditorError('Várakozási idő: legalább 0,001 mp legyen.')
                document['cooldown'] = dict(duration_ms=str(duration))
            else:
                document['cooldown'] = None
        else:
            self.category_forms[category].collect(document)
            if category == 3: document['child']['socket_profile_id'] = document['child']['socket_profile_id'] or None
            if category == 8:
                scripted = document['child']['container_kind'] == '3'
                document['loot'] = None if scripted else dict(pick_mode=self.loot_mode.currentData(), entries=self.loot_table.entries())
        if category in (1, 2, 3):
            self.refinement.collect(document)
            document['bonuses'] = self.bonuses.entries()
        else:
            document['bonuses'] = []
        return document

    def check(self):
        try: document = self.collect()
        except api.EditorError as error: self.owner.error(str(error)); return
        packet, key = self.packet, self.original_key
        self.run(lambda: catalog.validate(packet, document, key), lambda digest: self.status.setText(
            'Ellenőrzés rendben: a teljes katalógus érvényes ezzel a tárggyal (új hash %s…). Még nincs mentve.' % digest[:12]),
            'A játék katalógusellenőrzője vizsgálja a tárgyat…')

    def save(self):
        if self.packet is None or self.document is None: return
        try: document = self.collect()
        except api.EditorError as error: self.owner.error(str(error)); return
        root, packet, key = self.owner.root, self.packet, self.original_key
        new_key = (int(document['base']['category_id']), int(document['base']['local_id']))
        def done(result):
            self.dirty = False; self.document = None; self.original_key = None
            self.loaded(result, new_key)
            self.owner.statusBar().showMessage('Tárgy mentve: %d:%d' % new_key, 6000)
        self.run(lambda: catalog.save(root, packet, document, key), done, 'Mentés a helyi protos adatbázisba…')

    def discard(self):
        if self.original_key is None:
            self.dirty = False; self.show_document(None); return
        self.show_document(catalog.Catalog(self.packet).item(self.original_key))
