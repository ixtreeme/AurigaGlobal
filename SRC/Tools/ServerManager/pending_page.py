"""One place to inspect, discard and approve several durable edits."""
import datetime
from PySide6.QtCore import Qt,QTimer
from PySide6.QtWidgets import (QAbstractItemView,QComboBox,QDialog,QDialogButtonBox,QHBoxLayout,QHeaderView,QLabel,QLineEdit,QMessageBox,
    QPushButton,QSplitter,QTableWidget,QTableWidgetItem,QVBoxLayout,QWidget)
import backend as api
import change_review as review
from pending_changes import CATALOG_KINDS
from pending_publish import PendingPublisher

STATES={'staged':'Feltöltésre vár','approved':'Jóváhagyva','publishing':'Feltöltés folyamatban','unconfirmed':'Eredmény ellenőrzése szükséges'}


class PendingPage(QWidget):
    def __init__(self,owner,queue):
        super().__init__();self.owner=owner;self.queue=queue;self.signature=None;self.visible_entries=[]
        self.publisher=PendingPublisher(owner,queue,self)
        layout=QVBoxLayout(self);hint=QLabel('A szerkesztők ide mentik az ideiglenes változatokat. Több szerkesztés után egy közös összesítőben hagyhatod jóvá a feltöltést. Az adatok addig a szerveren változatlanok.');hint.setWordWrap(True);hint.setObjectName('Subtle');layout.addWidget(hint)
        bar=QHBoxLayout();self.search=QLineEdit();self.search.setPlaceholderText('Keresés a függő módosítások neve és tartalma szerint…');self.search.textChanged.connect(self.fill)
        bar.addWidget(self.search,1);self.scope_filter=QComboBox();self.scope_filter.addItems(['Aktuális szerver','Minden szerver']);self.scope_filter.currentIndexChanged.connect(self.fill);bar.addWidget(self.scope_filter)
        self.refresh_button=QPushButton('Lista frissítése');self.refresh_button.clicked.connect(self.reload);bar.addWidget(self.refresh_button);layout.addLayout(bar)
        self.target=QLabel();self.target.setWordWrap(True);layout.addWidget(self.target)
        split=QSplitter(Qt.Orientation.Vertical);self.table=QTableWidget(0,5);self.table.setHorizontalHeaderLabels(['Érintett szerkesztés','Változások','Állapot','Ideiglenesen mentve','Célgép'])
        self.table.setSelectionBehavior(QAbstractItemView.SelectionBehavior.SelectRows);self.table.setSelectionMode(QAbstractItemView.SelectionMode.ExtendedSelection);self.table.setEditTriggers(QAbstractItemView.EditTrigger.NoEditTriggers)
        self.table.setAlternatingRowColors(True);self.table.verticalHeader().hide();self.table.horizontalHeader().setSectionResizeMode(0,QHeaderView.ResizeMode.Stretch)
        for col,width in ((1,110),(2,245),(3,185),(4,220)):self.table.setColumnWidth(col,width)
        self.table.setColumnHidden(4,True)
        self.table.itemSelectionChanged.connect(self.selection_changed);self.table.itemDoubleClicked.connect(self.open_selected);split.addWidget(self.table)
        self.details=QTableWidget(0,4);self.details.setHorizontalHeaderLabels(review.ReviewModel.HEADERS);self.details.setEditTriggers(QAbstractItemView.EditTrigger.NoEditTriggers);self.details.setAlternatingRowColors(True);self.details.verticalHeader().hide()
        self.details.setWordWrap(True);self.details.verticalHeader().setDefaultSectionSize(74)
        self.details.horizontalHeader().setSectionResizeMode(QHeaderView.ResizeMode.Stretch);split.addWidget(self.details);split.setSizes([360,240]);layout.addWidget(split,1)
        bar=QHBoxLayout();self.open_button=QPushButton('Szerkesztés folytatása');self.open_button.clicked.connect(self.open_selected);bar.addWidget(self.open_button)
        self.discard_button=QPushButton('Kijelöltek visszavonása');self.discard_button.setProperty('role','danger');self.discard_button.clicked.connect(self.discard);bar.addWidget(self.discard_button)
        self.check_button=QPushButton('Feltöltési eredmény ellenőrzése');self.check_button.clicked.connect(self.reconcile);bar.addWidget(self.check_button);bar.addStretch();layout.addLayout(bar)
        bar=QHBoxLayout();self.selected_button=QPushButton('Kijelöltek átnézése…');self.selected_button.clicked.connect(lambda:self.approve(True));bar.addWidget(self.selected_button)
        self.approve_button=QPushButton('Összes átnézése és feltöltése…');self.approve_button.setProperty('role','success');self.approve_button.clicked.connect(lambda:self.approve(False));bar.addWidget(self.approve_button);bar.addStretch();layout.addLayout(bar)
        self.status=QLabel('Még nincs függő módosítás.');self.status.setWordWrap(True);layout.addWidget(self.status)
        self.legacy_button=QPushButton('Korábbi ideiglenes mentések…');self.legacy_button.clicked.connect(self.legacy);layout.addWidget(self.legacy_button,alignment=Qt.AlignmentFlag.AlignLeft)
        queue.changed.connect(self.changed)

    def changed(self):
        self.signature=None
        if self.isVisible():self.fill()
        try:count=len(self.queue.entries(current=False))
        except (api.EditorError,OSError,ValueError) as error:self.status.setText(str(error));return
        self.owner.navigation.item(11).setText('Függő módosítások'+(' (%d)'%count if count else ''))

    def reload(self):
        self.queue._loaded_root=None;self.signature=None;self.fill()

    def tick(self):
        signature=(review.encoded(review.scope(self.owner)),self.queue.revision)
        if signature!=self.signature:
            self.changed();self.signature=signature

    def fill(self,*_):
        try:entries=self.queue.entries(current=self.scope_filter.currentIndex()==0)
        except (api.EditorError,OSError,ValueError) as error:self.status.setText(str(error));return
        selected={e['id'] for e in self.selected()};needle=self.search.text().casefold().strip()
        self.visible_entries=[e for e in entries if not needle or needle in (e['body']['title']+' '+str(e['body']['rows'])).casefold()]
        self.table.blockSignals(True);self.table.setRowCount(len(self.visible_entries))
        for row,e in enumerate(self.visible_entries):
            created=datetime.datetime.fromisoformat(e['created']).astimezone().strftime('%Y-%m-%d %H:%M') if e['created'] else ''
            for col,value in enumerate((e['body']['title'],str(len(e['body']['rows'])),STATES[e['state']],created,e['body']['destination'])):
                cell=QTableWidgetItem(value);cell.setData(Qt.ItemDataRole.UserRole,e['id']);self.table.setItem(row,col,cell)
            if e['id'] in selected:self.table.selectRow(row)
        self.table.blockSignals(False)
        self.table.setColumnHidden(4,self.scope_filter.currentIndex()==0)
        self.target.setText('Aktuális cél: '+(entries[0]['body']['destination'] if entries else self.owner.location.text().replace(' • ',' · ')))
        self.status.setText('%d függő szerkesztés · %d látható. Az „Összes” a keresés által elrejtett szerkesztéseket is tartalmazza.'%(len(entries),len(self.visible_entries)))
        self.selection_changed()
        self.signature=(review.encoded(review.scope(self.owner)),self.queue.revision)

    def selected(self):
        return [self.visible_entries[i.row()] for i in self.table.selectionModel().selectedRows() if i.row()<len(self.visible_entries)]

    def selection_changed(self):
        selected=self.selected();rows=self.queue.summary(selected);self.details.setRowCount(len(rows))
        for i,row in enumerate(rows):
            for j,value in enumerate(row):cell=QTableWidgetItem(str(value));cell.setToolTip(str(value));self.details.setItem(i,j,cell)
        idle=not self.owner.busy();ready=all(e['state']=='staged' and e['scope']==review.scope(self.owner) for e in selected)
        can_edit=len(selected)==1 and selected[0]['body']['kind']!='catalog_pet_evolution'
        self.open_button.setEnabled(idle and can_edit and ready);self.discard_button.setEnabled(idle and bool(selected) and ready)
        self.selected_button.setEnabled(idle and bool(selected) and ready);self.approve_button.setEnabled(idle and bool(self.queue.entries()) and all(e['state']=='staged' for e in self.queue.entries()))
        self.check_button.setEnabled(idle and any(e['state']!='staged' for e in selected));self.refresh_button.setEnabled(idle)

    def approve(self,selected):
        entries=self.selected() if selected else self.queue.entries()
        if not entries or self.owner.busy():return
        if any(e.dirty for e in self.owner.editors) or self.owner.item_page.dirty:
            self.owner.error('Az összesítő előtt mentsd ideiglenesen a még szerkesztett mezőket, vagy vesd el a mentetlen szerkesztést.');return
        self.publisher.start(entries)

    def discard(self):
        entries=self.selected()
        if not entries:return
        if QMessageBox.question(self,'Függő szerkesztések visszavonása','Visszavonod ezt a(z) %d ideiglenes szerkesztést? Az éles adatokat ez nem módosítja.'%len(entries),
            QMessageBox.StandardButton.Yes|QMessageBox.StandardButton.No,QMessageBox.StandardButton.No)!=QMessageBox.StandardButton.Yes:return
        try:self.queue.discard({e['id'] for e in entries});self.owner.refresh_pending_views();self.fill()
        except (api.EditorError,OSError,ValueError) as error:self.owner.error(str(error))

    def open_selected(self,*_):
        selected=self.selected()
        if len(selected)!=1 or self.owner.busy():return
        e=selected[0];body=e['body'];data=body['payload'];kind=body['kind']
        if kind=='catalog_pet_evolution':return
        if kind=='file':
            relative=data['relative'];editor=self.owner.config_editor if relative=='server.json' else self.owner.quest_page.editor if relative.startswith('data/quest/') else self.owner.data_page.editor
            page=1 if relative=='server.json' else 3 if relative.startswith('data/quest/') else 2
            if editor.load(relative):self.owner.navigation.setCurrentRow(page)
        elif kind in CATALOG_KINDS:
            page=self.owner.item_page;op=data['operation']
            page.loaded(self.queue.overlay_packet(self.queue.baseline([e])))
            if op['type']=='item':self.owner.navigation.setCurrentRow(8);page.open_item(tuple(op['key']))
            elif op['type']=='container':page.show_containers();page.edit_container(key=tuple(op['key']))
            elif op['type']=='pet_chest':
                self.owner.navigation.setCurrentRow(8);page.open_item(tuple(op['key']))
            elif op['group'] is not None:page.show_drops();page.open_drop_dialog(page.catalog_model(True).drop_group(str(op['key'])),str(op['key']))
            else:self.owner.error('A törlés tervezete a kijelölés visszavonásával állítható vissza.')
        elif kind.startswith('ssh_profile'):
            self.owner.navigation.setCurrentRow(5);self.owner.remote_page.load(data.get('profile') or data['removed'])
        else:self.owner.error('Ennek a műveletnek a paraméterei az összesítőben látszanak. Visszavonás után az eredeti oldalon új tervezet készíthető.')

    def reconcile(self):self.publisher.reconcile(self.selected())

    def legacy(self):
        try:
            dialog=LegacyDrafts(self.owner,self.queue);dialog.exec();dialog.deleteLater();self.fill()
        except (api.EditorError,OSError,ValueError) as error:self.owner.error(str(error))


class LegacyDrafts(QDialog):
    """Older per-save snapshots stay discoverable, and can be explicitly adopted."""
    def __init__(self,owner,queue):
        super().__init__(owner);self.owner=owner;self.queue=queue;self.setWindowTitle('Korábbi ideiglenes mentések');self.resize(1000,600);self.entries=[]
        root=review.directory(owner)
        for path in sorted(root.iterdir(),reverse=True) if root.exists() else []:
            if not (path/'status.json').is_file():continue
            import json
            status=json.loads((path/'status.json').read_text(encoding='utf-8'))
            if status.get('state') not in STATES:continue
            stage=review.Stage(path,status['checksum']);body,_=stage.read();self.entries.append(dict(stage=stage,body=body,state=status['state']))
        layout=QVBoxLayout(self);note=QLabel('A korábbi kezelőverzió külön mentett változatai. Az átvétel a helyi függő listába ment; feltöltés ezután is csak a közös jóváhagyással indul.');note.setWordWrap(True);layout.addWidget(note)
        self.table=QTableWidget(len(self.entries),3);self.table.setHorizontalHeaderLabels(['Szerkesztés','Célgép','Állapot']);self.table.setEditTriggers(QAbstractItemView.EditTrigger.NoEditTriggers);self.table.setSelectionBehavior(QAbstractItemView.SelectionBehavior.SelectRows);self.table.setSelectionMode(QAbstractItemView.SelectionMode.SingleSelection)
        self.table.horizontalHeader().setSectionResizeMode(QHeaderView.ResizeMode.Stretch)
        for i,e in enumerate(self.entries):
            for j,value in enumerate((e['body']['title'],e['body'].get('destination','Korábbi célgép'),STATES[e['state']])):self.table.setItem(i,j,QTableWidgetItem(value))
        layout.addWidget(self.table,1);bar=QHBoxLayout();preview=QPushButton('Tartalom megtekintése');preview.clicked.connect(self.preview);bar.addWidget(preview)
        adopt=QPushButton('Átvétel a függő módosítások közé');adopt.clicked.connect(self.adopt);bar.addWidget(adopt);layout.addLayout(bar)
        buttons=QDialogButtonBox(QDialogButtonBox.StandardButton.Close);buttons.rejected.connect(self.reject);layout.addWidget(buttons)

    def selected(self):
        row=self.table.currentRow();return self.entries[row] if 0<=row<len(self.entries) else None

    def preview(self):
        entry=self.selected()
        if not entry:return
        dialog=review.ChangeReviewDialog(self.owner,entry['stage']);dialog.approve_button.hide();dialog.exec();dialog.deleteLater()

    def adopt(self):
        entry=self.selected()
        if not entry:return
        body=entry['body'];old=body['target'];current=review.target(self.owner)
        if body.get('scope') is not None:matches=body['scope']==review.scope(self.owner)
        else:matches=isinstance(old,list) and len(old)==3 and old[1:]==current[1:]
        if not matches:self.owner.error('Az átvételhez válaszd ki a mentés eredeti szerverét.');return
        if entry['state']!='staged':self.owner.error('Ennek a régebbi feltöltésnek az eredményét előbb ellenőrizni kell.');return
        try:
            files={key:dict(path=str(entry['stage'].path/'files'/key),sha256=value) for key,value in body.get('files',{}).items()}
            self.queue.enqueue(body['title'],body['kind'],body['payload'],body['rows'],files);entry['stage'].state('superseded');self.accept()
        except (api.EditorError,OSError,ValueError) as error:self.owner.error(str(error))
