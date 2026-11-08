"""Typed forge aid rules and explicit per-level chance tables."""
import copy
from PySide6.QtWidgets import QWidget,QVBoxLayout,QComboBox,QPushButton,QLabel
from pet_lifecycle_editor import table,cell,text
import item_catalog as catalog
import backend as api
class ForgeAidPanel(QWidget):
    def __init__(self,changed,Form):
        super().__init__();self.changed=changed;self.loading=False;layout=QVBoxLayout(self)
        self.preset=QComboBox()
        for code,label in [('blessing','Áldástekercs'),('metal','Mágikus fém'),('dragon_god','Sárkányisten-tekercs'),('smith_book','Kovácskönyv'),('musin','Musin (biztos siker)'),('memo','Emléktekercs (előírt szint)'),('black_dragon','Feketesárkány-tekercs (foglalatkő +4)'),('low','Kis esélynövelő (+5 százalékpont)'),('medium','Közepes esélynövelő (+10 százalékpont)'),('high','Nagy esélynövelő (+15 százalékpont)')]:self.preset.addItem(label,code)
        layout.addWidget(self.preset);seed=QPushButton('Szerkeszthető mintaszabály betöltése');layout.addWidget(seed);seed.clicked.connect(self.seed)
        self.form=Form(changed)
        self.form.add('child','aid_kind','Működés','choice',choices=[(1,'Fejlesztőtekercs'),(2,'Esélynövelő segéd')])
        self.form.add('child','units','Felhasznált darab',range=(1,65535))
        self.form.add('child','target_mask','Célok (fegyver 1, páncél 2, ékszer 4, kő 8)',range=(1,15))
        self.form.add('child','minimum_level','Legkisebb forrásszint',range=(0,255));self.form.add('child','maximum_level','Legnagyobb forrásszint',range=(0,255))
        self.mode=self.form.add('child','chance_mode','Sikeresség','choice',choices=[(1,'Recept szerinti'),(2,'Rögzített esély'),(3,'Hozzáadott százalékpont'),(4,'Szintenkénti táblázat')])
        self.form.add('child','chance_bp','Esély / hozzáadott esély','scaled',scale=100,unit='%',range=(0,10000))
        self.form.add('child','failure_kind','Sikertelenség','choice',choices=[(0,'Recept szerint'),(1,'Tárgy és szint megmarad'),(2,'Tárgy megsemmisül'),(3,'Egy szinttel csökken (0 alatt nem)')])
        self.form.add('child','booster_group','Esélynövelő csoport (tekercsnél 0)',range=(0,3));self.form.finish();layout.addWidget(self.form)
        self.rows=table(['Forrásszint','Sikeres fejlesztés (%)']);layout.addWidget(self.rows);self.rows.itemChanged.connect(self.notify);self.mode.widget.currentIndexChanged.connect(self.controls)
        self.fit=QPushButton('Táblázat igazítása a megadott szinttartományhoz');self.fit.clicked.connect(self.fit_levels);layout.addWidget(self.fit)
        note=QLabel('Tekercs mellett legfeljebb három külön csoportú esélynövelő választható. Segéd, alapanyag és Yang siker és kudarc esetén is együtt fogy. A minta minden értéke szerkeszthető; nincs általános +9 korlát.');note.setWordWrap(True);layout.addWidget(note)
    def notify(self,*_):
        if not self.loading:self.changed()
    def controls(self,*_):
        visible=self.mode.widget.currentData()=='4';self.rows.setVisible(visible);self.fit.setVisible(visible);self.notify()
    def fit_levels(self):
        fields={f.column:f for f in self.form.fields}
        low=int(fields['minimum_level'].value());high=int(fields['maximum_level'].value())
        if low>high:raise api.EditorError('A legkisebb szint nem lehet nagyobb a legnagyobbnál.')
        existing={text(self.rows,n,0):text(self.rows,n,1) for n in range(self.rows.rowCount())}
        self.loading=True;self.rows.setRowCount(0)
        for level in range(low,high+1):
            n=self.rows.rowCount();self.rows.insertRow(n);cell(self.rows,n,0,level);cell(self.rows,n,1,existing.get(str(level),'0'))
        self.loading=False;self.notify()
    def load(self,document):
        self.loading=True;self.document=copy.deepcopy(document);self.form.load(document);self.rows.setRowCount(0)
        for r in sorted(document.get('forge_levels',[]),key=lambda r:int(r['source_level'])):
            n=self.rows.rowCount();self.rows.insertRow(n);cell(self.rows,n,0,r['source_level']);cell(self.rows,n,1,catalog.display(r['chance_bp'],100))
        self.controls();self.loading=False
    def seed(self):
        d=copy.deepcopy(self.document);row,levels=catalog.Catalog.forge_defaults(self.preset.currentData());d['child'].update(row);d['forge_levels']=levels;self.load(d);self.notify()
    def collect(self,document):
        self.form.collect(document);document['forge_levels']=[]
        if document['child']['chance_mode']=='4':
            for n in range(self.rows.rowCount()):
                try:level=int(text(self.rows,n,0));chance=catalog.scaled(text(self.rows,n,1),100)
                except (ValueError,api.EditorError):raise api.EditorError('Érvénytelen szint vagy esély a kovácssegéd táblázatában.') from None
                document['forge_levels'].append(dict(source_level=str(level),chance_bp=str(chance)))
