"""Ordinary costume authoring. No SQL or saved-instance timer editing."""
from PySide6.QtCore import Qt
from PySide6.QtWidgets import (QComboBox, QFormLayout, QLabel, QLineEdit,
    QTableWidget, QTableWidgetItem, QHeaderView, QVBoxLayout, QWidget)
import backend as api
import item_catalog as catalog


class CostumePanel(QWidget):
    def __init__(self, changed):
        super().__init__(); self.changed = changed; self.loading = True
        layout = QVBoxLayout(self); form = QFormLayout()
        self.kind = QComboBox()
        for key, label in ((1, 'Testkosztüm'), (2, 'Haj'), (3, 'Fegyverkosztüm')): self.kind.addItem(label, str(key))
        self.weapon = QComboBox()
        for key, label in enumerate(('Kard', 'Tőr', 'Íj', 'Kétkezes', 'Csengő', 'Legyező', 'Lovas lándzsa'), 1): self.weapon.addItem(label, str(key))
        self.clock = QComboBox()
        for key, label in ((1, 'Korlátlan'), (2, 'Létrehozástól'), (3, 'Első használattól'), (4, 'Csak viselés közben')): self.clock.addItem(label, str(key))
        self.duration = QLineEdit('0'); self.duration.setPlaceholderText('Másodperc, legfeljebb 3 tizedesjegy')
        form.addRow('Kosztüm típusa', self.kind); form.addRow('Szükséges fegyver', self.weapon)
        form.addRow('Lejárat számítása', self.clock); form.addRow('Teljes idő (mp)', self.duration); layout.addLayout(form)
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
        self.weapon.setEnabled(self.kind.currentData() == '3')
        self.duration.setEnabled(self.clock.currentData() != '1')
        self.notify()

    def load(self, document):
        self.loading = True; child = document['child']
        for widget, value in ((self.kind, child['costume_kind']), (self.weapon, child.get('weapon_subtype') or '1'), (self.clock, child['clock_kind'])):
            widget.setCurrentIndex(widget.findData(str(value)))
        self.duration.setText(catalog.display(child['duration_ms']))
        values = {(int(v['character_class']), int(v['sex'])): v['model_asset'] for v in document.get('visuals', [])}
        for row in range(8): self.visuals.item(row, 2).setText(values.get((row//2+1, row%2+1), ''))
        self.controls(); self.loading = False

    def collect(self, document):
        clock, kind = self.clock.currentData(), self.kind.currentData()
        duration = catalog.scaled(self.duration.text()) if clock != '1' else 0
        if not 0 <= duration <= 2**63-1 or (clock != '1' and not duration):
            raise api.EditorError('Kosztüm időtartama: pozitív másodpercet adj meg, vagy válaszd a korlátlant.')
        document['child'].update(costume_kind=kind, weapon_subtype=self.weapon.currentData() if kind == '3' else None,
                                 clock_kind=clock, duration_ms=str(duration))
        document['visuals'] = [dict(character_class=str(row//2+1), sex=str(row%2+1), model_asset=self.visuals.item(row, 2).text().strip())
                               for row in range(8) if self.visuals.item(row, 2).text().strip()]
