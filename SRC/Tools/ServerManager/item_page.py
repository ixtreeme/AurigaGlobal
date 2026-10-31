"""Item catalog editor page for the local package's protos database.

Reads, validates and saves through item_catalog (native CatalogEditorTool).
The page never builds SQL and never edits more than one item per save.
"""
import copy
import re

from PySide6.QtCore import QThread, Qt, QTimer, Signal
from PySide6.QtGui import QStandardItem, QStandardItemModel
from PySide6.QtWidgets import (QAbstractItemView, QCheckBox, QComboBox, QDialog, QDialogButtonBox, QFormLayout,
    QGridLayout, QHBoxLayout, QHeaderView, QLabel, QLineEdit, QListWidget, QMenu, QMessageBox, QPushButton, QScrollArea,
    QSizePolicy, QSpinBox, QSplitter, QStackedWidget, QTabWidget, QTableWidget, QTableWidgetItem, QToolButton, QVBoxLayout, QWidget)

import backend as api
import item_catalog as catalog
from skill_book_editor import SkillBookPanel
from costume_editor import CostumePanel
from gathering_editor import ToolPanel, FishingPanel
from polymorph_editor import PolymorphPanel
from soul_editor import SoulPanel
from companion_editor import CompanionPanel
from pet_care_editor import PetHatchPanel,PetFoodPanel
from pet_lifecycle_editor import PetSupplyPanel
from attribute_editor import AttributePanel,AttributeServicePanel
from item_workflows import NewItemDialog, CATEGORY_HELP, SECTION_HELP, TYPE_SECTION, readable_name, summary_lines
from manager_ui import card_layout, line_icon

KEY_ROLE = int(Qt.ItemDataRole.UserRole)
BONUS_LABELS = {
    'bonus.movement_speed_percent': 'Mozgási sebesség', 'bonus.attack_speed_percent': 'Támadási sebesség',
    'bonus.cast_speed_percent': 'Varázslási sebesség', 'bonus.strength': 'Erő', 'bonus.dexterity': 'Ügyesség',
    'bonus.constitution': 'Életerő', 'bonus.intelligence': 'Intelligencia',
    'bonus.experience_percent': 'Tapasztalatbónusz', 'bonus.global_outgoing': 'Általános sebzésbónusz',
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
UNITS = {1: (1000, 'pont'), 2: (100, '%'), 3: (1, 'pont')}
RESISTANCE_LABELS = ['Fizikai', 'Mágikus', 'Kard', 'Tőr', 'Íj', 'Kétkezes', 'Csengő', 'Legyező', 'Lovas lándzsa',
                     'Tűz', 'Villám', 'Jég', 'Szél', 'Föld', 'Sötétség']
EFFECT_DEFAULTS = {
    1: dict(hp_raw='1000', sp_raw='0', max_hp_bp='0', max_sp_bp='0', flat_bonus_cap_bp='10000'),
    2: dict(hp_raw='1000', sp_raw='0', flat_bonus_cap_bp='10000', tick_interval_ms='1000', max_pool_per_tick_bp='1000'),
    3: dict(hp_per_tick_raw='100', sp_per_tick_raw='0', tick_interval_ms='1000', duration_ms='10000'),
    4: dict(pool_kind='1', supply_kind='1', capacity_raw='1000000', tick_interval_ms='1000', max_pool_per_tick_bp='700', active_max_pool_bonus_bp='0'),
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
                self.widget.setFocusProxy(self.edit)
            else:
                self.widget = self.edit
        if options.get('hint'): self.widget.setToolTip(options['hint'])

    def load(self, value):
        for control in (self.widget, getattr(self, 'edit', self.widget)):
            if control.property('invalid'):
                control.setProperty('invalid', False)
                control.style().unpolish(control); control.style().polish(control)
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
        try:
            value = self._value()
        except api.EditorError:
            self.widget.setProperty('invalid', True)
            self.widget.style().unpolish(self.widget); self.widget.style().polish(self.widget)
            if hasattr(self, 'edit') and self.edit is not self.widget:
                self.edit.setProperty('invalid', True)
                self.edit.style().unpolish(self.edit); self.edit.style().polish(self.edit)
            raise
        if self.widget.property('invalid'):
            self.widget.setProperty('invalid', False)
            self.widget.style().unpolish(self.widget); self.widget.style().polish(self.widget)
            if hasattr(self, 'edit') and self.edit is not self.widget:
                self.edit.setProperty('invalid', False)
                self.edit.style().unpolish(self.edit); self.edit.style().polish(self.edit)
        return value

    def _value(self):
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

    def section(self, title, hint=''):
        row = (self.count + self.columns - 1) // self.columns
        label = QLabel(title); label.setObjectName('SectionTitle')
        self.grid.addWidget(label, row, 0, 1, self.columns * 2)
        self.grid.setRowMinimumHeight(row, 36)
        self.count = (row + 1) * self.columns
        if hint: self.note(hint)

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
        bit = 1 << (int(self.source) - 1)
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

    `preview` updates the table behind the dialog. Only explicit acceptance
    changes the draft; Cancel, X and Escape restore the original values.
    """

    def __init__(self, parent, targets, maximum, current, preview=None, steps=None):
        super().__init__(parent); self.setWindowTitle('Fejlesztési értékek kitöltése'); self.resize(580, 360)
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
        hint = QLabel('A táblázatban élő előnézetet látsz. Az „Alkalmazás a piszkozatban” veszi át az értékeket; '
                      'a Mégse, az Esc és az ablak bezárása visszaállítja az eredeti értékeket. '
                      'Az új szintek az elfogadott növekménnyel folytatják a sort. A kezdőszint alatti szintek megmaradnak.')
        hint.setWordWrap(True); hint.setObjectName('FieldHint'); layout.addWidget(hint); layout.addStretch()
        buttons = QDialogButtonBox(QDialogButtonBox.StandardButton.Ok | QDialogButtonBox.StandardButton.Cancel)
        buttons.button(QDialogButtonBox.StandardButton.Ok).setText('Alkalmazás a piszkozatban')
        self.revert_button = buttons.button(QDialogButtonBox.StandardButton.Cancel)
        self.revert_button.setText('Mégse')
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

    def reject(self):
        self.reverted = True
        if self.on_preview: self.on_preview(None)
        super().reject()


REWARD_KINDS = [(1, 'Tárgy'), (2, 'Yang'), (3, 'Tapasztalat'), (4, 'Szörny idézése'), (5, 'Szörnycsoport idézése'),
                (6, 'Lassítás'), (7, 'HP-szívás'), (8, 'Mérgezés')]
# Display scale and unit of the amount per reward kind; drain HP is bp of max HP.
AMOUNT_UNITS = {1: (1, 'db'), 2: (1, 'Yang'), 3: (1, 'TP'), 4: (1, 'szörny vnum'), 5: (1, 'csoport-ID'), 6: (1, 'pont'),
                7: (100, '% max HP'), 8: (1, '')}


class LootTable(QWidget):
    """Rewards of one container. Weighted mode shows relative weights and the
    resulting chance; independent mode shows each row's own chance in %."""
    COLUMNS = ['Jutalom', 'Tárgy', 'Mennyiség', 'Súly', 'Ritka bónusz %', 'Esély']

    def __init__(self, changed, allowed_kinds=None):
        super().__init__(); self.changed = changed; self.mode = '1'; self.items = QStandardItemModel(self); self.allowed_kinds = allowed_kinds
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
        for value, text in REWARD_KINDS:
            if self.allowed_kinds is None or value in self.allowed_kinds: kind.addItem(text, str(value))
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


FAILURE_KINDS = [('1', 'A tárgy megmarad (szintje nem változik)'), ('2', 'A tárgy megsemmisül'), ('3', 'Visszaesik a megadott szintre')]
MAX_GOLD = 9223372036854775807
MAX_INGREDIENTS = 32


def item_model(model):
    """Combo model of every item; the 'category:local' key is the user data."""
    items = QStandardItemModel()
    for row in model.items():
        key = '%s:%s' % (row['category_id'], row['local_id'])
        entry = QStandardItem('%s  %s%s' % (key, row['content_code'], '  (kivezetve)' if row['retired'] == '1' else ''))
        entry.setData(key, Qt.ItemDataRole.UserRole); items.appendRow(entry)
    return items


def item_combo(items, key):
    combo = QComboBox(); combo.setModel(items); combo.setEditable(True); combo.setInsertPolicy(QComboBox.InsertPolicy.NoInsert)
    combo.completer().setFilterMode(Qt.MatchFlag.MatchContains); combo.completer().setCaseSensitivity(Qt.CaseSensitivity.CaseInsensitive)
    index = combo.findData(key) if key else -1
    if key and index < 0:
        missing = QStandardItem(key + '  (nem létező tárgy)'); missing.setData(key, Qt.ItemDataRole.UserRole); items.appendRow(missing)
        index = combo.findData(key)
    combo.setCurrentIndex(index)
    return combo


def chosen_key(combo):
    """Key of a searchable combo; typed text that names no item is no choice."""
    index = combo.currentIndex()
    if index < 0 or combo.currentText() != combo.itemText(index): return None
    return tuple(int(part) for part in combo.currentData().split(':'))


def recipe_text(recipe):
    if not recipe: return 'Nincs recept — megadás…'
    parts = ['%s Yang' % format(int(recipe['gold_cost']), ',').replace(',', ' '), catalog.display(recipe['success_chance_bp'], 100) + '%']
    count = len(recipe.get('ingredients') or [])
    if count: parts.append('%d hozzávaló' % count)
    if int(recipe['success_chance_bp']) < 10000:
        parts.append({'1': 'kudarcnál marad', '2': 'kudarcnál megsemmisül', '3': 'kudarcnál +%s' % recipe.get('failure_level')}[str(recipe['failure_kind'])])
    return ' · '.join(parts)


def moved_recipe(recipe, level):
    """A copy of a chain recipe for target level `level`. A fall-back level
    keeps its distance from the source level; at +0 there is nowhere to fall."""
    result = copy.deepcopy(recipe); result.update(recipe_id=None, code=None, target_category_id=None, target_local_id=None,
                                                  source_level=str(level - 1), target_level=str(level))
    if str(result['failure_kind']) == '3':
        distance = max(int(recipe['source_level']) - int(recipe['failure_level']), 1)
        if level - 1 - distance >= 0: result['failure_level'] = str(level - 1 - distance)
        else: result.update(failure_kind='1', failure_level=None)
    return result


class RecipeDialog(QDialog):
    """Conditions of one refinement recipe: Yang, chance, failure rule and
    ingredients. With `source_max` it also picks the source level and a target
    item (transformation); otherwise the source level is fixed."""

    def __init__(self, parent, title, recipe, items, levels_of, source_level=None, source_max=None, self_key=None):
        super().__init__(parent); self.setWindowTitle(title); self.resize(700, 540)
        self.items, self.levels_of, self.self_key = items, levels_of, self_key
        self.transform = source_max is not None; self.fixed_source = source_level
        self.original = copy.deepcopy(recipe) if recipe else None
        recipe = copy.deepcopy(recipe) if recipe else dict(gold_cost='0', success_chance_bp='10000', failure_kind='1', failure_level=None, ingredients=[])
        self.result_recipe = None; self.deleted = False
        layout = QVBoxLayout(self); form = QFormLayout()
        if self.transform:
            self.source = QSpinBox(); self.source.setRange(0, source_max); self.source.setPrefix('+')
            self.source.setValue(min(int(recipe.get('source_level') or 0), source_max))
            target = '%s:%s' % (recipe['target_category_id'], recipe['target_local_id']) if recipe.get('target_category_id') else None
            self.target = item_combo(items, target); self.target_level = QSpinBox(); self.target_level.setPrefix('+')
            form.addRow('Forrásszint', self.source); form.addRow('Céltárgy', self.target); form.addRow('Célszint', self.target_level)
            self.target.currentIndexChanged.connect(self.target_changed); self.target_changed()
            self.target_level.setValue(int(recipe.get('target_level') or 0))
        self.gold = QLineEdit(str(recipe.get('gold_cost') or '0'))
        self.chance = QLineEdit(catalog.display(recipe.get('success_chance_bp') or '0', 100))
        self.failure = QComboBox()
        for value, label in FAILURE_KINDS: self.failure.addItem(label, value)
        self.failure.setCurrentIndex(max(0, self.failure.findData(str(recipe.get('failure_kind') or '1'))))
        self.fall = QSpinBox(); self.fall.setPrefix('+')
        form.addRow('Yang-díj', self.gold); form.addRow('Sikeresély (%)', self.chance)
        form.addRow('Sikertelenség esetén', self.failure); form.addRow('Visszaesés szintje', self.fall)
        layout.addLayout(form)
        self.failure.currentIndexChanged.connect(self.failure_changed)
        if self.transform: self.source.valueChanged.connect(self.failure_changed)
        self.failure_changed()
        if recipe.get('failure_level') is not None: self.fall.setValue(int(recipe['failure_level']))
        layout.addWidget(QLabel('Hozzávalók:'))
        self.table = QTableWidget(0, 3); self.table.setHorizontalHeaderLabels(['Tárgy', 'Szint', 'Darab'])
        self.table.horizontalHeader().setSectionResizeMode(0, QHeaderView.ResizeMode.Stretch)
        self.table.setColumnWidth(1, 80); self.table.setColumnWidth(2, 110)
        self.table.verticalHeader().hide(); self.table.setSelectionBehavior(QAbstractItemView.SelectionBehavior.SelectRows)
        layout.addWidget(self.table, 1)
        for ingredient in recipe.get('ingredients') or []: self.append(ingredient)
        row = QHBoxLayout(); add, remove = QPushButton('Hozzávaló hozzáadása'), QPushButton('Kijelölt törlése')
        add.clicked.connect(lambda: self.append(None)); remove.clicked.connect(self.remove)
        row.addWidget(add); row.addWidget(remove); row.addStretch(); layout.addLayout(row)
        hint = QLabel('A Yang-díj és a hozzávalók sikertelenség esetén is elfogynak. 100%-os esélynél a fejlesztés mindig sikerül, '
                      '0%-nál soha. A visszaesési szint a forrásszintnél kisebb legyen.')
        hint.setWordWrap(True); hint.setObjectName('FieldHint'); layout.addWidget(hint)
        buttons = QDialogButtonBox(QDialogButtonBox.StandardButton.Ok | QDialogButtonBox.StandardButton.Cancel)
        buttons.button(QDialogButtonBox.StandardButton.Ok).setText('Alkalmaz')
        buttons.button(QDialogButtonBox.StandardButton.Cancel).setText('Mégse')
        if self.original:
            delete = buttons.addButton('Recept törlése', QDialogButtonBox.ButtonRole.DestructiveRole)
            delete.clicked.connect(self.delete)
        buttons.accepted.connect(self.accept_recipe); buttons.rejected.connect(self.reject); layout.addWidget(buttons)

    def source_level(self):
        return self.source.value() if self.transform else self.fixed_source

    def target_changed(self, *_):
        key = chosen_key(self.target)
        top = self.levels_of(key) if key else None
        self.target_level.setRange(0, top or 0); self.target_level.setEnabled(top is not None)

    def failure_changed(self, *_):
        source = self.source_level()
        self.fall.setRange(0, max(source - 1, 0)); self.fall.setEnabled(self.failure.currentData() == '3' and source > 0)

    def append(self, ingredient):
        row = self.table.rowCount(); self.table.insertRow(row)
        key = '%s:%s' % (ingredient['category_id'], ingredient['local_id']) if ingredient else None
        combo = item_combo(self.items, key); level = QSpinBox(); level.setPrefix('+'); amount = QSpinBox(); amount.setRange(1, 2 ** 31 - 1)
        def limit(*_):
            chosen = chosen_key(combo); level.setRange(0, (self.levels_of(chosen) or 0) if chosen else 0)
        limit(); combo.currentIndexChanged.connect(limit)
        if ingredient: level.setValue(int(ingredient['refinement_level'])); amount.setValue(int(ingredient['amount']))
        for column, widget in enumerate((combo, level, amount)): self.table.setCellWidget(row, column, widget)

    def remove(self):
        for row in sorted({index.row() for index in self.table.selectedIndexes()}, reverse=True): self.table.removeRow(row)

    def delete(self):
        self.deleted = True; self.accept()

    def collect(self):
        try: gold = int(self.gold.text().strip().replace(' ', ''))
        except ValueError: raise api.EditorError('A Yang-díj egész szám legyen.') from None
        if not 0 <= gold <= MAX_GOLD: raise api.EditorError('A Yang-díj kívül esik a megengedett tartományon.')
        chance = catalog.scaled(self.chance.text(), 100)
        if not 0 <= chance <= 10000: raise api.EditorError('A sikeresély 0 és 100% között legyen.')
        failure = self.failure.currentData(); source = self.source_level()
        if failure == '3' and source == 0: raise api.EditorError('A +0 szintről nincs hová visszaesni; válassz másik kudarcszabályt.')
        recipe = dict(recipe_id=(self.original or {}).get('recipe_id'), code=(self.original or {}).get('code'),
                      gold_cost=str(gold), success_chance_bp=str(chance), failure_kind=failure,
                      failure_level=str(self.fall.value()) if failure == '3' else None, ingredients=[])
        if self.transform:
            target = chosen_key(self.target)
            if target is None: raise api.EditorError('Válaszd ki a céltárgyat.')
            if self.levels_of(target) is None: raise api.EditorError('A céltárgy nem létezik.')
            if target == tuple(self.self_key or ()) and self.target_level.value() != source + 1:
                raise api.EditorError('Ugyanazon a tárgyon csak a következő szintre lehet fejleszteni.')
            recipe.update(source_level=str(source), target_category_id=str(target[0]), target_local_id=str(target[1]),
                          target_level=str(self.target_level.value()))
        seen = set()
        if self.table.rowCount() > MAX_INGREDIENTS: raise api.EditorError('Legfeljebb %d hozzávaló adható meg.' % MAX_INGREDIENTS)
        for row in range(self.table.rowCount()):
            key = chosen_key(self.table.cellWidget(row, 0))
            if key is None: raise api.EditorError('%d. hozzávaló: válassz tárgyat a listából.' % (row + 1))
            level = self.table.cellWidget(row, 1).value()
            if (key, level) in seen: raise api.EditorError('%d. hozzávaló: ugyanaz a tárgy és szint kétszer szerepel.' % (row + 1))
            seen.add((key, level))
            recipe['ingredients'].append(dict(category_id=str(key[0]), local_id=str(key[1]), refinement_level=str(level),
                                              amount=str(self.table.cellWidget(row, 2).value())))
        return recipe

    def accept_recipe(self):
        try: self.result_recipe = self.collect()
        except api.EditorError as error:
            QMessageBox.warning(self, 'Fejlesztési recept', str(error)); return
        self.accept()


class TransformDialog(QDialog):
    """Recipes that turn this item into another item (or another kind of
    same-item recipe). The list is applied to the panel on Alkalmaz."""

    def __init__(self, parent, recipes, items, levels_of, source_max, self_key):
        super().__init__(parent); self.setWindowTitle('Átalakítások'); self.resize(760, 380)
        self.recipes = copy.deepcopy(recipes); self.items, self.levels_of, self.source_max, self.self_key = items, levels_of, source_max, self_key
        self.result_recipes = None
        layout = QVBoxLayout(self)
        hint = QLabel('Átalakítás: a tárgypéldány egy megadott szintről másik tárggyá válik (pl. kard +9 → hosszú kard +0). '
                      'A példány azonosítója, bónuszai és kövei megmaradnak.')
        hint.setWordWrap(True); hint.setObjectName('Subtle'); layout.addWidget(hint)
        self.table = QTableWidget(0, 4); self.table.setHorizontalHeaderLabels(['Forrásszint', 'Céltárgy', 'Célszint', 'Feltételek'])
        self.table.horizontalHeader().setSectionResizeMode(1, QHeaderView.ResizeMode.Stretch)
        self.table.horizontalHeader().setSectionResizeMode(3, QHeaderView.ResizeMode.Stretch)
        self.table.verticalHeader().hide(); self.table.setSelectionBehavior(QAbstractItemView.SelectionBehavior.SelectRows)
        self.table.setEditTriggers(QAbstractItemView.EditTrigger.NoEditTriggers)
        self.table.cellDoubleClicked.connect(lambda row, _: self.edit(row))
        layout.addWidget(self.table, 1)
        row = QHBoxLayout(); add, edit, remove = QPushButton('Új átalakítás…'), QPushButton('Szerkesztés…'), QPushButton('Kijelölt törlése')
        add.clicked.connect(lambda: self.edit(None)); edit.clicked.connect(self.edit_selected)
        remove.clicked.connect(self.remove)
        for button in (add, edit, remove): row.addWidget(button)
        row.addStretch(); layout.addLayout(row)
        buttons = QDialogButtonBox(QDialogButtonBox.StandardButton.Ok | QDialogButtonBox.StandardButton.Cancel)
        buttons.button(QDialogButtonBox.StandardButton.Ok).setText('Alkalmaz')
        buttons.button(QDialogButtonBox.StandardButton.Cancel).setText('Mégse')
        buttons.accepted.connect(self.accept_list); buttons.rejected.connect(self.reject); layout.addWidget(buttons)
        self.render()

    def label(self, recipe):
        if recipe.get('target_category_id') is None: return 'ugyanez a tárgy'
        key = '%s:%s' % (recipe['target_category_id'], recipe['target_local_id'])
        for row in range(self.items.rowCount()):
            if self.items.item(row).data(Qt.ItemDataRole.UserRole) == key: return self.items.item(row).text()
        return key + '  (nem létező tárgy)'

    def render(self):
        self.table.setRowCount(len(self.recipes))
        for row, recipe in enumerate(self.recipes):
            for column, value in enumerate(('+' + str(recipe['source_level']), self.label(recipe), '+' + str(recipe['target_level']), recipe_text(recipe))):
                self.table.setItem(row, column, QTableWidgetItem(value))

    def current(self):
        rows = {index.row() for index in self.table.selectedIndexes()}
        return min(rows) if rows else None

    def edit_selected(self):
        index = self.current()
        if index is not None: self.edit(index)

    def edit(self, index):
        recipe = self.recipes[index] if index is not None else None
        if recipe is not None and recipe.get('target_category_id') is None:
            recipe = dict(recipe, target_category_id=str(self.self_key[0]), target_local_id=str(self.self_key[1]))
        dialog = RecipeDialog(self, 'Átalakítás', recipe, self.items, self.levels_of, source_max=self.source_max, self_key=self.self_key)
        if dialog.exec() != QDialog.DialogCode.Accepted: return
        if dialog.deleted: self.recipes.pop(index)
        elif dialog.result_recipe is not None:
            if index is None: self.recipes.append(dialog.result_recipe)
            else: self.recipes[index] = dialog.result_recipe
        self.render()

    def remove(self):
        for row in sorted({index.row() for index in self.table.selectedIndexes()}, reverse=True): self.recipes.pop(row)
        self.render()

    def accept_list(self):
        self.result_recipes = self.recipes; self.accept()


class RefinementPanel(QWidget):
    """Kind, maximum and the full cumulative +0..+max curve of one item.

    Stat gains live in the table cells until collect(); level bonuses live in
    self.levels. Rows are added/removed in place so typed values survive.
    """
    STAT_COLUMNS = {1: [('physical_attack_gain_raw', 'Fizikai támadás +'), ('magical_attack_gain_raw', 'Mágikus támadás +')],
                    2: [('physical_defense_gain_raw', 'Fizikai védekezés +'), ('magical_defense_gain_raw', 'Mágikus védekezés +')],
                    3: [], 4: [], 7: [], 11: [], 15: []}

    def __init__(self, changed):
        super().__init__(); self.changed = changed; self.category = 1; self.levels = []; self.definitions = []; self.loading = False
        self.rules = {}
        # Chain recipe per target level (+N row: +N-1 -> +N) and every other
        # recipe of this item (transformations); see load()/collect().
        self.recipes = {}; self.extra = []; self.items = QStandardItemModel(); self.levels_of = lambda key: None; self.self_key = None
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
        self.recipe_box = QWidget(); bottom = QHBoxLayout(self.recipe_box); bottom.setContentsMargins(0, 0, 0, 0)
        self.recipe_hint = QLabel('A „Recept” oszlop a +N szint elérésének feltétele (+N-1 → +N): Yang, esély, kudarcszabály, '
                                  'hozzávalók. Recept nélkül a szint fejlesztéssel nem érhető el.')
        self.recipe_hint.setWordWrap(True); self.recipe_hint.setObjectName('FieldHint')
        self.transform_button = QPushButton('Átalakítások…'); self.transform_button.clicked.connect(self.edit_transforms)
        bottom.addWidget(self.recipe_hint, 1); bottom.addWidget(self.transform_button)
        layout.addWidget(self.recipe_box)

    def stats(self):
        return self.STAT_COLUMNS[self.category]

    def with_recipes(self):
        return self.category in catalog.RECIPE_CATEGORIES

    def recipe_column(self):
        return len(self.stats()) + 2 if self.with_recipes() else None

    def set_catalog(self, model, key):
        """Item choices and level ranges for recipes, from the loaded catalog."""
        self.items = item_model(model); self.levels_of = model.target_levels; self.self_key = tuple(key)

    def set_category(self, category, definitions):
        self.category = category; self.definitions = definitions
        self.kind.blockSignals(True); self.kind.clear()
        self.kind.addItem('Nincs fejlesztés', '1')
        self.kind.addItem({1: 'Fegyverfejlesztés', 2: 'Páncélfejlesztés', 3: 'Ékszerfejlesztés', 4: 'Kosztümfejlesztés', 7: 'Foglalatkő-fejlesztés', 11: 'Eszközfejlesztés', 15: 'Társhívó fejlesztése'}[category], str(catalog.REFINEMENT_KIND[category]))
        self.kind.blockSignals(False)

    def load(self, refinement, levels, recipes=None):
        self.loading = True
        self.rules = {}  # step per ('stat', column) / ('bonus', id) / ('recipe', field), from the fill dialog
        self.kind.setCurrentIndex(max(0, self.kind.findData(str(refinement['profile_kind']))))
        self.levels = [copy.deepcopy(level) for level in levels]
        self.recipes, self.extra = {}, []
        for recipe in copy.deepcopy(recipes or []):
            level = int(recipe['target_level'])
            chain = recipe.get('target_category_id') is None and level == int(recipe['source_level']) + 1
            if chain and level not in self.recipes: self.recipes[level] = recipe
            else: self.extra.append(recipe)
        self.recipe_box.setVisible(self.with_recipes()); self.update_transforms()
        self.maximum.blockSignals(True); self.maximum.setValue(int(refinement['max_level'])); self.maximum.blockSignals(False)
        self.render()
        self.kind_changed()
        self.loading = False

    def kind_changed(self, *_):
        none = self.kind.currentData() == '1'
        if none and self.maximum.value():
            self.maximum.setValue(0)
        self.maximum.setEnabled(not none); self.step_button.setEnabled(not none); self.transform_button.setEnabled(not none)
        if not self.loading: self.changed()

    def render(self):
        stats = self.stats()
        self.table.blockSignals(True)
        self.table.clear(); self.table.setColumnCount(2 + len(stats) + (1 if self.with_recipes() else 0)); self.table.setRowCount(0)
        self.table.setHorizontalHeaderLabels(['Szint', *[label + ' (pont)' for _, label in stats], 'Szintbónuszok',
                                              *(['Recept (előző szintről)'] if self.with_recipes() else [])])
        self.table.horizontalHeader().setSectionResizeMode(QHeaderView.ResizeMode.Stretch)
        self.table.horizontalHeader().setSectionResizeMode(0, QHeaderView.ResizeMode.ResizeToContents)
        if self.with_recipes():  # the recipe summary is long; never cut it
            self.table.horizontalHeader().setSectionResizeMode(self.recipe_column(), QHeaderView.ResizeMode.ResizeToContents)
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
        if self.with_recipes():
            recipe = QPushButton(); recipe.setEnabled(row > 0)
            recipe.clicked.connect(lambda _=False, index=row: self.edit_recipe(index))
            self.table.setCellWidget(row, self.recipe_column(), recipe)
            self.update_recipe(row)

    def update_recipe(self, row):
        column = self.recipe_column()
        if column is None or self.table.cellWidget(row, column) is None: return
        button = self.table.cellWidget(row, column); recipe = self.recipes.get(row)
        button.setText('—' if row == 0 else recipe_text(recipe))
        names = []
        for ingredient in (recipe or {}).get('ingredients') or []:
            key = '%s:%s' % (ingredient['category_id'], ingredient['local_id'])
            label = next((self.items.item(i).text() for i in range(self.items.rowCount()) if self.items.item(i).data(Qt.ItemDataRole.UserRole) == key), key)
            names.append('%s +%s ×%s' % (label, ingredient['refinement_level'], ingredient['amount']))
        button.setToolTip('\n'.join([recipe_text(recipe), *names]) if row and recipe else '')

    def edit_recipe(self, index):
        dialog = RecipeDialog(self, 'Recept: +%d → +%d' % (index - 1, index), self.recipes.get(index), self.items, self.levels_of,
                              source_level=index - 1, self_key=self.self_key)
        if dialog.exec() != QDialog.DialogCode.Accepted: return
        if dialog.deleted: self.recipes.pop(index, None)
        elif dialog.result_recipe is not None:
            self.recipes[index] = dict(dialog.result_recipe, source_level=str(index - 1), target_level=str(index),
                                       target_category_id=None, target_local_id=None)
        else: return
        self.update_recipe(index); self.changed()

    def update_transforms(self):
        self.transform_button.setText('Átalakítások… (%d)' % len(self.extra) if self.extra else 'Átalakítások…')

    def edit_transforms(self):
        dialog = TransformDialog(self, self.extra, self.items, self.levels_of, self.maximum.value(), self.self_key)
        if dialog.exec() == QDialog.DialogCode.Accepted and dialog.result_recipes is not None:
            self.extra = dialog.result_recipes; self.update_transforms(); self.changed()

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
            self.recipes.pop(len(self.levels) - 1, None)
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
            if self.with_recipes() and len(self.levels) >= 2: self.continue_recipe(len(self.levels))
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

    def continue_recipe(self, level):
        """The new top level's recipe continues the one below it: same
        ingredients and failure distance, Yang and chance advanced by step."""
        top = self.recipes.get(level - 1)
        if top is None: return
        below = self.recipes.get(level - 2) if level >= 3 else None
        recipe = moved_recipe(top, level)
        for field, high in (('gold_cost', MAX_GOLD), ('success_chance_bp', 10000)):
            value = int(top[field]); step = self.step_for(('recipe', field), value, int(below[field]) if below else None)
            recipe[field] = str(min(max(value + step, 0), high))
        self.recipes[level] = recipe

    @staticmethod
    def rule_key(target):
        if target['kind'] == 'recipe': return ('recipe', target['field'])
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
        recipes = [dict(label='Recept: Yang-díj', kind='recipe', field='gold_cost', scale=1, unit='Yang', range=(0, MAX_GOLD)),
                   dict(label='Recept: sikeresély', kind='recipe', field='success_chance_bp', scale=100, unit='%', range=(0, 10000))]
        # Bonuses first: they are what a filled curve most often needs.
        return ([t for t in targets if t['kind'] == 'bonus'] + [t for t in targets if t['kind'] == 'stat'] +
                (recipes if self.with_recipes() else []))

    def snapshot(self):
        columns = range(1, len(self.stats()) + 1)
        return dict(levels=copy.deepcopy(self.levels), recipes=copy.deepcopy(self.recipes),
                    texts=[[self.table.item(row, column).text() for column in columns] for row in range(len(self.levels))])

    def restore(self, state):
        self.levels = copy.deepcopy(state['levels']); self.recipes = copy.deepcopy(state['recipes'])
        self.table.blockSignals(True)
        for row, texts in enumerate(state['texts']):
            for column, text in enumerate(texts, 1): self.table.item(row, column).setText(text)
            self.update_button(row); self.update_recipe(row)
        self.table.blockSignals(False)

    def current_value(self, target, level, state=None):
        state = state or self.snapshot()
        if target['kind'] == 'recipe':
            recipe = state['recipes'].get(level)
            return int(recipe[target['field']]) if recipe else None
        if target['kind'] == 'stat':
            try: return catalog.scaled(state['texts'][level][target['column'] - 1])
            except api.EditorError: return None
        entry = next((e for e in state['levels'][level].get('bonuses') or [] if str(e['bonus_id']) == target['id']), None)
        return int(entry['value_scaled']) if entry else None

    def recipe_for(self, level):
        """Existing chain recipe, else a copy of the nearest one (below
        first), else a free, always successful recipe without ingredients."""
        if level in self.recipes: return self.recipes[level]
        near = sorted(self.recipes, key=lambda other: (other > level, abs(other - level)))
        self.recipes[level] = (moved_recipe(self.recipes[near[0]], level) if near else
                               dict(recipe_id=None, code=None, source_level=str(level - 1), target_level=str(level), target_category_id=None,
                                    target_local_id=None, gold_cost='0', success_chance_bp='10000', failure_kind='1', failure_level=None, ingredients=[]))
        return self.recipes[level]

    def apply_fill(self, target, values, notify=True):
        self.table.blockSignals(True)
        for level, value in values.items():
            if target['kind'] == 'recipe':
                self.recipe_for(level)[target['field']] = str(value); self.update_recipe(level)
                continue
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
        accepted = dialog.exec() == QDialog.DialogCode.Accepted
        # An accepted step is remembered even if only +1 exists yet.
        self.restore(state)
        if accepted and not dialog.reverted and dialog.last is not None:
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
        recipes = []
        if self.with_recipes() and self.kind.currentData() != '1':
            for level in range(1, len(self.levels)):
                if level in self.recipes:
                    recipes.append(dict(copy.deepcopy(self.recipes[level]), source_level=str(level - 1), target_level=str(level),
                                        target_category_id=None, target_local_id=None))
            for recipe in self.extra:
                if int(recipe['source_level']) > self.maximum.value():
                    raise api.EditorError('Átalakítás a +%s szintről: a tárgy legmagasabb szintje most +%d. Módosítsd vagy töröld '
                                          'az átalakítást.' % (recipe['source_level'], self.maximum.value()))
                recipes.append(copy.deepcopy(recipe))
        document['recipes'] = recipes


class ItemPage(QWidget):
    def __init__(self, owner):
        super().__init__(); self.owner = owner
        self.packet = self.document = self.original_key = self.task = None
        self.dirty = False; self.target = None; self.attempted = None; self.loading = False
        self.checked_document = None
        layout = QVBoxLayout(self); layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(12)
        bar = QHBoxLayout()
        purpose = QLabel('Tárgytípusok keresése, készítése és szerkesztése. Konkrét példányok kipróbálása: Tárgypróbapad.')
        purpose.setWordWrap(True); purpose.setObjectName('Subtle'); bar.addWidget(purpose, 1)
        self.start_db_button = QPushButton('Helyi adatbázis indítása'); self.start_db_button.clicked.connect(self.start_database)
        self.reload_button = QPushButton('Katalógus újraolvasása'); self.reload_button.clicked.connect(self.reload)
        self.reload_button.setIcon(line_icon('refresh'))
        self.new_button = QPushButton('Új tárgytípus…'); self.new_button.setObjectName('Primary')
        self.new_button.clicked.connect(self.new_item_wizard)
        for widget in (self.start_db_button, self.reload_button, self.new_button): bar.addWidget(widget)
        layout.addLayout(bar)
        self.status = QLabel('Válassz helyi szervercsomagot.'); self.status.setWordWrap(True); self.status.setObjectName('Subtle')
        self.status.setTextFormat(Qt.TextFormat.PlainText)
        layout.addWidget(self.status)
        self.workspace = QStackedWidget(); layout.addWidget(self.workspace, 1)
        self.catalog_page, side = card_layout()
        browser_title = QHBoxLayout()
        title = QLabel('Katalógus böngészése'); title.setObjectName('SectionTitle'); browser_title.addWidget(title)
        self.result_count = QLabel(''); self.result_count.setObjectName('Subtle'); browser_title.addWidget(self.result_count)
        browser_title.addStretch()
        self.resume_button = QPushButton('Szerkesztés folytatása'); self.resume_button.clicked.connect(self.resume_editing)
        browser_title.addWidget(self.resume_button); side.addLayout(browser_title)
        filters = QHBoxLayout()
        self.category_filter = QComboBox(); self.category_filter.addItem('Minden kategória', 0)
        for category, name in catalog.CATEGORIES.items(): self.category_filter.addItem(name, category)
        self.category_filter.setAccessibleName('Tárgykategória szűrése')
        self.search = QLineEdit(); self.search.setPlaceholderText('Keresés elnevezésre, tartalomkódra vagy azonosítóra…')
        self.search.setAccessibleName('Keresés a tárgykatalógusban')
        self.clear_filter_button = QPushButton('Szűrők törlése'); self.clear_filter_button.clicked.connect(self.clear_filters)
        filters.addWidget(self.search, 1); filters.addWidget(self.category_filter); filters.addWidget(self.clear_filter_button)
        side.addLayout(filters)
        filter_note = QHBoxLayout()
        self.show_retired = QCheckBox('Kivezetett tárgytípusok megjelenítése'); filter_note.addWidget(self.show_retired)
        filter_note.addStretch()
        name_note = QLabel('Az elnevezés a belső kódból készül; a játékbeli fordítás külön adat.')
        name_note.setObjectName('FieldHint'); name_note.setWordWrap(True); filter_note.addWidget(name_note)
        side.addLayout(filter_note)
        self.list = QTableWidget(0, 6)
        self.list.setHorizontalHeaderLabels(['Elnevezés', 'Kategória', 'Azonosító', 'Szinttől', 'Állapot', 'Tartalomkód'])
        self.list.horizontalHeader().setSectionResizeMode(0, QHeaderView.ResizeMode.Stretch)
        self.list.horizontalHeader().setSectionResizeMode(5, QHeaderView.ResizeMode.Stretch)
        for column in (1, 2, 3, 4): self.list.horizontalHeader().setSectionResizeMode(column, QHeaderView.ResizeMode.ResizeToContents)
        self.list.verticalHeader().hide(); self.list.setEditTriggers(QAbstractItemView.EditTrigger.NoEditTriggers)
        self.list.verticalHeader().setDefaultSectionSize(40)
        self.list.setAlternatingRowColors(True); self.list.setShowGrid(False)
        self.list.setSelectionBehavior(QAbstractItemView.SelectionBehavior.SelectRows)
        self.list.setSelectionMode(QAbstractItemView.SelectionMode.SingleSelection)
        self.list.itemSelectionChanged.connect(self.selected)
        self.list.itemDoubleClicked.connect(lambda *_: self.edit_selected())
        side.addWidget(self.list, 1)
        self.empty_list = QLabel('A katalógus betöltése után itt jelennek meg a tárgytípusok.')
        self.empty_list.setObjectName('Subtle'); self.empty_list.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.empty_list.setWordWrap(True); side.addWidget(self.empty_list)
        selection = QHBoxLayout()
        self.selection_description = QLabel('Jelölj ki egy tárgytípust a részletekhez. Dupla kattintás: szerkesztés.')
        self.selection_description.setWordWrap(True); self.selection_description.setObjectName('Subtle')
        selection.addWidget(self.selection_description, 1)
        self.copy_button = QPushButton('Új tárgy ez alapján…'); self.copy_button.clicked.connect(self.duplicate_wizard)
        self.edit_button = QPushButton('Szerkesztés megnyitása'); self.edit_button.setObjectName('Primary')
        self.edit_button.clicked.connect(self.edit_selected)
        selection.addWidget(self.copy_button); selection.addWidget(self.edit_button); side.addLayout(selection)
        for signal in (self.category_filter.currentIndexChanged, self.search.textChanged, self.show_retired.toggled):
            signal.connect(lambda *_: self.fill_list())
        self.workspace.addWidget(self.catalog_page)
        self.editor_page = QWidget(); body = QVBoxLayout(self.editor_page); body.setContentsMargins(0, 0, 0, 0)
        head = QHBoxLayout()
        self.back_button = QPushButton('← Katalógus'); self.back_button.clicked.connect(self.back_to_catalog); head.addWidget(self.back_button)
        self.heading = QLabel('Nincs megnyitott tárgytípus'); self.heading.setObjectName('SectionTitle')
        self.heading.setWordWrap(True); self.heading.setTextFormat(Qt.TextFormat.PlainText)
        self.badge = QLabel(''); self.badge.setObjectName('SavedBadge')
        self.editor_copy_button = QPushButton('Új tárgy ez alapján…'); self.editor_copy_button.clicked.connect(self.duplicate_wizard)
        head.addWidget(self.heading, 1); head.addWidget(self.badge); head.addWidget(self.editor_copy_button); body.addLayout(head)
        self.identity_line = QLabel(); self.identity_line.setTextFormat(Qt.TextFormat.PlainText)
        self.identity_line.setObjectName('Subtle'); self.identity_line.setWordWrap(True); body.addWidget(self.identity_line)
        self.split = QSplitter()
        self.section_nav = QListWidget(); self.section_nav.setObjectName('ItemSections'); self.section_nav.setMinimumWidth(185)
        self.section_nav.setMaximumWidth(230); self.section_nav.setAccessibleName('Tárgyszerkesztő szakaszai')
        self.split.addWidget(self.section_nav)
        detail, detail_layout = card_layout()
        self.section_title = QLabel(); self.section_title.setObjectName('SectionTitle'); detail_layout.addWidget(self.section_title)
        self.section_hint = QLabel(); self.section_hint.setWordWrap(True); self.section_hint.setObjectName('Subtle'); detail_layout.addWidget(self.section_hint)
        self.tabs = QTabWidget(); self.tabs.setObjectName('ItemEditorTabs'); self.tabs.tabBar().hide(); detail_layout.addWidget(self.tabs, 1)
        self.split.addWidget(detail); self.split.setStretchFactor(1, 1); self.split.setSizes([205, 850])
        body.addWidget(self.split, 1)
        self.build_forms()
        self.overview_page = self.build_item_overview()
        self.review_page = self.build_item_review()
        self.section_nav.currentRowChanged.connect(self.tabs.setCurrentIndex)
        self.tabs.currentChanged.connect(self.section_changed)
        save_card, save_layout = card_layout()
        self.inline_error = QLabel(); self.inline_error.setWordWrap(True); self.inline_error.setObjectName('InlineError'); self.inline_error.hide()
        save_layout.addWidget(self.inline_error)
        actions = QHBoxLayout()
        self.check_button = QPushButton('Tárgy ellenőrzése'); self.check_button.clicked.connect(self.check)
        self.save_button = QPushButton('Változtatások mentése'); self.save_button.setObjectName('Primary'); self.save_button.clicked.connect(self.save)
        self.discard_button = QPushButton('Módosítások visszaállítása'); self.discard_button.clicked.connect(self.discard_requested)
        self.save_note = QLabel(''); self.save_note.setObjectName('Subtle'); self.save_note.setWordWrap(True)
        actions.addWidget(self.save_note, 1)
        for widget in (self.discard_button, self.check_button, self.save_button): actions.addWidget(widget)
        save_layout.addLayout(actions); body.addWidget(save_card)
        self.workspace.addWidget(self.editor_page)
        self.preview_timer = QTimer(self); self.preview_timer.setSingleShot(True); self.preview_timer.setInterval(180)
        self.preview_timer.timeout.connect(self.refresh_item_overview)
        self.show_document(None)

    def build_item_overview(self):
        page = QWidget(); layout = QVBoxLayout(page); layout.setSpacing(18)
        self.overview_title = QLabel(); self.overview_title.setObjectName('Title')
        self.overview_title.setTextFormat(Qt.TextFormat.PlainText); self.overview_title.setWordWrap(True)
        layout.addWidget(self.overview_title)
        self.overview_details = QLabel(); self.overview_details.setWordWrap(True)
        self.overview_details.setTextFormat(Qt.TextFormat.PlainText)
        self.overview_details.setTextInteractionFlags(Qt.TextInteractionFlag.TextSelectableByMouse)
        layout.addWidget(self.overview_details)
        label = QLabel('Mit szeretnél beállítani?'); label.setObjectName('SectionTitle'); layout.addWidget(label)
        self.overview_links = QVBoxLayout(); self.overview_links.setSpacing(8); layout.addLayout(self.overview_links)
        help_text = QLabel('Ez a nézet egy tárgytípus szabályait mutatja. Egy játékosnál lévő konkrét példány darabszáma, '
                           'fejlesztési szintje és véletlen bónuszai a Tárgypróbapadon vizsgálhatók.')
        help_text.setObjectName('FieldHint'); help_text.setWordWrap(True); layout.addWidget(help_text)
        layout.addStretch()
        return scroll(page)

    def build_item_review(self):
        page = QWidget(); layout = QVBoxLayout(page)
        self.review_summary = QLabel(); self.review_summary.setWordWrap(True); layout.addWidget(self.review_summary)
        self.changes_table = QTableWidget(0, 3)
        self.changes_table.setHorizontalHeaderLabels(['Változó beállítás', 'Mentett érték', 'Új érték'])
        self.changes_table.horizontalHeader().setSectionResizeMode(QHeaderView.ResizeMode.Stretch)
        self.changes_table.verticalHeader().hide(); self.changes_table.setEditTriggers(QAbstractItemView.EditTrigger.NoEditTriggers)
        self.changes_table.setAlternatingRowColors(True); self.changes_table.setWordWrap(True)
        layout.addWidget(self.changes_table, 1)
        note = QLabel('A mentés egyetlen tárgytípust módosít a helyi katalógusban. A teljes katalógust a játék saját '
                      'ellenőrzője vizsgálja. A többi tárgy által is használt profilok védettek maradnak.')
        note.setObjectName('FieldHint'); note.setWordWrap(True); layout.addWidget(note)
        return page

    def chosen_key(self):
        items = self.list.selectedItems()
        return tuple(items[0].data(KEY_ROLE)) if items else None

    def clear_filters(self):
        for widget in (self.search, self.category_filter, self.show_retired): widget.blockSignals(True)
        self.search.clear(); self.category_filter.setCurrentIndex(0); self.show_retired.setChecked(False)
        for widget in (self.search, self.category_filter, self.show_retired): widget.blockSignals(False)
        self.fill_list()

    def back_to_catalog(self):
        self.workspace.setCurrentWidget(self.catalog_page)
        self.fill_list(); self.controls()

    def resume_editing(self):
        if self.document:
            self.workspace.setCurrentWidget(self.editor_page)
            self.refresh_item_overview()

    def edit_selected(self):
        key = self.chosen_key()
        if key is None: return
        if self.document is not None and self.original_key == key:
            self.resume_editing(); return
        if self.may_discard(): self.open_item(key)

    def new_item_wizard(self):
        if self.packet is None: return
        dialog = NewItemDialog(self, self.packet, self.category_filter.currentData() or 1)
        if dialog.exec() == QDialog.DialogCode.Accepted and self.may_discard():
            self.open_draft(dialog.result_document)

    def duplicate_wizard(self):
        if self.packet is None: return
        key = self.original_key if self.workspace.currentWidget() == self.editor_page else self.chosen_key()
        if key is None: return
        source = catalog.Catalog(self.packet).item(key)
        dialog = NewItemDialog(self, self.packet, source=source)
        if dialog.exec() == QDialog.DialogCode.Accepted and self.may_discard():
            self.open_draft(dialog.result_document)

    def open_draft(self, document):
        self.original_key = None
        self.list.blockSignals(True); self.list.clearSelection(); self.list.blockSignals(False)
        self.show_document(document)
        self.tabs.setCurrentWidget(self.overview_page)
        self.status.setText('Új tárgytípus piszkozata. Állítsd be a működését, majd nézd át és mentsd a változtatásokat.')

    def rebuild_sections(self, preferred=None):
        self.section_nav.blockSignals(True); self.section_nav.clear()
        for index in range(self.tabs.count()):
            self.section_nav.addItem(self.tabs.tabText(index))
            self.section_nav.item(index).setToolTip(self.tabs.tabToolTip(index))
        index = self.tabs.indexOf(preferred) if preferred else 0
        self.tabs.setCurrentIndex(max(0, index))
        self.section_nav.setCurrentRow(self.tabs.currentIndex())
        self.section_nav.blockSignals(False)
        self.section_changed(self.tabs.currentIndex())

    def section_changed(self, index):
        if self.loading or index < 0: return
        self.section_nav.blockSignals(True); self.section_nav.setCurrentRow(index); self.section_nav.blockSignals(False)
        title = self.tabs.tabText(index)
        self.section_title.setText(title)
        self.section_hint.setText(SECTION_HELP.get(title, CATEGORY_HELP[int(self.document['base']['category_id'])][1] if self.document else ''))
        if self.tabs.currentWidget() == self.review_page: self.refresh_review()
        elif self.tabs.currentWidget() == self.overview_page: self.refresh_item_overview()

    def refresh_item_overview(self, document=None):
        if not self.document: return
        if document is None and self.tabs.currentWidget() == self.overview_page:
            try: document = self.collect()
            except api.EditorError: pass
        if document is None:
            document = copy.deepcopy(self.document)
            # Preview partially typed fields without validating or moving focus.
            forms = [self.base_form, self.use_form]
            category = int(document['base']['category_id'])
            if category in self.category_forms and category not in (5, 9, 11, 12, 13, 14, 15, 17):
                forms.append(self.category_forms[category])
            for form in forms:
                for field in form.fields:
                    try:
                        if field.kind in ('bool', 'choice', 'bits'): value = field._value()
                        elif field.kind == 'scaled': value = str(catalog.scaled(field.edit.text(), field.options['scale']))
                        else: value = field.edit.text().strip()
                        document.setdefault(field.section, {})
                        if document[field.section] is None: document[field.section] = {}
                        document[field.section][field.column] = value
                    except (api.EditorError, ValueError): pass
            if category in (1, 2, 3, 4, 7, 11, 15):
                document.setdefault('refinement', {})['max_level'] = str(self.refinement.maximum.value())
                document['bonuses'] = [None] * self.bonuses.table.rowCount()
        base = document['base']
        self.overview_title.setText(readable_name(base))
        self.overview_details.setText('\n'.join(summary_lines(document)))
        self.heading.setText(readable_name(base) + ' · ' + catalog.CATEGORIES[int(base['category_id'])])
        self.identity_line.setText('Tárgytípus %s:%s · %s · %s' % (base['category_id'], base['local_id'], base['content_code'],
                                  'új, még nem mentett' if self.original_key is None else 'meglévő katalógusbejegyzés'))

    def show_form_error(self, message):
        self.inline_error.setText('Javítandó: ' + message); self.inline_error.show()
        for widget in self.tabs.findChildren(QWidget):
            if not widget.property('invalid'): continue
            for index in range(self.tabs.count()):
                page = self.tabs.widget(index)
                if widget is page or page.isAncestorOf(widget):
                    self.tabs.setCurrentIndex(index)
                    if isinstance(page, QScrollArea): page.ensureWidgetVisible(widget)
                    widget.setFocus()
                    return

    def refresh_review(self):
        if not self.document: return
        try: document = self.collect()
        except api.EditorError as error:
            self.review_summary.setText('A változtatások áttekintéséhez előbb javítsd a hibás vagy hiányzó mezőt.')
            self.changes_table.setRowCount(0); self.show_form_error(str(error)); return
        previous = catalog.Catalog(self.packet).item(self.original_key) if self.original_key else {}
        labels = {field.column: field for form in self.findChildren(Form) for field in form.fields}
        sections = {'base': 'Alapadatok', 'child': 'Tárgy működése', 'use': 'Használat', 'trade': 'Kereskedelem',
                    'appearance': 'Megjelenés', 'refinement': 'Fejlesztés', 'levels': 'Fejlesztési szintek',
                    'bonuses': 'Állandó bónuszok', 'recipes': 'Receptek', 'resistance': 'Ellenállások',
                    'attack': 'Támadás', 'ammo': 'Lőszer', 'pet_food_rules': 'Pet táplálása'}
        changes = []
        def display(value, column, context=None):
            if value is None: return 'Nincs'
            field = labels.get(column)
            if field:
                if field.kind == 'bool': return 'Igen' if str(value) == '1' else 'Nem'
                if field.kind == 'choice': return next((str(text) for key, text in field.options['choices'] if str(key) == str(value)), str(value))
                if field.kind == 'scaled': return catalog.display(value, field.options['scale']) + ' ' + field.options.get('unit', '')
            if column in ('bonus_id', 'value_scaled'):
                bonus_id = value if column == 'bonus_id' else (context or {}).get('bonus_id')
                definition = next((row for row in self.packet['bonuses'] if str(row['id']) == str(bonus_id)), None)
                if definition:
                    if column == 'bonus_id': return BONUS_LABELS.get(definition['code'], definition['code'])
                    scale, unit = UNITS.get(int(definition['unit']), (1, ''))
                    return catalog.display(value, scale) + ' ' + unit
            if column.endswith('_raw'): return catalog.display(value) + ' pont'
            if column.endswith('_bp'): return catalog.display(value, 100) + ' %'
            if column.endswith('_ms'): return catalog.display(value, 1000) + ' mp'
            return str(value)
        def visit(before, after, path=(), before_context=None, after_context=None):
            if before == after: return
            if isinstance(after, dict) or isinstance(before, dict):
                left, right = before or {}, after or {}
                for key in dict.fromkeys([*left, *right]): visit(left.get(key), right.get(key), (*path, key), left, right)
            elif isinstance(after, list) or isinstance(before, list):
                left, right = before or [], after or []
                for index in range(max(len(left), len(right))):
                    visit(left[index] if index < len(left) else None, right[index] if index < len(right) else None, (*path, '+%d' % index if path and path[0] == 'levels' else '%d. sor' % (index + 1)))
            else:
                column = str(path[-1]); field = labels.get(column)
                if column.endswith('_profile_id') or column in ('profile_id', 'category_id', 'local_id'): return
                name = field.label if field else {'level': 'Szint', 'value_scaled': 'Bónusz értéke', 'bonus_id': 'Bónuszazonosító',
                    'gold_cost': 'Yang-költség', 'success_chance_bp': 'Sikeresély', 'amount': 'Darabszám',
                    'physical_attack_gain_raw': 'Fizikai támadás többlete', 'magical_attack_gain_raw': 'Mágikus támadás többlete',
                    'physical_defense_gain_raw': 'Fizikai védekezés többlete', 'magical_defense_gain_raw': 'Mágikus védekezés többlete'}.get(column, column.replace('_', ' '))
                group = sections.get(path[0], str(path[0]).replace('_', ' '))
                changes.append((' · '.join([group, *map(str, path[1:-1]), name]), display(before, column, before_context), display(after, column, after_context)))
        visit(previous, document)
        self.review_summary.setText(('Új tárgytípus · %d beállítás kerül mentésre.' if self.original_key is None else
                                     '%d megváltozott beállítás. A mentett tárgyhoz hasonlítjuk az aktuális piszkozatot.') % len(changes))
        self.changes_table.setRowCount(len(changes))
        for row, values in enumerate(changes):
            for column, text in enumerate(values):
                item = QTableWidgetItem(text); item.setToolTip(text); self.changes_table.setItem(row, column, item)
        self.changes_table.resizeRowsToContents()

    def discard_requested(self):
        label = 'Új tárgy piszkozatának eldobása' if self.original_key is None else 'Mentetlen módosítások visszaállítása'
        dialog = QMessageBox(QMessageBox.Icon.Question, label,
            'A piszkozat bezárul, és az új tárgy nem kerül a katalógusba.' if self.original_key is None else
            'A szerkesztő a tárgy utoljára betöltött, mentett értékeit tölti vissza.', parent=self)
        discard = dialog.addButton('Piszkozat eldobása' if self.original_key is None else 'Módosítások visszaállítása', QMessageBox.ButtonRole.DestructiveRole)
        cancel = dialog.addButton('Szerkesztés folytatása', QMessageBox.ButtonRole.RejectRole)
        dialog.setDefaultButton(cancel); dialog.exec()
        if dialog.clickedButton() is discard: self.discard()

    # --- forms -------------------------------------------------------------------
    def build_forms(self):
        changed = self.mark_dirty
        identity = Form(changed)
        identity.section('Belső azonosítás', 'A tartalomkód alapján találod meg a tárgyat a katalógusban és a játék szabályaiban.')
        identity.add('base', 'content_code', 'Egyedi tartalomkód', 'text', hint='Állandó belső azonosító, pl. weapon.vaskard. Kisbetű, szám, _ és .')
        identity.section('Játékbeli név és leírás', 'Ezek fordítási kulcsok. A hozzájuk tartozó megjelenő szöveget a kliens fordításai tartalmazzák.')
        identity.add('base', 'name_key', 'Játékbeli név fordítási kulcsa', 'text', hint='A kliens ebből a fordítási kulcsból keresi ki a megjelenő nevet.')
        identity.add('base', 'description_key', 'Leírás fordítási kulcsa', 'text')
        identity.section('Ikon és modell', 'Kliensoldali erőforrás-útvonalak. A szerkesztő nem másolja át automatikusan ezeket az állományokat.')
        identity.add('base', 'icon_asset', 'Leltárikon útvonala', 'text', hint='Kliensoldali útvonal, pl. icons/weapon/sword_iron.png')
        self.appearance_field = identity.add('appearance', 'model_asset', 'Megjelenési modell útvonala', 'text', optional=True,
                                        placeholder='Üres = nincs külön modell', hint='Felszerelés modellje; foglalatkőnél és küldetéstárgynál a földre dobott tárgy modellje.')
        identity.finish(); self.identity_form = identity; self.identity_page = scroll(identity)
        base = Form(changed)
        base.section('Használati követelmény')
        base.add('base', 'min_level', 'Szükséges karakterszint', range=(0, 65535))
        self.binding_field = base.add('base', 'binding_policy', 'Karakterhez kötés', 'choice', choices=[(1, 'Nincs kötés'), (2, 'Megszerzéskor kötődik'), (3, 'Felszereléskor kötődik')])
        base.section('Helyfoglalás és darabszám')
        base.add('base', 'inventory_width', 'Leltárszélesség', unit='mező', range=(1, 255))
        base.add('base', 'inventory_height', 'Leltármagasság', unit='mező', range=(1, 255))
        self.stack_field = base.add('base', 'max_stack', 'Legnagyobb halomméret', unit='darab', range=(1, 4294967295), hint='Ennyi példány fér egy leltárhelyre. Felszerelésnél mindig 1.')
        base.add('base', 'weight_grams', 'Súly (gramm)', range=(0, 4294967295))
        base.section('Bolti érték')
        base.add('base', 'buy_amount', 'Játékos vételára az NPC-nél', unit='Yang', range=(0, 2 ** 63 - 1))
        base.add('base', 'sell_amount', 'Játékos eladási ára az NPC-nél', unit='Yang', range=(0, 2 ** 63 - 1))
        base.section('Katalógusállapot')
        base.add('base', 'retired', 'Új példányok készítése', 'bool', text='Letiltva: a tárgytípus kivezetett')
        base.note('A kategória és a helyi azonosító létrehozás után nem változik, és soha nem kerül újrahasznosításra.')
        base.fields.extend(identity.fields)
        base.finish(); self.base_form = base
        use = Form(changed)
        use.section('Ki és mikor használhatja?')
        use.add('use', 'allowed_classes', 'Engedélyezett kasztok', 'bits', names=['Harcos', 'Nindzsa', 'Sura', 'Sámán'])
        use.add('use', 'allowed_sexes', 'Engedélyezett karakterek', 'bits', names=['Férfi', 'Nő'])
        use.add('use', 'use_during_combat', 'Harc közben', 'bool', text='Harc közben is használható')
        use.section('Átadás, eldobás és eladás')
        use.add('trade', 'allow_trade', 'Kereskedés', 'bool', text='Játékosok között cserélhető')
        use.add('trade', 'allow_drop', 'Eldobás', 'bool', text='Földre dobható')
        use.add('trade', 'allow_sell', 'Eladás', 'bool', text='NPC-nek eladható')
        use.finish(); self.use_form = use
        weapon = Form(changed)
        weapon.section('Fegyverfajta és alapértékek')
        weapon.add('child', 'weapon_subtype', 'Fegyvertípus', 'choice',
                   choices=[(1, 'Kard'), (2, 'Tőr'), (3, 'Íj'), (4, 'Kétkezes'), (5, 'Csengő'), (6, 'Legyező'), (7, 'Lovas lándzsa')])
        weapon.add('child', 'physical_attack_base_raw', 'Fizikai támadás', 'scaled', scale=1000, unit='pont', range=(0, 10 ** 15))
        weapon.add('child', 'magical_attack_base_raw', 'Mágikus támadás', 'scaled', scale=1000, unit='pont', range=(0, 10 ** 15))
        weapon.section('Támadás működése')
        weapon.add('attack', 'interval_ticks', 'Támadási időköz', unit='tick', range=(1, 4294967295))
        weapon.add('attack', 'windup_ticks', 'Felkészülés', unit='tick', range=(0, 4294967295), hint='Nem lehet hosszabb a támadási időköznél.')
        weapon.add('attack', 'reach_millimeters', 'Hatótáv', 'scaled', scale=1000, unit='méter', range=(1, 4294967295))
        weapon.add('attack', 'max_targets', 'Célpontok száma', range=(1, 1024))
        weapon.section('Lőszerhasználat')
        weapon.add('ammo', 'ammo_kind', 'Lőszer', 'choice', choices=[(1, 'Nem használ lőszert'), (2, 'Lőszert fogyaszt a halomból')])
        weapon.add('ammo', 'units_per_hit', 'Lőszer találatonként', range=(0, 65535), hint='Lőszer nélkül 0, lőszerrel legalább 1.')
        weapon.finish()
        armor = Form(changed, 2)
        armor.section('Felszerelési hely és alapvédelem')
        armor.add('child', 'equip_slot', 'Felszerelési hely', 'choice', choices=[(2, 'Testpáncél'), (3, 'Sisak'), (4, 'Pajzs'), (5, 'Cipő')])
        armor.add('child', 'physical_defense_base_raw', 'Fizikai védekezés', 'scaled', scale=1000, unit='pont', range=(0, 10 ** 15))
        armor.add('child', 'magical_defense_base_raw', 'Mágikus védekezés', 'scaled', scale=1000, unit='pont', range=(0, 10 ** 15))
        self.add_resistances(armor)
        armor.finish()
        accessory = Form(changed, 2)
        accessory.section('Ékszerfajta és foglalatok')
        accessory.add('child', 'equip_slot', 'Felszerelési hely', 'choice', choices=[(6, 'Karkötő'), (7, 'Nyaklánc'), (8, 'Fülbevaló')])
        self.socket_field = accessory.add('child', 'socket_profile_id', 'Foglalatprofil', 'choice', choices=[('', 'Nincs foglalat')])
        self.add_resistances(accessory)
        accessory.note('Ékszer alapértéket nem kap: a statokat bónuszok és szintbónuszok adják. A foglalatprofilok külön tartalmak.')
        accessory.finish()
        material = Form(changed)
        material.add('child', 'material_kind', 'Alapanyag típusa', 'choice', choices=[(1, 'Bőr'), (2, 'Vér'), (3, 'Gyökér'), (4, 'Tű'), (5, 'Ékkő')])
        material.note('Az alapanyag nem fejleszthető, nem köthető felszereléskor, és nem kap bónuszt vagy megjelenést.')
        material.finish()
        container = QWidget(); container_layout = QVBoxLayout(container)
        container_layout.setAlignment(Qt.AlignmentFlag.AlignTop)
        container_form = Form(changed)
        self.container_kind_field = container_form.add('child', 'container_kind', 'Nyitás', 'choice',
            choices=[(1, 'Használatkor nyílik'), (2, 'Kulccsal nyílik'), (3, 'Quest nyitja (beépített tartalom nélkül)'), (4, 'Fejlődő pet tojása')])
        self.lock_field = container_form.add('child', 'lock_id', 'Zárazonosító', range=(0, 65535),
            hint='Kulccsal nyíló ládánál legalább 1; az a kulcs nyitja, amelynek ugyanez a zárazonosítója.')
        container_form.finish(); container_form.setSizePolicy(QSizePolicy.Policy.Expanding,QSizePolicy.Policy.Maximum)
        container_layout.addWidget(container_form)
        self.loot_mode_row=QWidget(); mode_row = QHBoxLayout(self.loot_mode_row); mode_row.addWidget(QLabel('Sorsolás:'))
        self.loot_mode = QComboBox(); self.loot_mode.addItem('Pontosan egy jutalom, súly szerint', '1')
        self.loot_mode.addItem('Minden sor külön, saját esélyével', '2')
        self.loot_mode.currentIndexChanged.connect(lambda *_: (self.loot_table.set_mode(self.loot_mode.currentData()), changed()))
        mode_row.addWidget(self.loot_mode); mode_row.addStretch(); container_layout.addWidget(self.loot_mode_row)
        self.loot_table = LootTable(changed); container_layout.addWidget(self.loot_table, 1)
        self.pet_hatch_panel=PetHatchPanel(changed,Form);container_layout.addWidget(self.pet_hatch_panel)
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
        quest_form = Form(changed)
        self.quest_dispatch_field = quest_form.add('child','dispatch_kind','Használat','choice',choices=[
            (1,'Gyűjthető token (közvetlenül nem használható)'),(2,'Első megfelelő quest kezelése'),(3,'Minden megfelelő quest kezelése')])
        self.quest_group_field = quest_form.add('child','group_code','Küldetéscsoport','text',optional=True,
            hint='Üresen a tárgyhoz kötött questek kezelik. Megadva ennek a csoportnak a kezelői kapják a használatot.')
        self.quest_contexts_field = quest_form.add('child','allowed_contexts','Használható','bits',names=['Világ','Párbaj','Aréna','Céhháború'])
        self.quest_observer_field = quest_form.add('child','allow_observer','Megfigyelő','bool',text='Megfigyelőként is használható')
        quest_form.note('A használat önmagában nem fogyasztja el a tárgyat. A quest dönti el, hogy kell-e belőle levonni vagy jutalmat adni.')
        quest_form.finish()
        stone_form=Form(changed)
        self.stone_kind_field=stone_form.add('child','stone_kind','Típus','choice',choices=[(1,'Behelyezhető kő'),(2,'Kőszilánk')])
        self.stone_targets_field=stone_form.add('child','allowed_targets','Célfelszerelés','bits',names=['Fegyver','Testpáncél'])
        self.stone_grade_field=stone_form.add('child','minimum_socket_grade','Legalacsonyabb foglalat','choice',choices=[(1,'Normál'),(2,'Arany')])
        self.stone_group_fields=[stone_form.add('child',name,label,'text',optional=True,
            hint='Azonos vagy kereszteződő csoportú kövek nem tehetők ugyanabba a felszerelésbe.')
            for name,label in [('primary_group','Első kizárócsoport'),('secondary_group','Második kizárócsoport')]]
        stone_form.note('A kő bónuszai behelyezve hatnak. A sikeresély a szerver szabálya. Sikertelen behelyezéskor egy kő fogy el és törött foglalat marad. A szilánk nem helyezhető be, és nem ad bónuszt.')
        stone_form.finish()
        self.stone_kind_field.widget.currentIndexChanged.connect(self.stone_controls)
        self.quest_dispatch_field.widget.currentIndexChanged.connect(lambda *_: self.quest_controls())
        potion = QWidget(); potion_layout = QVBoxLayout(potion)
        recovery = Form(changed)
        self.delivery_field = recovery.add('recovery', 'delivery_kind', 'Hatás módja', 'choice',
                                           choices=[(1, 'Azonnali'), (2, 'Tartalék (adagolva tölt)'), (3, 'Időszakos'), (4, 'Automata palack')])
        self.recovery_units_field = recovery.add('recovery', 'units_per_use', 'Egység használatonként', range=(1, 65535))
        self.recovery_contexts_field = recovery.add('recovery', 'allowed_contexts', 'Használható', 'bits', names=['Világ', 'Párbaj', 'Aréna', 'Céhháború'])
        recovery.add('recovery', 'allow_during_pvp', 'PvP', 'bool', text='PvP közben is használható')
        self.arena_cost_field = recovery.add('recovery', 'arena_quota_cost', 'Arénakvóta használatonként', range=(0, 65535),
            hint='0: kvótamentes. Pozitív költség csak arénát engedő tárgyhoz; a mérkőzés tiltása minden italra érvényes.')
        for box in self.recovery_contexts_field.boxes: box.toggled.connect(lambda *_: self.recovery_controls())
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
        automatic = Form(changed)
        automatic.add('effect', 'pool_kind', 'Mit tölt?', 'choice', choices=[(1, 'Életerő (HP)'), (2, 'Mana (SP)')])
        self.supply_field = automatic.add('effect', 'supply_kind', 'Ellátás', 'choice', choices=[(1, 'Véges töltet'), (2, 'Végtelen')])
        self.capacity_field = automatic.add('effect', 'capacity_raw', 'Teljes kapacitás', 'scaled', scale=1000, unit='pont', range=(0, 10 ** 15),
            hint='Új palack induló töltete. Végtelen ellátásnál 0; a megmaradt töltet palackonként külön változik.')
        automatic.add('effect', 'tick_interval_ms', 'Visszatöltés időköze', 'scaled', scale=1000, unit='mp', range=(1, 2 ** 63 - 1))
        automatic.add('effect', 'max_pool_per_tick_bp', 'Maximum visszatöltés időközönként', 'scaled', scale=100, unit='%', range=(1, 10000))
        automatic.add('effect', 'active_max_pool_bonus_bp', 'Aktív maximális HP/SP bónusz', 'scaled', scale=100, unit='%', range=(0, 10000),
                      hint='Ugyanannak a poolnak a maximumát növeli, amelyet a palack tölt. Kikapcsoláskor megszűnik.')
        automatic.note('Az automata palack egyenként kezelhető, arénában tiltott. Bekapcsoláskor nem fogy el; csak a '
                       'tényleges helyreállítás csökkenti a töltetét. Az üres véges palack eltűnik.')
        automatic.finish()
        self.supply_field.widget.currentIndexChanged.connect(lambda *_: self.recovery_controls())
        for kind, form in ((1, instant), (2, reserve), (3, periodic), (4, automatic)):
            self.effect_forms[kind] = form; self.effects.addWidget(form)
        self.delivery_field.widget.currentIndexChanged.connect(lambda *_: (self.effects.setCurrentWidget(self.effect_forms[int(self.delivery_field.value())]), self.recovery_controls()))
        potion_layout.addWidget(self.effects); potion_layout.addStretch()
        self.book_panel = SkillBookPanel(changed, Form)
        self.costume_panel = CostumePanel(changed)
        self.companion_panel=CompanionPanel(changed,Form)
        self.companion_panel.kind.widget.currentIndexChanged.connect(self.companion_controls)
        self.soul_panel=SoulPanel(changed,Form)
        self.soul_panel.kind.widget.currentIndexChanged.connect(lambda *_:self.soul_controls())
        self.poly_panel=PolymorphPanel(changed,Form)
        self.poly_panel.kind.widget.currentIndexChanged.connect(lambda *_:self.poly_controls())
        self.tool_panel=ToolPanel(changed)
        self.fishing_panel=FishingPanel(changed,Form,LootTable)
        self.pet_supply_panel=PetSupplyPanel(changed,Form)
        self.pet_supply_panel.kind.widget.currentIndexChanged.connect(lambda *_:self.pet_supply_controls())
        self.category_forms = {1: weapon, 2: armor, 3: accessory, 5: recovery, 6: material, 7: stone_form, 8: container_form,
                               10: quest_form, 16: token_form, 18: key_form}
        self.category_pages = {1: scroll(weapon), 2: scroll(armor), 3: scroll(accessory), 4: self.costume_panel, 5: scroll(potion), 6: scroll(material),
                               7: scroll(stone_form), 8: scroll(container), 9: self.book_panel, 10: scroll(quest_form), 11: self.tool_panel, 12: self.fishing_panel, 13: self.poly_panel, 14: self.soul_panel, 15: scroll(self.companion_panel), 16: scroll(token_form), 17: scroll(self.pet_supply_panel), 18: scroll(key_form)}
        # Timed stat effects are another typed consumable, never a recovery subtype.
        timed_page = QWidget(); timed_layout = QVBoxLayout(timed_page)
        self.timed_form = Form(changed)
        self.timed_form.add('timed', 'duration_ms', 'Időtartam', 'scaled', scale=1000, unit='mp', range=(1, 2 ** 63 - 1))
        self.timed_form.add('timed', 'units_per_use', 'Darab használatonként', range=(1, 65535))
        self.timed_form.add('timed', 'allowed_contexts', 'Használható', 'bits', names=['Világ', 'Párbaj', 'Aréna', 'Céhháború'])
        self.timed_form.add('timed', 'allow_during_pvp', 'PvP', 'bool', text='PvP közben is használható')
        self.timed_group_field = self.timed_form.add('timed', 'group_id', 'Hatáscsoport', 'choice', choices=[('', 'Új hatáscsoport')])
        self.timed_form.finish(); timed_layout.addWidget(self.timed_form)
        self.timed_group_form = Form(changed)
        self.timed_group_form.add('timed_group', 'code', 'Csoport kódja', 'text')
        self.timed_group_form.add('timed_group', 'reapply_kind', 'Ismételt használat', 'choice',
                                  choices=[(1, 'Elutasítás, amíg a csoport hatása aktív'), (2, 'A teljes aktív hatás lecserélése')])
        self.timed_group_form.add('timed_group', 'clock_kind', 'Idő számítása', 'choice',
                                  choices=[(1, 'Csak bejelentkezve telik'), (2, 'Kijelentkezve is telik')])
        self.timed_group_form.add('timed_group', 'remove_on_death', 'Halál', 'bool', text='Halálkor megszűnik')
        self.timed_group_form.note('Egy csoportból egyszerre egy hatás aktív. Meglévő csoport szabályai közösek és itt nem módosíthatók; '
                                    'más szabályokhoz válassz új csoportot.')
        self.timed_group_form.finish(); timed_layout.addWidget(self.timed_group_form)
        timed_cooldown = QHBoxLayout()
        self.timed_cooldown_check = QCheckBox('Várakozási idő használat után')
        self.timed_cooldown_edit = QLineEdit('1')
        self.timed_cooldown_check.toggled.connect(lambda on: (self.timed_cooldown_edit.setEnabled(on), changed()))
        self.timed_cooldown_edit.textEdited.connect(lambda *_: changed())
        timed_cooldown.addWidget(self.timed_cooldown_check); timed_cooldown.addWidget(self.timed_cooldown_edit)
        timed_cooldown.addWidget(QLabel('mp')); timed_cooldown.addStretch(); timed_layout.addLayout(timed_cooldown)
        timed_layout.addWidget(QLabel('A hatás bónuszai (együtt kerülnek fel és együtt szűnnek meg):'))
        self.timed_bonuses = BonusTable(changed, '3'); timed_layout.addWidget(self.timed_bonuses, 1)
        toggle_page = QWidget(); toggle_layout = QVBoxLayout(toggle_page)
        self.toggle_form = Form(changed)
        self.toggle_form.add('toggle', 'duration_ms', 'Összes aktív idő', 'scaled', scale=1000, unit='mp', range=(1, 2 ** 63 - 1))
        self.toggle_form.add('toggle', 'variant_id', 'Variáns', range=(1, 65535))
        self.toggle_form.add('toggle', 'requirements', 'Feltételek', 'bits', names=['Házas karakter', 'Csoportvezető'])
        self.toggle_form.add('toggle', 'allowed_contexts', 'Használható', 'bits', names=['Világ', 'Párbaj', 'Aréna', 'Céhháború'])
        self.toggle_form.add('toggle', 'allow_during_pvp', 'PvP', 'bool', text='PvP közben is használható')
        self.toggle_group_field = self.toggle_form.add('toggle', 'group_id', 'Hatáscsoport', 'choice', choices=[('', 'Új hatáscsoport')])
        self.toggle_form.finish(); toggle_layout.addWidget(self.toggle_form)
        self.toggle_group_form = Form(changed)
        self.toggle_group_form.add('toggle_group', 'code', 'Csoport kódja', 'text')
        self.toggle_group_form.add('toggle_group', 'remove_on_death', 'Halál', 'bool', text='Halálkor megszűnik')
        self.toggle_group_form.note('Csak bekapcsolva és bejelentkezve fogy az idő. Egy csoportban egy tárgy aktív: azonos variáns másik példánya tiltott, eltérő variáns lecseréli. A harmat nem halmozható.')
        self.toggle_group_form.finish(); toggle_layout.addWidget(self.toggle_group_form)
        toggle_cooldown = QHBoxLayout()
        self.toggle_cooldown_check = QCheckBox('Várakozási idő bekapcsolás után')
        self.toggle_cooldown_edit = QLineEdit('1')
        self.toggle_cooldown_check.toggled.connect(lambda on: (self.toggle_cooldown_edit.setEnabled(on), changed()))
        self.toggle_cooldown_edit.textEdited.connect(lambda *_: changed())
        toggle_cooldown.addWidget(self.toggle_cooldown_check); toggle_cooldown.addWidget(self.toggle_cooldown_edit)
        toggle_cooldown.addWidget(QLabel('mp')); toggle_cooldown.addStretch(); toggle_layout.addLayout(toggle_cooldown)
        toggle_layout.addWidget(QLabel('A hatás bónuszai (együtt kerülnek fel és együtt szűnnek meg):'))
        self.toggle_bonuses = BonusTable(changed, '3'); toggle_layout.addWidget(self.toggle_bonuses, 1)
        wrapper = QWidget(); wrapper_layout = QVBoxLayout(wrapper)
        kind_row = QHBoxLayout(); kind_row.addWidget(QLabel('Fogyasztható típusa:'))
        self.consumable_kind = QComboBox(); self.consumable_kind.addItem('Gyógyítás / regeneráció', '1')
        self.consumable_kind.addItem('Időzített stat-hatás', '2'); self.consumable_kind.addItem('Kapcsolható harmat', '3'); kind_row.addWidget(self.consumable_kind); kind_row.addStretch()
        self.consumable_kind.addItem('Foglalattisztítás / kőkiemelés','4')
        self.consumable_kind.addItem('Bónuszhozzáadás / váltás / védelem','5')
        self.attribute_service_panel=AttributeServicePanel(changed,Form)
        self.socket_service_form=Form(changed)
        self.socket_service_form.add('socket_service','service_kind','Művelet','choice',choices=[
            ('1','Törött foglalatok tisztítása'),('2','Minden kő kiemelése — törött foglalat marad'),('3','Utolsó kő kiemelése — üres foglalat marad')])
        self.socket_service_form.add('socket_service','units_per_use','Felhasznált darab',range=(1,65535))
        self.socket_service_form.note('A foglalat helye és minősége megmarad. A kiemelt kövek tárgytípusa és fejlesztési szintje változatlan; helyhiány esetén a művelet nem végezhető el.')
        self.socket_service_form.finish()
        wrapper_layout.addLayout(kind_row)
        self.consumable_pages = QStackedWidget(); self.consumable_pages.addWidget(potion); self.consumable_pages.addWidget(timed_page); self.consumable_pages.addWidget(toggle_page); self.consumable_pages.addWidget(self.socket_service_form);self.consumable_pages.addWidget(self.attribute_service_panel)
        wrapper_layout.addWidget(self.consumable_pages, 1)
        self.category_pages[5] = scroll(wrapper)
        self.consumable_kind.currentIndexChanged.connect(lambda index: (self.consumable_pages.setCurrentIndex(index), self.recovery_controls(), changed()))
        self.timed_group_field.widget.currentIndexChanged.connect(lambda *_: self.timed_group_changed())
        self.toggle_group_field.widget.currentIndexChanged.connect(lambda *_: self.toggle_group_changed())
        self.refinement = RefinementPanel(changed)
        self.refinement.maximum.valueChanged.connect(lambda value: self.tool_panel.set_maximum(value) if not self.loading and self.document and int(self.document['base']['category_id'])==11 else None)
        self.pet_food_panel=PetFoodPanel(changed,Form);self.pet_food_page=scroll(self.pet_food_panel)
        self.attribute_panel=AttributePanel(changed);self.attribute_page=scroll(self.attribute_panel)
        self.bonuses = BonusTable(changed, '1')
        bonus_page = QWidget(); bonus_layout = QVBoxLayout(bonus_page)
        hint = QLabel('Az alap bónuszok minden fejlesztési szinten érvényesek. Szintenkénti bónuszt a Fejlesztés lapon adhatsz meg.')
        hint.setWordWrap(True); hint.setObjectName('FieldHint'); bonus_layout.addWidget(hint); bonus_layout.addWidget(self.bonuses, 1)
        self.base_page, self.use_page, self.bonus_page = scroll(base), scroll(use), bonus_page

    def companion_controls(self,*_):
        if not self.document or int(self.document['base']['category_id'])!=15:return
        growth=self.companion_panel.kind.value()=='3'
        if growth and not self.loading:self.refinement.kind.setCurrentIndex(self.refinement.kind.findData('1'))
        self.refinement.setEnabled(not growth)

    def stone_controls(self, *_):
        usable=self.stone_kind_field.value()=='1'
        for field in (self.stone_targets_field,self.stone_grade_field,*self.stone_group_fields):field.widget.setEnabled(usable)
        self.refinement.setEnabled(usable);self.bonuses.setEnabled(usable)
        if not self.loading and usable and not int(self.stone_targets_field.value()):
            self.stone_targets_field.load({'child':{'allowed_targets':'1'}})

    def pet_supply_controls(self):
        if not self.document or int(self.document['base']['category_id'])!=17:return
        single=self.pet_supply_panel.kind.value() in ('7','8');self.stack_field.edit.setEnabled(not single)
        if single and not self.loading:self.stack_field.edit.setText('1')

    def soul_controls(self):
        if not self.document or int(self.document['base']['category_id'])!=14:return
        core=self.soul_panel.kind.value()=='1';self.stack_field.edit.setEnabled(not core)
        if core and not self.loading:self.stack_field.edit.setText('1')

    def poly_controls(self):
        if not self.document or int(self.document['base']['category_id'])!=13:return
        book=self.poly_panel.kind.value()=='2'
        self.stack_field.edit.setEnabled(not book)
        if book and not self.loading:self.stack_field.edit.setText('1')

    def quest_controls(self):
        usable=self.quest_dispatch_field.value()!='1'
        for field in (self.quest_group_field,self.quest_contexts_field,self.quest_observer_field):field.widget.setEnabled(usable)
        if not self.loading:
            if not usable:
                self.quest_group_field.load('');self.quest_contexts_field.load('0');self.quest_observer_field.load('0')
            elif self.quest_contexts_field.value()=='0':self.quest_contexts_field.load('1')

    def add_resistances(self, form):
        form.section('Ellenállások', 'Százalékos ellenállás a megfelelő sebzéstípus ellen. A negatív érték sérülékenységet jelent.')
        for column, label in zip(catalog.RESISTANCES, RESISTANCE_LABELS):
            form.add('resistance', column, label + ' ellenállás', 'scaled', scale=100, unit='%', range=(-10000, 10000))

    def container_kind_changed(self):
        kind = self.container_kind_field.value()
        self.lock_field.widget.setEnabled(kind == '2')
        if kind != '2' and not self.loading: self.lock_field.edit.setText('0')
        for widget in (self.loot_mode, self.loot_table): widget.setEnabled(kind not in ('3','4'))
        for widget in (self.loot_mode_row,self.loot_table): widget.setVisible(kind not in ('3','4'))
        self.pet_hatch_panel.setVisible(kind=='4')
        self.loot_hint.setVisible(kind == '3')

    def recovery_controls(self):
        automatic = self.consumable_kind.currentData() == '1' and self.delivery_field.value() == '4'
        toggle = self.consumable_kind.currentData() == '3'
        self.stack_field.edit.setEnabled(not (automatic or toggle))
        self.recovery_units_field.widget.setEnabled(not automatic)
        self.recovery_contexts_field.boxes[2].setEnabled(not automatic)
        unlimited = self.supply_field.value() == '2'
        self.capacity_field.widget.setEnabled(not unlimited)
        if not self.loading:
            if toggle: self.stack_field.edit.setText('1')
            if automatic:
                self.stack_field.edit.setText('1'); self.recovery_units_field.edit.setText('1')
                self.recovery_contexts_field.boxes[2].setChecked(False)
            if unlimited:
                finite = self.capacity_field.edit.text().strip()
                if finite not in ('', '0'): self.finite_capacity_text = finite
                self.capacity_field.edit.setText('0')
            elif self.capacity_field.edit.text().strip() in ('', '0'):
                self.capacity_field.edit.setText(getattr(self, 'finite_capacity_text', '1000'))
            if not self.recovery_contexts_field.boxes[2].isChecked(): self.arena_cost_field.edit.setText('0')
        self.arena_cost_field.widget.setEnabled(not automatic and self.recovery_contexts_field.boxes[2].isChecked())

    def timed_group_changed(self):
        selected = self.timed_group_field.value()
        if not self.loading and self.document is not None:
            group = catalog.Catalog(self.packet).row('timed_effect_group', selected, 'group_id') if selected else None
            if group is None: group = catalog.Catalog.timed_defaults(self.document['base']['local_id'])['timed_group']
            self.timed_group_form.load(dict(timed_group=group))
            self.mark_dirty()
        self.timed_group_form.setEnabled(not bool(selected))

    def toggle_group_changed(self):
        selected = self.toggle_group_field.value()
        if not self.loading and self.document is not None:
            group = catalog.Catalog(self.packet).row('toggle_effect_group', selected, 'group_id') if selected else None
            if group is None: group = catalog.Catalog.toggle_defaults(self.document['base']['local_id'])['toggle_group']
            self.toggle_group_form.load(dict(toggle_group=group))
            self.mark_dirty()
        self.toggle_group_form.setEnabled(not bool(selected))

    def cooldown_toggled(self):
        self.cooldown_edit.setEnabled(self.cooldown_check.isChecked())

    # --- state ---------------------------------------------------------------------
    def active(self):
        return self.task is not None

    def local(self):
        return not self.owner.is_remote() and bool(self.owner.root)

    def mark_dirty(self):
        if self.loading or self.document is None: return
        self.checked_document = None
        self.inline_error.hide()
        self.dirty = True; self.update_badge(); self.controls()
        self.preview_timer.start()

    def update_badge(self):
        self.badge.setProperty('dirty', 'true' if self.dirty or (self.document and self.original_key is None) else 'false')
        self.badge.setText('Új piszkozat' if self.document and self.original_key is None else 'Mentetlen módosítás' if self.dirty else 'Mentett tárgytípus' if self.document else '')
        self.badge.setVisible(bool(self.document)); self.badge.style().unpolish(self.badge); self.badge.style().polish(self.badge)

    def may_discard(self):
        if not self.dirty and not (self.document and self.original_key is None): return True
        dialog = QMessageBox(QMessageBox.Icon.Question, 'Mentetlen tárgyszerkesztés',
            'A(z) „%s” tárgytípus szerkesztése még nincs mentve. Másik tárgy megnyitásához vagy az ablak bezárásához el kell döntened, mi legyen a piszkozattal.' % readable_name(self.document['base']), parent=self)
        discard = dialog.addButton('Mentetlen szerkesztés eldobása', QMessageBox.ButtonRole.DestructiveRole)
        keep = dialog.addButton('Szerkesztés folytatása', QMessageBox.ButtonRole.RejectRole)
        dialog.setDefaultButton(keep); dialog.exec()
        if dialog.clickedButton() is discard:
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
        self.tabs.setEnabled(has and not busy)
        self.section_nav.setEnabled(has and not busy)
        self.back_button.setEnabled(not busy)
        self.copy_button.setEnabled(local and db and not busy and self.chosen_key() is not None)
        self.editor_copy_button.setEnabled(local and db and not busy and has and self.original_key is not None and not self.dirty)
        self.editor_copy_button.setToolTip('Készíts új tárgytípust a mentett beállítások alapján.' if not self.dirty else
                                         'A másolat készítése előtt mentsd a jelenlegi módosításokat.')
        self.edit_button.setEnabled(not busy and self.chosen_key() is not None)
        self.resume_button.setVisible(has)
        self.resume_button.setEnabled(not busy)
        self.resume_button.setText('Mentetlen piszkozat folytatása' if self.dirty or (has and self.original_key is None) else 'Megnyitott tárgy folytatása')
        self.check_button.setEnabled(has and not busy and self.packet is not None)
        running = bool(state.get('running'))
        can_save = has and not busy and db and not running and (self.dirty or self.original_key is None)
        self.save_button.setEnabled(can_save)
        self.save_button.setText('Új tárgytípus mentése' if has and self.original_key is None else 'Változtatások mentése')
        self.discard_button.setEnabled(has and not busy and (self.dirty or self.original_key is None))
        self.discard_button.setText('Piszkozat eldobása…' if has and self.original_key is None else 'Módosítások visszaállítása…')
        self.save_note.setText('A mentéshez állítsd le a helyi játékszervert.' if has and running else
                               'Művelet folyamatban…' if busy else 'Az adatbázis nem érhető el; a mentéshez indítsd el.' if has and not db else
                               ('Ellenőrzés rendben. A piszkozat még nincs mentve.' if self.dirty or self.original_key is None else
                                'Ellenőrzés rendben. A mentett értékek érvényesek.') if self.checked_document is not None else
                               'Mentetlen piszkozat. Mentés előtt a teljes katalógust ellenőrizzük.' if has and (self.dirty or self.original_key is None) else
                               'A tárgy betöltött értékei megegyeznek a mentett állapottal.' if has else '')
        self.save_button.setToolTip(self.save_note.text() if not can_save else 'Ellenőrzés és mentés a helyi tárgykatalógusba.')

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
                self.status.setText(outcome['error']); self.show_form_error(outcome['error']); self.owner.error(outcome['error'])
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
        self.timed_bonuses.set_definitions(packet['bonuses'])
        self.toggle_bonuses.set_definitions(packet['bonuses'])
        self.socket_field.widget.blockSignals(True); self.socket_field.widget.clear()
        self.socket_field.widget.addItem('Nincs foglalat', '')
        for pid, code in model.choices('accessory_socket_profile'): self.socket_field.widget.addItem(code + '  (#' + pid + ')', pid)
        self.socket_field.widget.blockSignals(False)
        self.loot_table.set_items(model.items())
        items = model.items()
        retired = sum(row['retired'] == '1' for row in items)
        self.status.setText('Helyi tárgykatalógus · %d tárgytípus · %d kivezetett' % (len(items), retired))
        self.status.setToolTip('Katalógus-ellenőrzőösszeg: ' + packet['hash'])
        self.dirty = False; self.document = None
        select = tuple(select) if select is not None else None
        if select is not None and any((int(r['category_id']), int(r['local_id'])) == select for r in items): self.open_item(select)
        else: self.original_key = None; self.show_document(None)
        self.fill_list()

    def fill_list(self):
        if self.packet is None: return
        selected = self.chosen_key() or self.original_key
        category = self.category_filter.currentData(); needle = self.search.text().strip().lower()
        rows = [row for row in catalog.Catalog(self.packet).items()
                if (not category or int(row['category_id']) == category) and (self.show_retired.isChecked() or row['retired'] != '1')
                and (not needle or needle in (readable_name(row) + ' ' + row['content_code'] + ' ' + row['name_key'] + ' ' + catalog.CATEGORIES.get(int(row['category_id']), '') + ' ' + row['category_id'] + ':' + row['local_id']).lower())]
        self.list.blockSignals(True); self.list.setRowCount(len(rows)); target = -1
        for index, row in enumerate(rows):
            key = (int(row['category_id']), int(row['local_id']))
            cells = [readable_name(row), catalog.CATEGORIES.get(key[0], str(key[0])), '%d:%d' % key, row['min_level'], 'Kivezetett' if row['retired'] == '1' else 'Aktív', row['content_code']]
            for column, value in enumerate(cells):
                item = QTableWidgetItem(value); item.setData(KEY_ROLE, key)
                item.setToolTip('%s\nAzonosító: %d:%d\nFordítási kulcs: %s' % (row['content_code'], *key, row['name_key']))
                self.list.setItem(index, column, item)
            if selected is not None and key == tuple(selected): target = index
        self.list.clearSelection()
        if target >= 0: self.list.selectRow(target)
        self.list.blockSignals(False)
        self.result_count.setText('%d találat / %d tárgytípus' % (len(rows), len(catalog.Catalog(self.packet).items())))
        self.empty_list.setText('Nincs találat. Módosítsd a keresést, vagy töröld a szűrőket.' if not rows else '')
        self.empty_list.setVisible(not rows)
        self.selected()

    def selected(self):
        key = self.chosen_key()
        if key and self.packet:
            document = catalog.Catalog(self.packet).item(key)
            self.selection_description.setText(readable_name(document['base']) + '\n' + summary_lines(document)[1])
        else:
            self.selection_description.setText('Jelölj ki egy tárgytípust a részletekhez. Dupla kattintás: szerkesztés.')
        self.controls()

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
        self.open_draft(document)

    def duplicate(self):
        if self.packet is None or self.original_key is None or not self.may_discard(): return
        try: document = catalog.Catalog(self.packet).duplicate(self.original_key)
        except api.EditorError as error: self.owner.error(str(error)); return
        self.open_draft(document)
        self.status.setText('Új, még nem mentett másolat. A név, megjelenés és beállítások átvehetők; az eredeti tárgy változatlan.')

    # --- document <-> forms ------------------------------------------------------
    def show_document(self, document):
        self.loading = True; self.document = copy.deepcopy(document); self.dirty = False
        self.checked_document = None
        self.preview_timer.stop(); self.inline_error.hide()
        current = self.tabs.currentWidget()
        while self.tabs.count(): self.tabs.removeTab(0)
        if document is None:
            self.heading.setText('Nincs megnyitott tárgytípus'); self.identity_line.clear(); self.section_nav.clear()
            self.workspace.setCurrentWidget(self.catalog_page)
            self.loading = False; self.update_badge(); self.controls(); return
        base = document['base']; category = int(base['category_id'])
        equipment = category in (1, 2, 3, 4, 11, 15)
        refinable = equipment or category==7
        self.tabs.addTab(self.overview_page, 'Áttekintés')
        self.tabs.addTab(self.base_page, 'Alapbeállítások')
        self.tabs.addTab(self.category_pages[category], TYPE_SECTION[category])
        self.tabs.addTab(self.use_page, 'Használat és csere')
        if refinable:
            self.tabs.addTab(self.refinement, 'Fejlesztés és receptek'); self.tabs.addTab(self.bonus_page, 'Állandó bónuszok')
        self.stack_field.edit.setEnabled(not equipment)
        self.appearance_field.widget.setEnabled(category in (1,2,3,4,11,7,10))
        self.base_form.load(document); self.use_form.load(document);self.pet_food_panel.load(document)
        if category in (1,2,3,4):
            self.tabs.addTab(self.attribute_page,'Véletlen bónuszok');self.attribute_panel.load(document,self.packet['bonuses'])
        if category == 5:
            attribute_service=document.get('attribute_service') is not None
            self.attribute_service_panel.load(document)
            service=document.get('socket_service') is not None
            self.socket_service_form.load(dict(socket_service=document.get('socket_service') or dict(service_kind='1',units_per_use='1')))
            timed = document.get('timed') is not None
            toggle = document.get('toggle') is not None
            recovery_doc = document if not (timed or toggle or service or attribute_service) else catalog.Catalog(self.packet).new(5)
            self.category_forms[5].load(recovery_doc)
            kind = int(recovery_doc['recovery']['delivery_kind'])
            self.finite_capacity_text = catalog.display(recovery_doc['effect']['capacity_raw']) if kind == 4 and recovery_doc['effect']['supply_kind'] == '1' else '1000'
            for value, form in self.effect_forms.items():
                form.load(dict(effect=recovery_doc['effect'] if value == kind else EFFECT_DEFAULTS[value]))
            self.effects.setCurrentWidget(self.effect_forms[kind])
            cooldown = document.get('cooldown')
            self.cooldown_check.setChecked(bool(cooldown) and not (timed or toggle))
            self.cooldown_edit.setText(catalog.display(cooldown['duration_ms']) if cooldown and not (timed or toggle) else '1')
            self.cooldown_toggled()
            timed_doc = copy.deepcopy(document) if timed else dict(document, **catalog.Catalog.timed_defaults(base['local_id']))
            combo = self.timed_group_field.widget
            combo.blockSignals(True); combo.clear(); combo.addItem('Új hatáscsoport', '')
            for gid, code in catalog.Catalog(self.packet).choices('timed_effect_group', 'group_id'):
                combo.addItem(code + ' (#' + gid + ')', gid)
            combo.blockSignals(False)
            timed_doc['timed']['group_id'] = timed_doc['timed'].get('group_id') or ''
            self.timed_form.load(timed_doc); self.timed_group_form.load(timed_doc)
            self.timed_group_changed()
            self.timed_bonuses.load(timed_doc['timed_bonuses'])
            self.timed_cooldown_check.setChecked(bool(cooldown) and timed)
            self.timed_cooldown_edit.setText(catalog.display(cooldown['duration_ms']) if cooldown and timed else '1')
            self.timed_cooldown_edit.setEnabled(self.timed_cooldown_check.isChecked())
            toggle_doc = copy.deepcopy(document) if toggle else dict(document, **catalog.Catalog.toggle_defaults(base['local_id']))
            combo = self.toggle_group_field.widget
            combo.blockSignals(True); combo.clear(); combo.addItem('Új hatáscsoport', '')
            for gid, code in catalog.Catalog(self.packet).choices('toggle_effect_group', 'group_id'):
                combo.addItem(code + ' (#' + gid + ')', gid)
            combo.blockSignals(False)
            toggle_doc['toggle']['group_id'] = toggle_doc['toggle'].get('group_id') or ''
            self.toggle_form.load(toggle_doc); self.toggle_group_form.load(toggle_doc)
            self.toggle_group_changed()
            self.toggle_bonuses.load(toggle_doc['toggle_bonuses'])
            self.toggle_cooldown_check.setChecked(bool(cooldown) and toggle)
            self.toggle_cooldown_edit.setText(catalog.display(cooldown['duration_ms']) if cooldown and toggle else '1')
            self.toggle_cooldown_edit.setEnabled(self.toggle_cooldown_check.isChecked())
            selected_kind = 4 if attribute_service else 3 if service else 2 if toggle else 1 if timed else 0
            self.consumable_kind.setCurrentIndex(selected_kind)
            self.consumable_pages.setCurrentIndex(selected_kind)
            self.recovery_controls()
        elif category == 15:
            self.companion_panel.set_forms(catalog.Catalog(self.packet).tables['companion_form']);self.companion_panel.load(document)
        elif category == 14:
            self.soul_panel.load(document);self.soul_controls()
        elif category == 13:
            self.poly_panel.set_forms(catalog.Catalog(self.packet).tables['polymorph_form'])
            self.poly_panel.load(document)
            self.poly_controls()
        elif category == 11:
            self.tool_panel.load(document)
        elif category == 12:
            self.fishing_panel.set_items(catalog.Catalog(self.packet).items())
            self.fishing_panel.load(document)
        elif category == 4:
            self.costume_panel.load(document)
        elif category == 17:
            self.pet_supply_panel.set_catalog(catalog.Catalog(self.packet));self.pet_supply_panel.load(document)
        elif category == 9:
            self.book_panel.load(document)
        else:
            document = dict(document, child=dict(document['child'], socket_profile_id=document['child'].get('socket_profile_id') or ''))
            self.category_forms[category].load(document)
        if category == 8:
            self.pet_hatch_panel.set_targets(catalog.Catalog(self.packet));self.pet_hatch_panel.load(document)
            loot = document.get('loot') or dict(pick_mode='1', entries=[])
            self.loot_mode.blockSignals(True); self.loot_mode.setCurrentIndex(max(0, self.loot_mode.findData(str(loot['pick_mode'])))); self.loot_mode.blockSignals(False)
            self.loot_table.load(loot['entries'], loot['pick_mode'])
            self.container_kind_changed()
        if category == 10:self.quest_controls()
        if refinable:
            self.refinement.setEnabled(True);self.bonuses.setEnabled(True)
            self.refinement.set_category(category, self.packet['bonuses'] if self.packet else [])
            if self.packet: self.refinement.set_catalog(catalog.Catalog(self.packet), (category, int(base['local_id'])))
            self.refinement.load(document['refinement'], document['levels'], document.get('recipes'))
            self.bonuses.load(document.get('bonuses'))
        if category==7:self.stone_controls()
        if category==15:self.companion_controls()
        if category==17:self.pet_supply_controls()
        if category != 15: self.tabs.addTab(self.pet_food_page, 'Pet táplálása')
        self.tabs.addTab(self.identity_page, 'Azonosítók és megjelenés')
        self.tabs.addTab(self.review_page, 'Mentés áttekintése')
        for index in range(self.tabs.count()):
            self.tabs.setTabToolTip(index, SECTION_HELP.get(self.tabs.tabText(index), CATEGORY_HELP[category][1]))
        while self.overview_links.count():
            item = self.overview_links.takeAt(0)
            if item.widget(): item.widget().deleteLater()
        for label, target in (('Típus és működés beállítása →', self.category_pages[category]),
                              ('Szintkövetelmény, méret és árak →', self.base_page),
                              ('Változtatások áttekintése mentés előtt →', self.review_page)):
            link = QPushButton(label); link.setObjectName('Quiet')
            link.clicked.connect(lambda _, page=target: self.tabs.setCurrentWidget(page))
            self.overview_links.addWidget(link)
        self.loading = False
        self.rebuild_sections(current)
        self.refresh_item_overview(); self.update_badge(); self.controls()
        self.workspace.setCurrentWidget(self.editor_page)

    def collect(self):
        document = copy.deepcopy(self.document); category = int(document['base']['category_id'])
        self.base_form.collect(document); self.use_form.collect(document)
        if not document['appearance'].get('model_asset'): document['appearance'] = None
        if category in (1,2,3,4):self.attribute_panel.collect(document)
        if category == 5:
            document['attribute_service']=None
            document['socket_service']=None
            if self.consumable_kind.currentData()=='5':
                self.attribute_service_panel.collect(document)
                document.update(recovery=None,effect=None,timed=None,toggle=None,cooldown=None)
                check,edit=None,None
            elif self.consumable_kind.currentData()=='4':
                self.socket_service_form.collect(document)
                document.update(recovery=None,effect=None,timed=None,toggle=None,cooldown=None)
                check,edit=None,None
            elif self.consumable_kind.currentData() == '3':
                if document.get('toggle') is None: document.update(catalog.Catalog.toggle_defaults(document['base']['local_id']))
                self.toggle_form.collect(document); self.toggle_group_form.collect(document)
                document['toggle']['group_id'] = document['toggle']['group_id'] or None
                document['toggle_group']['group_id'] = document['toggle']['group_id']
                document['toggle_bonuses'] = self.toggle_bonuses.entries()
                document['recovery'] = None; document['effect'] = None; document['timed'] = None
                check, edit = self.toggle_cooldown_check, self.toggle_cooldown_edit
            elif self.consumable_kind.currentData() == '2':
                if document.get('timed') is None: document.update(catalog.Catalog.timed_defaults(document['base']['local_id']))
                self.timed_form.collect(document); self.timed_group_form.collect(document)
                document['timed']['group_id'] = document['timed']['group_id'] or None
                document['timed_group']['group_id'] = document['timed']['group_id']
                document['timed_bonuses'] = self.timed_bonuses.entries()
                document['recovery'] = None; document['effect'] = None
                document['toggle'] = None
                check, edit = self.timed_cooldown_check, self.timed_cooldown_edit
            else:
                document['timed'] = None; document['toggle'] = None
                if document.get('recovery') is None:
                    fresh = catalog.Catalog(self.packet).new(5)
                    document.update(recovery=fresh['recovery'], effect=fresh['effect'])
                self.category_forms[5].collect(document)
                kind = int(document['recovery']['delivery_kind'])
                holder = {}; self.effect_forms[kind].collect(holder)
                document['effect'] = dict(holder['effect'], delivery_kind=str(kind))
                check, edit = self.cooldown_check, self.cooldown_edit
            if check is not None and check.isChecked():
                try: duration = catalog.scaled(edit.text())
                except api.EditorError: raise api.EditorError('Várakozási idő: érvénytelen szám.') from None
                if not 1 <= duration <= 2 ** 63 - 1: raise api.EditorError('Várakozási idő: 0,001 mp és a megengedett maximum között legyen.')
                document['cooldown'] = dict(duration_ms=str(duration))
            else:
                document['cooldown'] = None
        elif category == 15:
            self.companion_panel.collect(document);document['appearance']=None
        elif category == 14:
            self.soul_panel.collect(document)
        elif category == 13:
            self.poly_panel.collect(document)
            if document['poly']['polymorph_kind']=='2':document['base']['max_stack']='1'
        elif category == 11:
            self.tool_panel.collect(document)
        elif category == 12:
            self.fishing_panel.collect(document)
        elif category == 4:
            self.costume_panel.collect(document)
        elif category == 17:
            self.pet_supply_panel.collect(document)
        elif category == 9:
            self.book_panel.collect(document)
        else:
            self.category_forms[category].collect(document)
            if category == 10:document['child']['group_code']=document['child']['group_code'] or ''
            if category == 3: document['child']['socket_profile_id'] = document['child']['socket_profile_id'] or None
            if category == 8:
                scripted = document['child']['container_kind'] in ('3','4')
                if document['child']['container_kind']=='4':self.pet_hatch_panel.collect(document)
                else:document['hatch']=None
                document['loot'] = None if scripted else dict(pick_mode=self.loot_mode.currentData(), entries=self.loot_table.entries())
        if category in (1, 2, 3, 4, 7, 11, 15):
            if category==15 and document['child']['companion_kind']=='3':
                document['refinement']=dict(profile_kind='1',max_level='0')
                document['levels']=[dict(level='0',**{field:'0' for field in catalog.GAINS},extra_bonus_profile_id=None,bonuses=[])]
            else:self.refinement.collect(document)
            document['bonuses'] = self.bonuses.entries()
        else:
            document['bonuses'] = []
        if category==11 and len(document['tool_levels'])!=len(document['levels']):
            raise api.EditorError('Az eszköz gyakorlási táblájának +0-tól a fejlesztési maximumig kell tartania.')
        if category==7:
            for field in ('primary_group','secondary_group'):document['child'][field]=document['child'][field] or ''
            if document['child']['stone_kind']=='2':
                document['child'].update(allowed_targets='0',minimum_socket_grade='0',primary_group='',secondary_group='')
                document['bonuses']=[];document['refinement']=dict(profile_kind='1',max_level='0')
                document['levels']=[dict(level='0',**{field:'0' for field in catalog.GAINS},extra_bonus_profile_id=None,bonuses=[])]
                document['recipes']=[]
        if category!=15:self.pet_food_panel.collect(document)
        else:document['pet_food_rules']=[]
        return document

    def check(self):
        if self.document is None or self.packet is None or self.active() or self.owner.busy(): return
        try: document = self.collect()
        except api.EditorError as error: self.show_form_error(str(error)); self.owner.error(str(error)); return
        packet, key = self.packet, self.original_key
        def checked(digest):
            self.checked_document = copy.deepcopy(document)
            self.inline_error.hide()
            self.status.setText('Ellenőrzés rendben. A teljes katalógus érvényes a tárgy beállításaival. A piszkozat még nincs mentve.')
            self.refresh_item_overview(document)
        self.run(lambda: catalog.validate(packet, document, key), checked,
            'A játék katalógusellenőrzője vizsgálja a tárgyat…')

    def save(self):
        if self.packet is None or self.document is None: return
        if self.workspace.currentWidget() != self.editor_page or self.active() or self.owner.busy(): return
        state = getattr(self, 'state', None) or {}
        if not self.local() or not state.get('database_running') or state.get('running'):
            self.show_form_error('A mentéshez válassz helyi szervert, indítsd el az adatbázist és állítsd le a játékszervert.')
            return
        if not self.dirty and self.original_key is not None: return
        try: document = self.collect()
        except api.EditorError as error: self.show_form_error(str(error)); self.owner.error(str(error)); return
        root, packet, key = self.owner.root, self.packet, self.original_key
        new_key = (int(document['base']['category_id']), int(document['base']['local_id']))
        def done(result):
            self.dirty = False; self.document = None; self.original_key = None
            self.loaded(result, new_key)
            self.status.setText('Mentve: %s · tárgytípus %d:%d. A katalógus a mentett értékeket mutatja.' % (readable_name(document['base']), *new_key))
            self.owner.statusBar().showMessage('Tárgytípus mentve: %d:%d' % new_key, 6000)
        self.run(lambda: catalog.save(root, packet, document, key), done, 'Tárgy ellenőrzése és mentése a helyi katalógusba…')

    def discard(self):
        if self.original_key is None:
            self.dirty = False; self.show_document(None); return
        self.show_document(catalog.Catalog(self.packet).item(self.original_key))
