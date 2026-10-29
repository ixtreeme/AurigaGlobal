"""Pet hatch and feeding catalog controls; no player-state or SQL writes."""
from PySide6.QtWidgets import QCheckBox,QComboBox,QFormLayout,QLabel,QSizePolicy,QVBoxLayout,QWidget
import backend as api
import item_catalog as catalog


class PetHatchPanel(QWidget):
    def __init__(self,changed,Form):
        super().__init__();layout=QVBoxLayout(self)
        self.setSizePolicy(QSizePolicy.Policy.Expanding,QSizePolicy.Policy.Maximum)
        self.target=QComboBox();self.target.currentIndexChanged.connect(lambda *_:changed())
        targets=QFormLayout();targets.addRow('Kikelő fejlődő pet',self.target);layout.addLayout(targets)
        self.form=Form(changed,3)
        self.form.add('hatch','gold_cost','Keltetés pénzköltsége')
        self.form.note('A tulajdonságok százalékban jelennek meg. A szerver külön rögzített döntését ellenőrizzük. '
                       'Az életidő a kiválasztott hívótárgy beállítása. Ez új tartalom szerkesztése, nem automatikus régi tojáskonverzió.')
        for prefix,label in [('hp','Max HP'),('monster','Szörny elleni sebzés'),('pve_normal','Normál PvE sebzés')]:
            for suffix,caption in [('min','minimum'),('max','maximum'),('step','lépés')]:
                self.form.add('hatch',f'{prefix}_{suffix}',f'{label} {caption}','scaled',scale=10,unit='%',range=(1 if suffix=='step' else 0,32767))
        self.form.grid.setColumnStretch(5,1);self.form.setSizePolicy(QSizePolicy.Policy.Expanding,QSizePolicy.Policy.Maximum)
        self.form.finish();layout.addWidget(self.form)
        hint=QLabel('Egy keltetés egy tojást fogyaszt. A név, az új példány, a petállapot és a pénz egyetlen művelethez tartozik.')
        hint.setWordWrap(True);hint.setObjectName('FieldHint');layout.addWidget(hint)

    def set_targets(self,model):
        self.target.blockSignals(True);self.target.clear()
        for row in model.tables['item_companion']:
            if row['companion_kind']!='3':continue
            base=next((b for b in model.tables['item_base'] if b['category_id']==row['category_id'] and b['local_id']==row['local_id']),None)
            if base and base['retired']=='0':self.target.addItem(f"{base['content_code']} — {row['category_id']}:{row['local_id']}",f"{row['category_id']}:{row['local_id']}")
        self.target.blockSignals(False)

    def load(self,document):
        hatch=document.get('hatch') or catalog.Catalog.hatch_defaults()
        selected=f"{hatch.get('target_category_id')}:{hatch.get('target_local_id')}"
        self.target.blockSignals(True);self.target.setCurrentIndex(max(0,self.target.findData(selected)));self.target.blockSignals(False)
        self.form.load(dict(hatch=hatch))

    def collect(self,document):
        selected=self.target.currentData()
        if not selected:raise api.EditorError('Előbb hozz létre fejlődő pet hívótárgyat, majd válaszd ki a tojás céljaként.')
        self.form.collect(document);category,local=selected.split(':')
        document['hatch'].update(target_category_id=category,target_local_id=local)


class PetFoodPanel(QWidget):
    def __init__(self,changed,Form):
        super().__init__();self.changed=changed;self.loading=True
        layout=QVBoxLayout(self)
        hint=QLabel('Az etetési szabály külön a tárgy rendes használatától. A petablak aktív petet, '
                    'a közvetlen töltés inaktív petet igényel. A „Teljes halom” az összes kijelölt darabot fogyasztja '
                    'egyetlen hatásért. Tele időkeretnél és hatás nélküli etetésnél nincs fogyasztás. '
                    'Petállapotot hordozó tárgyat ez a művelet nem áldoz fel.')
        hint.setWordWrap(True);layout.addWidget(hint)
        self.cube_enabled=QCheckBox('Használható a pet etetőablakában');layout.addWidget(self.cube_enabled)
        self.cube=Form(changed)
        self.kind=self.cube.add('food','food_kind','Hatás','choice',choices=[(1,'Kapacitásarányos életidőtöltés'),(3,'Fegyver/páncél árából tárgy EXP')])
        self.cube_consumption=self.cube.add('food','consumption_kind','Fogyasztás','choice',choices=[(1,'Megadott darabszám'),(2,'Teljes halom — egy hatás')])
        self.cube_units=self.cube.add('food','units','Elfogyasztott darabszám',range=(1,4294967295))
        self.fraction=self.cube.add('food','lifetime_fraction_bp','Életidő a kapacitásból','scaled',scale=100,unit='%',range=(1,10000))
        self.divisor=self.cube.add('food','experience_price_divisor','Vételár osztója EXP-hez',range=(1,4294967295))
        self.cube.finish();self.cube.setSizePolicy(QSizePolicy.Policy.Expanding,QSizePolicy.Policy.Maximum);layout.addWidget(self.cube)
        self.direct_enabled=QCheckBox('Közvetlenül inaktív petre használható');layout.addWidget(self.direct_enabled)
        self.direct=Form(changed)
        self.direct_consumption=self.direct.add('food','consumption_kind','Fogyasztás','choice',choices=[(1,'Megadott darabszám'),(2,'Teljes halom — egy hatás')])
        self.direct_units=self.direct.add('food','units','Elfogyasztott darabszám',range=(1,4294967295))
        self.dead_fraction=self.direct.add('food','dead_recovery_bp','Lejárt pet új életideje','scaled',scale=100,unit='%',range=(1,10000))
        self.direct.note('A még élő, inaktív pet teljes keretre töltődik. A lejárt pet a beállított hányadot kapja vissza; a tárgy és a többi petadat megmarad.')
        self.direct.finish();self.direct.setSizePolicy(QSizePolicy.Policy.Expanding,QSizePolicy.Policy.Maximum);layout.addWidget(self.direct);layout.addStretch()
        for widget in (self.cube_enabled,self.direct_enabled):widget.toggled.connect(self.controls)
        for combo in (self.kind.widget,self.cube_consumption.widget,self.direct_consumption.widget):combo.currentIndexChanged.connect(self.controls)
        self.load({});self.loading=False

    def controls(self,*_):
        self.cube.setEnabled(self.cube_enabled.isChecked());self.direct.setEnabled(self.direct_enabled.isChecked())
        self.cube_units.widget.setEnabled(self.cube_consumption.value()=='1');self.direct_units.widget.setEnabled(self.direct_consumption.value()=='1')
        self.fraction.widget.setEnabled(self.kind.value()=='1');self.divisor.widget.setEnabled(self.kind.value()=='3')
        if not self.loading:self.changed()

    def load(self,document):
        self.loading=True;rules={r['service_kind']:r for r in document.get('pet_food_rules',[])}
        # Price-derived EXP exists only for equipment; do not offer a choice
        # that the native catalog must reject for every other category.
        equipment=str(document.get('base',{}).get('category_id')) in ('1','2')
        self.kind.widget.model().item(self.kind.widget.findData('3')).setEnabled(equipment)
        cube=rules.get('1',dict(food_kind='1',consumption_kind='1',units='1',lifetime_fraction_bp='300',experience_price_divisor='2'))
        direct=rules.get('2',dict(consumption_kind='1',units='1',dead_recovery_bp='5000'))
        self.cube.load(dict(food=cube));self.direct.load(dict(food=direct))
        self.cube_enabled.setChecked('1' in rules);self.direct_enabled.setChecked('2' in rules)
        self.controls();self.loading=False

    @staticmethod
    def consumption(choice,units):
        return dict(consumption_kind=choice.value(),units=units.value() if choice.value()=='1' else '0')

    def collect(self,document):
        rules=[]
        if self.cube_enabled.isChecked():
            kind=self.kind.value()
            rules.append(dict(service_kind='1',food_kind=kind,**self.consumption(self.cube_consumption,self.cube_units),
                              lifetime_fraction_bp=self.fraction.value() if kind=='1' else '0',dead_recovery_bp='0',
                              experience_price_divisor=self.divisor.value() if kind=='3' else '0'))
        if self.direct_enabled.isChecked():
            rules.append(dict(service_kind='2',food_kind='2',**self.consumption(self.direct_consumption,self.direct_units),
                              lifetime_fraction_bp='0',dead_recovery_bp=self.dead_fraction.value(),experience_price_divisor='0'))
        document['pet_food_rules']=rules
