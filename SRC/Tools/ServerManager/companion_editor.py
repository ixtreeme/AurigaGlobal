"""Fixed companion seals; persistent instances and growth pets are separate."""
from PySide6.QtWidgets import QComboBox,QLabel,QVBoxLayout,QWidget,QSizePolicy
import backend as api
from growth_pet_editor import GrowthPetPanel
from pet_lifecycle_editor import PetTrainingPanel


class CompanionPanel(QWidget):
    def __init__(self,changed,Form):
        super().__init__();self.changed=changed;self.loading=True;self.forms={}
        layout=QVBoxLayout(self);self.settings=Form(changed)
        self.kind=self.settings.add('child','companion_kind','Társ típusa','choice',choices=[(1,'Fix követő pet'),(2,'Lovagolható mount'),(3,'Fejlődő pet')])
        self.clock=self.settings.add('child','clock_kind','Időszámítás','choice',choices=[(1,'Korlátlan'),(2,'Létrehozástól valós idő'),(3,'Első idézéstől valós idő'),(4,'Aktív követés és lovaglás ideje')])
        self.duration=self.settings.add('child','duration_ms','Időtartam','scaled',scale=1000,unit='mp',range=(1,2**63-1))
        self.dungeon=self.settings.add('child','dungeon_only_bonus','Bónusz érvényessége','choice',choices=[(0,'Minden engedélyezett térképen'),(1,'Csak dungeonban')])
        self.settings.note('A pet bónusza követéskor, a mount bónusza lovagláskor érvényes. '
                           'Felüléskor a követő eltűnik; leszálláskor egy követő tér vissza. A fejlődő pet saját szintjét és evolúcióját az alábbi külön táblák szabályozzák.')
        self.settings.finish();self.settings.setSizePolicy(QSizePolicy.Policy.Expanding,QSizePolicy.Policy.Maximum);layout.addWidget(self.settings)
        self.growth=GrowthPetPanel(changed);layout.addWidget(self.growth)
        self.training=PetTrainingPanel(changed,Form);layout.addWidget(self.training)
        self.follower=QComboBox();layout.addWidget(QLabel('Követő lény formája'));layout.addWidget(self.follower)
        self.follower_details=QLabel();self.follower_details.setWordWrap(True);self.follower_details.setObjectName('FieldHint');layout.addWidget(self.follower_details)
        self.new_follower=Form(changed);self.add_form_fields(self.new_follower,'new_companion_form');layout.addWidget(self.new_follower)
        self.riding_label=QLabel('Lovagláskor megjelenő forma');layout.addWidget(self.riding_label)
        self.riding=QComboBox();layout.addWidget(self.riding)
        self.riding_details=QLabel();self.riding_details.setWordWrap(True);self.riding_details.setObjectName('FieldHint');layout.addWidget(self.riding_details)
        self.new_riding=Form(changed);self.add_form_fields(self.new_riding,'new_riding_form');layout.addWidget(self.new_riding)
        hint=QLabel('A formák közös, rögzített tartalmak. Meglévő forma átírása helyett új kódot adj meg. '
                    'A megszerzett példány időkeretét és visszaidézési állapotát ez az oldal nem módosítja.')
        hint.setWordWrap(True);hint.setObjectName('FieldHint');layout.addWidget(hint);layout.addStretch()
        for combo in (self.kind.widget,self.clock.widget,self.follower,self.riding):combo.currentIndexChanged.connect(self.controls)
        self.loading=False

    @staticmethod
    def add_form_fields(form,section):
        form.setSizePolicy(QSizePolicy.Policy.Expanding,QSizePolicy.Policy.Maximum)
        form.add(section,'code','Új forma kódja','text')
        form.add(section,'mob_template_id','Lény sablonazonosítója',range=(1,4294967295))
        form.add(section,'model_asset','Modell erőforrása','text')
        form.finish()

    def controls(self,*_):
        mount=self.kind.value()=='2';growth=self.kind.value()=='3';self.growth.setVisible(growth);self.training.setVisible(growth)
        for value in ('2','3'):self.clock.widget.model().item(self.clock.widget.findData(value)).setEnabled(not growth)
        if growth and self.clock.value() in ('2','3'):self.clock.load('4')
        self.riding.setEnabled(mount);self.riding_label.setVisible(mount);self.riding.setVisible(mount)
        self.new_riding.setVisible(mount and self.riding.currentData()=='new')
        self.new_follower.setVisible(self.follower.currentData()=='new')
        self.follower_details.setText(self.forms.get(self.follower.currentData(),{}).get('model_asset',''))
        self.riding_details.setText(self.forms.get(self.riding.currentData(),{}).get('model_asset',''))
        self.riding_details.setVisible(mount)
        self.duration.widget.setEnabled(self.clock.value()!='1')
        if not self.loading:self.changed()

    def set_forms(self,rows):
        was=self.loading;self.loading=True;self.forms={r['form_id']:r for r in rows}
        for combo in (self.follower,self.riding):
            combo.clear()
            if combo is self.riding:combo.addItem('Ugyanaz, mint a követő','same')
            for key,r in sorted(self.forms.items(),key=lambda pair:int(pair[0])):
                combo.addItem(f"{r['code']} — lény {r['mob_template_id']}",key)
            combo.addItem('Új forma létrehozása…','new')
        self.growth.set_forms(rows);self.loading=was

    def load(self,document):
        self.loading=True;child=document['child'];self.settings.load(document);self.growth.load(document);self.training.load(document)
        self.follower.setCurrentIndex(max(0,self.follower.findData('new' if document.get('new_companion_form') else child['form_id'])))
        riding='new' if document.get('new_riding_form') else 'same' if not child.get('riding_form_id') or child['riding_form_id']==child['form_id'] else child['riding_form_id']
        self.riding.setCurrentIndex(max(0,self.riding.findData(riding)))
        next_id=max((int(k) for k in self.forms),default=0)+1
        default=dict(code=f'companion.new_form_{next_id}',mob_template_id='101',model_asset='models/companion.model')
        self.new_follower.load(dict(new_companion_form=document.get('new_companion_form') or default))
        self.new_riding.load(dict(new_riding_form=document.get('new_riding_form') or dict(default,code=f'companion.new_form_{next_id+1}')))
        self.controls();self.loading=False

    def collect(self,document):
        child=document['child'];child['companion_kind']=self.kind.value();child['clock_kind']=self.clock.value()
        child['duration_ms']='0' if child['clock_kind']=='1' else self.duration.value()
        child['dungeon_only_bonus']=self.dungeon.value()
        if child['companion_kind']=='3':self.growth.collect(document);self.training.collect(document)
        form=self.follower.currentData()
        if form is None:raise api.EditorError('Válassz követőformát.')
        child['form_id']=None if form=='new' else form
        document['new_companion_form']=None
        if form=='new':self.new_follower.collect(document)
        document['new_riding_form']=None;document['riding_same_form']=False;child['riding_form_id']=None
        if child['companion_kind']=='2':
            riding=self.riding.currentData()
            if riding is None:raise api.EditorError('Válassz lovaglási formát.')
            if riding=='same':document['riding_same_form']=True
            elif riding=='new':self.new_riding.collect(document)
            else:child['riding_form_id']=riding
