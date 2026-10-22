"""Package search and reviewed installations using RemoteWork's SSH jobs."""
import json

from PySide6.QtCore import QTimer
from PySide6.QtWidgets import (QAbstractItemView, QHBoxLayout, QHeaderView, QLabel,
    QLineEdit, QPlainTextEdit, QScrollArea, QTableWidget, QTableWidgetItem, QVBoxLayout, QWidget)

from backend import EditorError
import remote_packages as packages
import ssh_connection as ssh


class RemotePackages:
    def __init__(self, work):
        self.work = work
        self.page = work.page
        self.plan = None
        self.pending = ''
        widget = QWidget()
        layout = QVBoxLayout(widget)
        self.destination = QLabel()
        self.destination.setWordWrap(True)
        layout.addWidget(self.destination)
        hint = QLabel('FreeBSD csomagkezelő. Keress egy csomagra, vagy listázd a telepítetteket. '
                      'A csomagneveket add a telepítendő listához, majd ellenőrizd a tervet. '
                      'Telepítéshez és katalógusfrissítéshez root SSH-felhasználó szükséges.')
        hint.setWordWrap(True)
        layout.addWidget(hint)
        row = QHBoxLayout()
        self.search = QLineEdit()
        self.search.setPlaceholderText('Csomagnév vagy névrészlet, pl. ninja, cmake, mariadb')
        self.search_button = work.button('Keresés', lambda: self.start('search'))
        self.installed_button = work.button('Telepített csomagok', lambda: self.start('installed'))
        self.update_button = work.button('Katalógus frissítése', lambda: self.start('update'))
        self.update_button.setToolTip('pkg update: csak az elérhető csomagok jegyzékét frissíti.')
        row.addWidget(self.search, 1)
        for button in (self.search_button, self.installed_button, self.update_button):
            row.addWidget(button)
        layout.addLayout(row)
        self.search.returnPressed.connect(lambda: self.start('search'))
        self.table = QTableWidget(0, 3)
        self.table.setHorizontalHeaderLabels(['Csomag', 'Verzió', 'Leírás'])
        self.table.horizontalHeader().setSectionResizeMode(2, QHeaderView.ResizeMode.Stretch)
        self.table.setColumnWidth(0, 220)
        self.table.setColumnWidth(1, 145)
        self.table.setMinimumHeight(150)
        self.table.setEditTriggers(QAbstractItemView.EditTrigger.NoEditTriggers)
        self.table.setSelectionBehavior(QAbstractItemView.SelectionBehavior.SelectRows)
        self.table.itemDoubleClicked.connect(lambda *_: self.add_selected())
        layout.addWidget(self.table, 2)
        row = QHBoxLayout()
        self.count = QLabel('Még nincs lekért csomaglista.')
        self.add_button = work.button('Kijelöltek hozzáadása', self.add_selected)
        row.addWidget(self.count, 1)
        row.addWidget(self.add_button)
        layout.addLayout(row)
        row = QHBoxLayout()
        row.addWidget(QLabel('Telepítendő csomagok'))
        self.names = QLineEdit()
        self.names.setPlaceholderText('Pontos csomagnevek szóközzel, pl. cmake ninja pkgconf')
        row.addWidget(self.names, 1)
        self.plan_button = work.button('Telepítési terv', lambda: self.start('plan'))
        row.addWidget(self.plan_button)
        layout.addLayout(row)
        self.preview = QPlainTextEdit()
        self.preview.setReadOnly(True)
        self.preview.setPlaceholderText('A terv itt mutatja meg a függőségeket, frissítéseket és a letöltés méretét.')
        self.preview.setMinimumHeight(150)
        layout.addWidget(self.preview, 1)
        row = QHBoxLayout()
        note = QLabel('A telepítés a szerver rendszerét módosítja. Kapcsolatszakadáskor is tovább futhat; '
                      'napló: /var/log/auriga-manager/packages. Telepítés közben az appot hagyd nyitva.')
        note.setWordWrap(True)
        row.addWidget(note, 1)
        self.install_button = work.button('Terv szerinti telepítés', lambda: self.start('install'))
        self.install_button.setObjectName('Primary')
        row.addWidget(self.install_button)
        self.scroll = QScrollArea()
        self.scroll.setWidgetResizable(True)
        self.scroll.setWidget(widget)
        container = QWidget()
        outer = QVBoxLayout(container)
        outer.setContentsMargins(0, 0, 0, 0)
        outer.addWidget(self.scroll, 1)
        outer.addLayout(row)
        work.controls_list.extend([self.search, self.names, self.table])
        self.names.textChanged.connect(self.invalidate)
        self.page.tabs.addTab(container, 'Csomagkezelő')
        self.controls(False)

    def invalidate(self, *_):
        self.plan = None
        self.preview.clear()
        self.install_button.setEnabled(False)

    def reset(self):
        self.invalidate()
        self.table.setRowCount(0)
        self.count.setText('Még nincs lekért csomaglista.')
        self.names.clear()
        self.search.clear()

    def controls(self, enabled):
        enabled = enabled and self.page.target_allowed()
        for button in (self.search_button, self.installed_button, self.update_button, self.plan_button):
            button.setEnabled(enabled)
        self.install_button.setEnabled(enabled and self.plan is not None)
        self.add_button.setEnabled(enabled and self.table.rowCount() > 0)
        # Display the editable SSH endpoint; the operation itself validates its profile.
        host = self.page.target['host'].text()
        user = self.page.target['user'].text()
        self.destination.setText('Csomagok célgépe: ' + (user + '@' + host if host else 'Válassz távoli SSH-profilt.'))

    def add_selected(self):
        rows = sorted({index.row() for index in self.table.selectedIndexes()})
        selected = [self.table.item(row, 0).text() for row in rows]
        self.names.setText(' '.join(dict.fromkeys([*self.names.text().split(), *selected])))

    def start(self, operation):
        work = self.work
        if work.active() or self.page.probe_process is not None or self.page.owner_busy():
            return
        try:
            profile = self.page.operation_profile()
            if operation == 'install' and self.plan is None:
                raise EditorError('Telepítés előtt készíts telepítési tervet.')
            request = packages.request(operation, query=self.search.text(), packages=self.names.text(),
                                       fingerprint=self.plan['fingerprint'] if self.plan else '')
            if operation in ('plan', 'install', 'update'):
                self.invalidate()
            work.session = ssh.Session(profile, parent=self.page)
            work.request = request
            work.begin_log()
            self.pending = ''
            work.start_worker(request, 'packages')
        except (EditorError, OSError, ValueError) as error:
            work.cleanup()
            self.page.owner.error(str(error))

    def display(self, text, final=False):
        self.pending += text
        lines = self.pending.splitlines(keepends=True)
        self.pending = ''
        for line in lines:
            if not final and not line.endswith(('\n', '\r')):
                self.pending = line
            elif not line.startswith(packages.RESULT):
                self.work.append(line)

    def completed(self, output):
        lines = [line[len(packages.RESULT):] for line in output.splitlines() if line.startswith(packages.RESULT)]
        if len(lines) != 1:
            raise ValueError('A csomagművelet siker-visszajelzése hiányzik vagy sérült.')
        result = json.loads(lines[0])
        operation = self.work.request['operation']
        if result.get('id') != self.work.request['id'] or result.get('operation') != operation:
            raise ValueError('A csomagművelet válasza másik kéréshez tartozik.')
        if operation in ('search', 'installed'):
            rows = result['rows']
            if (not isinstance(rows, list) or len(rows) > 500
                    or any(not isinstance(row, list) or len(row) != 3
                           or any(not isinstance(cell, str) for cell in row) for row in rows)):
                raise ValueError('Hibás csomaglista érkezett.')
            self.table.setRowCount(len(rows))
            for row, cells in enumerate(rows):
                for column, cell in enumerate(cells):
                    self.table.setItem(row, column, QTableWidgetItem(cell))
            self.count.setText('%d csomag; %d megjelenítve.' % (result['total'], len(rows)))
            return self.count.text()
        if operation == 'plan':
            packages.validate(dict(operation='install', packages=result['packages'], fingerprint=result['fingerprint']))
            if result['packages'] != self.work.request['packages'] or not isinstance(result['plan'], str):
                raise ValueError('A telepítési terv nem a kért csomagokhoz tartozik.')
            self.plan = result
            self.preview.setPlainText(result['plan'])
            QTimer.singleShot(0, lambda: self.scroll.verticalScrollBar().setValue(
                self.scroll.verticalScrollBar().maximum()))
            return 'A telepítési terv elkészült. Ellenőrizd, majd indítsd a terv szerinti telepítést.'
        return ('A csomagok telepítése sikeresen befejeződött.' if operation == 'install' else
                'A csomagkatalógus frissítése sikeresen befejeződött.')
