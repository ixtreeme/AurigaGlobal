"""Typed book profile UI. No SQL, runtime skill callbacks or legacy item IDs."""
import copy

from PySide6.QtCore import Qt
from PySide6.QtWidgets import (QAbstractItemView, QComboBox, QHBoxLayout, QHeaderView, QLabel,
    QListWidget, QPushButton, QScrollArea, QSplitter, QTabWidget, QTableWidget,
    QTableWidgetItem, QVBoxLayout, QWidget)

import backend as api
import item_catalog as catalog


class SkillBookPanel(QWidget):
    def __init__(self, changed, form_type, parent=None):
        super().__init__(parent)
        self.changed = changed
        self.loading = True
        self.steps = []
        self.current = -1
        self.drafts = {}
        self.current_operation = None
        layout = QVBoxLayout(self)
        samples = QHBoxLayout()
        self.sample = QComboBox()
        for code, name in [('master', 'Mesterkönyv'), ('leadership', 'Vezetés'), ('combo', 'Kombó'),
                           ('language', 'Nyelv'), ('support', 'Új kiegészítő képesség'), ('passive', 'Anti-passzív'),
                           ('forget', 'Felejtőkönyv'), ('horse', 'Lóképességpont')]:
            self.sample.addItem(name, code)
        apply = QPushButton('Szabályminta betöltése')
        apply.clicked.connect(self.apply_sample)
        samples.addWidget(self.sample); samples.addWidget(apply); samples.addStretch()
        layout.addLayout(samples)
        note = QLabel('A minta lecseréli a könyv céljait és szintszabályait. A minták fejlesztői kiindulópontok; '
                      'mentés előtt ellenőrizd a célképességet és az értékeket.')
        note.setWordWrap(True); note.setObjectName('FieldHint'); layout.addWidget(note)
        self.tabs = QTabWidget(); layout.addWidget(self.tabs, 1)
        rules = QWidget(); rules_layout = QVBoxLayout(rules)
        self.head = form_type(self.edited)
        self.operation = self.head.add('book', 'operation_kind', 'Művelet', 'choice',
                                      choices=[(1, 'Képességtanulás'), (2, 'Felejtés és pontvisszaadás'), (3, 'Lóképességpont szerzése')])
        self.target_mode = self.head.add('book', 'target_mode', 'Célzás', 'choice',
                                        choices=[(1, 'Fix képesség'), (2, 'Példányhoz kötött, engedélylistából'), (3, 'Lóképességpont-készlet')])
        self.head.add('book', 'units_per_use', 'Könyv próbálkozásonként', range=(1, 65535))
        self.masteries = self.head.add('book', 'allowed_masteries', 'Képességfokozat', 'bits', names=['Normál', 'Mester', 'Nagymester'])
        self.bypass = self.head.add('book', 'allow_delay_bypass', 'Várakozás', 'bool', text='Várakozáselhagyó hatás használható')
        self.bonus = self.head.add('book', 'bonus_policy', 'Olvasási bónusz', 'choice',
                                  choices=[(1, 'Figyelmen kívül hagyja'), (2, 'Erősített esély és a hatás fogyasztása')])
        self.head.finish(); rules_layout.addWidget(self.head)
        hint = QLabel('Fix célhoz egy sor kell. Példányhoz kötött könyvnél minden megengedett képességet sorolj fel. '
                      'A képességazonosító nem tárgyazonosító; a létezését és a karakter jogosultságát a szerver is ellenőrzi.')
        hint.setWordWrap(True); hint.setObjectName('FieldHint'); rules_layout.addWidget(hint)
        self.targets = QTableWidget(0, 3)
        self.targets.setHorizontalHeaderLabels(['Képesség ID', 'Visszaadott pont', 'Tiltott saját nyelv'])
        self.targets.horizontalHeader().setSectionResizeMode(QHeaderView.ResizeMode.Stretch)
        self.targets.setSelectionBehavior(QAbstractItemView.SelectionBehavior.SelectRows)
        self.targets.setMinimumHeight(130)
        self.targets.itemChanged.connect(lambda *_: self.edited())
        rules_layout.addWidget(self.targets, 1)
        buttons = QHBoxLayout()
        self.add_target = QPushButton('Cél hozzáadása'); self.remove_target = QPushButton('Cél törlése')
        self.add_target.clicked.connect(self.add_target_row)
        self.remove_target.clicked.connect(self.delete_target)
        buttons.addWidget(self.add_target); buttons.addWidget(self.remove_target); buttons.addStretch(); rules_layout.addLayout(buttons)
        area = QScrollArea(); area.setWidgetResizable(True); area.setWidget(rules); self.tabs.addTab(area, 'Cél és használat')

        step_page = QWidget(); step_layout = QVBoxLayout(step_page)
        split = QSplitter(Qt.Orientation.Horizontal)
        left = QWidget(); left_layout = QVBoxLayout(left)
        self.step_list = QListWidget(); self.step_list.currentRowChanged.connect(self.select_step)
        left_layout.addWidget(self.step_list, 1)
        row = QHBoxLayout(); add_step = QPushButton('+'); remove_step = QPushButton('−')
        add_step.setToolTip('Új aktuális szint hozzáadása'); remove_step.setToolTip('Kijelölt szintszabály törlése')
        add_step.clicked.connect(self.add_step); remove_step.clicked.connect(self.delete_step)
        row.addWidget(add_step); row.addWidget(remove_step); left_layout.addLayout(row)
        split.addWidget(left)
        self.step = form_type(self.edited)
        self.step.add('step', 'current_level', 'Jelenlegi szint / szabad lópont', range=(0, 39))
        self.step.add('step', 'next_level', 'Siker után következő szint', range=(0, 40))
        self.step.add('step', 'min_character_level', 'Karakter minimumszintje', range=(0, 65535))
        self.step.add('step', 'required_successes', 'Szükséges sikeres olvasások', range=(1, 65535))
        self.step.add('step', 'success_chance_bp', 'Siker esélye', 'scaled', scale=100, unit='%', range=(0, 10000))
        self.step.add('step', 'boosted_chance_bp', 'Esély olvasási bónusszal', 'scaled', scale=100, unit='%', range=(0, 10000))
        self.step.add('step', 'experience_cost', 'XP próbálkozásonként', range=(0, 2 ** 63 - 1))
        self.step.add('step', 'delay_minimum_ms', 'Olvasási idő minimuma', 'scaled', scale=1000, unit='mp', range=(0, 2 ** 63 - 1))
        self.step.add('step', 'delay_maximum_ms', 'Olvasási idő maximuma', 'scaled', scale=1000, unit='mp', range=(0, 2 ** 63 - 1))
        self.step.add('step', 'delay_on_failure', 'Sikertelen próba', 'bool', text='Sikertelen olvasás után is várni kell')
        self.step.note('Az olvasásszámláló a képességhez tartozik. A megadott szám elérésekor egyszer lép szintet és nulláz; '
                       'a sikertelen próba is elfogyasztja a könyvet. 0% biztos kudarc, 100% biztos siker. A könyv 40-es '
                       'képességhatára nem a tárgyak fejlesztési maximuma.')
        self.step.finish()
        detail = QScrollArea(); detail.setWidgetResizable(True); detail.setWidget(self.step); split.addWidget(detail)
        split.setSizes([155, 650]); split.setStretchFactor(1, 1); step_layout.addWidget(split)
        self.tabs.addTab(step_page, 'Szintenkénti szabályok')
        self.problem = QLabel(); self.problem.setWordWrap(True); self.problem.setObjectName('FieldHint'); layout.addWidget(self.problem)
        self.operation.widget.currentIndexChanged.connect(self.operation_changed)
        self.target_mode.widget.currentIndexChanged.connect(self.target_controls)
        self.bonus.widget.currentIndexChanged.connect(self.bonus_controls)
        self.loading = False

    def edited(self):
        if not self.loading: self.changed()

    def insert_target(self, target):
        row = self.targets.rowCount(); self.targets.insertRow(row)
        self.targets.setItem(row, 0, QTableWidgetItem(str(target['skill_id'])))
        for column, field, choices in [(1, 'refund_pool', ['Nincs', 'Normál', 'Másodlagos', 'Lovas']),
                                        (2, 'forbidden_empire', ['Nincs', 'Shinsoo', 'Chunjo', 'Jinno'])]:
            combo = QComboBox()
            for i, label in enumerate(choices): combo.addItem(label, str(i))
            index = combo.findData(str(target[field])); combo.setCurrentIndex(index)
            combo.currentIndexChanged.connect(lambda *_: self.edited())
            self.targets.setCellWidget(row, column, combo)
        self.target_controls()
        self.edited()

    def add_target_row(self):
        if self.targets.rowCount() >= 254:
            self.problem.setText('Legfeljebb 254 célképesség adható meg.'); return
        if self.target_mode.value() == '1' and self.targets.rowCount(): return
        used = {self.targets.item(row, 0).text().strip() for row in range(self.targets.rowCount())}
        skill = next(i for i in range(1,255) if str(i) not in used)
        self.insert_target(dict(skill_id=str(skill), refund_pool='1' if self.operation.value() == '2' else '0', forbidden_empire='0'))

    def delete_target(self):
        row = self.targets.currentRow()
        if row >= 0: self.targets.removeRow(row); self.target_controls(); self.edited()

    def target_controls(self, *_):
        enabled = self.target_mode.value() != '3'
        for widget in (self.targets, self.add_target, self.remove_target): widget.setEnabled(enabled)
        self.add_target.setEnabled(enabled and self.targets.rowCount() < 254 and
                                   (self.target_mode.value() != '1' or not self.targets.rowCount()))
        operation = self.operation.value()
        for i in range(3): self.target_mode.widget.model().item(i).setEnabled((i == 2) == (operation == '3'))
        for row in range(self.targets.rowCount()):
            for column, kind in [(1, '2'), (2, '1')]:
                widget = self.targets.cellWidget(row, column)
                if widget is not None: widget.setEnabled(operation == kind)

    def operation_changed(self, *_):
        if self.loading: return
        selected, previous = self.operation.value(), self.current_operation
        if previous is not None:
            try:
                draft = {}; self.collect(draft, canonicalize=False); draft['book']['operation_kind'] = previous
            except api.EditorError as error:
                self.operation.widget.blockSignals(True)
                self.operation.widget.setCurrentIndex(self.operation.widget.findData(previous))
                self.operation.widget.blockSignals(False); self.problem.setText(str(error)); return
            self.drafts[previous] = draft
        template = {'1': 'master', '2': 'forget', '3': 'horse'}[selected]
        self.load(self.drafts.get(selected) or catalog.Catalog.book_defaults(template), reset_drafts=False)
        self.edited()

    def apply_sample(self):
        self.load(catalog.Catalog.book_defaults(self.sample.currentData())); self.edited()

    def bonus_controls(self, *_):
        field = next(f for f in self.step.fields if f.column == 'boosted_chance_bp')
        field.widget.setEnabled(self.bonus.value() == '2')
        field.widget.setToolTip('Kikapcsolt olvasási bónusznál a normál sikeresély érvényes; a mentés ezt állítja be minden szinten.')

    def save_step(self):
        if 0 <= self.current < len(self.steps):
            holder = {}; self.step.collect(holder); self.steps[self.current].update(holder['step'])

    def select_step(self, index):
        if self.loading: return
        try: self.save_step()
        except api.EditorError as error:
            self.problem.setText(str(error)); self.step_list.blockSignals(True)
            self.step_list.setCurrentRow(self.current); self.step_list.blockSignals(False); return
        self.problem.clear(); self.current = index
        self.loading = True
        self.step.setEnabled(index >= 0)
        if index >= 0: self.step.load(dict(step=self.steps[index]))
        self.bonus_controls()
        self.loading = False

    def refresh_steps(self, selected=0):
        self.loading = True; self.current = -1; self.step_list.clear()
        for row in self.steps: self.step_list.addItem(row['current_level'] + ' → ' + row['next_level'])
        self.loading = False
        if self.steps: self.step_list.setCurrentRow(min(selected, len(self.steps) - 1))
        else: self.step.setEnabled(False)

    def add_step(self):
        try: self.save_step()
        except api.EditorError as error: self.problem.setText(str(error)); return
        used = {r['current_level'] for r in self.steps}
        operation = self.operation.value()
        masteries = int(self.masteries.value())
        allowed = [i for i in range(40) if masteries & (1 << (2 if i >= 30 else 1 if i >= 20 else 0))]
        level = next((i for i in (range(1,20) if operation == '2' else range(20) if operation == '3' else allowed)
                      if str(i) not in used), None)
        if level is None: return
        row = copy.deepcopy(self.steps[-1]) if self.steps else catalog.Catalog.book_defaults({'2': 'forget', '3': 'horse'}.get(operation, 'master'))['book_steps'][0]
        row.update(current_level=str(level), next_level=str(level - 1 if operation == '2' else level + 1))
        self.steps.append(row); self.refresh_steps(len(self.steps) - 1); self.edited()

    def delete_step(self):
        if self.current >= 0:
            index = self.current; del self.steps[index]; self.refresh_steps(index); self.edited()

    def load(self, document, reset_drafts=True):
        self.loading = True
        if reset_drafts: self.drafts = {}
        self.current_operation = document['book']['operation_kind']
        self.head.load(document); self.targets.setRowCount(0)
        for row in document['book_targets']: self.insert_target(row)
        self.steps = copy.deepcopy(document['book_steps'])
        self.target_controls()
        operation = self.operation.value()
        self.masteries.widget.setEnabled(operation == '1'); self.bonus.widget.setEnabled(operation == '1')
        self.bypass.widget.setEnabled(operation != '2')
        self.problem.clear(); self.loading = False; self.refresh_steps()

    def collect(self, document, canonicalize=True):
        self.save_step(); self.head.collect(document)
        targets = []
        for row in range(self.targets.rowCount()):
            value = catalog.scaled(self.targets.item(row, 0).text(), 1)
            if not 1 <= value <= 254: raise api.EditorError('Képességazonosító: 1 és 254 közötti egész szám kell.')
            targets.append(dict(skill_id=str(value), refund_pool=self.targets.cellWidget(row, 1).currentData(),
                                forbidden_empire=self.targets.cellWidget(row, 2).currentData()))
        steps = copy.deepcopy(self.steps)
        if canonicalize and document['book']['bonus_policy'] == '1':
            for step in steps: step['boosted_chance_bp'] = step['success_chance_bp']
        document.update(book_targets=targets, book_steps=steps)
