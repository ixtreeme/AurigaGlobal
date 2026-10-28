"""Typed tool progression and fish/bait authoring. No SQL or instance edits."""
import copy
import re
from PySide6.QtCore import Qt
from PySide6.QtWidgets import (QComboBox,QFormLayout,QHBoxLayout,QHeaderView,QLabel,
    QTableWidget,QTableWidgetItem,QVBoxLayout,QWidget)
import backend as api
import item_catalog as catalog


class ToolPanel(QWidget):
    COLUMNS=[('level','Szint'),('power','Eszközerő'),('gather_bonus_bp','Gyűjtési bónusz %'),
             ('required_points','Szükséges pont'),('practice_numerator','Pontesély N'),('practice_denominator','Pontesély D'),
             ('refine_chance_bp','Fejlesztési esély %'),('failure_kind','Kudarc'),('failure_level','Kudarc szintje'),('failure_loss_bp','Pontvesztés %')]
    def __init__(self,changed):
        super().__init__();self.changed=changed;self.loading=True;self.drafts={};self.maximum=0
        layout=QVBoxLayout(self);form=QFormLayout();self.kind=QComboBox();self.pool=QComboBox()
        for i,label in enumerate(('Csákány','Horgászbot','Fejsze','Metszőolló'),1):self.kind.addItem(label,str(i))
        for i,label in enumerate(('Nem halászati eszköz','Első zsákmánytábla','Második zsákmánytábla'),1):self.pool.addItem(label,str(i))
        form.addRow('Eszköz típusa',self.kind);form.addRow('Halászati zsákmány',self.pool);layout.addLayout(form)
        hint=QLabel('A maximumot a Fejlesztés fülön állítsd be. A gyakorlópont külön példányállapot; sikeres fejlesztéskor '
                    'nullázódik. N/D a pontos pontszerzési esély (például 1/5). Kudarc: pontvesztés az aktuális szinten, '
                    'vagy visszaállítás megadott szintre, 0 ponttal. A legmagasabb szinten nincs további fejlődés.')
        hint.setWordWrap(True);layout.addWidget(hint)
        self.table=QTableWidget(0,len(self.COLUMNS));self.table.setHorizontalHeaderLabels([c[1] for c in self.COLUMNS])
        self.table.verticalHeader().hide();self.table.horizontalHeader().setSectionResizeMode(QHeaderView.ResizeMode.ResizeToContents)
        self.table.horizontalHeader().setStretchLastSection(True);layout.addWidget(self.table,1)
        self.table.itemChanged.connect(self.notify);self.kind.currentIndexChanged.connect(self.controls);self.pool.currentIndexChanged.connect(self.notify)
        self.controls();self.loading=False

    def notify(self,*_):
        if not self.loading:self.changed()

    def controls(self,*_):
        rod=self.kind.currentData()=='2';self.pool.setEnabled(rod)
        if rod and self.pool.currentData()=='1':self.pool.setCurrentIndex(self.pool.findData('2'))
        self.notify()

    def load(self,document):
        self.loading=True
        self.kind.setCurrentIndex(self.kind.findData(document['child']['tool_kind']))
        self.pool.setCurrentIndex(self.pool.findData(document['child']['fishing_pool']))
        self.drafts={int(r['level']):copy.deepcopy(r) for r in document['tool_levels']}
        self.maximum=int(document['refinement']['max_level']);self.render();self.controls();self.loading=False

    def capture(self):
        for row in range(self.table.rowCount()):
            values={}
            for col,(field,_) in enumerate(self.COLUMNS):
                if field=='failure_kind':value=self.table.cellWidget(row,col).currentData()
                else:
                    value=self.table.item(row,col).text().strip()
                    if field in ('gather_bonus_bp','refine_chance_bp','failure_loss_bp'):
                        # Keep malformed text until collect() explains it. Raw drafts
                        # are not sent to the catalog until exact conversion succeeds.
                        try:value=str(catalog.scaled(value,100))
                        except api.EditorError:value='!'+value
                    if field=='failure_level' and not value:value=None
                values[field]=value
            previous=self.drafts.get(row,{})
            if values['failure_kind']=='1':values['failure_level']=None
            else:values['failure_loss_bp']='0'
            # Terminal controls are a projection, not a destructive edit to a
            # previously nonterminal draft when the maximum is temporarily lowered.
            if row==self.maximum and previous.get('required_points') not in (None,'0'):
                previous.update(power=values['power'],gather_bonus_bp=values['gather_bonus_bp'])
            else:self.drafts[row]=values

    def render(self):
        was=self.loading;self.loading=True
        # Qt defers deletion of replaced cell widgets. Hide the old controls
        # before rebuilding so they cannot briefly paint over the +0 row.
        for row in range(self.table.rowCount()):
            widget=self.table.cellWidget(row,7)
            if widget is not None:widget.hide()
        self.table.clearContents();self.table.setRowCount(self.maximum+1)
        for row in range(self.maximum+1):
            value=copy.deepcopy(self.drafts.get(row,catalog.Catalog.tool_level(row)))
            if row==self.maximum:
                value.update({k:v for k,v in catalog.Catalog.tool_level(row,True).items() if k not in ('power','gather_bonus_bp')})
            elif value.get('required_points')=='0':
                value.update({k:v for k,v in catalog.Catalog.tool_level(row).items() if k not in ('power','gather_bonus_bp')})
            self.drafts.setdefault(row,copy.deepcopy(value))
            for col,(field,_) in enumerate(self.COLUMNS):
                active=row!=self.maximum or field in ('power','gather_bonus_bp')
                if field=='failure_kind':
                    widget=QComboBox();widget.addItem('Pontvesztés','1');widget.addItem('Szint és pont nullázása','2')
                    widget.setCurrentIndex(widget.findData(value[field]));widget.setEnabled(active)
                    widget.currentIndexChanged.connect(lambda *_,r=row:(self.failure_controls(r),self.notify()));self.table.setCellWidget(row,col,widget)
                else:
                    text=value.get(field)
                    if field in ('gather_bonus_bp','refine_chance_bp','failure_loss_bp') and text is not None:
                        text=text[1:] if text.startswith('!') else catalog.display(text,100)
                    cell=QTableWidgetItem('+'+str(row) if field=='level' else str(text or ''))
                    if field=='level' or not active:cell.setFlags(Qt.ItemFlag.ItemIsEnabled|Qt.ItemFlag.ItemIsSelectable)
                    self.table.setItem(row,col,cell)
            self.failure_controls(row)
        self.loading=was

    def failure_controls(self,row):
        if row>=self.table.rowCount():return
        kind=self.table.cellWidget(row,7).currentData()
        for col,active in ((8,kind=='2'),(9,kind=='1')):
            cell=self.table.item(row,col)
            if cell is not None:
                flags=Qt.ItemFlag.ItemIsEnabled|Qt.ItemFlag.ItemIsSelectable
                if row!=self.maximum and active:flags|=Qt.ItemFlag.ItemIsEditable
                cell.setFlags(flags)

    def set_maximum(self,maximum):
        self.capture();self.maximum=maximum;self.render();self.notify()

    def collect(self,document):
        self.capture();result=[]
        for row in range(self.maximum+1):
            value=copy.deepcopy(self.drafts[row]);value['level']=str(row)
            if row==self.maximum:
                value.update({k:v for k,v in catalog.Catalog.tool_level(row,True).items() if k not in ('power','gather_bonus_bp')})
            for key,text in value.items():
                if text is not None and not re.fullmatch(r'[0-9]+',text):
                    raise api.EditorError(f'Eszköz +{row}, {dict(self.COLUMNS)[key]}: érvénytelen szám.')
            result.append(value)
        document['child'].update(tool_kind=self.kind.currentData(),fishing_pool=self.pool.currentData() if self.kind.currentData()=='2' else '1')
        document['tool_levels']=result


class FishingPanel(QWidget):
    def __init__(self,changed,Form,LootTable):
        super().__init__();self.changed=changed;self.loading=True
        layout=QVBoxLayout(self);self.form=Form(changed)
        self.kind=self.form.add('child','fishing_kind','Típus','choice',choices=[(1,'Élő hal'),(2,'Döglött hal'),(3,'Csali')])
        self.power=self.form.add('child','bait_power','Csali ereje',range=(0,2147483647));self.form.finish();layout.addWidget(self.form)
        row=QHBoxLayout();self.grill=QComboBox();row.addWidget(QLabel('Sütés eredménye'));row.addWidget(self.grill,1);layout.addLayout(row)
        self.mode=QComboBox();self.mode.addItem('Pontosan egy eredmény, súlyozottan','1');self.mode.addItem('Soronkénti esély','2')
        layout.addWidget(self.mode);self.loot=LootTable(changed,allowed_kinds={1});self.loot.table.setColumnHidden(4,True);layout.addWidget(self.loot,1)
        hint=QLabel('Élő hal használatakor egy darab fogy; eredménye csak tárgy lehet. Döglött hal sütése az egész kijelölt '
                    'halmot dolgozza fel ugyanannyi eredménytárggyá. Csali cseréjekor egy darab fogy és felülírja a bot korábbi csaliját.')
        hint.setWordWrap(True);layout.addWidget(hint)
        self.kind.widget.currentIndexChanged.connect(self.controls);self.grill.currentIndexChanged.connect(self.notify)
        self.mode.currentIndexChanged.connect(lambda *_:(self.loot.set_mode(self.mode.currentData()),self.notify()))
        self.loading=False

    def notify(self,*_):
        if not self.loading:self.changed()

    def set_items(self,rows):
        self.grill.clear();self.grill.addItem('Nem süthető','')
        for row in rows:
            if row['category_id']=='5' and row['retired']=='0':
                key=row['category_id']+':'+row['local_id'];self.grill.addItem(key+' '+row['content_code'],key)
        self.loot.set_items(rows)

    def controls(self,*_):
        kind=self.kind.value();self.power.widget.setEnabled(kind=='3');self.grill.setEnabled(kind=='2')
        self.mode.setEnabled(kind=='1');self.loot.setEnabled(kind=='1');self.notify()

    def load(self,document):
        self.loading=True;self.form.load(document)
        child=document['child'];key=child.get('grill_category_id')
        self.grill.setCurrentIndex(max(0,self.grill.findData(key+':'+child['grill_local_id'] if key else '')))
        loot=document.get('loot') or dict(pick_mode='1',entries=[])
        self.mode.setCurrentIndex(self.mode.findData(loot['pick_mode']));self.loot.load(loot['entries'],loot['pick_mode'])
        self.controls();self.loading=False

    def collect(self,document):
        self.form.collect(document);child=document['child'];kind=child['fishing_kind']
        if kind!='3':child['bait_power']='0'
        selected=self.grill.currentData() if kind=='2' else ''
        child['grill_category_id'],child['grill_local_id']=selected.split(':') if selected else (None,None)
        document['loot']=dict(pick_mode=self.mode.currentData(),entries=self.loot.entries()) if kind=='1' else None
