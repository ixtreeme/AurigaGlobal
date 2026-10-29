"""Independent Soul core/scroll definitions; no saved instance editing."""
import copy
from PySide6.QtCore import Qt
from PySide6.QtWidgets import (QComboBox,QHBoxLayout,QHeaderView,QLabel,QSpinBox,
    QSizePolicy,QTableWidget,QTableWidgetItem,QVBoxLayout,QWidget)
import backend as api
import item_catalog as catalog


class SoulPanel(QWidget):
    COLUMNS=[('charge_capacity','Töltésmaximum'),('cycle_charge_cost','Töltés / ciklus'),
             ('strikes_per_cycle','Ütés / ciklus'),('initial_charge','Kezdő töltés'),('initial_strikes','Kezdő ütéskeret'),
             ('recharge_interval_ms','1 töltés ideje (mp)'),('first_multiplier_bp','1. ciklus szorzó (%)'),
             ('second_multiplier_bp','2. ciklus szorzó (%)'),('third_multiplier_bp','3. ciklus szorzó (%)')]
    def __init__(self,changed,Form):
        super().__init__();self.changed=changed;self.loading=True;self.drafts={};self.maximum=0
        layout=QVBoxLayout(self);self.form=Form(changed)
        self.kind=self.form.add('child','soul_kind','Működés','choice',choices=[(1,'Aktiválható lélek'),(2,'Lélekfejlesztő tekercs')])
        self.channel=self.form.add('child','damage_channel','Szín és sebzési csatorna','choice',choices=[(1,'Vörös — normál támadás'),(2,'Kék — képesség')])
        self.chance=self.form.add('child','refine_chance_bp','Tekercs sikeresélye','scaled',scale=100,unit='%',range=(0,10000))
        self.form.setSizePolicy(QSizePolicy.Policy.Preferred,QSizePolicy.Policy.Maximum);layout.addWidget(self.form)
        self.core=QWidget();core=QVBoxLayout(self.core);row=QHBoxLayout();row.addWidget(QLabel('Legmagasabb fejlesztési szint'))
        self.max_level=QSpinBox();self.max_level.setRange(0,255);row.addWidget(self.max_level);row.addStretch();core.addLayout(row)
        hint=QLabel('Egy prototípushoz minden +szint külön töltési és sebzési szabályt kap. '
                    'A töltésmaximum 1–3 teljes ciklus. A szorzó teljes sebzést jelent: 150% = 1,5×. '
                    'A töltődés az aktív szerveridőt követi; nincs offline időből jóváírás.')
        hint.setObjectName('FieldHint');hint.setWordWrap(True);core.addWidget(hint)
        self.table=QTableWidget(0,len(self.COLUMNS)+1);self.table.setHorizontalHeaderLabels(['Szint']+[label for _,label in self.COLUMNS]);self.table.verticalHeader().hide()
        self.table.horizontalHeader().setSectionResizeMode(QHeaderView.ResizeMode.ResizeToContents);self.table.horizontalHeader().setStretchLastSection(True);core.addWidget(self.table,1)
        layout.addWidget(self.core,1)
        hint=QLabel('Az aktiválás nem fogyaszt tárgyat. Ütésenként az adott csatorna kerete csökken; '
                    'ciklus végén töltést veszít. A tekercs siker és kudarc esetén is egy darabot fogyaszt. '
                    'A tárgy saját töltése és megmaradt ütéskerete használat közben változik.')
        hint.setWordWrap(True);hint.setSizePolicy(QSizePolicy.Policy.Preferred,QSizePolicy.Policy.Maximum);layout.addWidget(hint)
        self.empty=QWidget();layout.addWidget(self.empty,1)
        self.kind.widget.currentIndexChanged.connect(self.controls);self.max_level.valueChanged.connect(self.set_maximum);self.table.itemChanged.connect(self.notify)
        self.loading=False

    def notify(self,*_):
        if not self.loading:self.changed()

    def controls(self,*_):
        core=self.kind.value()=='1';self.channel.widget.setEnabled(core);self.chance.widget.setEnabled(not core);self.core.setVisible(core);self.empty.setVisible(not core);self.notify()

    def capture(self):
        for row in range(self.table.rowCount()):
            self.drafts[row]={k:self.table.item(row,col).text() for col,(k,_) in enumerate(self.COLUMNS,1)}

    def render(self):
        was=self.loading;self.loading=True;self.table.setRowCount(self.maximum+1)
        for row in range(self.maximum+1):
            if row not in self.drafts:
                source=copy.deepcopy(self.drafts.get(row-1,catalog.Catalog.soul_level(0)))
                self.drafts[row]={k:catalog.display(source[k],1000 if k=='recharge_interval_ms' else 100 if k.endswith('_bp') else 1) for k,_ in self.COLUMNS} if 'level' in source else source
            cell=QTableWidgetItem('+'+str(row));cell.setFlags(Qt.ItemFlag.ItemIsEnabled|Qt.ItemFlag.ItemIsSelectable);self.table.setItem(row,0,cell)
            for col,(key,_) in enumerate(self.COLUMNS,1):self.table.setItem(row,col,QTableWidgetItem(self.drafts[row][key]))
        self.loading=was

    def set_maximum(self,value):
        if self.loading:return
        self.capture();self.maximum=value;self.render();self.notify()

    def load(self,document):
        self.loading=True;self.form.load(dict(child=dict(document['child'],damage_channel=document['child'].get('damage_channel') or '1')))
        self.maximum=int(document['refinement']['max_level']);self.max_level.setValue(self.maximum)
        self.drafts={int(r['level']):{k:catalog.display(r[k],1000 if k=='recharge_interval_ms' else 100 if k.endswith('_bp') else 1) for k,_ in self.COLUMNS} for r in document['soul_levels']}
        self.render();self.controls();self.loading=False

    def collect(self,document):
        self.capture();document['child']['soul_kind']=self.kind.value();core=document['child']['soul_kind']=='1'
        document['child']['damage_channel']=self.channel.value() if core else None
        document['child']['refine_chance_bp']='0' if core else self.chance.value()
        document['bonuses']=[]
        document['refinement'].update(profile_kind='8' if core else '1',max_level=str(self.maximum) if core else '0')
        document['levels']=[dict(level=str(i),**{k:'0' for k in catalog.GAINS},extra_bonus_profile_id=None,bonuses=[]) for i in range((self.maximum if core else 0)+1)]
        if not core:
            document['child'].update(damage_channel=None,profile_id=None);document['soul_levels']=[];return
        document['base']['max_stack']='1';document['child']['refine_chance_bp']='0';result=[]
        for row in range(self.maximum+1):
            values=dict(level=str(row))
            for key,label in self.COLUMNS:
                try:values[key]=str(catalog.scaled(self.drafts[row][key],1000 if key=='recharge_interval_ms' else 100 if key.endswith('_bp') else 1))
                except api.EditorError:raise api.EditorError(f'Lélek +{row}, {label}: érvénytelen szám.') from None
            result.append(values)
        document['soul_levels']=result
