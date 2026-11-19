"""Channel/core planner backed by the ordinary checked server.json editor."""
import copy
import json
from pathlib import Path

from PySide6.QtCore import Qt
from PySide6.QtWidgets import (QAbstractItemView, QCheckBox, QComboBox, QDialog, QDialogButtonBox,
    QFileDialog, QFormLayout, QHBoxLayout, QLabel, QLineEdit, QListWidget, QListWidgetItem,
    QMessageBox, QPushButton, QSpinBox, QTreeWidget, QTreeWidgetItem, QVBoxLayout)
import backend as api
import channel_config as topology


def hint(text):
    value = QLabel(text)
    value.setWordWrap(True)
    value.setObjectName('Subtle')
    return value


def finish_buttons(dialog, accept='Alkalmazás'):
    buttons = QDialogButtonBox()
    apply = buttons.addButton(accept, QDialogButtonBox.ButtonRole.AcceptRole)
    apply.setObjectName('Primary')
    buttons.addButton('Mégse', QDialogButtonBox.ButtonRole.RejectRole)
    buttons.accepted.connect(dialog.accept)
    buttons.rejected.connect(dialog.reject)
    return buttons


class MapDialog(QDialog):
    def __init__(self, config, channel, names, selected=(), parent=None):
        super().__init__(parent)
        self.setWindowTitle('Core térképei · CH%d' % channel)
        self.resize(660, 620)
        self.result_maps = None
        self.names = names
        layout = QVBoxLayout(self)
        layout.addWidget(hint('Csak a kijelölt térképek futnak ebben a core-ban. A közös játékadatok nem másolódnak.'))
        self.search = QLineEdit()
        self.search.setPlaceholderText('Térkép neve vagy azonosítója…')
        layout.addWidget(self.search)
        self.maps = QListWidget()
        self.maps.setSelectionMode(QAbstractItemView.SelectionMode.NoSelection)
        owners = {}
        for key in topology.members(config, channel):
            for value in topology.effective(config, key).get('map_allow', []): owners.setdefault(value, []).append(key)
        known = sorted(set(names) | set(owners) | set(selected))
        for identity in known:
            caption = '%d · %s' % (identity, names.get(identity, 'Térkép'))
            if identity in owners: caption += '  —  ' + ', '.join(owners[identity])
            item = QListWidgetItem(caption)
            item.setData(Qt.ItemDataRole.UserRole, identity)
            item.setFlags(item.flags() | Qt.ItemFlag.ItemIsUserCheckable)
            item.setCheckState(Qt.CheckState.Checked if identity in selected else Qt.CheckState.Unchecked)
            self.maps.addItem(item)
        self.search.textChanged.connect(self.filter)
        layout.addWidget(self.maps, 1)
        self.extra = QLineEdit()
        self.extra.setPlaceholderText('További térképazonosítók, pl. 21, 41')
        layout.addWidget(self.extra)
        self.transfer = QCheckBox('A térképek áthelyezése a CH más core-jaitól')
        self.transfer.setChecked(True)
        self.transfer.setToolTip('Csak ezen a csatornán módosítja a felosztást. A többi CH térképei megmaradnak.')
        layout.addWidget(self.transfer)
        layout.addWidget(hint('Legfeljebb 64 térkép/core. Ha egy másik core kiürül, külön törölheted.'))
        self.problem = hint('')
        self.problem.setObjectName('InlineError')
        self.problem.hide()
        layout.addWidget(self.problem)
        layout.addWidget(finish_buttons(self, 'Térképek kiválasztása'))

    def filter(self, text):
        for index in range(self.maps.count()):
            item = self.maps.item(index)
            item.setHidden(text.casefold() not in item.text().casefold())

    def accept(self):
        try:
            selected = [self.maps.item(i).data(Qt.ItemDataRole.UserRole) for i in range(self.maps.count())
                        if self.maps.item(i).checkState() == Qt.CheckState.Checked]
            selected += topology.map_ids(self.extra.text())
            self.result_maps = topology.map_ids(selected)
            if not self.result_maps: raise api.EditorError('Válassz legalább egy térképet.')
            if self.names and set(self.result_maps) - set(self.names):
                raise api.EditorError('A csomag térképindexében nem szereplő térképet választottál.')
            super().accept()
        except api.EditorError as error:
            self.problem.setText(str(error)); self.problem.show()


class NewChannelDialog(QDialog):
    def __init__(self, config, parent=None):
        super().__init__(parent)
        self.setWindowTitle('Új csatorna')
        self.resize(520, 280)
        layout = QVBoxLayout(self)
        form = QFormLayout()
        self.number = QSpinBox()
        self.number.setRange(1, 255)
        self.number.setValue(topology.next_channel(config) or 1)
        form.addRow('Új CH száma', self.number)
        self.source = QComboBox()
        for channel in topology.channels(config):
            if channel != 99: self.source.addItem('CH%d · térképfelosztás átvétele' % channel, channel)
        self.source.addItem('Egy core · térképek kiválasztása', None)
        form.addRow('Felépítés', self.source)
        layout.addLayout(form)
        layout.addWidget(hint('A mintából csak a térképfelosztást vesszük át. Az üres core-ok kimaradnak; '
                              'a portok és azonosítók újak lesznek. SQL, TLS és játékszabályok a közös alapértékekből jönnek. '
                              'A CH99 a közös csatorna.'))
        layout.addWidget(finish_buttons(self, 'Csatorna létrehozása'))


class ChannelDialog(QDialog):
    def __init__(self, config, saved_config, map_names=None, running=False, parent=None):
        super().__init__(parent)
        self.value, self.saved_config = copy.deepcopy(config), copy.deepcopy(saved_config)
        self.map_names = map_names or {}
        self.setWindowTitle('Csatornák és core-ok')
        self.resize(min(1040, parent.width() - 40) if parent else 1040,
                    min(680, parent.height() - 60) if parent else 680)
        self.setMinimumSize(820, 540)
        layout = QVBoxLayout(self)
        title = QLabel('Csatornák és core-ok')
        title.setObjectName('Title')
        layout.addWidget(title)
        layout.addWidget(hint('CH = játékosok által választható csatorna. Core = a CH bizonyos térképeit futtató folyamat. '
                              'A CH99 közös területeket szolgál ki.'))
        bar = QHBoxLayout()
        self.add_channel_button = QPushButton('Új CH')
        self.add_core_button = QPushButton('Új core')
        self.edit_button = QPushButton('Térképek módosítása')
        self.delete_button = QPushButton('Kijelölt törlése')
        self.delete_button.setObjectName('Stop')
        for widget, method in ((self.add_channel_button, self.add_channel), (self.add_core_button, self.add_core),
                               (self.edit_button, self.edit_maps), (self.delete_button, self.remove)):
            widget.clicked.connect(method)
            bar.addWidget(widget)
        bar.addStretch()
        layout.addLayout(bar)
        self.tree = QTreeWidget()
        self.tree.setHeaderLabels(['Csatorna / core', 'Térképek', 'Játék / P2P port', 'Core ID'])
        self.tree.setColumnWidth(0, 260)
        self.tree.setColumnWidth(1, 300)
        self.tree.setColumnWidth(2, 170)
        self.tree.currentItemChanged.connect(self.selection_changed)
        self.tree.itemDoubleClicked.connect(lambda item, column: self.edit_maps())
        layout.addWidget(self.tree, 1)
        self.details = hint('')
        layout.addWidget(self.details)
        tools = QHBoxLayout()
        self.compact_button = QPushButton('Fölösleges felülírások elhagyása')
        self.compact_button.clicked.connect(self.compact)
        tools.addWidget(self.compact_button)
        self.export_button = QPushButton('Unity CH-lista exportálása…')
        self.export_button.clicked.connect(self.export)
        tools.addWidget(self.export_button)
        tools.addStretch()
        layout.addLayout(tools)
        self.notice = hint('A játék fut. Tervezni lehet; a konfigurációt a szerver leállítása után tudod menteni.' if running else
                           'Az átvétel után a Beállítások oldalon az Ellenőrzés és mentés gombbal véglegesítsd a változtatásokat.')
        layout.addWidget(self.notice)
        self.problem = hint('')
        self.problem.setObjectName('InlineError'); self.problem.hide()
        layout.addWidget(self.problem)
        layout.addWidget(finish_buttons(self, 'Módosítások átvétele'))
        self.rebuild()

    def selected(self):
        item = self.tree.currentItem()
        return item.data(0, Qt.ItemDataRole.UserRole) if item else None

    def rebuild(self, selected=None):
        selected = selected or self.selected()
        self.tree.clear()
        first = None
        for channel in topology.channels(self.value):
            keys = topology.members(self.value, channel)
            count = sum(len(topology.effective(self.value, key).get('map_allow', [])) for key in keys)
            group = QTreeWidgetItem(self.tree, ['CH%d%s' % (channel, ' · közös' if channel == 99 else ''),
                                               '%d core · %d térkép' % (len(keys), count)])
            group.setData(0, Qt.ItemDataRole.UserRole, (channel, None))
            if first is None: first = group
            if selected == (channel, None): first = group
            for key in keys:
                value = topology.effective(self.value, key)
                maps = value.get('map_allow', [])
                caption = ', '.join(map(str, maps)) if maps else 'Üres · nincs kiosztott térkép'
                item = QTreeWidgetItem(group, [key, caption, '%s / %s' % (value['network']['port'], value['network']['p2p_port']),
                                               str(value['server'].get('server_id', 0))])
                item.setData(0, Qt.ItemDataRole.UserRole, (channel, key))
                item.setToolTip(1, '\n'.join('%s · %s' % (m, self.map_names.get(m, 'Térkép')) for m in maps) or caption)
                if selected == (channel, key): first = item
        self.tree.expandAll()
        self.tree.setCurrentItem(first)
        try:
            synced = topology.client_manifest(self.value) == topology.client_manifest(self.saved_config)
        except (api.EditorError, KeyError): synced = False
        self.export_button.setEnabled(synced)
        self.export_button.setToolTip('A mentett, érvényes CH-lista exportálása a Unity klienshez.' if synced else
                                       'Előbb vedd át, ellenőrizd és mentsd a CH-módosításokat, majd nyisd meg újra ezt az ablakot.')
        self.selection_changed()

    def selection_changed(self, *args):
        selection = self.selected()
        self.add_core_button.setEnabled(selection is not None)
        self.delete_button.setEnabled(selection is not None)
        self.edit_button.setEnabled(bool(selection and selection[1]))
        if not selection: self.details.setText(''); return
        channel, key = selection
        if key:
            value = topology.effective(self.value, key)
            self.details.setText('%s · %d térkép · %s. Közös játékadatok; saját naplók és futási állományok.' %
                (key, len(value.get('map_allow', [])), value.get('working_directory', 'run/' + key)))
        else:
            self.details.setText('CH%d · %d core. Új core hozzáadásakor választhatod ki az átadandó térképeket.' %
                                 (channel, len(topology.members(self.value, channel))))

    def apply(self, operation):
        try:
            value, selected = operation()
            self.value = value
            self.problem.hide()
            self.rebuild(selected)
        except (api.EditorError, ValueError, KeyError) as error:
            self.problem.setText(str(error)); self.problem.show()

    def pick_maps(self, channel, selected=()):
        dialog = MapDialog(self.value, channel, self.map_names, selected, self)
        return dialog if dialog.exec() == QDialog.DialogCode.Accepted else None

    def add_channel(self):
        dialog = NewChannelDialog(self.value, self)
        if dialog.exec() != QDialog.DialogCode.Accepted: return
        number, source = dialog.number.value(), dialog.source.currentData()
        maps = None
        if source is None:
            picker = self.pick_maps(number)
            if not picker: return
            maps = picker.result_maps
        def operation():
            candidate, names = topology.add_channel(self.value, number, source=source, maps=maps)
            return candidate, (number, None)
        self.apply(operation)

    def add_core(self):
        selection = self.selected()
        if not selection: return
        picker = self.pick_maps(selection[0])
        if not picker: return
        def operation():
            candidate, name = topology.add_core(self.value, selection[0], picker.result_maps, picker.transfer.isChecked())
            return candidate, (selection[0], name)
        self.apply(operation)

    def edit_maps(self):
        selection = self.selected()
        if not selection or not selection[1]: return
        channel, key = selection
        picker = self.pick_maps(channel, topology.effective(self.value, key).get('map_allow', []))
        if picker:
            self.apply(lambda: (topology.assign_maps(self.value, key, picker.result_maps, picker.transfer.isChecked()), selection))

    def remove(self):
        selection = self.selected()
        if not selection: return
        channel, key = selection
        count = 1 if key else len(topology.members(self.value, channel))
        answer = QMessageBox.question(self, 'Core / csatorna törlése',
            '%s törlése a konfigurációból (%d core)?\nA hozzá tartozó térképek kiosztását ellenőrizd. '
            'A naplók, mentések, karakterek és közös játékadatok megmaradnak.' % (key or 'CH%d' % channel, count),
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No, QMessageBox.StandardButton.No)
        if answer == QMessageBox.StandardButton.Yes:
            self.apply(lambda: (topology.remove(self.value, channel=channel, name=key), (channel, None)))

    def compact(self):
        self.apply(lambda: (topology.compact(self.value), self.selected()))
        self.notice.setText('A közös alapértékekkel egyező felülírásokat elhagytuk. Az effektív core-beállítások megmaradtak.')

    def export(self):
        try:
            packet = topology.client_manifest(self.saved_config)
            path, _ = QFileDialog.getSaveFileName(self, 'Unity CH-lista exportálása', 'AurigaServerChannels.json', 'JSON (*.json)')
            if not path: return
            Path(path).write_text(json.dumps(packet, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
            self.notice.setText('Exportálva. Unity: Metin2 → Import server channels…; válaszd ki ezt a JSON-fájlt.')
        except (api.EditorError, OSError) as error:
            self.problem.setText(str(error)); self.problem.show()
