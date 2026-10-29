"""Polymorph definitions. Instance-bound forms/progress are not edited here."""
from PySide6.QtCore import Qt
from PySide6.QtWidgets import (QComboBox,QHBoxLayout,QHeaderView,QLabel,QPushButton,
    QTableWidget,QTableWidgetItem,QVBoxLayout,QWidget,QTabWidget)
import backend as api
import item_catalog as catalog


class PolymorphPanel(QWidget):
    def __init__(self,changed,Form):
        super().__init__();self.changed=changed;self.loading=True;self.forms={}
        layout=QVBoxLayout(self);self.common=Form(changed)
        self.kind=self.common.add('poly','polymorph_kind','Működés','choice',choices=[(1,'Elfogyó gömb'),(2,'Gyakorlókönyv')])
        self.common.finish();layout.addWidget(self.common)
        self.tabs=QTabWidget();layout.addWidget(self.tabs,1)
        definitions=QWidget();layout=QVBoxLayout(definitions);self.tabs.addTab(definitions,'Forma és használat')
        row=QHBoxLayout();self.target=QComboBox();self.target.addItem('A tárgypéldány saját formája','0');self.target.addItem('Rögzített forma','1')
        self.form=QComboBox();row.addWidget(self.target);row.addWidget(self.form,1);layout.addLayout(row)
        self.form_details=Form(changed)
        self.form_details.add('new_poly_form','code','Új forma kódja','text')
        self.form_details.add('new_poly_form','mob_template_id','Mob-sablon azonosítója',range=(8,4294967295))
        self.form_details.add('new_poly_form','book_bonus','Könyv formabónusza','choice',choices=[(1,'Nincs'),(2,'Támadás'),(3,'Védelem'),(4,'Mozgás')])
        self.form_details.note('A már létrehozott formák azonosítója és jelentése nem írható át. Új formakódot adj meg a változathoz. '
                               'Példányhoz kötött módban az új forma a választható formák közé kerül; magát a kötést a megszerzett tárgy kapja.')
        self.form_details.finish();layout.addWidget(self.form_details)
        self.orb=Form(changed)
        for key,label,minimum in [('mob_level_grace','Mob-szint engedmény',0),('grace_loss_numerator','Engedmény csökkenése: számláló',0),('grace_loss_denominator','Engedmény csökkenése: nevező',1)]:
            self.orb.add('poly',key,label,range=(minimum,65535))
        self.orb.note('A forma szintje kisebb legyen, mint a karakterszint + max(0, engedmény − lefelé kerekített szintarány).')
        self.orb.finish();layout.addWidget(self.orb)
        self.book=Form(changed)
        self.book.add('poly','max_book_grade','Könyv legmagasabb fokozata',range=(0,255))
        self.book.add('poly','practice_per_grade','Gyakorlási szorzó fejlesztéskor',range=(0,4294967295))
        self.book.add('poly','book_chance_base_percent','Átváltozás alapesélye (%)',range=(-10000,10000))
        self.book.note('Esély: karakterszint − mob szintje + alapesély + készségszint + könyvfokozat, 0–100% közé korlátozva. '
                       'Fejlesztéskor az előző fokozat × gyakorlási szorzó az új gyakorlásszám; a könyv nem fogy el.')
        self.book.finish();layout.addWidget(self.book)
        layout.addStretch();rules=QWidget();layout=QVBoxLayout(rules);self.tabs.addTab(rules,'Készségszintek és időtartamok')
        defaults=QPushButton('Készség- és mesterfok-sablon kitöltése');defaults.clicked.connect(self.fill_defaults);layout.addWidget(defaults)
        hint=QLabel('A sablon a vizsgált forrást követi: a 40. készségszinten a mob statjainak aránya 0%. '
                    'Ezt kiadás előtt külön ellenőrizni kell; az érték itt szerkeszthető.')
        hint.setObjectName('FieldHint');hint.setWordWrap(True);layout.addWidget(hint)
        self.skills=QTableWidget(41,4);self.skills.setHorizontalHeaderLabels(['Készségszint','Átváltozás (mp)','Támadási bónusz %','Mob statjainak aránya %'])
        self.skills.horizontalHeader().setSectionResizeMode(QHeaderView.ResizeMode.Stretch);self.skills.verticalHeader().hide();layout.addWidget(self.skills,1)
        self.masteries=QTableWidget(4,3);self.masteries.setHorizontalHeaderLabels(['Mesterfok','Átváltozás (mp)','Formabónusz (mp)'])
        self.masteries.horizontalHeader().setSectionResizeMode(QHeaderView.ResizeMode.Stretch);self.masteries.verticalHeader().hide();self.masteries.setMaximumHeight(170);layout.addWidget(self.masteries)
        for table in (self.skills,self.masteries):table.itemChanged.connect(self.notify)
        self.kind.widget.currentIndexChanged.connect(self.controls);self.form.currentIndexChanged.connect(self.controls);self.target.currentIndexChanged.connect(self.controls)
        self.loading=False

    def notify(self,*_):
        if not self.loading:self.changed()

    def controls(self,*_):
        book=self.kind.value()=='2';self.book.setVisible(book);self.orb.setVisible(not book);self.masteries.setVisible(book)
        self.skills.setColumnHidden(1,book);self.skills.setColumnHidden(2,book)
        self.form_details.setVisible(self.form.currentData()=='new');self.notify()

    def set_forms(self,rows):
        was=self.loading;self.loading=True;self.forms={r['form_id']:r for r in rows}
        self.form.clear();self.form.addItem('Nem hozok létre új formát','')
        for key,r in sorted(self.forms.items(),key=lambda pair:int(pair[0])):self.form.addItem(f"{key} — {r['code']} (mob: {r['mob_template_id']})",key)
        self.form.addItem('Új forma létrehozása…','new');self.loading=was

    def put(self,table,row,col,text,readonly=False):
        cell=QTableWidgetItem(str(text))
        if readonly:cell.setFlags(Qt.ItemFlag.ItemIsEnabled|Qt.ItemFlag.ItemIsSelectable)
        table.setItem(row,col,cell)

    def load_rules(self,skills,masteries):
        for row,r in enumerate(skills):
            for col,value in enumerate((r['skill_level'],catalog.display(r['duration_ms']),catalog.display(r['attack_bonus_bp'],100),catalog.display(r['power_bp'],100))):self.put(self.skills,row,col,value,col==0)
        for row in range(4):
            r=masteries[row] if masteries else dict(duration_ms=str((10+5*row)*1000),bonus_duration_ms=str((9+5*row)*1000))
            for col,value in enumerate((('Normál','Mester','Nagymester','Tökéletes')[row],catalog.display(r['duration_ms']),catalog.display(r['bonus_duration_ms']))):self.put(self.masteries,row,col,value,col==0)

    def fill_defaults(self):
        self.loading=True;self.load_rules(catalog.Catalog.polymorph_defaults()['poly_skills'],[]);self.loading=False;self.notify()

    def load(self,document):
        self.loading=True;poly=document['poly'];self.common.load(document)
        fixed=document['child'].get('fixed_form_id');self.target.setCurrentIndex(1 if fixed else 0)
        self.form.setCurrentIndex(max(0,self.form.findData(fixed or '')))
        next_id=max((int(k) for k in self.forms),default=0)+1
        self.form_details.load(dict(new_poly_form=dict(code=f'form.new_form_{next_id}',mob_template_id='101',book_bonus='1')))
        orb=poly if poly['polymorph_kind']=='1' else dict(mob_level_grace='20',grace_loss_numerator='3',grace_loss_denominator='10')
        book=poly if poly['polymorph_kind']=='2' else dict(max_book_grade='30',practice_per_grade='50',book_chance_base_percent='29')
        self.orb.load(dict(poly=orb));self.book.load(dict(poly=book));self.load_rules(document['poly_skills'],document['poly_masteries'])
        self.controls();self.loading=False

    def collect(self,document):
        self.common.collect(document);book=document['poly']['polymorph_kind']=='2'
        if book:
            self.book.collect(document);document['poly'].update(mob_level_grace='0',grace_loss_numerator='0',grace_loss_denominator='1')
        else:
            self.orb.collect(document);document['poly'].update(max_book_grade='0',practice_per_grade='0',book_chance_base_percent='0')
        skills=[]
        for row in range(41):
            try:
                r=dict(skill_level=str(row),duration_ms='0' if book else str(catalog.scaled(self.skills.item(row,1).text())),
                       attack_bonus_bp='0' if book else str(catalog.scaled(self.skills.item(row,2).text(),100)),
                       power_bp=str(catalog.scaled(self.skills.item(row,3).text(),100)))
            except api.EditorError:raise api.EditorError(f'Átváltozás, {row}. készségszint: érvénytelen szám.') from None
            skills.append(r)
        document['poly_skills']=skills;document['poly_masteries']=[]
        if book:
            for row in range(4):
                document['poly_masteries'].append(dict(mastery=str(row),duration_ms=str(catalog.scaled(self.masteries.item(row,1).text())),
                                                       bonus_duration_ms=str(catalog.scaled(self.masteries.item(row,2).text()))))
        fixed=self.target.currentData()=='1';selected=self.form.currentData()
        document.update(poly_target_fixed=fixed,new_poly_form=None)
        if fixed and not selected:raise api.EditorError('Rögzített átváltozáshoz válassz formát, vagy hozz létre újat.')
        if selected=='new':self.form_details.collect(document)
        document['child']['fixed_form_id']=selected if fixed and selected not in ('','new') else None
