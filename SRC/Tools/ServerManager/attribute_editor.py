"""Typed instance-attribute authoring. No player instance or SQL editing."""
import copy
from PySide6.QtWidgets import (QCheckBox,QComboBox,QGridLayout,QHBoxLayout,QSizePolicy,QLabel,QPushButton,QTabWidget,QVBoxLayout,QWidget)
from pet_lifecycle_editor import table,cell,text
from growth_pet_editor import number
import item_catalog as catalog
import backend as api

POOLS=[('1','Normál'),('2','Ritka'),('3','Nyolcadik')]
UNITS={1:(1000,'pont'),2:(100,'%'),3:(1,'egész')}

class AttributePanel(QWidget):
    def __init__(self,changed):
        super().__init__();self.changed=changed;self.loading=True;self.definitions=[];self.capacities=[];self.exclusions=[]
        layout=QVBoxLayout(self);self.enabled=QCheckBox('A tárgy kaphat saját példánybónuszokat');layout.addWidget(self.enabled)
        self.body=QWidget();body=QVBoxLayout(self.body);layout.addWidget(self.body)
        self.allow_lock=QCheckBox('Egy normál bónuszhely védhető');body.addWidget(self.allow_lock)
        from PySide6.QtWidgets import QSpinBox
        row=QGridLayout()
        for group_index,(label,maximum) in enumerate( [('Normál',5),('Ritka',2),('Nyolcadik',1)]):
            row.addWidget(QLabel(label),group_index,0);capacity=QSpinBox();capacity.setRange(0,maximum);self.capacities.append(capacity);row.addWidget(capacity,group_index,1)
            exclusion=QCheckBox('Alap-/fejlesztési bónuszt kizár');self.exclusions.append(exclusion);row.addWidget(exclusion,group_index,2)
            capacity.valueChanged.connect(self.notify);exclusion.toggled.connect(self.notify)
        body.addLayout(row)
        hint=QLabel('Szerkesztési minták, nem jóváhagyott balance. A 0 kapacitás kikapcsolja a csoportot. A már megszerzett értékeket az új sorsolási tábla nem írja át. A ritka és nyolcadik csoport közösen tiltja az ismétlődést.')
        hint.setWordWrap(True);body.addWidget(hint)
        self.values=table(['Csoport','Bónusz','Típus súlya','Minőségi szint','Érték','Egység']);body.addWidget(self.values)
        buttons=QHBoxLayout();add=QPushButton('Értékszint hozzáadása');remove=QPushButton('Kijelölt sorok törlése')
        add.clicked.connect(lambda:self.append());remove.clicked.connect(self.remove);buttons.addWidget(add);buttons.addWidget(remove);buttons.addStretch();body.addLayout(buttons)
        self.values.itemChanged.connect(self.notify);self.allow_lock.toggled.connect(self.notify);self.enabled.toggled.connect(self.controls)
        self.loading=False;self.controls()

    def notify(self,*_):
        if not self.loading:self.changed()

    def controls(self,*_):self.body.setVisible(self.enabled.isChecked());self.notify()

    def definition(self,identity):return next((d for d in self.definitions if str(d['id'])==str(identity)),None)

    def append(self,value=None,weight='1'):
        value=value or dict(pool_kind='1',bonus_id=str(self.definitions[0]['id']),tier='1',value_scaled='1000')
        row=self.values.rowCount();self.values.insertRow(row);pool=QComboBox();bonus=QComboBox()
        for identity,label in POOLS:pool.addItem(label,identity)
        from item_page import BONUS_LABELS
        for d in self.definitions:bonus.addItem(BONUS_LABELS.get(d['code'],d['code']),str(d['id']))
        pool.setCurrentIndex(pool.findData(str(value['pool_kind'])));index=bonus.findData(str(value['bonus_id']))
        if index<0:bonus.addItem('Ismeretlen #'+str(value['bonus_id']),str(value['bonus_id']));index=bonus.count()-1
        bonus.setCurrentIndex(index);self.values.setCellWidget(row,0,pool);self.values.setCellWidget(row,1,bonus)
        d=self.definition(value['bonus_id']);scale,unit=UNITS.get(int(d['unit']) if d else 0,(1,'?'))
        for column,content in [(2,weight),(3,value['tier']),(4,catalog.display(value['value_scaled'],scale)),(5,unit)]:cell(self.values,row,column,content,column!=5)
        def update(*_):
            for i in range(self.values.rowCount()):
                if self.values.cellWidget(i,1) is bonus:
                    definition=self.definition(bonus.currentData());cell(self.values,i,5,UNITS.get(int(definition['unit']) if definition else 0,(1,'?'))[1],False);break
            self.notify()
        pool.currentIndexChanged.connect(self.notify);bonus.currentIndexChanged.connect(update);self.notify()

    def remove(self):
        for row in sorted({i.row() for i in self.values.selectedIndexes()},reverse=True):self.values.removeRow(row)
        self.notify()

    def load(self,document,definitions):
        self.loading=True;self.definitions=[d for d in definitions if int(d['sources'])&8]
        self.enabled.setChecked(document.get('attribute_profile') is not None)
        self.allow_lock.setChecked(str((document.get('attribute_profile') or {}).get('allow_lock','0'))=='1')
        groups={r['pool_kind']:r for r in document['attribute_groups']}
        for i in range(3):
            group=groups.get(str(i+1),{});self.capacities[i].setValue(int(group.get('capacity','0')));self.exclusions[i].setChecked(group.get('exclude_static')=='1')
        candidates={(r['pool_kind'],r['bonus_id']):r for r in document['attribute_candidates']};self.values.setRowCount(0)
        for v in document['attribute_values']:self.append(v,candidates[(v['pool_kind'],v['bonus_id'])]['weight'])
        self.controls();self.loading=False

    def collect(self,document):
        if not self.enabled.isChecked():document['attribute_profile']=None;return
        document['attribute_profile']=dict(allow_lock='1' if self.allow_lock.isChecked() else '0')
        groups=[dict(pool_kind=str(i+1),capacity=str(w.value()),exclude_static='1' if self.exclusions[i].isChecked() else '0') for i,w in enumerate(self.capacities) if w.value()]
        allowed={g['pool_kind'] for g in groups};candidates={};values=[]
        for row in range(self.values.rowCount()):
            pool=self.values.cellWidget(row,0).currentData()
            if pool not in allowed:continue
            identity=self.values.cellWidget(row,1).currentData();definition=self.definition(identity)
            if not definition:raise api.EditorError('Ismeretlen példánybónusz.')
            weight=number(text(self.values,row,2),'Típus súlya',1,1000000);key=(pool,identity)
            if key in candidates and candidates[key]['weight']!=weight:raise api.EditorError('Egy bónusztípus minden értékszintjén azonos súly kell.')
            candidates[key]=dict(pool_kind=pool,bonus_id=identity,weight=weight)
            tier=number(text(self.values,row,3),'Minőségi szint',1,16);scale=UNITS[int(definition['unit'])][0]
            values.append(dict(pool_kind=pool,bonus_id=identity,tier=tier,value_scaled=str(catalog.scaled(text(self.values,row,4),scale))))
        document.update(attribute_groups=groups,attribute_candidates=list(candidates.values()),attribute_values=values)


class AttributeServicePanel(QWidget):
    def __init__(self,changed,Form):
        super().__init__();self.changed=changed;self.loading=True;layout=QVBoxLayout(self);self.form=Form(self.notify,2)
        self.operation=self.form.add('attribute_service','operation_kind','Művelet','choice',choices=[('1','Bónusz hozzáadása'),('2','Csoport újrasorsolása'),('3','Védelem hozzáadása'),('4','Védelem áthelyezése'),('5','Védelem törlése')])
        self.pool=self.form.add('attribute_service','pool_kind','Csoport','choice',choices=POOLS)
        self.form.add('attribute_service','units_per_use','Felhasznált darab',range=(1,65535))
        self.success=self.form.add('attribute_service','success_bp','Hozzáadás sikeresélye','scaled',scale=100,range=(1,10000),unit='%')
        self.targets=self.form.add('attribute_service','target_mask','Célok','bits',names=['Fegyver','Páncél','Ékszer','Kosztüm'])
        self.form.add('attribute_service','minimum_count','Legkevesebb meglévő bónusz',range=(0,5))
        self.form.add('attribute_service','maximum_count','Legtöbb meglévő bónusz',range=(0,5))
        self.add_count=self.form.add('attribute_service','add_count','Hozzáadott bónuszok',range=(1,5))
        self.form.add('attribute_service','maximum_target_level','Céltárgy legnagyobb minimumszintje (0: nincs korlát)',range=(0,65535))
        self.consume=self.form.add('attribute_service','consume_on_failure','Sikertelen hozzáadás is fogyaszt','bool')
        self.form.finish();self.form.setSizePolicy(QSizePolicy.Policy.Expanding,QSizePolicy.Policy.Maximum);layout.addWidget(self.form)
        self.tiers=table(['Minőségi szint','Esély (%)']);layout.addWidget(self.tiers)
        buttons=QHBoxLayout();self.add=QPushButton('Minőségi szint hozzáadása');self.remove=QPushButton('Kijelölt törlése');buttons.addWidget(self.add);buttons.addWidget(self.remove);buttons.addStretch();layout.addLayout(buttons)
        hint=QLabel('A minőségi esélyek összege legfeljebb 100%. A hiányzó tartomány sikertelen előkészítés, fogyasztás nélkül. A típusok súlyait a céltárgy Példánybónuszok lapján állítod. A védelem a helyet és a meglévő értéket őrzi meg.')
        hint.setWordWrap(True);layout.addWidget(hint)
        self.add.clicked.connect(lambda:self.add_tier());self.remove.clicked.connect(self.remove_tiers);self.tiers.itemChanged.connect(self.notify);self.operation.widget.currentIndexChanged.connect(self.controls)
        self.loading=False

    def notify(self,*_):
        if not self.loading:self.changed()
    def add_tier(self,value=None):
        value=value or dict(tier=str(self.tiers.rowCount()+1),chance_bp='10000');row=self.tiers.rowCount();self.tiers.insertRow(row);cell(self.tiers,row,0,value['tier']);cell(self.tiers,row,1,catalog.display(value['chance_bp'],100));self.notify()
    def remove_tiers(self):
        for row in sorted({i.row() for i in self.tiers.selectedIndexes()},reverse=True):self.tiers.removeRow(row)
        self.notify()
    def controls(self,*_):
        op=self.operation.widget.currentData();rolling=op in ('1','2');adding=op=='1'
        for widget in (self.tiers,self.add,self.remove):widget.setVisible(rolling)
        for field in (self.success,self.add_count,self.consume):field.widget.setEnabled(adding)
        self.pool.widget.setEnabled(rolling);self.targets.boxes[3].setEnabled(rolling)
        if not self.loading:
            if adding and self.add_count.edit.text() in ('','0'):self.add_count.edit.setText('1')
            if rolling and not self.tiers.rowCount():self.add_tier()
            self.notify()
    def load(self,document):
        self.loading=True;d=document if document.get('attribute_service') else catalog.Catalog.attribute_service_defaults();self.form.load(d);self.tiers.setRowCount(0)
        for value in d['attribute_tiers']:self.add_tier(value)
        self.controls();self.loading=False
    def collect(self,document):
        op=self.operation.widget.currentData();rolling=op in ('1','2');adding=op=='1';ignored={'success_bp','consume_on_failure','add_count'} if not adding else set()
        if not rolling:ignored.add('pool_kind')
        document['attribute_service']={f.column:f.value() for f in self.form.fields if f.column not in ignored}
        head=document['attribute_service']
        if not adding:head.update(success_bp='10000',consume_on_failure='0',add_count='0')
        if not rolling:head.update(pool_kind='1',target_mask=str(int(head['target_mask'])&7))
        document['attribute_tiers']=[dict(tier=number(text(self.tiers,r,0),'Minőségi szint',1,16),chance_bp=str(catalog.scaled(text(self.tiers,r,1),100))) for r in range(self.tiers.rowCount())] if rolling else []
