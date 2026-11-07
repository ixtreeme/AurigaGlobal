"""Typed stole/rune rules and special bonus tools; no player-state editing."""
from PySide6.QtWidgets import QWidget,QVBoxLayout,QFormLayout,QHBoxLayout,QLabel,QComboBox,QSpinBox,QLineEdit,QPushButton
from pet_lifecycle_editor import table,cell,text
from growth_pet_editor import number
from attribute_editor import UNITS
import item_catalog as catalog
import backend as api

class SpecialBonusPanel(QWidget):
    def __init__(self,changed,tools=False):
        super().__init__();self.changed=changed;self.tools=tools;self.loading=True;self.definitions=[]
        layout=QVBoxLayout(self);form=QFormLayout();layout.addLayout(form)
        self.kind=QComboBox();choices=['Stóla hat bónuszának cseréje','Rúnatöltés','Jelmezreset (három normál bónusz)','Jelmez ritka bónuszának adása','Jelmez kiválasztott ritka bónuszának törlése','Kapcsolt normál/skill bónusz'] if tools else ['Hat bónuszos stóla','Töltéses rúna']
        for i,label in enumerate(choices,1):self.kind.addItem(label,str(i))
        form.addRow('Működés',self.kind)
        self.first=QSpinBox();self.second=QSpinBox();self.first.setRange(1,65535 if tools else 7);self.second.setRange(1,4)
        form.addRow('Felhasznált darab' if tools else 'Rúna helye (1–7)',self.first)
        if not tools:form.addRow('Stóla fokozata',self.second)
        self.capacity=QLineEdit('0');self.initial=QLineEdit('0');form.addRow('Tárolt töltés (%)' if tools else 'Teljes töltés (mp)',self.capacity)
        if not tools:form.addRow('Kezdő töltés (mp)',self.initial)
        self.bonus=QComboBox();self.value=QLineEdit('1')
        if tools:form.addRow('Adott ritka bónusz',self.bonus);form.addRow('Adott bónusz értéke',self.value)
        self.rows=table(['Dobás (1–3)','Értékszint','Esély (%)'] if tools else ['Bónuszhely','Változat (1–4)','Minimum töltés (%)','Bónusz','Érték','Egység']);layout.addWidget(self.rows)
        buttons=QHBoxLayout();add=QPushButton('Sor hozzáadása');remove=QPushButton('Kijelölt sorok törlése');buttons.addWidget(add);buttons.addWidget(remove);layout.addLayout(buttons)
        add.clicked.connect(lambda:self.append());remove.clicked.connect(self.remove)
        self.seed=QPushButton('Szerkeszthető mintatáblázat betöltése');layout.addWidget(self.seed);self.seed.clicked.connect(self.seed_rows)
        hint=QLabel('A már kiadott bónusz és töltés megmarad. Stólához hat külön bónusz és négy értékváltozat kell. Rúnához két bónusz töltési fokozatai kellenek; a hetedik hely készletfeltételes és nem fogyaszt töltést.' if not tools else 'A reset három dobásának esélye külön adható meg; mindhárom összege 100%. A bónuszadó és a részben használható töltő tárgy nem halmozható. A művelet csak a megerősítés után fogyaszt anyagot.');hint.setWordWrap(True);layout.addWidget(hint)
        self.kind.currentIndexChanged.connect(self.controls);self.rows.itemChanged.connect(self.notify);self.first.valueChanged.connect(self.controls);self.second.valueChanged.connect(self.notify);self.capacity.textEdited.connect(self.notify);self.initial.textEdited.connect(self.notify);self.value.textEdited.connect(self.notify);self.bonus.currentIndexChanged.connect(self.notify);self.loading=False;self.controls()
    def notify(self,*_):
        if not self.loading:self.changed()
    def seed_rows(self):
        self.rows.setRowCount(0)
        if self.tools:
            for draw in range(3):self.append(dict(draw_no=str(draw),tier='1',chance_bp='10000'))
        elif self.kind.currentData()=='1':
            for index,bonus in enumerate((29,30,31,32,1,3)):
                for variant in range(4):self.append(dict(bonus_index=str(index),variant_no=str(variant),minimum_charge_bp='0',bonus_id=str(bonus),value_scaled=str((variant+1)*(1000 if bonus<=4 else 1))))
        else:
            if self.first.value()<7 and catalog.scaled(self.capacity.text())==0:self.capacity.setText('600');self.initial.setText('600')
            for index,bonus in enumerate((29,1)):
                for tier,bp in enumerate((0,) if self.first.value()==7 else (0,600,1100,2100,4100,6100,8100)):
                    self.append(dict(bonus_index=str(index),variant_no='0',minimum_charge_bp=str(bp),bonus_id=str(bonus),value_scaled=str((tier+1)*(1000 if bonus<=4 else 1))))
        self.notify()
    def controls(self,*_):
        kind=self.kind.currentData()
        if self.tools:self.capacity.setEnabled(kind=='2');self.bonus.setEnabled(kind=='4');self.value.setEnabled(kind=='4');self.rows.setVisible(kind=='3');self.seed.setVisible(kind=='3')
        else:self.first.setEnabled(kind=='2');self.second.setEnabled(kind=='1');self.capacity.setEnabled(kind=='2' and self.first.value()<7);self.initial.setEnabled(self.capacity.isEnabled())
        self.notify()
    def append(self,row=None):
        index=self.rows.rowCount();self.rows.insertRow(index)
        if self.tools:
            row=row or dict(draw_no='0',tier='1',chance_bp='10000')
            for j,v in enumerate((str(int(row['draw_no'])+1),row['tier'],catalog.display(row['chance_bp'],100))):cell(self.rows,index,j,v)
        else:
            row=row or dict(bonus_index='0',variant_no='0',minimum_charge_bp='0',bonus_id='29',value_scaled='1');bonus=QComboBox()
            from item_page import BONUS_LABELS
            for d in self.definitions:bonus.addItem(BONUS_LABELS.get(d['code'],d['code']),str(d['id']))
            bonus.setCurrentIndex(bonus.findData(str(row['bonus_id'])));self.rows.setCellWidget(index,3,bonus);bonus.currentIndexChanged.connect(self.notify)
            definition=next((d for d in self.definitions if str(d['id'])==str(row['bonus_id'])),None);scale,unit=UNITS.get(int(definition['unit']) if definition else 0,(1,'?'))
            for j,v in [(0,str(int(row['bonus_index'])+1)),(1,str(int(row['variant_no'])+1)),(2,catalog.display(row['minimum_charge_bp'],100)),(4,catalog.display(row['value_scaled'],scale)),(5,unit)]:cell(self.rows,index,j,v,j!=5)
        self.notify()
    def remove(self):
        for row in sorted({i.row() for i in self.rows.selectedIndexes()},reverse=True):self.rows.removeRow(row)
        self.notify()
    def load(self,document,definitions):
        self.loading=True;self.definitions=[d for d in definitions if int(d['sources'])&8];child=document['child'];self.kind.setCurrentIndex(self.kind.findData(child['operation_kind' if self.tools else 'regalia_kind']));self.first.setValue(int(child['units' if self.tools else 'rune_slot']) or 1);self.second.setValue(int(child.get('stole_grade') or 1));self.capacity.setText(catalog.display(child['parameter' if self.tools else 'capacity_ms'],100 if self.tools else 1000));self.initial.setText(catalog.display(child.get('initial_ms','0')))
        if self.tools:
            from item_page import BONUS_LABELS
            self.bonus.clear()
            for d in self.definitions:self.bonus.addItem(BONUS_LABELS.get(d['code'],d['code']),str(d['id']))
            self.bonus.setCurrentIndex(max(0,self.bonus.findData(str(child.get('bonus_id') or 29))));definition=next(d for d in self.definitions if str(d['id'])==str(self.bonus.currentData()));self.value.setText(catalog.display(child.get('value_scaled') or '1',UNITS[int(definition['unit'])][0]))
        self.rows.setRowCount(0)
        for row in document['special_tiers' if self.tools else 'regalia_rules']:self.append(row)
        self.controls();self.loading=False
    def collect(self,document):
        c=document['child'];kind=self.kind.currentData()
        if self.tools:
            c.update(operation_kind=kind,units=str(self.first.value()),parameter=str(catalog.scaled(self.capacity.text(),100)) if kind=='2' else '0',bonus_id=None,value_scaled=None)
            if kind=='4':
                c['bonus_id']=self.bonus.currentData();definition=next(d for d in self.definitions if str(d['id'])==c['bonus_id']);c['value_scaled']=str(catalog.scaled(self.value.text(),UNITS[int(definition['unit'])][0]))
            if kind in ('2','4'):document['base']['max_stack']='1'
            document['special_tiers']=[dict(draw_no=str(int(number(text(self.rows,r,0),'Dobás',1,3))-1),tier=number(text(self.rows,r,1),'Értékszint',1,16),chance_bp=str(catalog.scaled(text(self.rows,r,2),100))) for r in range(self.rows.rowCount())] if kind=='3' else []
        else:
            slot=self.first.value() if kind=='2' else 0;c.update(regalia_kind=kind,rune_slot=str(slot),stole_grade=str(self.second.value()) if kind=='1' else '0',capacity_ms=str(catalog.scaled(self.capacity.text())) if kind=='2' and slot<7 else '0',initial_ms=str(catalog.scaled(self.initial.text())) if kind=='2' and slot<7 else '0');document['base']['max_stack']='1';document['bonuses']=[]
            rows=[]
            for r in range(self.rows.rowCount()):
                bonus=self.rows.cellWidget(r,3).currentData();definition=next(d for d in self.definitions if str(d['id'])==bonus)
                rows.append(dict(bonus_index=str(int(number(text(self.rows,r,0),'Bónuszhely',1,6))-1),variant_no=str(int(number(text(self.rows,r,1),'Változat',1,4))-1),minimum_charge_bp=str(catalog.scaled(text(self.rows,r,2),100)),bonus_id=bonus,value_scaled=str(catalog.scaled(text(self.rows,r,4),UNITS[int(definition['unit'])][0]))))
            document['regalia_rules']=rows
