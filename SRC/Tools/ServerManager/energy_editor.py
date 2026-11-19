"""Authored birth-strength chances for native energy crystals."""
from PySide6.QtWidgets import QWidget,QVBoxLayout,QHBoxLayout,QTableWidget,QPushButton,QSpinBox,QDoubleSpinBox,QLabel

class EnergyTierTable(QWidget):
    def __init__(self,changed,parent=None):
        super().__init__(parent);self.changed=changed
        layout=QVBoxLayout(self);layout.addWidget(QLabel('Az erősség létrehozáskor dől el és a tárgyon megmarad. Az esélyek összege 100% legyen.'))
        self.table=QTableWidget(0,2);self.table.setHorizontalHeaderLabels(['Erősség','Esély']);layout.addWidget(self.table)
        buttons=QHBoxLayout();add=QPushButton('Új erősség');remove=QPushButton('Kijelölt erősség törlése');buttons.addWidget(add);buttons.addWidget(remove);layout.addLayout(buttons)
        add.clicked.connect(lambda:self.add());remove.clicked.connect(self.remove)
    def add(self,percent=1,chance=100):
        row=self.table.rowCount();self.table.insertRow(row)
        strength=QSpinBox();strength.setRange(1,100);strength.setSuffix('%');strength.setValue(int(percent))
        weight=QDoubleSpinBox();weight.setRange(.01,100);weight.setDecimals(2);weight.setSuffix('%');weight.setValue(int(chance)/100)
        for column,widget in enumerate((strength,weight)):
            self.table.setCellWidget(row,column,widget);widget.valueChanged.connect(lambda *_:self.changed())
        self.changed()
    def remove(self):
        row=self.table.currentRow()
        if row>=0:self.table.removeRow(row);self.changed()
    def load(self,rows):
        self.table.setRowCount(0)
        for row in rows:self.add(row['percent'],row['chance_bp'])
    def entries(self):
        return [dict(percent=str(self.table.cellWidget(n,0).value()),chance_bp=str(round(self.table.cellWidget(n,1).value()*100))) for n in range(self.table.rowCount())]
