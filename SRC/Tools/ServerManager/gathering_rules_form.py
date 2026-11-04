"""World gathering rules. Saving uses the game's native, catalog-aware parser."""
import copy

from PySide6.QtCore import Signal
from PySide6.QtWidgets import (QComboBox, QDoubleSpinBox, QFormLayout, QHBoxLayout,
    QLabel, QLineEdit, QListWidget, QPushButton, QScrollArea, QSpinBox, QSplitter,
    QTableWidget, QTableWidgetItem, QVBoxLayout, QWidget)

import backend as api
from item_catalog import CATEGORIES


KINDS = {1: 'Csákány', 2: 'Horgászbot', 3: 'Balta', 4: 'Olló'}
POOLS = {1: 'Nincs', 2: 'Első halállomány', 3: 'Második halállomány'}
FIELDS = {
    'id': ('Hely azonosítója', 1, 4294967295, 1),
    'map': ('Map azonosítója', 1, 2147483647, 1),
    'race': ('NPC / lelőhely VNUM', 0, 4294967295, 1),
    'range': ('Hatótáv / dobástáv', 1, 1000, 1),
    'duration_ms': ('Időtartam (másodperc)', 1000, 120000, 1000),
    'minimum_power': ('Szükséges eszközerő', 0, 2147483647, 1),
    'power_step': ('Eszközerő lépésköze', 1, 2147483647, 1),
    'base_chance_bp': ('Alapesély (%)', 0, 10000, 100),
    'power_chance_bp': ('Esély lépésenként (%)', 0, 10000, 100),
    'bait_chance_bp': ('Esély csaliképességenként (%)', 0, 10000, 100),
}


def integer(value, low, high, label):
    if type(value) is not int or not low <= value <= high:
        raise api.EditorError('%s: egész szám szükséges %d és %d között.' % (label, low, high))
    return value


class GatheringRulesForm(QWidget):
    changed = Signal()
    error = Signal(str)

    def __init__(self):
        super().__init__()
        self._value = {'version': 2, 'sites': []}
        self._row = -1
        self._loading = False
        layout = QVBoxLayout(self)
        help_text = QLabel('Szabadvíz: a játékos száraz partról, valódi vízre dob. '
            'Lelőhely: az adott mapon álló NPC vagy gyűjthető objektum VNUM-ja. '
            'Egy kísérlet egy súlyozott jutalomsort választ; a végső esély legfeljebb 100%. '
            'Mentéshez a játékszervert állítsd le, a helyi adatbázis maradjon bekapcsolva. '
            'A módosítás a következő szerverindításkor lép életbe.')
        help_text.setWordWrap(True)
        layout.addWidget(help_text)
        split = QSplitter()
        left = QWidget(); left_layout = QVBoxLayout(left)
        self.sites = QListWidget(); self.sites.currentRowChanged.connect(self.select)
        left_layout.addWidget(self.sites)
        buttons = QHBoxLayout()
        self.add_button = QPushButton('Új hely'); self.add_button.clicked.connect(self.add_site)
        self.delete_button = QPushButton('Hely törlése'); self.delete_button.clicked.connect(self.delete_site)
        buttons.addWidget(self.add_button); buttons.addWidget(self.delete_button)
        left_layout.addLayout(buttons); split.addWidget(left)
        self.detail = QWidget(); detail_layout = QVBoxLayout(self.detail)
        form = QFormLayout()
        self.name = QLineEdit(); self.name.textChanged.connect(self.edited)
        form.addRow('Hely neve', self.name)
        self.mode = QComboBox(); self.mode.addItems(['NPC / lelőhely', 'Szabadvízi halászat'])
        self.mode.currentIndexChanged.connect(self.mode_changed); form.addRow('Hely típusa', self.mode)
        self.kind = QComboBox()
        for key, text in KINDS.items(): self.kind.addItem(text, key)
        self.kind.currentIndexChanged.connect(self.kind_changed); form.addRow('Eszköz', self.kind)
        self.pool = QComboBox()
        for key, text in POOLS.items(): self.pool.addItem(text, key)
        self.pool.currentIndexChanged.connect(self.edited); form.addRow('Halállomány', self.pool)
        self.inputs = {}
        for key, (label, low, high, scale) in FIELDS.items():
            if scale > 1:
                widget = QDoubleSpinBox(); widget.setDecimals(3 if scale == 1000 else 2)
                widget.setRange(low / scale, high / scale)
                widget.setSingleStep(1); widget.valueChanged.connect(self.edited)
            elif high <= 2147483647:
                widget = QSpinBox(); widget.setRange(low, high); widget.valueChanged.connect(self.edited)
            else:
                widget = QLineEdit(); widget.textChanged.connect(self.edited)
            self.inputs[key] = widget; form.addRow(label, widget)
        detail_layout.addLayout(form)
        hint = QLabel('Végső esély = alap + eszközszint bónusza + '
            '⌊eszközerő / lépésköz⌋ × lépésenkénti esély + csali × csalinkénti esély. '
            'A súlyok egymáshoz viszonyított arányok, nem százalékok. '
            'A jutalom tárgyazonosítója az új katalógus kategória:helyi ID párja.')
        hint.setWordWrap(True); detail_layout.addWidget(hint)
        self.rewards = QTableWidget(0, 4)
        self.rewards.setHorizontalHeaderLabels(['Kategória', 'Helyi tárgy-ID', 'Darab', 'Súly'])
        self.rewards.horizontalHeader().setStretchLastSection(True)
        self.rewards.itemChanged.connect(self.edited); detail_layout.addWidget(self.rewards)
        reward_buttons = QHBoxLayout()
        self.reward_add = QPushButton('Jutalom hozzáadása'); self.reward_add.clicked.connect(self.add_reward)
        self.reward_delete = QPushButton('Jutalom törlése'); self.reward_delete.clicked.connect(self.delete_reward)
        reward_buttons.addWidget(self.reward_add); reward_buttons.addWidget(self.reward_delete)
        detail_layout.addLayout(reward_buttons)
        scroll = QScrollArea(); scroll.setWidgetResizable(True); scroll.setWidget(self.detail)
        split.addWidget(scroll); split.setStretchFactor(1, 2); layout.addWidget(split)
        self.set_value(self._value)

    @property
    def value(self):
        self.flush()
        return copy.deepcopy(self._value)

    def set_value(self, value):
        # Reject unrepresentable source values rather than clamp them in Qt controls.
        if not isinstance(value, dict) or set(value) != {'version', 'sites'} or type(value['version']) is not int or value['version'] not in (1, 2) or not isinstance(value['sites'], list) or len(value['sites']) > 256:
            raise api.EditorError('A gyűjtőhelyűrlap 1-es vagy 2-es sémaverziót és sites listát vár. Javítsd a Forráskód lapon.')
        for site in value['sites']:
            if not isinstance(site, dict) or set(site) != set(FIELDS) | {'name', 'kind', 'pool', 'rewards'}:
                raise api.EditorError('Hiányzó vagy ismeretlen gyűjtőhelymező. Javítsd a Forráskód lapon.')
            if not isinstance(site['name'], str) or type(site['kind']) is not int or site['kind'] not in KINDS or type(site['pool']) is not int or site['pool'] not in POOLS:
                raise api.EditorError('Hibás helynév, eszköz vagy halállomány.')
            for key, (label, low, high, _) in FIELDS.items(): integer(site[key], low, high, label)
            if not isinstance(site['rewards'], list) or len(site['rewards']) > 32: raise api.EditorError('Legfeljebb 32 jutalomsor adható meg.')
            for reward in site['rewards']:
                if not isinstance(reward, dict) or set(reward) != {'category', 'local', 'count', 'weight'}: raise api.EditorError('Hibás jutalomsor.')
                integer(reward['category'], 1, 18, 'Kategória')
                for key, maximum in (('local', 4294967295), ('count', 10000), ('weight', 4294967295)):
                    integer(reward[key], 1, maximum, 'Jutalom: ' + key)
        self._loading = True
        try:
            self._value = copy.deepcopy(value); self._row = -1
            self.sites.clear()
            for site in value['sites']: self.sites.addItem(self.label(site))
            self.sites.setCurrentRow(0 if value['sites'] else -1)
            self.show_site(self.sites.currentRow())
        finally: self._loading = False

    @staticmethod
    def label(site):
        return '%s · %s' % (site['id'], site['name'] or 'Névtelen hely')

    def edited(self, *_):
        if not self._loading: self.changed.emit()

    def read_number(self, key):
        label, low, high, scale = FIELDS[key]; widget = self.inputs[key]
        if isinstance(widget, QLineEdit):
            text = widget.text().strip()
            if not text.isascii() or not text.isdigit(): raise api.EditorError(label + ': egész szám szükséges.')
            value = int(text)
        else:
            widget.interpretText(); value = round(widget.value() * scale)
        return integer(value, low, high, label)

    def flush(self):
        if self._row < 0: return
        site = copy.deepcopy(self._value['sites'][self._row])
        site.update({key: self.read_number(key) for key in FIELDS})
        site.update(name=self.name.text(), kind=self.kind.currentData(), pool=self.pool.currentData())
        rewards = []
        for row in range(self.rewards.rowCount()):
            reward = {'category': self.rewards.cellWidget(row, 0).currentData()}
            for col, key, high in ((1, 'local', 4294967295), (2, 'count', 10000), (3, 'weight', 4294967295)):
                text = self.rewards.item(row, col).text().strip()
                if not text.isascii() or not text.isdigit(): raise api.EditorError('A jutalom %s mezője egész szám legyen.' % key)
                reward[key] = integer(int(text), 1, high, 'Jutalom: ' + key)
            rewards.append(reward)
        site['rewards'] = rewards
        self._value['sites'][self._row] = site
        self.sites.item(self._row).setText(self.label(site))

    def select(self, row):
        if self._loading: return
        try: self.flush()
        except api.EditorError as error:
            self._loading = True; self.sites.setCurrentRow(self._row); self._loading = False
            self.error.emit(str(error)); return
        self._loading = True
        try: self.show_site(row)
        finally: self._loading = False

    def show_site(self, row):
        self._row = row; valid = 0 <= row < len(self._value['sites'])
        self.detail.setEnabled(valid); self.delete_button.setEnabled(valid)
        self.add_button.setEnabled(len(self._value['sites']) < 256)
        self.rewards.setRowCount(0)
        if not valid: return
        site = self._value['sites'][row]
        self.name.setText(site['name']); self.mode.setCurrentIndex(1 if site['race'] == 0 else 0)
        self.kind.setCurrentIndex(self.kind.findData(site['kind'])); self.pool.setCurrentIndex(self.pool.findData(site['pool']))
        for key, (_, _, _, scale) in FIELDS.items():
            widget = self.inputs[key]
            if isinstance(widget, QLineEdit): widget.setText(str(site[key]))
            else: widget.setValue(site[key] / scale if scale > 1 else site[key])
        for reward in site['rewards']: self.insert_reward(reward)
        self.controls()

    def controls(self):
        water = self.mode.currentIndex() == 1; rod = self.kind.currentData() == 2
        self.kind.setEnabled(not water); self.inputs['race'].setEnabled(not water)
        self.pool.setEnabled(rod); self.inputs['bait_chance_bp'].setEnabled(rod)
        self.reward_add.setEnabled(self.rewards.rowCount() < 32)

    def mode_changed(self, *_):
        if self._loading: return
        if self.mode.currentIndex() == 1:
            self.kind.setCurrentIndex(self.kind.findData(2)); self.inputs['race'].setText('0')
            if self.pool.currentData() == 1: self.pool.setCurrentIndex(self.pool.findData(2))
            self.inputs['duration_ms'].setValue(max(5, self.inputs['duration_ms'].value()))
            self._value['version'] = 2
        elif self.inputs['race'].text() == '0': self.inputs['race'].setText('1')
        self.controls(); self.edited()

    def kind_changed(self, *_):
        if self._loading: return
        rod = self.kind.currentData() == 2
        if not rod:
            self.pool.setCurrentIndex(self.pool.findData(1)); self.inputs['bait_chance_bp'].setValue(0)
        elif self.pool.currentData() == 1: self.pool.setCurrentIndex(self.pool.findData(2))
        self.controls(); self.edited()

    def insert_reward(self, reward):
        row = self.rewards.rowCount(); self.rewards.insertRow(row)
        category = QComboBox()
        for key, text in CATEGORIES.items(): category.addItem(text, key)
        category.setCurrentIndex(category.findData(reward['category'])); category.currentIndexChanged.connect(self.edited)
        self.rewards.setCellWidget(row, 0, category)
        for col, key in enumerate(('local', 'count', 'weight'), 1): self.rewards.setItem(row, col, QTableWidgetItem(str(reward[key])))

    def add_reward(self):
        if self._row < 0 or self.rewards.rowCount() >= 32: return
        self.insert_reward({'category': 6, 'local': 0, 'count': 1, 'weight': 1})
        self.rewards.setCurrentCell(self.rewards.rowCount() - 1, 1)
        self.rewards.editItem(self.rewards.currentItem()); self.controls(); self.edited()

    def delete_reward(self):
        row = self.rewards.currentRow()
        if row >= 0: self.rewards.removeRow(row); self.controls(); self.edited()

    def add_site(self):
        try: self.flush()
        except api.EditorError as error: self.error.emit(str(error)); return
        if len(self._value['sites']) >= 256: return
        ids = {site['id'] for site in self._value['sites']}; new_id = 1
        while new_id in ids: new_id += 1
        site = dict(id=new_id, name='Új gyűjtőhely', map=1, race=1, kind=1, pool=1,
            range=400, duration_ms=5000, minimum_power=0, power_step=1,
            base_chance_bp=10000, power_chance_bp=0, bait_chance_bp=0, rewards=[])
        self._value['sites'].append(site); self.sites.addItem(self.label(site))
        self.sites.setCurrentRow(len(self._value['sites']) - 1); self.edited()

    def delete_site(self):
        if self._row < 0: return
        row = self._row; del self._value['sites'][row]
        self._loading = True
        try:
            self._row = -1; self.sites.takeItem(row)
            self.sites.setCurrentRow(min(row, len(self._value['sites']) - 1)); self.show_site(self.sites.currentRow())
        finally: self._loading = False
        self.edited()
