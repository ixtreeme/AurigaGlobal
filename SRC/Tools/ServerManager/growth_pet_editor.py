"""Growth progression authoring only; never edits a player's pet state."""
import copy
import re
from PySide6.QtCore import Qt
from PySide6.QtWidgets import (QComboBox,QFormLayout,QHeaderView,QHBoxLayout,QLabel,
    QPushButton,QSpinBox,QTableWidget,QTableWidgetItem,QVBoxLayout,QWidget)
import backend as api
import item_catalog as catalog


def number(text,label,low=0,high=2**63-1):
    text=str(text).strip()
    if not re.fullmatch('[0-9]+',text) or not low<=int(text)<=high:
        raise api.EditorError(f'{label}: {low} és {high} közötti egész szám kell.')
    return str(int(text))


class GrowthPetPanel(QWidget):
    def __init__(self,changed):
        super().__init__();self.changed=changed;self.loading=True;self.forms=[];self.drafts={};self.old_maximum=1
        layout=QVBoxLayout(self);form=QFormLayout()
        self.maximum=QSpinBox();self.maximum.setRange(1,255)
        self.policy=QComboBox();self.policy.addItem('Mob EXP önállóan is elég','1');self.policy.addItem('Mob és tárgy EXP együtt kell','2')
        form.addRow('Pet legmagasabb szintje',self.maximum);form.addRow('Szintlépési szabály',self.policy);layout.addLayout(form)
        hint=QLabel('Ez a pet saját szintje, külön a hívótárgy +N fejlesztésétől. Minden sor a következő szinthez '
                    'szükséges EXP. Az evolúciós szakasz plafonján külön evolúció kell, amely egy szintet is lép. '
                    'A legmagasabb szinten a két EXP-küszöb nulla. Az új sorok 1 EXP-es szerkesztési minták: állítsd be a kívánt értékeket.')
        hint.setWordWrap(True);layout.addWidget(hint)
        self.levels=QTableWidget(0,3);self.levels.setHorizontalHeaderLabels(['Szint','Mob EXP','Tárgy EXP'])
        self.levels.verticalHeader().hide();self.levels.horizontalHeader().setSectionResizeMode(QHeaderView.ResizeMode.Stretch)
        self.levels.setMinimumHeight(210);layout.addWidget(self.levels)
        layout.addWidget(QLabel('Evolúciós szakaszok — a 0. szakasz az induló forma'))
        self.stages=QTableWidget(0,3);self.stages.setHorizontalHeaderLabels(['Szakasz','Szintplafon','Követőforma'])
        self.stages.verticalHeader().hide();self.stages.horizontalHeader().setSectionResizeMode(QHeaderView.ResizeMode.Stretch)
        self.stages.setMinimumHeight(150);layout.addWidget(self.stages)
        buttons=QHBoxLayout();add=QPushButton('Szakasz hozzáadása');remove=QPushButton('Utolsó szakasz törlése')
        add.clicked.connect(self.add_stage);remove.clicked.connect(self.remove_stage);buttons.addWidget(add);buttons.addWidget(remove);buttons.addStretch();layout.addLayout(buttons)
        self.maximum.valueChanged.connect(self.resize_levels);self.policy.currentIndexChanged.connect(self.notify)
        self.levels.itemChanged.connect(self.notify);self.stages.itemChanged.connect(self.notify)
        self.load(catalog.Catalog.growth_defaults());self.loading=False

    def notify(self,*_):
        if not self.loading:self.changed()

    def set_forms(self,rows):
        self.forms=copy.deepcopy(rows)

    def load(self,document):
        was=self.loading;self.loading=True;default=catalog.Catalog.growth_defaults()
        profile=document.get('growth') or default['growth']
        self.maximum.setValue(int(profile['maximum_level']));self.old_maximum=self.maximum.value()
        self.policy.setCurrentIndex(self.policy.findData(str(profile['experience_policy'])))
        self.drafts={int(r['level']):copy.deepcopy(r) for r in document.get('growth_levels',default['growth_levels'])}
        self.render_levels();self.render_stages(document.get('growth_stages',default['growth_stages']));self.loading=was

    def capture_levels(self):
        for row in range(self.levels.rowCount()):
            level=row+1
            if level==self.old_maximum:continue
            self.drafts[level]=dict(level=str(level),mob_experience=self.levels.item(row,1).text().strip(),item_experience=self.levels.item(row,2).text().strip())

    def render_levels(self):
        was=self.loading;self.loading=True;maximum=self.maximum.value()
        self.levels.clearContents();self.levels.setRowCount(maximum)
        for row in range(maximum):
            level=row+1;values=self.drafts.get(level,dict(mob_experience='1',item_experience='1'))
            if level==maximum:values=dict(mob_experience='0',item_experience='0')
            elif values['mob_experience']=='0' and values['item_experience']=='0':values=dict(mob_experience='1',item_experience='1')
            for col,value in enumerate([str(level),values['mob_experience'],values['item_experience']]):
                item=QTableWidgetItem(value)
                if col==0 or level==maximum:item.setFlags(Qt.ItemFlag.ItemIsEnabled|Qt.ItemFlag.ItemIsSelectable)
                self.levels.setItem(row,col,item)
        self.old_maximum=maximum;self.loading=was

    def stage_rows(self,validate=False):
        result=[]
        for row in range(self.stages.rowCount()):
            maximum=self.stages.item(row,1).text().strip()
            if validate:maximum=number(maximum,f'{row}. szakasz szintplafonja',1,255)
            result.append(dict(stage=str(row),maximum_level=maximum,form_id=None if row==0 else self.stages.cellWidget(row,2).currentData()))
        return result

    def render_stages(self,rows):
        was=self.loading;self.loading=True
        for row in range(self.stages.rowCount()):
            widget=self.stages.cellWidget(row,2)
            if widget:widget.hide()
        self.stages.clearContents();self.stages.setRowCount(len(rows))
        for row,value in enumerate(rows):
            item=QTableWidgetItem(str(row));item.setFlags(Qt.ItemFlag.ItemIsEnabled|Qt.ItemFlag.ItemIsSelectable);self.stages.setItem(row,0,item)
            self.stages.setItem(row,1,QTableWidgetItem(str(value['maximum_level'])))
            combo=QComboBox();combo.addItem('Induló követőforma',None)
            for form in self.forms:combo.addItem(f"{form['code']} — lény {form['mob_template_id']}",form['form_id'])
            selected=None if row==0 else value.get('form_id');index=combo.findData(selected)
            if index<0 and selected:
                combo.addItem(f'Hiányzó forma: {selected}',selected);index=combo.count()-1
            combo.setCurrentIndex(max(0,index));combo.setEnabled(row!=0)
            combo.currentIndexChanged.connect(self.notify);self.stages.setCellWidget(row,2,combo)
        self.loading=was

    def resize_levels(self,maximum):
        if self.loading:return
        self.capture_levels();rows=self.stage_rows()
        if rows and rows[-1]['maximum_level']==str(self.old_maximum):rows[-1]['maximum_level']=str(maximum)
        self.render_levels();self.render_stages(rows);self.notify()

    def add_stage(self):
        rows=self.stage_rows()
        if len(rows)>=16:return
        rows.append(dict(stage=str(len(rows)),maximum_level=str(self.maximum.value()),form_id=None));self.render_stages(rows);self.notify()

    def remove_stage(self):
        rows=self.stage_rows()
        if len(rows)<=1:return
        rows.pop();rows[-1]['maximum_level']=str(self.maximum.value());self.render_stages(rows);self.notify()

    def collect(self,document):
        self.capture_levels();maximum=self.maximum.value()
        levels=[]
        for row in range(maximum):
            level=row+1
            levels.append(dict(level=str(level),mob_experience=number(self.levels.item(row,1).text(),f'{level}. szint mob EXP'),
                               item_experience=number(self.levels.item(row,2).text(),f'{level}. szint tárgy EXP')))
        document.update(growth=dict(maximum_level=str(maximum),experience_policy=self.policy.currentData()),
                        growth_levels=levels,growth_stages=self.stage_rows(True))
