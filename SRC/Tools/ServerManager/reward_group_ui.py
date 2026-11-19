"""Local reusable item groups beside the ordinary item library."""
import copy
from PySide6.QtCore import Qt,QSize
from PySide6.QtGui import QDrag
from PySide6.QtWidgets import (QAbstractItemView,QDialog,QHBoxLayout,QHeaderView,QLabel,QLineEdit,QListWidget,
    QListWidgetItem,QMenu,QMessageBox,QPushButton,QTableWidget,QTableWidgetItem,QVBoxLayout,QWidget)
import backend as api
import item_catalog as catalog
import reward_groups as groups
from content_names import item_name,search_text
from item_icons import item_icon
from reward_split import GROUP_MIME,ROW_MIME,decode,mime_data
from drop_editor import DropGroupDialog,CHANCE,WEIGHT,PER_KILL,MAX_WEIGHT_TOTAL

ROLE=int(Qt.ItemDataRole.UserRole)


class ItemGroupDialog(DropGroupDialog):
    def __init__(self,parent,model,group=None,entries=None):
        self.original_group=copy.deepcopy(group)
        document=model.new_drop_group();document['group'].update(target_kind='2',target_value='0',roll_kind='1')
        document['entries']=copy.deepcopy(group['entries'] if group else entries or [])
        super().__init__(parent,model,document,enable_groups=False)
        self.setWindowTitle('Tárgycsoport szerkesztése' if group else 'Új tárgycsoport')
        for control in (self.target,self.mob,self.mob_hint,self.rank,self.roll,self.kills,self.minimum,self.help):self.form.setRowVisible(control,False)
        self.group_name=QLineEdit(group['name'] if group else '');self.group_name.setMaxLength(96);self.group_name.setPlaceholderText('pl. Fejlesztő alapanyagok')
        self.group_name.setAccessibleName('Tárgycsoport neve');self.form.insertRow(0,'Csoport neve',self.group_name)
        hint=QLabel('A csoport külön esélyt és súlyt tárol. Beillesztéskor a megnyitott drop sorsolási módjának megfelelő érték érvényesül. A szintkorlát csak mobdropban számít.')
        hint.setWordWrap(True);hint.setObjectName('FieldHint');self.form.insertRow(1,'',hint)
        self.save_button.setText('Helyi csoport mentése');self.note.setText('A csoport helyi sablonként mentődik. A beillesztett sorok önálló másolatok; szerverre a drop ideiglenes mentése és közös jóváhagyása után kerülnek.')
        self.table.setColumnHidden(PER_KILL,True);self.group_name.setFocus()

    def row_state(self,row):
        self.table.cellWidget(row,CHANCE).setEnabled(True);self.table.cellWidget(row,WEIGHT).setEnabled(True)

    def mode_changed(self,*_):
        super().mode_changed();self.table.setColumnHidden(CHANCE,False);self.table.setColumnHidden(WEIGHT,False)

    def refresh(self,*_):
        self.total.setText('%d tárgy a csoportban. A csoport beillesztése a meglévő tartalomhoz ad új sorokat.'%self.table.rowCount())

    def collect(self):
        name=self.group_name.text().strip()
        if not name:raise api.EditorError('Adj nevet a tárgycsoportnak.')
        rows=super().collect_entries();bases=groups.bases_of(self.model);result=[]
        for number,row in enumerate(rows):
            weight=self.number(number,WEIGHT)
            if weight is None or not 1<=weight<=MAX_WEIGHT_TOTAL:raise api.EditorError('%d. sor: a súly pozitív egész szám legyen.'%(number+1))
            result.append(groups.make_entry(self.model,(row['category_id'],row['local_id']),dict(row,weight=str(weight)),bases))
        return dict(name=name,entries=groups.validate_items(result,self.model))


class GroupList(QListWidget):
    def __init__(self,panel):
        super().__init__(panel);self.panel=panel;self.setAccessibleName('Mentett helyi tárgycsoportok');self.setIconSize(QSize(32,32))
        if panel.split is not None:
            self.setDragDropMode(QAbstractItemView.DragDropMode.DragDrop)
            self.setDefaultDropAction(Qt.DropAction.CopyAction)
        self.setEditTriggers(QAbstractItemView.EditTrigger.NoEditTriggers)

    def startDrag(self,actions):
        item=self.currentItem()
        if item is None:return
        drag=QDrag(self);drag.setMimeData(mime_data(GROUP_MIME,self.panel.split.library.session,[item.data(ROLE),item.data(ROLE+1)]));drag.setPixmap(item.icon().pixmap(40,40));drag.exec(Qt.DropAction.CopyAction)

    def removable(self,event):
        if self.panel.split is None:return None
        table=self.panel.split.library.partner
        return decode(event.mimeData(),ROW_MIME,table.session) if event.source() is table else None

    def dragEnterEvent(self,event):
        if self.removable(event):event.setDropAction(Qt.DropAction.MoveAction);event.accept()
        else:event.ignore()

    def dragMoveEvent(self,event):self.dragEnterEvent(event)

    def dropEvent(self,event):
        tokens=self.removable(event)
        if tokens and self.panel.split.library.partner.remove_tokens(tokens):event.setDropAction(Qt.DropAction.MoveAction);event.accept()
        else:event.ignore()


class GroupPanel(QWidget):
    def __init__(self, editor, parent, model, split=None, capture=None, apply=None, *, distribute=None):
        super().__init__();self.editor=editor;self.model=model;self.split=split;self.capture=capture;self.apply_rows=apply
        self.distribute = distribute
        self.store=groups.GroupStore(groups.owner_of(parent));self.bases=groups.bases_of(model);self.loaded=[]
        if distribute:
            layout = QHBoxLayout(self)
            left, details = QVBoxLayout(), QVBoxLayout()
            layout.addLayout(left, 1)
            layout.addLayout(details, 2)
        else:
            layout = QVBoxLayout(self)
            left = details = layout
        layout.setContentsMargins(6,6,6,6)
        title=QLabel('Mentett tárgycsoportok');title.setObjectName('SectionTitle');left.addWidget(title)
        self.search=QLineEdit();self.search.setPlaceholderText('Csoport vagy benne lévő tárgy neve…');left.addWidget(self.search)
        self.list=GroupList(self);left.addWidget(self.list,1)
        if distribute:
            heading = QLabel('A kijelölt csoport tartalma')
            heading.setObjectName('SectionTitle')
            details.addWidget(heading)
        headers = ['Tárgy', 'Db', 'Esély %', 'Súly']
        if distribute:
            headers.insert(1, 'Fejlesztés')
        self.preview = QTableWidget(0, len(headers))
        self.preview.setHorizontalHeaderLabels(headers)
        self.preview.verticalHeader().hide()
        self.preview.setMinimumHeight(120)
        self.preview.setEditTriggers(QAbstractItemView.EditTrigger.NoEditTriggers);self.preview.setIconSize(QSize(28,28))
        if not distribute:self.preview.setMaximumHeight(210)
        self.preview.horizontalHeader().setSectionResizeMode(0,QHeaderView.ResizeMode.Stretch)
        for column in range(1, len(headers)):
            self.preview.horizontalHeader().setSectionResizeMode(column, QHeaderView.ResizeMode.ResizeToContents)
        details.addWidget(self.preview, 1 if distribute else 0)
        self.feedback=QLabel();self.feedback.setWordWrap(True);self.feedback.setObjectName('FieldHint')
        self.feedback.setTextFormat(Qt.TextFormat.PlainText)
        details.addWidget(self.feedback)
        row=QHBoxLayout();new=QPushButton('Új csoport…');self.edit_button=QPushButton('Szerkesztés…');row.addWidget(new);row.addWidget(self.edit_button);left.addLayout(row)
        self.delete_button=QPushButton('Sablon törlése…' if distribute else 'Törlés…')
        actions = QHBoxLayout()
        if capture:
            new.setToolTip('A Tárgyak fülön kijelölt tárgyak induló tartalomként bekerülnek.')
            capture_button = QPushButton('Tartalomból mentés…')
            actions.addWidget(capture_button)
            menu = QMenu(capture_button)
            menu.addAction('Kijelölt tárgysorokból…', lambda: self.capture_group(True))
            menu.addAction('Teljes tartalom tárgyaiból…', lambda: self.capture_group(False))
            capture_button.setMenu(menu)
        actions.addWidget(self.delete_button)
        left.addLayout(actions)
        self.add_button = QPushButton('Teljes csoport hozzáadása több helyre…' if distribute else 'Teljes csoport hozzáadása →')
        if distribute:
            self.add_button.setProperty('role', 'primary')
            self.remove_button = QPushButton('Teljes csoport kivétele több helyről…')
            self.remove_button.setProperty('role', 'danger')
            self.remove_button.clicked.connect(lambda: self.apply_current(operation='remove'))
            distribution = QHBoxLayout()
            distribution.addWidget(self.add_button)
            distribution.addWidget(self.remove_button)
            details.addLayout(distribution)
            self.update_button = QPushButton('Csoport esélye és darabszáma több helyen…')
            self.update_button.setProperty('role', 'primary')
            self.update_button.clicked.connect(lambda: self.apply_current(operation='update'))
            details.addWidget(self.update_button)
        else:
            details.addWidget(self.add_button)
        refresh=QPushButton('Csoportlista frissítése');refresh.clicked.connect(self.refresh);left.addWidget(refresh)
        if not distribute:
            note=QLabel('Húzd a csoportot jobbra a tartalomba. A bemásolt sorokat külön igazíthatod.');note.setWordWrap(True);note.setObjectName('FieldHint');layout.addWidget(note)
        new.clicked.connect(self.new_group);self.edit_button.clicked.connect(self.edit_group);self.delete_button.clicked.connect(self.delete_group)
        self.add_button.clicked.connect(lambda: self.apply_current());self.list.itemDoubleClicked.connect(lambda _:self.apply_current());self.list.currentItemChanged.connect(self.selection_changed);self.search.textChanged.connect(self.filter)
        self.refresh()

    def current(self):
        item=self.list.currentItem();return item.data(ROLE) if item else None

    def fail(self,error):self.feedback.setText(str(error));return False

    def refresh(self,*_):
        try:self.loaded=self.store.groups();self.filter()
        except api.EditorError as error:self.loaded=[];self.filter();self.fail(error)

    def filter(self,*_):
        current=self.current();needle=search_text(self.search.text().strip());self.list.blockSignals(True);self.list.clear()
        for group in self.loaded:
            if needle and needle not in search_text(group['name']+' '+' '.join(row['name'] for row in group['entries'])):continue
            item=QListWidgetItem(group['name']+'\n%d tárgy'%len(group['entries']));item.setData(ROLE,group['id']);item.setData(ROLE+1,groups.revision(group))
            first=group['entries'][0];base=self.bases.get((int(first['category_id']),int(first['local_id'])))
            if base:item.setIcon(item_icon(base.get('icon_asset','')))
            self.list.addItem(item)
            if group['id']==current:self.list.setCurrentItem(item)
        if self.list.currentItem() is None and self.list.count():self.list.setCurrentRow(0)
        self.list.blockSignals(False);self.selection_changed()

    def selection_changed(self,*_):
        identity=self.current();group=next((g for g in self.loaded if g['id']==identity),None)
        for button in (self.edit_button,self.delete_button,self.add_button):button.setEnabled(group is not None)
        if self.distribute:
            self.remove_button.setEnabled(group is not None)
            self.update_button.setEnabled(group is not None)
        self.preview.setRowCount(len(group['entries']) if group else 0)
        if not group:
            self.feedback.setText('Nincs találat.' if self.loaded else 'Az Új csoport gombbal hozhatsz létre helyi tárgycsoportot.')
            return
        for row,value in enumerate(group['entries']):
            base=self.bases.get((int(value['category_id']),int(value['local_id'])));name=item_name(base) if base else value['name']
            values = [name, value['amount'], catalog.display(value['chance_ppb'], catalog.DROP_CHANCE_SCALE), value['weight']]
            if self.distribute:
                values.insert(1, '+' + value['refinement_level'])
            for column,text in enumerate(values):
                item=QTableWidgetItem(text)
                item.setToolTip(text)
                if column==0 and base:item.setIcon(item_icon(base.get('icon_asset','')))
                self.preview.setItem(row,column,item)
        self.feedback.setText('%d tárgysor a sablonban.' % len(group['entries']) if self.distribute else
                              '%d tárgy · beillesztéskor önálló másolat készül.' % len(group['entries']))

    def edit(self,group=None,entries=None):
        try:
            if group:groups.validate_items(group['entries'],self.model)
            dialog=ItemGroupDialog(self.editor,self.model,group,entries)
            try:
                while True:
                    if dialog.exec()!=QDialog.DialogCode.Accepted:return
                    result=dialog.result_document
                    try:
                        saved=self.store.save(result['name'],result['entries'],identity=group['id'] if group else None,expected=groups.revision(group) if group else None)
                        break
                    except api.EditorError as error:dialog.show_error(str(error))
                self.refresh()
                for row in range(self.list.count()):
                    if self.list.item(row).data(ROLE)==saved['id']:self.list.setCurrentRow(row);break
                self.feedback.setText('A csoport helyben mentve. Nem történt szerverfeltöltés.')
            finally:dialog.deleteLater()
        except api.EditorError as error:self.fail(error)

    def new_group(self):
        try:
            keys=self.split.library.keys() if self.split is not None else []
            entries=groups.normalize_entries(groups.from_keys(self.model,keys)) if keys else []
            self.edit(entries=entries)
        except api.EditorError as error:self.fail(error)

    def edit_group(self):
        try:
            if self.current():self.edit(self.store.get(self.current()))
        except api.EditorError as error:self.fail(error)

    def capture_group(self,selected):
        try:self.edit(entries=self.capture(selected))
        except api.EditorError as error:self.fail(error)

    def delete_group(self):
        try:
            if not self.current():return
            group=self.store.get(self.current())
            if QMessageBox.question(self,'Helyi tárgycsoport törlése','Törlöd a(z) „%s” helyi csoportot? A dropokba már bemásolt sorok megmaradnak.'%group['name'])!=QMessageBox.StandardButton.Yes:return
            self.store.delete(group['id'],groups.revision(group));self.refresh()
        except api.EditorError as error:self.fail(error)

    def apply_current(self, *, operation='add'):
        item=self.list.currentItem()
        return self.apply(item.data(ROLE), item.data(ROLE+1), operation=operation) if item else False

    def apply(self, identity, expected, *, operation='add'):
        try:
            group=self.store.get(identity)
            if groups.revision(group)!=expected:raise api.EditorError('A csoport időközben megváltozott. Frissítsd a csoportlistát.')
            if self.distribute:
                self.distribute(group, operation=operation)
                return True
            if not self.apply_rows(copy.deepcopy(group['entries'])):return self.fail('A csoport nem került be. Ellenőrizd a jobb oldali hibaüzenetet.')
            self.feedback.setText('%d tárgy hozzáadva. A bemásolt sorokat külön módosíthatod.'%len(group['entries']));return True
        except api.EditorError as error:return self.fail(error)
