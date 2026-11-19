"""Ordinary costume authoring. No SQL or saved-instance timer editing."""
from PySide6.QtCore import Qt
from PySide6.QtWidgets import (QCheckBox, QComboBox, QFormLayout, QLabel, QLineEdit,
    QTableWidget, QTableWidgetItem, QHeaderView, QVBoxLayout, QWidget)
import backend as api
import item_catalog as catalog


class CostumePanel(QWidget):
    def __init__(self, changed):
        super().__init__(); self.changed = changed; self.loading = True
        layout = QVBoxLayout(self); form = QFormLayout()
        self.kind = QComboBox()
        for key, label in ((1, 'Testkosztüm'), (2, 'Haj'), (3, 'Fegyverkosztüm')): self.kind.addItem(label, str(key))
        self.kind.addItem('Kedvenc megjelenés', '4'); self.kind.addItem('Hátas megjelenés', '5'); self.kind.addItem('Páncéleffekt', '6'); self.kind.addItem('Fegyvereffekt', '7')
        self.weapon = QComboBox()
        for key, label in enumerate(('Kard', 'Tőr', 'Íj', 'Kétkezes', 'Csengő', 'Legyező', 'Lovas lándzsa'), 1): self.weapon.addItem(label, str(key))
        self.clock = QComboBox()
        for key, label in ((1, 'Korlátlan'), (2, 'Létrehozástól'), (3, 'Első használattól'), (4, 'Csak viselés közben')): self.clock.addItem(label, str(key))
        self.companion_form = QLineEdit(); form.addRow('Társforma azonosító', self.companion_form)
        self.companion_form.textEdited.connect(self.notify)
        self.body_effect = QLineEdit(); form.addRow('Páncéleffekt azonosító', self.body_effect)
        self.body_effect.textEdited.connect(self.notify)
        self.weapon_effects=[]
        for label in ('Kard','Tőr','Íj','Kétkezes','Csengő','Legyező','Speciális tőr','Speciális legyező'):
            field=QLineEdit('0');field.textEdited.connect(self.notify);form.addRow(label+' effekt',field);self.weapon_effects.append(field)
        self.duration = QLineEdit('0'); self.duration.setPlaceholderText('Másodperc, legfeljebb 3 tizedesjegy')
        form.addRow('Kosztüm típusa', self.kind); form.addRow('Szükséges fegyver', self.weapon)
        form.addRow('Lejárat számítása', self.clock); form.addRow('Teljes idő (mp)', self.duration); layout.addLayout(form)
        self.transfer=QCheckBox('Bónuszátvitelben fogadó és átadó lehet');form.addRow(self.transfer);self.transfer.toggled.connect(self.notify)
        hint = QLabel('A megjelenés osztályonként és nemenként választható. A használati beállításokban engedélyezett '
                      'összes változathoz kell modell. Az időtartam új példányokra vonatkozik; mentett hátralévő időt itt nem írunk át.')
        hint.setWordWrap(True); layout.addWidget(hint)
        self.visuals = QTableWidget(8, 3); self.visuals.setHorizontalHeaderLabels(['Osztály', 'Nem', 'Modell elérési útja'])
        self.visuals.verticalHeader().hide(); self.visuals.horizontalHeader().setSectionResizeMode(2, QHeaderView.ResizeMode.Stretch)
        for row in range(8):
            for col, value in enumerate((('Harcos', 'Nindzsa', 'Sura', 'Sámán')[row//2], ('Férfi', 'Nő')[row%2], '')):
                cell = QTableWidgetItem(value)
                if col != 2: cell.setFlags(Qt.ItemFlag.ItemIsEnabled | Qt.ItemFlag.ItemIsSelectable)
                self.visuals.setItem(row, col, cell)
        layout.addWidget(self.visuals, 1)
        self.kind.currentIndexChanged.connect(self.controls); self.clock.currentIndexChanged.connect(self.controls)
        self.weapon.currentIndexChanged.connect(self.notify); self.duration.textEdited.connect(self.notify)
        self.visuals.itemChanged.connect(self.notify); self.controls(); self.loading = False

    def notify(self, *_):
        if not self.loading: self.changed()

    def controls(self, *_):
        skin=self.kind.currentData() in ('4','5')
        self.companion_form.setEnabled(skin); self.visuals.setEnabled(self.kind.currentData() in ('1','2','3')); self.body_effect.setEnabled(self.kind.currentData()=='6')
        for field in self.weapon_effects:field.setEnabled(self.kind.currentData()=='7')
        self.weapon.setEnabled(self.kind.currentData() == '3')
        self.transfer.setEnabled(self.kind.currentData() in ('1','2','3'))
        if not self.transfer.isEnabled():self.transfer.setChecked(False)
        self.duration.setEnabled(self.clock.currentData() != '1')
        self.notify()

    def load(self, document):
        self.loading = True; child = document['child']
        self.transfer.setChecked(bool(document.get('costume_transfer_allowed',False)))
        for widget, value in ((self.kind, child['costume_kind']), (self.weapon, child.get('weapon_subtype') or '1'), (self.clock, child['clock_kind'])):
            widget.setCurrentIndex(widget.findData(str(value)))
        self.companion_form.setText(child.get('companion_form_id') or '')
        self.body_effect.setText(child.get('body_effect_id') or '0')
        for field,key in zip(self.weapon_effects,('weapon_sword_effect_id', 'weapon_dagger_effect_id', 'weapon_bow_effect_id', 'weapon_two_handed_effect_id', 'weapon_bell_effect_id', 'weapon_fan_effect_id', 'weapon_dagger_alternate_effect_id', 'weapon_fan_alternate_effect_id')):field.setText(child.get(key) or '0')
        self.duration.setText(catalog.display(child['duration_ms']))
        values = {(int(v['character_class']), int(v['sex'])): v['model_asset'] for v in document.get('visuals', [])}
        for row in range(8): self.visuals.item(row, 2).setText(values.get((row//2+1, row%2+1), ''))
        self.controls(); self.loading = False

    def collect(self, document):
        clock, kind = self.clock.currentData(), self.kind.currentData()
        document['costume_transfer_allowed']=self.transfer.isChecked() if kind in ('1','2','3') else False
        duration = catalog.scaled(self.duration.text()) if clock != '1' else 0
        if not 0 <= duration <= 2**63-1 or (clock != '1' and not duration):
            raise api.EditorError('Kosztüm időtartama: pozitív másodpercet adj meg, vagy válaszd a korlátlant.')
        document['child'].update(costume_kind=kind, weapon_subtype=self.weapon.currentData() if kind == '3' else None,
                                 clock_kind=clock, duration_ms=str(duration), companion_form_id=self.companion_form.text().strip() if kind in ('4','5') else None)
        for key in ('weapon_sword_effect_id', 'weapon_dagger_effect_id', 'weapon_bow_effect_id', 'weapon_two_handed_effect_id', 'weapon_bell_effect_id', 'weapon_fan_effect_id', 'weapon_dagger_alternate_effect_id', 'weapon_fan_alternate_effect_id'):document['child'][key]='0'
        if kind=='7':
            for field,key in zip(self.weapon_effects,('weapon_sword_effect_id', 'weapon_dagger_effect_id', 'weapon_bow_effect_id', 'weapon_two_handed_effect_id', 'weapon_bell_effect_id', 'weapon_fan_effect_id', 'weapon_dagger_alternate_effect_id', 'weapon_fan_alternate_effect_id')):
                value=field.text().strip()
                if not value.isascii() or not value.isdigit() or not 0<int(value)<=65535:raise api.EditorError('Minden fegyvereffekt azonosítója 1 és 65535 közé essen.')
                document['child'][key]=str(int(value))
            document['child']['body_effect_id']='0';document['visuals']=[];return
        document['child']['body_effect_id']='0'
        if kind=='6':
            value=self.body_effect.text().strip()
            if not value.isascii() or not value.isdigit() or not 0<int(value)<=65535:raise api.EditorError('A páncéleffekt azonosítója 1 és 65535 közé essen.')
            document['child']['body_effect_id']=str(int(value));document['visuals']=[];return
        if kind in ('4','5'):
            value=document['child']['companion_form_id']
            if not value.isascii() or not value.isdigit() or not 0<int(value)<=2**32-1:raise api.EditorError('A társforma azonosítója pozitív egész szám legyen.')
            document['visuals']=[];return
        document['visuals'] = [dict(character_class=str(row//2+1), sex=str(row%2+1), model_asset=self.visuals.item(row, 2).text().strip())
                               for row in range(8) if self.visuals.item(row, 2).text().strip()]
