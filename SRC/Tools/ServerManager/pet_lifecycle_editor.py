"""Typed pet training and supplies; authoring never edits a player instance."""
import copy
from PySide6.QtCore import Qt
from PySide6.QtWidgets import (QCheckBox,QComboBox,QHeaderView,QHBoxLayout,QLabel,QPushButton,
    QSizePolicy,QTableWidget,QTableWidgetItem,QTabWidget,QVBoxLayout,QWidget)
import backend as api
import item_catalog as catalog
from growth_pet_editor import number

EFFECTS=[(1,'Maximum HP (pont)'),(2,'Maximum HP (%)'),(3,'Szörny elleni sebzés'),(4,'Normál PvE sebzés'),
 (5,'Szörnyellenállás'),(6,'Emberellenállás'),(7,'Harcosellenállás'),(8,'Suraellenállás'),(9,'Ninjaellenállás'),
 (10,'Sámánellenállás'),(11,'Boss elleni sebzés'),(12,'Metin elleni sebzés'),(13,'PvE kritikus esély'),
 (14,'Tapasztalatbónusz'),(15,'Yangdrop-bónusz'),(16,'Támadóbónusz'),(17,'Blokkolási esély'),(18,'Nyílelkerülés')]
SUPPLIES=[(1,'Petképesség-könyv'),(2,'Egy petképesség törlése'),(3,'Összes petképesség törlése'),
 (4,'Életidő növelése'),(5,'Korlátlan életidő'),(6,'Születési tulajdonság növelése'),(7,'Petszállító doboz'),(8,'Pet megjelenési tárgy')]

def table(headers):
    result=QTableWidget(0,len(headers));result.setHorizontalHeaderLabels(headers);result.verticalHeader().hide()
    result.horizontalHeader().setSectionResizeMode(QHeaderView.ResizeMode.Stretch);result.setMinimumHeight(175);return result

def cell(widget,row,column,value,editable=True):
    item=QTableWidgetItem(str(value))
    if not editable:item.setFlags(Qt.ItemFlag.ItemIsEnabled|Qt.ItemFlag.ItemIsSelectable)
    widget.setItem(row,column,item)

def text(widget,row,column):
    item=widget.item(row,column)
    return item.text().strip() if item else ''

class PetTrainingPanel(QWidget):
    def __init__(self,changed,Form):
        super().__init__();self.changed=changed;self.loading=True
        layout=QVBoxLayout(self);self.enabled=QCheckBox('Korhoz és evolúcióhoz kötött petképességek és bónuszok');layout.addWidget(self.enabled)
        self.body=QWidget();body=QVBoxLayout(self.body);layout.addWidget(self.body)
        hint=QLabel('A pet kora valós időben nő; az aktív életidőtől külön adat. A megnyílt képességhely nem záródik vissza. '
                    'Az alábbi értékek szerkesztési minták: csak a saját tartalomhoz ellenőrizve használd őket. '
                    'Az első három hely a megadott evolúciótól, a negyedik például 15 napos kortól nyitható.')
        hint.setWordWrap(True);body.addWidget(hint)
        self.settings=Form(self.notify,2)
        for field,label,limit in [('hp_evolution_period','HP-növekmény szintperiódusa',255),('monster_evolution_period','Szörnybónusz periódusa',255),
            ('pve_evolution_period','PvE-bónusz periódusa',255),('evolution_min','Legkisebb növekmény (0,1%)',32767),('evolution_max','Legnagyobb növekmény (0,1%)',32767)]:
            self.settings.add('training',field,label,range=(0 if field.endswith('period') else 1,limit))
        self.settings.note('Növekmény csak evolúciókor jár, ha az új petszint osztható a periódussal. A 0 kikapcsolja. A szerver a rögzített döntést ellenőrzi.')
        self.settings.finish();self.settings.setSizePolicy(QSizePolicy.Policy.Expanding,QSizePolicy.Policy.Maximum);body.addWidget(self.settings)
        tabs=QTabWidget();self.tabs=tabs;body.addWidget(tabs)
        age_page=QWidget();age_layout=QVBoxLayout(age_page);self.ages=table(['Legkisebb kor (mp)','Maximum HP (pont)','Szörnyellenállás (%)','Emberellenállás (%)']);age_layout.addWidget(self.ages)
        buttons=QHBoxLayout();self.button(buttons,'Korhatár hozzáadása',self.add_age);self.button(buttons,'Kijelölt törlése',lambda:self.remove(self.ages));age_layout.addLayout(buttons);tabs.addTab(age_page,'Korbónuszok')
        self.slots=table(['Képességhely','Legkisebb evolúció','Legkisebb kor (mp)']);tabs.addTab(self.slots,'Helyek megnyitása')
        skill_page=QWidget();skill_layout=QVBoxLayout(skill_page);self.skills=table(['Képességazonosító','Tartalomkód','Hatás']);skill_layout.addWidget(self.skills)
        buttons=QHBoxLayout();self.button(buttons,'Képesség hozzáadása',self.add_skill);self.button(buttons,'Képesség törlése',lambda:self.remove(self.skills));skill_layout.addLayout(buttons)
        self.ranks=table(['Képességazonosító','Képességszint','Érték','Egység']);skill_layout.addWidget(self.ranks)
        buttons=QHBoxLayout();self.button(buttons,'Képességszint hozzáadása',self.add_rank);self.button(buttons,'Szint törlése',lambda:self.remove(self.ranks));skill_layout.addLayout(buttons);tabs.addTab(skill_page,'Képességek és szintek')
        for widget in (self.ages,self.slots,self.skills,self.ranks):widget.itemChanged.connect(self.notify)
        for widget in (self.skills,self.ranks):widget.itemChanged.connect(lambda item:self.update_units() if not self.loading and item.column()==0 else None)
        self.enabled.toggled.connect(self.controls);self.load({});self.loading=False

    @staticmethod
    def button(layout,label,action):
        button=QPushButton(label);button.clicked.connect(lambda *_:action());layout.addWidget(button)

    def notify(self,*_):
        if not self.loading:self.changed()

    def controls(self,*_):
        self.body.setVisible(self.enabled.isChecked());self.notify()

    def remove(self,widget):
        rows=sorted({i.row() for i in widget.selectedItems()},reverse=True)
        for row in rows:widget.removeRow(row)
        self.notify()

    def add_age(self):
        row=self.ages.rowCount();self.ages.insertRow(row)
        for col,value in enumerate(('0' if row==0 else str(row*86400),'0','0','0')):cell(self.ages,row,col,value)
        self.notify()

    def add_skill(self,definition=None):
        definition=definition or dict(skill_id=str(max((int(text(self.skills,r,0)) for r in range(self.skills.rowCount()) if text(self.skills,r,0).isdigit()),default=0)+1),code='pet.new_skill',effect_kind='7')
        row=self.skills.rowCount();self.skills.insertRow(row);cell(self.skills,row,0,definition['skill_id']);cell(self.skills,row,1,definition['code'])
        combo=QComboBox()
        for value,label in EFFECTS:combo.addItem(label,str(value))
        combo.setCurrentIndex(combo.findData(str(definition['effect_kind'])));combo.currentIndexChanged.connect(self.update_units);self.skills.setCellWidget(row,2,combo);self.update_units();self.notify()

    def effects(self):
        return {text(self.skills,r,0):self.skills.cellWidget(r,2).currentData() for r in range(self.skills.rowCount()) if self.skills.cellWidget(r,2) is not None}

    def update_units(self,*_):
        effects=self.effects()
        for row in range(self.ranks.rowCount()):cell(self.ranks,row,3,'HP' if effects.get(text(self.ranks,row,0))=='1' else '%',False)
        self.notify()

    def add_rank(self,rank=None):
        if rank is None:
            selected=max(0,self.skills.currentRow());skill=text(self.skills,selected,0)
            if not skill:return
            level=max((int(text(self.ranks,r,1)) for r in range(self.ranks.rowCount()) if text(self.ranks,r,0)==skill and text(self.ranks,r,1).isdigit()),default=0)+1
            rank=dict(skill_id=skill,skill_rank=str(level),value_scaled='100')
        row=self.ranks.rowCount();self.ranks.insertRow(row);effect=self.effects().get(str(rank['skill_id']))
        for col,value in enumerate([rank['skill_id'],rank['skill_rank'],catalog.display(rank['value_scaled'],1 if effect=='1' else 100),'HP' if effect=='1' else '%']):cell(self.ranks,row,col,value,col!=3)
        self.notify()

    def load(self,document):
        was=self.loading;self.loading=True;defaults=catalog.Catalog.training_defaults();enabled=document.get('training') is not None
        self.enabled.setChecked(enabled);self.settings.load(document if enabled else defaults)
        for widget in (self.ages,self.slots,self.skills,self.ranks):widget.setRowCount(0)
        for a in document.get('pet_age_tiers',defaults['pet_age_tiers']):
            row=self.ages.rowCount();self.ages.insertRow(row)
            for col,value in enumerate([catalog.display(a['minimum_age_ms']),a['max_hp_flat'],catalog.display(a['monster_resistance_bp'],100),catalog.display(a['human_resistance_bp'],100)]):cell(self.ages,row,col,value)
        for r in document.get('pet_slot_rules',defaults['pet_slot_rules']):
            row=self.slots.rowCount();self.slots.insertRow(row)
            for col,value in enumerate([str(int(r['slot'])+1),r['minimum_stage'],catalog.display(r['minimum_age_ms'])]):cell(self.slots,row,col,value,col!=0)
        for s in document.get('pet_skills',defaults['pet_skills']):self.add_skill(s)
        for r in document.get('pet_skill_ranks',defaults['pet_skill_ranks']):self.add_rank(r)
        self.controls();self.loading=was

    def collect(self,document):
        if not self.enabled.isChecked():document['training']=None;return
        self.settings.collect(document)
        document['pet_age_tiers']=[dict(minimum_age_ms=str(catalog.scaled(text(self.ages,r,0))),max_hp_flat=number(text(self.ages,r,1),'HP',0,4294967295),
            monster_resistance_bp=str(catalog.scaled(text(self.ages,r,2),100)),human_resistance_bp=str(catalog.scaled(text(self.ages,r,3),100))) for r in range(self.ages.rowCount())]
        document['pet_slot_rules']=[dict(slot=str(r),minimum_stage=number(text(self.slots,r,1),'Evolúció',0,15),minimum_age_ms=str(catalog.scaled(text(self.slots,r,2)))) for r in range(self.slots.rowCount())]
        document['pet_skills']=[dict(skill_id=number(text(self.skills,r,0),'Petképesség azonosító',1,4294967295),code=text(self.skills,r,1),effect_kind=self.skills.cellWidget(r,2).currentData()) for r in range(self.skills.rowCount())]
        effects=self.effects()
        document['pet_skill_ranks']=[dict(skill_id=number(text(self.ranks,r,0),'Petképesség azonosító',1,4294967295),skill_rank=number(text(self.ranks,r,1),'Petképesség szint',1,255),
            value_scaled=str(catalog.scaled(text(self.ranks,r,2),1 if effects.get(text(self.ranks,r,0))=='1' else 100))) for r in range(self.ranks.rowCount())]

class PetSupplyPanel(QWidget):
    def __init__(self,changed,Form):
        super().__init__();self.changed=changed;self.loading=True;layout=QVBoxLayout(self);self.forms={}
        self.settings=Form(changed);self.kind=self.settings.add('supply','supply_kind','Petellátmány típusa','choice',choices=SUPPLIES);self.settings.finish();self.settings.setSizePolicy(QSizePolicy.Policy.Expanding,QSizePolicy.Policy.Maximum);layout.addWidget(self.settings)
        self.study=Form(changed);self.skill=self.study.add('pet_study','skill_id','Petképesség','choice',choices=[])
        self.selected=self.study.add('pet_study','selected_allowed','Kiválasztott hely','bool',text='Biztos tanulás és fejlesztés a kijelölt helyen')
        self.known=self.study.add('pet_study','known_allowed','Már ismert képesség','bool',text='Külön eséllyel, tulajdonoshoz kötött várakozással')
        self.chance=self.study.add('pet_study','known_success_bp','Ismert képesség sikeresélye','scaled',scale=100,unit='%',range=(0,10000))
        self.delay=self.study.add('pet_study','known_cooldown_ms','Várakozás sikeres olvasás után','scaled',scale=1000,unit='mp',range=(0,2**63-1))
        self.forms[1]=self.study
        extension=Form(changed);extension.add('pet_extension','added_ms','Hozzáadott idő','scaled',scale=1000,unit='mp',range=(1,2**63-1))
        extension.add('pet_extension','maximum_capacity_ms','Legnagyobb új időkeret','scaled',scale=1000,unit='mp',range=(1,2**63-1))
        extension.note('A régi működés szerint a maradékhoz ad, és az összeg lesz az új teljes keret is. Nem a korábbi kapacitást tölti vissza.');self.forms[4]=extension
        enchant=Form(changed);enchant.add('pet_enchant','success_bp','Sikeresély','scaled',scale=100,unit='%',range=(0,10000))
        for name,label in [('step','Növekmény'),('hp_limit','HP-bónusz maximuma'),('monster_limit','Szörnybónusz maximuma'),('pve_limit','PvE-bónusz maximuma')]:enchant.add('pet_enchant',name,label,'scaled',scale=10,unit='%',range=(1,32767))
        enchant.note('A forrástárgy sikertelen próbánál is elfogy. A már maximális tulajdonságot nem lehet tovább növelni.');self.forms[6]=enchant
        skin=Form(changed);self.skin_form=skin.add('pet_skin','form_id','Megjelenő petforma','choice',choices=[])
        self.clock=skin.add('pet_skin','clock_kind','Skin időszámítása','choice',choices=[(1,'Korlátlan'),(2,'Létrehozástól valós idő'),(3,'Első használattól valós idő'),(4,'Felszerelt állapotban')])
        self.duration=skin.add('pet_skin','duration_ms','Időtartam','scaled',scale=1000,unit='mp',range=(1,2**63-1));self.forms[8]=skin
        for form in self.forms.values():form.finish();form.setSizePolicy(QSizePolicy.Policy.Expanding,QSizePolicy.Policy.Maximum);layout.addWidget(form)
        self.hint=QLabel();self.hint.setWordWrap(True);layout.addWidget(self.hint);layout.addStretch()
        self.kind.widget.currentIndexChanged.connect(self.controls);self.known.widget.toggled.connect(self.controls);self.clock.widget.currentIndexChanged.connect(self.controls);self.loading=False

    def set_catalog(self,model):
        for combo in (self.skill.widget,self.skin_form.widget):combo.blockSignals(True);combo.clear()
        skills={r['skill_id']:r['code'] for r in model.tables['pet_skill_definition']}
        for key,code in sorted(skills.items(),key=lambda pair:int(pair[0])):self.skill.widget.addItem(f'{code} — {key}',key)
        for row in model.tables['companion_form']:self.skin_form.widget.addItem(f"{row['code']} — {row['mob_template_id']}",row['form_id'])
        for combo in (self.skill.widget,self.skin_form.widget):combo.blockSignals(False)

    def controls(self,*_):
        kind=int(self.kind.value())
        for value,form in self.forms.items():form.setVisible(kind==value)
        self.chance.widget.setEnabled(self.known.value()=='1');self.delay.widget.setEnabled(self.known.value()=='1');self.duration.widget.setEnabled(self.clock.value()!='1')
        self.hint.setText({2:'A kiválasztott tanult képességet üríti. A hely nyitottsága és a tulajdonos várakozási ideje megmarad.',
            3:'Minden tanult képességet ürít; a zárolt helyeket nem nyitja meg.',5:'Explicit, tartós korlátlan életidőt ad az inaktív petnek.',
            7:'Egy doboz egy teljes petet hordoz. A hívó példányazonosítója, neve, kora, képességei és maradék életideje megmarad. A doboz kivételkor elfogy.',
            8:'Csak a pet megjelenését módosítja. Lejáratkor visszatér az evolúciós forma; a pet adatai nem változnak.'}.get(kind,''))
        if not self.loading:self.changed()

    def load(self,document):
        self.loading=True;defaults=catalog.Catalog.pet_supply_defaults();self.settings.load(document)
        for name,form in [('pet_study',self.study),('pet_extension',self.forms[4]),('pet_enchant',self.forms[6]),('pet_skin',self.forms[8])]:form.load({name:document.get(name) or defaults[name]})
        self.controls();self.loading=False

    def collect(self,document):
        kind=self.kind.value();document['supply']=dict(document.get('supply') or {},supply_kind=kind)
        if kind=='1':
            if not self.skill.value():raise api.EditorError('Előbb hozz létre fejlődő petképességet a hívótárgy beállításainál.')
            known=self.known.value();document['pet_study']=dict(skill_id=self.skill.value(),selected_allowed=self.selected.value(),known_allowed=known,
                known_success_bp=self.chance.value() if known=='1' else '0',known_cooldown_ms=self.delay.value() if known=='1' else '0')
        elif kind in ('4','6'):self.forms[int(kind)].collect(document)
        elif kind=='8':
            if not self.skin_form.value():raise api.EditorError('A skinhez válassz meglévő petformát.')
            document['pet_skin']=dict(form_id=self.skin_form.value(),clock_kind=self.clock.value(),duration_ms='0' if self.clock.value()=='1' else self.duration.value())
        if kind in ('7','8'):document['base']['max_stack']='1'
