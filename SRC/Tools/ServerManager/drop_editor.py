"""Monster drop table editing (IC-035). The dialog edits one group; the item
page saves it through item_catalog and reopens the dialog on a rejection."""
import copy

from PySide6.QtCore import Qt
from PySide6.QtGui import QStandardItem, QStandardItemModel
from PySide6.QtWidgets import (QAbstractItemView, QComboBox, QDialog, QDialogButtonBox, QFormLayout, QHBoxLayout,
                               QHeaderView, QLabel, QLineEdit, QPushButton, QSpinBox, QTableWidget, QTableWidgetItem,
                               QVBoxLayout,QWidget)

import backend as api
import item_catalog as catalog
from content_names import mob_choices, mob_name, mob_record
from reward_catalog import target_text, roll_text
from name_picker import enable_name_search
from reward_terms import BONUS_CHANCE_HEADER,BONUS_CHANCE_LABEL,MOB_BONUS_HELP
from reward_split import RewardDropTable,RewardSplitEditor

ROLL_HELP = {
    '1': 'Minden sor a saját esélyével, a többitől függetlenül esik: egy ölésből több tárgy is jöhet, vagy egy sem.',
    '2': 'Átlagosan N ölésenként esik egy tárgy, és a súlyok döntik el, melyik. A régi „kill” csoport.',
    '3': 'Csak a megadott gyilkos-szinttől, szorzók nélkül: az esély pontosan a táblában álló érték. '
         'A régi „limit” csoport; metinkőnél nem sorsol.',
    '4': 'Mint a „minden bejegyzés”, de csak dupla droppal (tolvajkesztyű, prémium, dupla tárgy bónusz). '
         'Metinkőnél nem sorsol.',
}
RANK_HELP = 'A szörnyrang közös dropja minden ilyen rangú szörnyre érvényes, metinkövekre nem, és csak „minden bejegyzés” módú lehet.'
MAX_INT = 2 ** 31 - 1
MAX_WEIGHT_TOTAL = 2 ** 32 - 1
ITEM, LEVEL, AMOUNT, CHANCE, WEIGHT, RARE, LOW, HIGH, PER_KILL = range(9)


def one_in(percent):
    """'≈ 1/250 000' for a per-kill chance in percent."""
    if percent <= 0: return '—'
    count = round(100.0 / percent)
    return '≈ 1/%s' % format(max(count, 1), ',').replace(',', ' ')


class DropGroupDialog(QDialog):
    """Target, roll mode and entries of one drop table. Chance and weight are
    separate columns, so switching the mode loses nothing; the unused one is
    hidden and saved as 0."""
    COLUMNS = ['Tárgy', 'Szint', 'Darab', 'Esély %', 'Súly', BONUS_CHANCE_HEADER, 'Gyilkos szintje', '…ig', 'Ölésenként']

    def __init__(self, parent, model, document=None, original_id=None, error=None, save_note=None,enable_groups=True):
        super().__init__(parent)
        from item_page import choice_view, item_model
        self.model, self.original_id = model, original_id
        document = copy.deepcopy(document) if document else model.new_drop_group()
        group = document['group']
        self.setWindowTitle((target_text(group) + ' droptáblája') if original_id else 'Új droptábla'); self.resize(1540, 820);self.setMinimumSize(1180,680)
        self.result_document = None; self.deleted = False
        self.items = item_model(model)
        self.stacks = {(int(r['category_id']), int(r['local_id'])): int(r['max_stack']) for r in model.items()}
        outer=QVBoxLayout(self);right=QWidget();layout=QVBoxLayout(right);layout.setContentsMargins(8,0,0,0)
        self.error = QLabel(); self.error.setObjectName('InlineError'); self.error.setWordWrap(True)
        self.error.setTextFormat(Qt.TextFormat.PlainText); layout.addWidget(self.error)
        self.show_error(error)
        self.form = QFormLayout(); layout.addLayout(self.form)
        self.code = QLineEdit(group.get('code') or ''); self.code.setMaxLength(96)
        self.code.hide()  # Retained for round-tripping; names are the editing surface.
        # A saved table keeps its code; a new one follows the target until edited.
        self.code_edited = original_id is not None or document.get('code_edited', False)
        self.code.textEdited.connect(lambda *_: setattr(self, 'code_edited', True))
        self.target = QComboBox()
        for value, label in catalog.DROP_TARGETS.items(): self.target.addItem(label, str(value))
        self.mob = QComboBox(); self.mob.setEditable(True); self.mob.setInsertPolicy(QComboBox.InsertPolicy.NoInsert)
        self.mob.setAccessibleName('Szörny keresése név szerint')
        self.mob.lineEdit().setPlaceholderText('Kezdd el beírni a szörny nevét…')
        mob_model = QStandardItemModel(self.mob)
        mob_model.setHorizontalHeaderLabels(['Szörny neve', 'Szint', 'Rang'])
        for identity, record in mob_choices():
            entry = QStandardItem(record['name']); entry.setData(identity, Qt.ItemDataRole.UserRole)
            mob_model.appendRow([entry, QStandardItem(str(record['level'])),
                                 QStandardItem(catalog.MOB_RANKS.get(record['rank'], 'Ismeretlen rang').split(' (')[0])])
        self.mob.setModel(mob_model); choice_view(self.mob)
        enable_name_search(self.mob,'Kezdd el beírni a szörny nevét…')
        self.mob_hint = QLabel(); self.mob_hint.setObjectName('FieldHint')
        self.rank = QComboBox()
        for value, label in catalog.MOB_RANKS.items(): self.rank.addItem(label.split(' (')[0], str(value))
        self.roll = QComboBox()
        for value, label in catalog.DROP_ROLLS.items(): self.roll.addItem(label, str(value))
        self.kills = QSpinBox(); self.kills.setRange(1, MAX_INT); self.kills.setSuffix(' ölés')
        self.minimum = QSpinBox(); self.minimum.setRange(1, 65535); self.minimum.setPrefix('Lv. ')
        self.help = QLabel(); self.help.setObjectName('FieldHint'); self.help.setWordWrap(True)
        for label, widget in (('Kinek a dropja', self.target), ('Szörny neve', self.mob), ('', self.mob_hint),
                              ('Szörnyrang', self.rank), ('Sorsolás', self.roll), ('Ölések száma (N)', self.kills),
                              ('Legkisebb gyilkos-szint', self.minimum), ('', self.help)):
            self.form.addRow(label, widget)
        self.target.setCurrentIndex(max(0, self.target.findData(str(group['target_kind']))))
        if str(group['target_kind']) == '2': self.rank.setCurrentIndex(max(0, self.rank.findData(str(group['target_value']))))
        else:
            value = str(group['target_value'] or '')
            index = self.mob.findData(value)
            if value and index < 0:
                entry = QStandardItem('Névtelen szörny'); entry.setData(value, Qt.ItemDataRole.UserRole)
                mob_model.appendRow(entry); index = self.mob.findData(value)
            self.mob.setCurrentIndex(index)
        self.roll.setCurrentIndex(max(0, self.roll.findData(str(group['roll_kind']))))
        self.kills.setValue(int(group.get('kills_per_drop') or 0) or 100)
        self.minimum.setValue(int(group.get('minimum_killer_level') or 0) or 1)
        self.table = RewardDropTable(0, len(self.COLUMNS)); self.table.setHorizontalHeaderLabels(self.COLUMNS)
        self.table.horizontalHeader().setSectionResizeMode(ITEM, QHeaderView.ResizeMode.Stretch)
        self.table.horizontalHeader().setMinimumSectionSize(65)
        self.table.setColumnWidth(ITEM,235)
        for column, width in ((LEVEL, 70), (AMOUNT, 90), (CHANCE, 110), (WEIGHT, 90), (RARE, 175), (LOW, 120), (HIGH, 80), (PER_KILL, 175)):
            self.table.setColumnWidth(column, width)
        self.table.horizontalHeaderItem(CHANCE).setToolTip('Esély ölésenként, semleges szorzókkal. A szintkülönbség, a prémium, '
                                                          'a privilégium és az események a régi szabályok szerint módosítják.')
        self.table.horizontalHeaderItem(LOW).setToolTip('A sor csak akkor sorsol, ha a gyilkos szintje az ablakon belül van.')
        self.table.horizontalHeaderItem(PER_KILL).setToolTip('Várható esély egy ölésre, semleges szorzókkal.')
        self.table.horizontalHeaderItem(RARE).setToolTip(MOB_BONUS_HELP)
        self.table.verticalHeader().hide(); self.table.setSelectionBehavior(QAbstractItemView.SelectionBehavior.SelectRows)
        layout.addWidget(self.table, 1)
        row = QHBoxLayout(); add, remove = QPushButton('Tárgy hozzáadása'), QPushButton('Kijelölt törlése')
        add.clicked.connect(lambda: self.append(None)); remove.clicked.connect(self.remove)
        self.total = QLabel(); self.total.setObjectName('Subtle'); self.total.setWordWrap(True)
        row.addWidget(add); row.addWidget(remove); row.addWidget(self.total, 1); layout.addLayout(row)
        for entry in document.get('entries') or []: self.append(entry)
        self.note = QLabel(save_note or 'A módosítások ideiglenes mentésbe kerülnek; a feltöltés és az élő frissítés a közös jóváhagyás után indul.'); self.note.setObjectName('Subtle'); self.note.setWordWrap(True)
        layout.addWidget(self.note)
        buttons = QDialogButtonBox(QDialogButtonBox.StandardButton.Ok | QDialogButtonBox.StandardButton.Cancel)
        self.save_button = buttons.button(QDialogButtonBox.StandardButton.Ok)
        self.save_button.setText('Ideiglenes mentés'); self.save_button.setEnabled(not save_note)
        buttons.button(QDialogButtonBox.StandardButton.Cancel).setText('Mégse')
        self.delete_button = None
        if original_id is not None:
            self.delete_button = buttons.addButton('Droptábla törlése…', QDialogButtonBox.ButtonRole.DestructiveRole)
            self.delete_button.setEnabled(not save_note); self.delete_button.clicked.connect(self.delete)
        buttons.accepted.connect(self.accept_group); buttons.rejected.connect(self.reject); layout.addWidget(buttons)
        for signal in (self.target.currentIndexChanged, self.roll.currentIndexChanged):
            signal.connect(self.mode_changed)
        for signal in (self.mob.currentIndexChanged, self.rank.currentIndexChanged): signal.connect(self.suggest_code)
        self.mob.currentIndexChanged.connect(self.describe_mob)
        self.kills.valueChanged.connect(self.refresh)
        self.describe_mob()
        self.mode_changed()
        self.split=RewardSplitEditor(self.items,right,self.table,self.add_items,self.remove_rows,self);outer.addWidget(self.split)
        if enable_groups:
            from reward_group_ui import GroupPanel
            self.group_panel=GroupPanel(self,parent,model,self.split,self.group_rows,self.add_group)
            self.split.attach_groups(self.group_panel)

    # --- form state ----------------------------------------------------------
    def show_error(self, message):
        self.error.setText('Javítandó: ' + message if message else ''); self.error.setVisible(bool(message))

    def target_kind(self): return self.target.currentData()

    def roll_kind(self): return '1' if self.target_kind() == '2' else self.roll.currentData()

    def target_value(self):
        if self.target_kind() == '2': return str(self.rank.currentData())
        index = self.mob.currentIndex()
        return str(self.mob.currentData()) if index >= 0 and self.mob.currentText() == self.mob.itemText(index) else ''

    def describe_mob(self, *_):
        record = mob_record(self.mob.currentData())
        rank = catalog.MOB_RANKS.get(record.get('rank'), '').split(' (')[0]
        self.mob_hint.setText('%s. szint · %s' % (record['level'], rank) if record else '')

    def mode_changed(self, *_):
        rank = self.target_kind() == '2'
        if rank and self.roll.currentData() != '1': self.roll.setCurrentIndex(self.roll.findData('1'))
        self.roll.setEnabled(not rank)
        roll = self.roll_kind()
        self.form.setRowVisible(self.mob, not rank); self.form.setRowVisible(self.rank, rank)
        self.form.setRowVisible(self.mob_hint, not rank)
        self.form.setRowVisible(self.kills, roll == '2'); self.form.setRowVisible(self.minimum, roll == '3')
        self.help.setText(ROLL_HELP[roll] + (' ' + RANK_HELP if rank else ''))
        # Only the column the mode uses is shown; the other keeps its values.
        self.table.setColumnHidden(CHANCE, roll == '2'); self.table.setColumnHidden(WEIGHT, roll != '2')
        for index in range(self.table.rowCount()): self.row_state(index)
        self.suggest_code(); self.refresh()

    def suggest_code(self, *_):
        if self.code_edited: return
        self.code.setText(self.model.drop_code(self.target_kind(), self.target_value(), self.roll_kind(), self.original_id))

    def row_state(self, row):
        weighted = self.roll_kind() == '2'
        self.table.cellWidget(row, CHANCE).setEnabled(not weighted); self.table.cellWidget(row, WEIGHT).setEnabled(weighted)

    # --- entries ---------------------------------------------------------------
    def append(self, entry):
        from item_page import item_combo
        row = self.table.rowCount(); self.table.insertRow(row)
        entry = entry or dict(refinement_level='0', amount='1', chance_ppb=str(catalog.DROP_CHANCE_SCALE), weight='1',
                              rare_attribute_chance_bp='0', killer_level_min='1', killer_level_max='65535')
        key = '%s:%s' % (entry['category_id'], entry['local_id']) if entry.get('category_id') else None
        item = item_combo(self.items, key)
        level, amount, low, high = QSpinBox(), QSpinBox(), QSpinBox(), QSpinBox()
        level.setPrefix('+'); low.setRange(1, 65535); high.setRange(1, 65535)
        chance = QLineEdit(catalog.display(entry.get('chance_ppb') or 0, catalog.DROP_CHANCE_SCALE) if int(entry.get('chance_ppb') or 0) else '')
        weight = QLineEdit(str(entry.get('weight') or '') if int(entry.get('weight') or 0) else '')
        rare = QLineEdit(catalog.display(entry.get('rare_attribute_chance_bp') or 0, 100))
        rare.setAccessibleName(BONUS_CHANCE_LABEL);rare.setToolTip(MOB_BONUS_HELP)
        chance.setPlaceholderText('pl. 0.5'); weight.setPlaceholderText('pl. 10')
        def limit(*_):
            chosen = self.key_of(item)
            level.setRange(0, (self.model.target_levels(chosen) or 0) if chosen else 0)
            amount.setRange(1, self.stacks.get(chosen, 1) if chosen else 1)
        limit(); item.currentIndexChanged.connect(limit)
        level.setValue(int(entry.get('refinement_level') or 0)); amount.setValue(int(entry.get('amount') or 1))
        low.setValue(int(entry.get('killer_level_min') or 1)); high.setValue(int(entry.get('killer_level_max') or 65535))
        for column, widget in ((ITEM, item), (LEVEL, level), (AMOUNT, amount), (CHANCE, chance), (WEIGHT, weight),
                               (RARE, rare), (LOW, low), (HIGH, high)):
            self.table.setCellWidget(row, column, widget)
        per_kill = QTableWidgetItem(''); per_kill.setFlags(Qt.ItemFlag.ItemIsEnabled); self.table.setItem(row, PER_KILL, per_kill)
        for edit in (chance, weight): edit.textEdited.connect(self.refresh)
        self.row_state(row); self.refresh()

    @staticmethod
    def key_of(combo):
        from item_page import chosen_key
        return chosen_key(combo)

    def remove(self):
        self.remove_rows({index.row() for index in self.table.selectedIndexes()})

    def remove_rows(self,rows):
        rows=sorted(set(rows),reverse=True)
        for row in rows: self.table.removeRow(row)
        if rows: self.refresh()

    def add_items(self,keys):
        if not keys:return False
        if self.table.rowCount()+len(keys)>catalog.MAX_DROP_ENTRIES:
            self.show_error('Legfeljebb %d tárgy lehet egy droptáblában.'%catalog.MAX_DROP_ENTRIES);return False
        if any(tuple(map(int,key.split(':'))) not in self.stacks for key in keys):return False
        for key in keys:
            category,local=key.split(':')
            self.append(dict(category_id=category,local_id=local,refinement_level='0',amount='1',chance_ppb=str(catalog.DROP_CHANCE_SCALE),weight='1',rare_attribute_chance_bp='0',killer_level_min='1',killer_level_max='65535'))
        self.show_error(None);return True

    def group_rows(self,selected=False):
        from reward_groups import from_mob
        rows=sorted({index.row() for index in self.table.selectedIndexes()}) if selected else None
        return from_mob(self.model,self.collect_entries(rows),self.roll_kind()=='2')

    def add_group(self,rows):
        from reward_groups import prepare
        try:
            entries=prepare(rows,self.model,target='mob',weighted=self.roll_kind()=='2',existing_rows=self.table.rowCount(),existing_weight=sum(self.number(row,WEIGHT) or 0 for row in range(self.table.rowCount())))
        except api.EditorError as error:self.show_error(str(error));return False
        self.table.setUpdatesEnabled(False)
        try:
            for entry in entries:self.append(entry)
        finally:self.table.setUpdatesEnabled(True)
        self.show_error(None);return True

    def number(self, row, column, scale=1):
        text = self.table.cellWidget(row, column).text().strip()
        if not text: return None
        try: return catalog.scaled(text, scale)
        except api.EditorError: return None

    def refresh(self, *_):
        roll = self.roll_kind(); rows = range(self.table.rowCount())
        if roll == '2':
            weights = [self.number(row, WEIGHT) for row in rows]
            total = sum(w for w in weights if w and w > 0)
            for row, weight in zip(rows, weights):
                if weight is None or weight <= 0 or not total: text = '?'
                else:
                    percent = 100.0 * weight / total / self.kills.value()
                    text = '%.4g%% (%s)' % (percent, one_in(percent))
                self.table.item(row, PER_KILL).setText(text)
            self.total.setText('Súlyok összege: %d. Átlagosan %d ölésenként pontosan egy sor nyer.' % (total, self.kills.value()))
        else:
            for row in rows:
                ppb = self.number(row, CHANCE, catalog.DROP_CHANCE_SCALE)
                percent = ppb / catalog.DROP_CHANCE_SCALE if ppb and ppb > 0 else 0
                self.table.item(row, PER_KILL).setText('%.4g%% (%s)' % (percent, one_in(percent)) if ppb else '?')
            self.total.setText('Minden sor a saját esélyével, egymástól függetlenül esik.' if roll != '3' else
                               'Az esélyeket semmi nem módosítja; a sor csak a megadott szinttől sorsol.')

    # --- result ----------------------------------------------------------------
    def collect(self):
        if not self.target_value(): raise api.EditorError('Válassz szörnyet a név szerinti listából.')
        code = self.code.text().strip()
        if not code: raise api.EditorError('Adj meg kódot a droptáblának.')
        roll = self.roll_kind()
        group = dict(group_id=self.original_id, code=code, target_kind=self.target_kind(), target_value=self.target_value(),
                     roll_kind=roll, kills_per_drop=str(self.kills.value()) if roll == '2' else '0',
                     minimum_killer_level=str(self.minimum.value()) if roll == '3' else '0')
        return dict(group=group, entries=self.collect_entries(), code_edited=self.code_edited)

    def collect_entries(self,rows=None):
        rows=list(range(self.table.rowCount())) if rows is None else list(rows);roll=self.roll_kind()
        if not rows: raise api.EditorError('A droptábla üres: adj hozzá vagy jelölj ki legalább egy tárgyat.')
        if len(rows) > catalog.MAX_DROP_ENTRIES:
            raise api.EditorError('Legfeljebb %d tárgy lehet egy droptáblában.' % catalog.MAX_DROP_ENTRIES)
        entries, total = [], 0
        for row in rows:
            where = '%d. sor' % (row + 1)
            key = self.key_of(self.table.cellWidget(row, ITEM))
            if key is None: raise api.EditorError(where + ': válassz tárgyat a listából.')
            entry = dict(category_id=str(key[0]), local_id=str(key[1]), refinement_level=str(self.table.cellWidget(row, LEVEL).value()),
                         amount=str(self.table.cellWidget(row, AMOUNT).value()), chance_ppb='0', weight='0',
                         killer_level_min=str(self.table.cellWidget(row, LOW).value()),
                         killer_level_max=str(self.table.cellWidget(row, HIGH).value()))
            if roll == '2':
                weight = self.number(row, WEIGHT)
                if weight is None or weight < 1 or weight > MAX_WEIGHT_TOTAL: raise api.EditorError(where + ': a súly legalább 1 egész szám legyen.')
                total += weight; entry['weight'] = str(weight)
            else:
                chance = self.number(row, CHANCE, catalog.DROP_CHANCE_SCALE)
                if chance is None or not 0 < chance <= catalog.DROP_CHANCE_SCALE * 100:
                    raise api.EditorError(where + ': az esély 0%-nál nagyobb, legfeljebb 100% legyen, legfeljebb 7 tizedesjeggyel.')
                entry['chance_ppb'] = str(chance)
            rare = self.number(row, RARE, 100) if self.table.cellWidget(row, RARE).text().strip() else 0
            if rare is None or not 0 <= rare <= 10000: raise api.EditorError(where + ': a bónusszal létrejövés esélye 0 és 100% között legyen.')
            entry['rare_attribute_chance_bp'] = str(rare)
            if int(entry['killer_level_min']) > int(entry['killer_level_max']):
                raise api.EditorError(where + ': a gyilkos szintablakának alsó határa nagyobb a felsőnél.')
            entries.append(entry)
        if total > MAX_WEIGHT_TOTAL: raise api.EditorError('A súlyok összege túl nagy.')
        return entries

    def accept_group(self):
        try: self.result_document = self.collect()
        except api.EditorError as error:
            self.show_error(str(error)); return
        self.accept()

    def delete(self):
        self.deleted = True; self.accept()
