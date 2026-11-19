"""Named chest contents and reviewed bulk reward distribution."""
import copy
from PySide6.QtCore import Qt, QTimer
from PySide6.QtWidgets import (QAbstractItemView,QCheckBox,QComboBox,QDialog,QDialogButtonBox,QGridLayout,QHBoxLayout,
    QHeaderView,QLabel,QLineEdit,QPushButton,QSpinBox,QSplitter,QTableWidget,QTableWidgetItem,QVBoxLayout,QWidget)
import backend as api
import item_catalog as catalog
from content_names import item_name,search_text
from reward_catalog import Rewards
from reward_terms import BONUS_CHANCE_LABEL,BULK_BONUS_HELP
from reward_split import RewardSplitEditor

ROLE=int(Qt.ItemDataRole.UserRole)


class ContainerLootDialog(QDialog):
    def __init__(self,parent,packet,key,*,document=None,error=None,save_note=None,model=None):
        super().__init__(parent)
        from item_page import LootTable
        self.rewards=Rewards(packet,model=model);self.key=tuple(key);self.result_document=None
        self.setWindowTitle(item_name(self.rewards.bases[self.key])+' · Láda tartalma');self.resize(1540,820);self.setMinimumSize(1180,680)
        outer=QVBoxLayout(self);right=QWidget();layout=QVBoxLayout(right);layout.setContentsMargins(8,0,0,0)
        title=QLabel(item_name(self.rewards.bases[self.key]));title.setObjectName('SectionTitle');layout.addWidget(title)
        hint=QLabel('Húzd a tárgyakat a tartalomba, vagy a sorokat vissza balra a kivételhez. A módosítások ideiglenes mentésbe kerülnek; jóváhagyás után, újraindítás nélkül élesednek.')
        hint.setWordWrap(True);hint.setObjectName('Subtle');layout.addWidget(hint)
        self.mode=QComboBox();self.mode.addItem('Pontosan egy jutalom, súly szerint','1');self.mode.addItem('Minden jutalom külön, saját eséllyel','2')
        layout.addWidget(self.mode)
        self.loot=LootTable(lambda:None);self.loot.set_items(self.rewards.model.items());layout.addWidget(self.loot,1)
        document=document or self.rewards.loot(self.key)
        self.mode.setCurrentIndex(self.mode.findData(str(document['pick_mode'])))
        self.loot.load(document['entries'],document['pick_mode']);self.mode.currentIndexChanged.connect(lambda:self.loot.set_mode(self.mode.currentData()))
        self.error=QLabel(error or '');self.error.setObjectName('InlineError');self.error.setWordWrap(True);self.error.setVisible(bool(error));layout.addWidget(self.error)
        self.note=QLabel(save_note or '');self.note.setWordWrap(True);self.note.setObjectName('Subtle');layout.addWidget(self.note)
        buttons=QDialogButtonBox(QDialogButtonBox.StandardButton.Save|QDialogButtonBox.StandardButton.Cancel)
        self.save_button=buttons.button(QDialogButtonBox.StandardButton.Save);self.save_button.setText('Ideiglenes mentés');self.save_button.setProperty('role','success')
        self.save_button.setEnabled(not save_note);buttons.button(QDialogButtonBox.StandardButton.Cancel).setText('Mégse')
        buttons.accepted.connect(self.accept_contents);buttons.rejected.connect(self.reject);layout.addWidget(buttons)
        self.split=RewardSplitEditor(self.loot.items,right,self.loot.table,self.add_items,self.loot.remove_rows,self);outer.addWidget(self.split)
        from reward_group_ui import GroupPanel
        self.group_panel=GroupPanel(self,parent,self.rewards.model,self.split,self.group_rows,self.add_group)
        self.split.attach_groups(self.group_panel)

    def add_items(self,keys):
        if not keys:return False
        if self.loot.table.rowCount()+len(keys)>catalog.MAX_DROP_ENTRIES:
            self.error.setText('Legfeljebb %d jutalom lehet egy ládában.'%catalog.MAX_DROP_ENTRIES);self.error.show();return False
        added=self.loot.add_items(keys)
        if added:self.error.hide()
        return added

    def group_rows(self,selected=False):
        from reward_groups import from_chest
        rows=sorted({index.row() for index in self.loot.table.selectedIndexes()}) if selected else range(self.loot.table.rowCount())
        rows=[row for row in rows if self.loot.table.cellWidget(row,0).currentData()=='1']
        return from_chest(self.rewards.model,self.loot.entries(rows),self.mode.currentData()=='1')

    def add_group(self,rows):
        from reward_groups import prepare
        try:
            entries=prepare(rows,self.rewards.model,target='box',weighted=self.mode.currentData()=='1',existing_rows=self.loot.table.rowCount(),existing_weight=sum(self.loot.raw_weight(row) or 0 for row in range(self.loot.table.rowCount())))
        except api.EditorError as error:self.error.setText(str(error));self.error.show();return False
        self.loot.table.setUpdatesEnabled(False)
        try:
            for entry in entries:self.loot.append(entry)
        finally:self.loot.table.setUpdatesEnabled(True)
        self.error.hide();return True

    def collect(self):
        document=dict(pick_mode=self.mode.currentData(),entries=self.loot.entries())
        if not document['entries']:
            document['entries']=[dict(reward_kind='9',item_category_id=None,item_local_id=None,amount='0',weight='1' if document['pick_mode']=='1' else '10000',rare_attribute_chance_bp='0',refinement_level='0')]
        self.rewards.set_loot(self.key,document)
        return document

    def accept_contents(self):
        try:self.result_document=self.collect()
        except api.EditorError as error:self.error.setText(str(error));self.error.show();return
        self.accept()


class GroupDistributionDialog(QDialog):
    """Choose mixed targets for a saved item group."""

    def __init__(self, parent, rewards, group, *, operation='add', save_note=None, selected=(), error=None, update_state=None):
        super().__init__(parent)
        self.rewards = rewards
        self.group = group
        self.save_note = save_note
        self.result_plan = None
        self.target_rows = rewards.targets()
        self.removing = operation == 'remove'
        self.updating = operation == 'update'
        self.match_counts = {}
        if self.removing or self.updating:
            self.match_counts = rewards.group_match_counts(group['entries'])
            self.target_rows = [target for target in self.target_rows if self.match_counts[target['key']]]
        titles = {'add': 'Tárgycsoport hozzáadása több helyre', 'remove': 'Tárgycsoport kivétele több helyről',
                  'update': 'Tárgycsoport esélye és darabszáma több helyen'}
        self.setWindowTitle(titles[operation])
        self.resize(1000, 820 if self.updating else 700)
        layout = QVBoxLayout(self)
        heading = QLabel(f"{group['name']} · {len(group['entries'])} tárgy")
        heading.setTextFormat(Qt.TextFormat.PlainText)
        heading.setObjectName('SectionTitle')
        layout.addWidget(heading)
        if self.updating:
            self.build_update_fields(layout, update_state or {})

        filters = QHBoxLayout()
        self.search = QLineEdit()
        self.search.setPlaceholderText('Láda vagy szörny neve…')
        self.kind = QComboBox()
        self.kind.addItem('Minden cél')
        self.kind.addItem('Csak ládák', 'Láda')
        self.kind.addItem('Csak mobdropok', 'Mobdrop')
        filters.addWidget(self.search, 1)
        filters.addWidget(self.kind)
        layout.addLayout(filters)
        headers = ['Cél neve', 'Típus', 'Sorsolás']
        if self.removing:
            headers.append('Kikerülő sorok')
        elif self.updating:
            headers.append('Egyező sorok')
        self.targets = QTableWidget(len(self.target_rows), len(headers))
        self.targets.setAccessibleName('Kijelölhető ládák és mobdropok')
        self.targets.setHorizontalHeaderLabels(headers)
        self.targets.horizontalHeader().setSectionResizeMode(0, QHeaderView.ResizeMode.Stretch)
        for column in range(1, len(headers)):
            self.targets.horizontalHeader().setSectionResizeMode(column, QHeaderView.ResizeMode.ResizeToContents)
        self.targets.verticalHeader().hide()
        self.targets.setAlternatingRowColors(True)
        self.targets.setEditTriggers(QAbstractItemView.EditTrigger.NoEditTriggers)
        self.targets.setSelectionBehavior(QAbstractItemView.SelectionBehavior.SelectRows)
        checked = set(selected)
        for row, target in enumerate(self.target_rows):
            values = [target['name'], target['kind'], target['mode']]
            if self.removing or self.updating:
                values.append(str(self.match_counts[target['key']]))
            for column, value in enumerate(values):
                cell = QTableWidgetItem(value)
                if column == 0:
                    cell.setCheckState(Qt.CheckState.Checked if target['key'] in checked else Qt.CheckState.Unchecked)
                self.targets.setItem(row, column, cell)
        layout.addWidget(self.targets, 1)
        selection = QHBoxLayout()
        select_visible = QPushButton('Láthatók kijelölése')
        clear = QPushButton('Kijelölések törlése')
        select_visible.clicked.connect(lambda: self.check_targets(True))
        clear.clicked.connect(lambda: self.check_targets(False))
        selection.addWidget(select_visible)
        selection.addWidget(clear)
        selection.addStretch()
        self.summary = QLabel()
        selection.addWidget(self.summary)
        layout.addLayout(selection)
        self.error = QLabel(error or '')
        self.error.setTextFormat(Qt.TextFormat.PlainText)
        self.error.setObjectName('InlineError')
        self.error.setWordWrap(True)
        self.error.setVisible(bool(error))
        layout.addWidget(self.error)
        if self.removing:
            instructions = ('A kijelölt célokból a csoport tárgyainak minden azonos fejlesztési szintű sora kikerül. '
                            'A kiürülő láda nem ad jutalmat, a kiürülő mobdroptábla megszűnik.')
        elif self.updating:
            instructions = ('A fenti értékeket dupla kattintással írhatod át. A kijelölt helyeken az azonos tárgyú és fejlesztési szintű sorok kapják meg őket. '
                            'Súlyozott sorsolásnál az Új súly oszlop érvényes.')
        else:
            instructions = ('Minden kijelölt célhoz új sorok kerülnek. '
                            'A cél sorsolási módja dönti el, hogy az esély vagy a súly érvényes.')
        note = QLabel(save_note or instructions + ' A feltöltést a Függő módosítások oldalon hagyhatod jóvá.')
        note.setObjectName('FieldHint')
        note.setWordWrap(True)
        layout.addWidget(note)

        buttons = QDialogButtonBox(QDialogButtonBox.StandardButton.Save | QDialogButtonBox.StandardButton.Cancel)
        self.save_button = buttons.button(QDialogButtonBox.StandardButton.Save)
        self.save_button.setText('Kivétel ideiglenes mentése' if self.removing else 'Ideiglenes mentés')
        self.save_button.setProperty('role', 'danger' if self.removing else 'success')
        buttons.button(QDialogButtonBox.StandardButton.Cancel).setText('Mégse')
        buttons.accepted.connect(self.accept_plan)
        buttons.rejected.connect(self.reject)
        layout.addWidget(buttons)
        self.search.textChanged.connect(self.filter_targets)
        self.kind.currentIndexChanged.connect(self.filter_targets)
        self.targets.itemChanged.connect(self.selection_changed)
        if self.updating:
            self.amount_enabled.toggled.connect(self.update_columns)
            self.rate_enabled.toggled.connect(self.update_columns)
            self.update_columns()
        self.selection_changed()

    def build_update_fields(self, layout, state):
        options = QHBoxLayout()
        self.amount_enabled = QCheckBox('Darabszám módosítása')
        self.rate_enabled = QCheckBox('Esély / súly módosítása')
        self.amount_enabled.setChecked(state.get('amount_enabled', True))
        self.rate_enabled.setChecked(state.get('rate_enabled', True))
        options.addWidget(self.amount_enabled)
        options.addWidget(self.rate_enabled)
        options.addStretch()
        layout.addLayout(options)
        entries = self.group['entries']
        self.values = QTableWidget(len(entries), 5)
        self.values.setAccessibleName('A kijelölt helyekre másolandó értékek')
        self.values.setHorizontalHeaderLabels(['Tárgy', 'Fejlesztés', 'Új darabszám', 'Új esély %', 'Új súly'])
        self.values.horizontalHeader().setSectionResizeMode(0, QHeaderView.ResizeMode.Stretch)
        for column in range(1, 5):
            self.values.horizontalHeader().setSectionResizeMode(column, QHeaderView.ResizeMode.ResizeToContents)
        self.values.verticalHeader().hide()
        self.values.setAlternatingRowColors(True)
        self.values.setMaximumHeight(min(230, 50 + 32 * len(entries)))
        for row, entry in enumerate(entries):
            default = (entry['amount'], catalog.display(entry['chance_ppb'], catalog.DROP_CHANCE_SCALE), entry['weight'])
            values = state['values'][row] if 'values' in state else default
            base = self.rewards.bases.get((int(entry['category_id']), int(entry['local_id'])))
            name = item_name(base) if base else entry['name']
            for column, value in enumerate((name, '+' + entry['refinement_level'], *values)):
                cell = QTableWidgetItem(value)
                if column < 2:
                    cell.setFlags(cell.flags() & ~Qt.ItemFlag.ItemIsEditable)
                self.values.setItem(row, column, cell)
        layout.addWidget(self.values)

    def update_columns(self):
        self.values.setColumnHidden(2, not self.amount_enabled.isChecked())
        for column in (3, 4):
            self.values.setColumnHidden(column, not self.rate_enabled.isChecked())
        self.selection_changed()

    def update_snapshot(self):
        if not self.updating:
            return None
        return dict(amount_enabled=self.amount_enabled.isChecked(), rate_enabled=self.rate_enabled.isChecked(),
                    values=[tuple(self.values.item(row, column).text() for column in (2, 3, 4))
                            for row in range(self.values.rowCount())])

    def filter_targets(self, *_):
        needle = search_text(self.search.text().strip())
        kind = self.kind.currentData()
        for row, target in enumerate(self.target_rows):
            visible = (kind is None or kind == target['kind']) and needle in search_text(target['name'])
            self.targets.setRowHidden(row, not visible)

    def check_targets(self, checked):
        self.targets.blockSignals(True)
        for row in range(len(self.target_rows)):
            if not checked or not self.targets.isRowHidden(row):
                self.targets.item(row, 0).setCheckState(Qt.CheckState.Checked if checked else Qt.CheckState.Unchecked)
        self.targets.blockSignals(False)
        self.selection_changed()

    def selected(self):
        return [target['key'] for row, target in enumerate(self.target_rows)
                if self.targets.item(row, 0).checkState() == Qt.CheckState.Checked]

    def selection_changed(self, *_):
        selected = self.selected()
        count = len(selected)
        if self.removing or self.updating:
            matched = sum(self.match_counts[target] for target in selected)
            rows = 'kikerülő sor' if self.removing else 'egyező sor'
            self.summary.setText(f'{count} kijelölt cél · {matched} {rows}' if self.target_rows else
                                 'A csoport tárgyai a megadott fejlesztési szinteken sehol sem szerepelnek.')
        else:
            self.summary.setText(f"{count} kijelölt cél · {count * len(self.group['entries'])} új tárgysor")
        fields_selected = not self.updating or self.amount_enabled.isChecked() or self.rate_enabled.isChecked()
        self.save_button.setEnabled(bool(selected) and fields_selected and not self.save_note)

    def accept_plan(self):
        if not self.save_button.isEnabled():
            return
        try:
            if self.updating:
                state = self.update_snapshot()
                amounts, chances, weights = zip(*state['values'])
                self.result_plan = self.rewards.plan_update_group(
                    self.group['entries'], self.selected(),
                    amounts=amounts if state['amount_enabled'] else None,
                    chances=chances if state['rate_enabled'] else None,
                    weights=weights if state['rate_enabled'] else None)
            else:
                plan = self.rewards.plan_remove_group if self.removing else self.rewards.plan_add_group
                self.result_plan = plan(self.group['entries'], self.selected())
            if not self.result_plan['changed']:
                raise api.EditorError('A kijelölt sorokban már ezek az értékek szerepelnek.')
        except api.EditorError as error:
            self.error.setText(str(error))
            self.error.show()
            return
        self.accept()


class BulkRewardDialog(QDialog):
    def __init__(self,parent,packet,*,item=None,state=None,error=None,save_note=None,model=None,operation='add'):
        super().__init__(parent)
        from item_page import item_model,item_combo
        self.operation=(state or {}).get('operation',operation)
        if self.operation not in ('add','update'):raise ValueError('Unknown bulk reward operation')
        self.rewards=Rewards(packet,model=model);self.result_plan=None;self.per_target={};self.save_note=save_note
        self.counts={};self.count_key=None
        self.setWindowTitle('Tárgy esélye és darabszáma több helyen' if self.operation=='update' else 'Tárgy hozzáadása több ládához és mobdrophoz');self.resize(1260,820);self.setMinimumSize(1000,680)
        layout=QVBoxLayout(self)
        hint=QLabel('1. Válassz tárgyat.  2. Jelöld ki a meglévő előfordulásokat.  3. Állítsd be és mentsd a módosítást.' if self.operation=='update' else
                    '1. Válassz tárgyat.  2. Jelöld ki a célokat.  3. Ellenőrizd és mentsd a változásokat együtt.')
        hint.setObjectName('SectionTitle');hint.setWordWrap(True);layout.addWidget(hint)
        form=QGridLayout();layout.addLayout(form)
        self.item=item_combo(item_model(self.rewards.model),'%s:%s'%tuple(item) if item else None)
        form.addWidget(QLabel('Tárgy'),0,0);form.addWidget(self.item,0,1,1,7)
        self.amount=QSpinBox();self.amount.setRange(1,2**31-1)
        self.level=QSpinBox();self.level.setRange(0,255);self.level.setPrefix('+')
        self.rare=QLineEdit('0')
        self.rare.setAccessibleName(BONUS_CHANCE_LABEL);self.rare.setToolTip(BULK_BONUS_HELP)
        self.existing=QComboBox();self.existing.addItem('Meglévő bejegyzés kihagyása','skip');self.existing.addItem('Meglévő bejegyzés módosítása','update')
        self.box_chance=QLineEdit('1');self.drop_chance=QLineEdit('1');self.weight=QSpinBox();self.weight.setRange(1,2**31-1)
        labels={}
        for column,label,widget in ((0,'Darab',self.amount),(2,'Fejlesztés',self.level),(4,BONUS_CHANCE_LABEL,self.rare),(6,'Ha már szerepel',self.existing)):
            labels[column]=QLabel(label);form.addWidget(labels[column],1,column);form.addWidget(widget,1,column+1)
            labels[column].setWordWrap(True)
        labels[4].setToolTip(BULK_BONUS_HELP)
        for column,label,widget in ((0,'Ládában esély %',self.box_chance),(2,'Mobdrop esély %',self.drop_chance),(4,'Súlyozott listákban súly',self.weight)):
            form.addWidget(QLabel(label),2,column);form.addWidget(widget,2,column+1)
        if self.operation=='update':
            self.change_amount=QCheckBox('Darabszám');self.change_amount.setChecked(True)
            self.change_chance=QCheckBox('Esély / súly');self.change_chance.setChecked(True)
            self.all_levels=QCheckBox('Minden fejlesztési szint');self.all_levels.setChecked(True)
            for column in (0,4,6):labels[column].hide()
            self.rare.hide();self.existing.hide();form.addWidget(self.change_amount,1,0);form.addWidget(self.all_levels,1,4,1,2);form.addWidget(self.change_chance,2,6,1,2)
        help=QLabel(('Csak a már szereplő sorok változnak. A megadott érték minden egyező sorra külön érvényes; a többi beállítás megmarad. ' if self.operation=='update' else '')+
                    'Az előnézet Darab és Esély / súly mezői célonként felülírhatók. Súlyozott listánál a súlyból számítjuk a várható esélyt.')
        help.setObjectName('FieldHint');help.setWordWrap(True);layout.addWidget(help)
        split=QSplitter();layout.addWidget(split,1)
        left=QWidget();side=QVBoxLayout(left);side.setContentsMargins(0,0,6,0);split.addWidget(left)
        filters=QHBoxLayout();self.search=QLineEdit();self.search.setPlaceholderText('Láda vagy szörny neve…')
        self.kind=QComboBox();self.kind.addItems(['Minden cél','Csak ládák','Csak mobdropok'])
        filters.addWidget(self.search,1);filters.addWidget(self.kind);side.addLayout(filters)
        self.targets=QTableWidget(0,3);self.targets.setHorizontalHeaderLabels(['Cél neve','Típus','Sorsolás'])
        self.targets.horizontalHeader().setSectionResizeMode(0,QHeaderView.ResizeMode.Stretch)
        for col in (1,2):self.targets.horizontalHeader().setSectionResizeMode(col,QHeaderView.ResizeMode.ResizeToContents)
        self.targets.verticalHeader().hide();self.targets.setAlternatingRowColors(True);self.targets.setWordWrap(False)
        self.targets.setSelectionBehavior(QAbstractItemView.SelectionBehavior.SelectRows);side.addWidget(self.targets,1)
        self.target_rows=self.rewards.targets();self.targets.setRowCount(len(self.target_rows))
        for row,target in enumerate(self.target_rows):
            for column,text in enumerate((target['name'],target['kind'],target['mode'])):
                cell=QTableWidgetItem(text);cell.setData(ROLE,target['key']);cell.setFlags(Qt.ItemFlag.ItemIsEnabled|Qt.ItemFlag.ItemIsSelectable)
                if column==0:cell.setFlags(cell.flags()|Qt.ItemFlag.ItemIsUserCheckable);cell.setCheckState(Qt.CheckState.Unchecked)
                self.targets.setItem(row,column,cell)
        selection=QHBoxLayout();all_visible=QPushButton('Láthatók kijelölése');clear=QPushButton('Kijelölések törlése')
        all_visible.clicked.connect(lambda:self.check_visible(True));clear.clicked.connect(lambda:self.check_visible(False,all_rows=True))
        selection.addWidget(all_visible);selection.addWidget(clear);side.addLayout(selection)
        right=QWidget();review=QVBoxLayout(right);review.setContentsMargins(6,0,0,0);split.addWidget(right)
        title=QLabel('Mentés előnézete');title.setObjectName('SectionTitle');review.addWidget(title)
        self.preview=QTableWidget(0,6);self.preview.setHorizontalHeaderLabels(['Cél','Művelet','Darab','Esély / súly','Várható esély','Típus'])
        self.preview.horizontalHeader().setSectionResizeMode(0,QHeaderView.ResizeMode.Stretch)
        for col in range(1,6):self.preview.horizontalHeader().setSectionResizeMode(col,QHeaderView.ResizeMode.ResizeToContents)
        self.preview.verticalHeader().hide();self.preview.setAlternatingRowColors(True);self.preview.setWordWrap(False);review.addWidget(self.preview,1)
        split.setSizes([560,680])
        self.summary=QLabel('');self.summary.setWordWrap(True);layout.addWidget(self.summary)
        self.error=QLabel(error or '');self.error.setObjectName('InlineError');self.error.setWordWrap(True);self.error.setVisible(bool(error));layout.addWidget(self.error)
        self.note=QLabel(save_note or 'A kijelölt ládák és mobdropok egyetlen ellenőrzött mentéssel frissülnek.');self.note.setObjectName('Subtle');self.note.setWordWrap(True);layout.addWidget(self.note)
        buttons=QDialogButtonBox(QDialogButtonBox.StandardButton.Save|QDialogButtonBox.StandardButton.Cancel)
        self.save_button=buttons.button(QDialogButtonBox.StandardButton.Save);self.save_button.setText('Ideiglenes mentés');self.save_button.setProperty('role','success')
        buttons.button(QDialogButtonBox.StandardButton.Cancel).setText('Mégse');buttons.accepted.connect(self.accept_plan);buttons.rejected.connect(self.reject);layout.addWidget(buttons)
        self.timer=QTimer(self);self.timer.setSingleShot(True);self.timer.setInterval(120);self.timer.timeout.connect(self.refresh)
        for control in (self.item,self.existing):control.currentIndexChanged.connect(self.schedule)
        for control in (self.amount,self.level,self.weight):control.valueChanged.connect(self.schedule)
        for control in (self.rare,self.box_chance,self.drop_chance):control.textChanged.connect(self.schedule)
        if self.operation=='update':
            for control in (self.change_amount,self.change_chance,self.all_levels):control.toggled.connect(self.schedule)
        self.item.editTextChanged.connect(self.schedule);self.targets.itemChanged.connect(self.schedule)
        self.search.textChanged.connect(self.filter);self.kind.currentIndexChanged.connect(self.filter)
        self.preview.itemChanged.connect(self.preview_edited)
        if state:self.restore(state)
        self.refresh()
        if error:self.error.setText(error);self.error.show()

    def schedule(self,*_):self.timer.start()

    def selected(self):
        return [self.targets.item(row,0).data(ROLE) for row in range(self.targets.rowCount())
                if self.targets.item(row,0).checkState()==Qt.CheckState.Checked]

    def filter(self,*_):
        needle=search_text(self.search.text().strip());kind=self.kind.currentIndex()
        for row,target in enumerate(self.target_rows):
            show=(self.operation!='update' or self.counts.get(target['key'],0)>0) and (not needle or needle in search_text(target['name'])) and (not kind or target['kind']==('Láda' if kind==1 else 'Mobdrop'))
            self.targets.setRowHidden(row,not show)

    def check_visible(self,value,all_rows=False):
        self.targets.blockSignals(True)
        for row in range(self.targets.rowCount()):
            if all_rows or not self.targets.isRowHidden(row):self.targets.item(row,0).setCheckState(Qt.CheckState.Checked if value else Qt.CheckState.Unchecked)
        self.targets.blockSignals(False);self.refresh()

    def plan(self):
        from item_page import chosen_key
        key=chosen_key(self.item)
        if key is None:raise api.EditorError('Válassz tárgyat a név szerinti listából.')
        if self.operation=='update':
            return self.rewards.plan_update(key,self.selected(),amount=self.amount.value() if self.change_amount.isChecked() else None,
                box_chance=self.box_chance.text() if self.change_chance.isChecked() else None,
                drop_chance=self.drop_chance.text() if self.change_chance.isChecked() else None,
                weight=self.weight.value() if self.change_chance.isChecked() else None,
                level=None if self.all_levels.isChecked() else self.level.value(),overrides=self.per_target)
        return self.rewards.plan_add(key,self.selected(),amount=self.amount.value(),level=self.level.value(),rare=self.rare.text(),
            box_chance=self.box_chance.text(),drop_chance=self.drop_chance.text(),weight=self.weight.value(),existing=self.existing.currentData(),overrides=self.per_target)

    def refresh(self):
        self.timer.stop();self.save_button.setEnabled(False)
        if self.operation=='update':
            from item_page import chosen_key
            self.amount.setEnabled(self.change_amount.isChecked());self.level.setEnabled(not self.all_levels.isChecked())
            for control in (self.box_chance,self.drop_chance,self.weight):control.setEnabled(self.change_chance.isChecked())
            key=chosen_key(self.item);level=None if self.all_levels.isChecked() else self.level.value()
            if (key,level)!=self.count_key:
                self.count_key=(key,level);self.counts=self.rewards.occurrences(key,level) if key else {};self.filter()
        try:plan=self.plan()
        except api.EditorError as error:self.error.setText(str(error));self.error.show();self.preview.setRowCount(0);self.summary.clear();return
        self.error.hide();self.preview.blockSignals(True);self.preview.setRowCount(len(plan['preview']))
        for row,change in enumerate(plan['preview']):
            for column,text in enumerate((change['name'],change['action'],change['amount'],change['rate'],change['chance'],change['kind'])):
                cell=QTableWidgetItem(text);cell.setData(ROLE,change['target']);cell.setToolTip(text)
                editable=column in (2,3)
                if self.operation=='update':
                    editable=editable and change['matched']>0 and (self.change_amount.isChecked() if column==2 else self.change_chance.isChecked())
                    cell.setToolTip('%d egyező sor\nDarab: %s → %s\nVárható esély soronként: %s → %s'%(change['matched'],change['before_amount'],change['amount'],change['before_chance'],change['chance']))
                if not editable:cell.setFlags(cell.flags()&~Qt.ItemFlag.ItemIsEditable)
                self.preview.setItem(row,column,cell)
        self.preview.blockSignals(False)
        self.summary.setText(('%d kijelölt cél · %d módosítás · %d módosuló bejegyzés'%(len(plan['preview']),plan['changed'],sum(row['altered'] for row in plan['preview']))) if self.operation=='update' else
                             '%d kijelölt cél · %d módosítás · %d változatlan, már szereplő bejegyzés'%(len(plan['preview']),plan['changed'],len(plan['preview'])-plan['changed']))
        self.save_button.setEnabled(bool(plan['changed']) and not self.save_note)

    def preview_edited(self,cell):
        if cell.column() not in (2,3):return
        key=cell.data(ROLE);field='amount' if cell.column()==2 else 'rate'
        self.per_target.setdefault(key,{})[field]=cell.text();self.schedule()

    def snapshot(self):
        from item_page import chosen_key
        result=dict(item=chosen_key(self.item),selected=self.selected(),amount=self.amount.value(),level=self.level.value(),rare=self.rare.text(),
                    box_chance=self.box_chance.text(),drop_chance=self.drop_chance.text(),weight=self.weight.value(),existing=self.existing.currentData(),
                    per_target=copy.deepcopy(self.per_target),operation=self.operation)
        if self.operation=='update':result.update(change_amount=self.change_amount.isChecked(),change_chance=self.change_chance.isChecked(),all_levels=self.all_levels.isChecked())
        return result

    def restore(self,state):
        if state.get('item'):self.item.setCurrentIndex(self.item.findData('%s:%s'%tuple(state['item'])))
        for name in ('amount','level','weight'):getattr(self,name).setValue(state[name])
        for name in ('rare','box_chance','drop_chance'):getattr(self,name).setText(state[name])
        self.existing.setCurrentIndex(self.existing.findData(state['existing']));self.per_target=copy.deepcopy(state.get('per_target',{}))
        if self.operation=='update':
            for name in ('change_amount','change_chance','all_levels'):getattr(self,name).setChecked(state.get(name,True))
        wanted=set(state.get('selected',[]))
        for row in range(self.targets.rowCount()):
            if self.targets.item(row,0).data(ROLE) in wanted:self.targets.item(row,0).setCheckState(Qt.CheckState.Checked)

    def accept_plan(self):
        try:self.result_plan=self.plan()
        except api.EditorError as error:self.error.setText(str(error));self.error.show();return
        if self.result_plan['changed'] and not self.save_note:self.accept()


class BulkRemoveDialog(QDialog):
    """Review removals before one checked, live mixed publication."""
    selected=BulkRewardDialog.selected
    check_visible=BulkRewardDialog.check_visible

    def __init__(self,parent,packet,*,item=None,state=None,error=None,save_note=None,model=None):
        super().__init__(parent)
        from item_page import item_model,item_combo
        self.rewards=Rewards(packet,model=model);self.result_plan=None;self.save_note=save_note;self.count_key=None;self.counts={}
        self.setWindowTitle('Tárgy eltávolítása több ládából és mobdropból');self.resize(1200,760);self.setMinimumSize(960,640)
        layout=QVBoxLayout(self)
        hint=QLabel('1. Válassz tárgyat.  2. Jelöld ki, honnan kerüljön ki.  3. Ellenőrizd és mentsd az eltávolítást.')
        hint.setObjectName('SectionTitle');hint.setWordWrap(True);layout.addWidget(hint)
        form=QGridLayout();layout.addLayout(form)
        self.item=item_combo(item_model(self.rewards.model),'%s:%s'%tuple(item) if item else None)
        form.addWidget(QLabel('Tárgy'),0,0);form.addWidget(self.item,0,1,1,5)
        self.level_mode=QComboBox();self.level_mode.addItems(['Minden fejlesztési szint','Csak a megadott fejlesztési szint'])
        self.level=QSpinBox();self.level.setRange(0,255);self.level.setPrefix('+');self.level.setEnabled(False)
        form.addWidget(QLabel('Eltávolítás'),1,0);form.addWidget(self.level_mode,1,1,1,3);form.addWidget(self.level,1,4)
        split=QSplitter();layout.addWidget(split,1)
        left=QWidget();side=QVBoxLayout(left);side.setContentsMargins(0,0,6,0);split.addWidget(left)
        filters=QHBoxLayout();self.search=QLineEdit();self.search.setPlaceholderText('Láda vagy szörny neve…')
        self.kind=QComboBox();self.kind.addItems(['Minden cél','Csak ládák','Csak mobdropok'])
        filters.addWidget(self.search,1);filters.addWidget(self.kind);side.addLayout(filters)
        self.targets=QTableWidget(0,3);self.targets.setHorizontalHeaderLabels(['Cél neve','Típus','Egyező sorok'])
        self.targets.horizontalHeader().setSectionResizeMode(0,QHeaderView.ResizeMode.Stretch)
        for column in (1,2):self.targets.horizontalHeader().setSectionResizeMode(column,QHeaderView.ResizeMode.ResizeToContents)
        self.targets.verticalHeader().hide();self.targets.setAlternatingRowColors(True);self.targets.setWordWrap(False)
        self.targets.setSelectionBehavior(QAbstractItemView.SelectionBehavior.SelectRows);side.addWidget(self.targets,1)
        self.target_rows=self.rewards.targets();self.targets.setRowCount(len(self.target_rows))
        for row,target in enumerate(self.target_rows):
            for column,text in enumerate((target['name'],target['kind'],'0')):
                cell=QTableWidgetItem(text);cell.setData(ROLE,target['key']);cell.setFlags(Qt.ItemFlag.ItemIsEnabled|Qt.ItemFlag.ItemIsSelectable)
                if column==0:cell.setFlags(cell.flags()|Qt.ItemFlag.ItemIsUserCheckable);cell.setCheckState(Qt.CheckState.Unchecked)
                self.targets.setItem(row,column,cell)
        buttons=QHBoxLayout();visible=QPushButton('Láthatók kijelölése');clear=QPushButton('Kijelölések törlése')
        visible.clicked.connect(lambda:self.check_visible(True));clear.clicked.connect(lambda:self.check_visible(False,all_rows=True))
        buttons.addWidget(visible);buttons.addWidget(clear);side.addLayout(buttons)
        right=QWidget();review=QVBoxLayout(right);review.setContentsMargins(6,0,0,0);split.addWidget(right)
        title=QLabel('Eltávolítás előnézete');title.setObjectName('SectionTitle');review.addWidget(title)
        self.preview=QTableWidget(0,5);self.preview.setHorizontalHeaderLabels(['Cél','Művelet','Kikerülő sorok','Megmaradó sorok','Típus'])
        self.preview.horizontalHeader().setSectionResizeMode(0,QHeaderView.ResizeMode.Stretch)
        for column in range(1,5):self.preview.horizontalHeader().setSectionResizeMode(column,QHeaderView.ResizeMode.ResizeToContents)
        self.preview.verticalHeader().hide();self.preview.setAlternatingRowColors(True);self.preview.setWordWrap(False)
        self.preview.setEditTriggers(QAbstractItemView.EditTrigger.NoEditTriggers);review.addWidget(self.preview,1);split.setSizes([500,680])
        self.summary=QLabel();self.summary.setWordWrap(True);layout.addWidget(self.summary)
        self.error=QLabel();self.error.setObjectName('InlineError');self.error.setWordWrap(True);layout.addWidget(self.error)
        note=QLabel(save_note or 'A kijelölteken a tárgy minden egyező sora kikerül. Üres láda nem ad jutalmat; az üres mobdroptábla megszűnik. A mentés élőben élesedik.')
        note.setObjectName('Subtle');note.setWordWrap(True);layout.addWidget(note)
        actions=QDialogButtonBox(QDialogButtonBox.StandardButton.Save|QDialogButtonBox.StandardButton.Cancel)
        self.save_button=actions.button(QDialogButtonBox.StandardButton.Save);self.save_button.setText('Eltávolítás ideiglenes mentése');self.save_button.setProperty('role','danger')
        actions.button(QDialogButtonBox.StandardButton.Cancel).setText('Mégse');actions.accepted.connect(self.accept_plan);actions.rejected.connect(self.reject);layout.addWidget(actions)
        self.timer=QTimer(self);self.timer.setSingleShot(True);self.timer.setInterval(120);self.timer.timeout.connect(self.refresh)
        self.item.currentIndexChanged.connect(self.schedule);self.item.editTextChanged.connect(self.schedule)
        self.level_mode.currentIndexChanged.connect(self.schedule);self.level.valueChanged.connect(self.schedule);self.targets.itemChanged.connect(self.schedule)
        self.search.textChanged.connect(self.filter);self.kind.currentIndexChanged.connect(self.filter)
        if state:self.restore(state)
        self.refresh()
        if error:self.error.setText(error);self.error.show()

    def schedule(self,*_):self.timer.start()

    def chosen(self):
        from item_page import chosen_key
        return chosen_key(self.item),self.level.value() if self.level_mode.currentIndex()==1 else None

    def filter(self,*_):
        needle=search_text(self.search.text().strip());kind=self.kind.currentIndex()
        for row,target in enumerate(self.target_rows):
            show=self.counts.get(target['key'],0)>0 and (not needle or needle in search_text(target['name'])) and (not kind or target['kind']==('Láda' if kind==1 else 'Mobdrop'))
            self.targets.setRowHidden(row,not show)

    def plan(self):
        key,level=self.chosen()
        if key is None:raise api.EditorError('Válassz tárgyat a név szerinti listából.')
        return self.rewards.plan_remove(key,self.selected(),level=level)

    def refresh(self):
        self.timer.stop();self.save_button.setEnabled(False);self.level.setEnabled(self.level_mode.currentIndex()==1)
        key,level=self.chosen()
        if (key,level)!=self.count_key:
            self.count_key=(key,level);self.counts=self.rewards.occurrences(key,level) if key else {}
            self.targets.blockSignals(True)
            for row,target in enumerate(self.target_rows):self.targets.item(row,2).setText(str(self.counts.get(target['key'],0)))
            self.targets.blockSignals(False);self.filter()
        try:plan=self.plan()
        except api.EditorError as error:self.error.setText(str(error));self.error.show();self.preview.setRowCount(0);self.summary.clear();return
        self.error.hide();self.preview.setRowCount(len(plan['preview']))
        for row,change in enumerate(plan['preview']):
            for column,text in enumerate((change['name'],change['action'],str(change['removed']),str(change['remaining']),change['kind'])):
                cell=QTableWidgetItem(text);cell.setToolTip(text);self.preview.setItem(row,column,cell)
        self.summary.setText('%d kijelölt cél · %d módosítás · %d eltávolítandó bejegyzés' %
                             (len(plan['preview']),plan['changed'],sum(row['removed'] for row in plan['preview'])))
        self.save_button.setEnabled(bool(plan['changed']) and not self.save_note)

    def snapshot(self):
        key,level=self.chosen()
        return dict(item=key,level=level,selected=self.selected())

    def restore(self,state):
        if state.get('item'):self.item.setCurrentIndex(self.item.findData('%s:%s'%tuple(state['item'])))
        self.level_mode.setCurrentIndex(0 if state.get('level') is None else 1);self.level.setValue(state.get('level') or 0)
        wanted=set(state.get('selected',[]))
        for row in range(self.targets.rowCount()):
            if self.targets.item(row,0).data(ROLE) in wanted:self.targets.item(row,0).setCheckState(Qt.CheckState.Checked)

    def accept_plan(self):
        try:self.result_plan=self.plan()
        except api.EditorError as error:self.error.setText(str(error));self.error.show();return
        if self.result_plan['changed'] and not self.save_note:self.accept()
