"""Explicit SSH connection and local-forward controls, independent of localhost."""
from pathlib import Path
import copy
import os
import shlex
import socket

from PySide6.QtCore import QProcess, QStandardPaths, QTimer
from PySide6.QtWidgets import (QAbstractItemView, QCheckBox, QComboBox, QFileDialog, QFormLayout,
    QHBoxLayout, QHeaderView, QLabel, QLineEdit, QMenu, QMessageBox, QPlainTextEdit,
    QPushButton, QScrollArea, QSpinBox, QTabWidget, QTableWidget,
    QTableWidgetItem, QVBoxLayout, QWidget)

from backend import EditorError
import ssh_connection as ssh
import ssh_password
from remote_work_page import RemoteWork


def store_path():
    return Path(QStandardPaths.writableLocation(QStandardPaths.StandardLocation.GenericConfigLocation)) / 'Auriga/ServerManager/ssh-profiles.json'


class RemotePage(QWidget):
    def __init__(self, parent, path=None):
        super().__init__()
        self.owner = parent
        self.store = ssh.ProfileStore(path or store_path())
        self.profiles, self.current_id = [], None
        self.probe_process = self.tunnel_process = None
        self.probe_session = self.tunnel_session = None
        self.probe_output, self.tunnel_output = '', ''
        self.probe_failure, self.tunnel_failure = '', ''
        self.loading, self.dirty = True, False
        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        title = QLabel('SSH-profilok és távoli eszközök')
        title.setObjectName('SectionTitle')
        layout.addWidget(title)
        hint = QLabel('A felső szerverválasztó kapcsolja az Áttekintést, a beállításokat és a fájlokat helyi vagy távoli célgépre. '
                      'Itt az SSH-profilokat és alagutakat állíthatod be; a build, feltöltés és csomagkezelés a fent kiválasztott távoli gépen fut.')
        hint.setWordWrap(True)
        hint.setObjectName('Subtle')
        layout.addWidget(hint)
        bar = QHBoxLayout()
        self.choice = QComboBox()
        self.choice.setPlaceholderText('Új, még nem mentett profil')
        self.choice.currentIndexChanged.connect(self.select_profile)
        bar.addWidget(self.choice, 1)
        self.new_button = self.make_button('Új profil', self.new)
        self.save_button = self.make_button('Profil mentése', self.save)
        self.reload_button = self.make_button('Lista újratöltése', self.reload)
        self.delete_button = self.make_button('Törlés', self.remove)
        for widget in (self.new_button, self.save_button, self.reload_button, self.delete_button):
            bar.addWidget(widget)
        layout.addLayout(bar)
        self.tabs = QTabWidget()
        layout.addWidget(self.tabs, 1)
        page = QWidget()
        form = QFormLayout(page)
        self.connection_form = form
        form.setContentsMargins(16, 12, 16, 12)
        self.name = QLineEdit()
        self.mode = QComboBox()
        self.mode.addItem('SSH-ugrógépen keresztül (ProxyJump)', 'proxyjump')
        self.mode.addItem('Közvetlen SSH (ugrógép nélkül)', 'direct')
        form.addRow('Profil neve', self.name)
        form.addRow('Kapcsolódás', self.mode)
        endpoints = QWidget()
        endpoint_layout = QHBoxLayout(endpoints)
        endpoint_layout.setContentsMargins(0, 5, 0, 5)
        self.target = self.endpoint(endpoint_layout, 'FreeBSD célgép')
        self.jump = self.endpoint(endpoint_layout, 'SSH-ugrógép')
        for endpoint in (self.target, self.jump):
            endpoint['auth'].currentIndexChanged.connect(self.update_auth)
            endpoint['auth'].currentIndexChanged.connect(self.changed)
            endpoint['remember_password'].toggled.connect(self.changed)
        form.addRow(endpoints)
        self.known_hosts = QLineEdit()
        self.identity_row = self.file_row(self.known_hosts)
        self.identity_row.setToolTip('A szerver azonosságát ellenőrzi; nem a belépéshez használt privátkulcs.')
        form.addRow('Szerverazonosítás (known_hosts)', self.identity_row)
        self.remote_root = QLineEdit()
        self.remote_root.setPlaceholderText('/usr/home/auriga/server — később is megadható')
        form.addRow('Távoli szervermappa', self.remote_root)
        self.auth_hint = QLabel()
        self.auth_hint.setWordWrap(True)
        self.auth_hint.setObjectName('Subtle')
        form.addRow(self.auth_hint)
        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setWidget(page)
        self.tabs.addTab(scroll, 'SSH-kapcsolat')
        forward_page = QWidget()
        forwards = QVBoxLayout(forward_page)
        forward_hint = QLabel('Például 13307 → 127.0.0.1:3306: a gépeden a 127.0.0.1:13307 címen érhető el '
                              'a FreeBSD gép helyi SQL-portja. A célcím mindig a FreeBSD szerver felől értendő. '
                              'A helyi figyelés kizárólag 127.0.0.1-en történik.')
        forward_hint.setWordWrap(True)
        forwards.addWidget(forward_hint)
        self.forward_table = QTableWidget(0, 3)
        self.forward_table.setHorizontalHeaderLabels(['Helyi port', 'Célcím a FreeBSD felől', 'Célport'])
        self.forward_table.horizontalHeader().setSectionResizeMode(QHeaderView.ResizeMode.Stretch)
        self.forward_table.setSelectionBehavior(QAbstractItemView.SelectionBehavior.SelectRows)
        self.forward_table.itemChanged.connect(self.changed)
        forwards.addWidget(self.forward_table, 1)
        forward_tools = QHBoxLayout()
        forward_tools.addWidget(self.make_button('Port hozzáadása', self.add_forward))
        forward_tools.addWidget(self.make_button('Kijelölt port törlése', self.remove_forward))
        forward_tools.addStretch()
        forwards.addLayout(forward_tools)
        self.tabs.addTab(forward_page, 'Porttovábbítások')
        self.connection_actions = QWidget()
        actions = QHBoxLayout(self.connection_actions)
        actions.setContentsMargins(0, 0, 0, 0)
        self.check_button = self.make_button('Kapcsolat ellenőrzése', self.check)
        self.check_button.setObjectName('Primary')
        self.quest_reload_button = self.make_button('Távoli questek', lambda: None)
        quest_menu = QMenu(self.quest_reload_button)
        quest_menu.addAction('Frissítés játék közben', self.reload_quests)
        quest_menu.addAction('Előkészítés leállított szervernél', lambda: self.reload_quests(offline=True))
        self.quest_reload_button.setMenu(quest_menu)
        self.tunnel_button = self.make_button('Portalagút indítása', self.start_tunnel)
        self.stop_button = self.make_button('Portalagút bontása', self.stop_tunnel)
        self.stop_button.setEnabled(False)
        for widget in (self.check_button, self.tunnel_button, self.stop_button, self.quest_reload_button):
            actions.addWidget(widget)
        actions.addStretch()
        layout.addWidget(self.connection_actions)
        self.status = QLabel('Nincs aktív portalagút. A ProxyJump-kapcsolat minden művelethez automatikusan felépül.')
        self.status.setWordWrap(True)
        layout.addWidget(self.status)
        self.output = QPlainTextEdit()
        self.output.setReadOnly(True)
        self.output.setMaximumHeight(110)
        self.output.document().setMaximumBlockCount(500)
        layout.addWidget(self.output)
        closing_hint = QLabel('Az app bezárása bontja az itt indított alagutakat. A távoli szervert ettől nem állítja le. '
                              'A kiválasztott távoli szerver az Áttekintés oldalon indítható és állítható le; a build és feltöltés külön füleken található.')
        closing_hint.setWordWrap(True)
        closing_hint.setObjectName('Subtle')
        layout.addWidget(closing_hint)
        self.closing_hint = closing_hint
        self.work = RemoteWork(self)
        self.remote_root.textChanged.connect(self.work.update_destination)
        self.tabs.currentChanged.connect(self.work_visibility)
        self.work_visibility()
        self.timer = QTimer(self)
        self.timer.timeout.connect(self.poll_tunnel)
        self.probe_timer = QTimer(self)
        self.probe_timer.setSingleShot(True)
        self.probe_timer.timeout.connect(self.probe_timeout)
        self.tunnel_timer = QTimer(self)
        self.tunnel_timer.setSingleShot(True)
        self.tunnel_timer.timeout.connect(self.tunnel_timeout)
        for edit in self.findChildren(QLineEdit):
            if edit not in (self.work.packages.search, self.work.packages.names):
                edit.textChanged.connect(self.changed)
        for spin in self.findChildren(QSpinBox):
            spin.valueChanged.connect(self.changed)
        self.mode.currentIndexChanged.connect(self.update_mode)
        self.mode.currentIndexChanged.connect(self.changed)
        self.loading = False
        self.reload(initial=True)

    @staticmethod
    def make_button(label, callback):
        button = QPushButton(label)
        button.clicked.connect(callback)
        return button

    def work_visibility(self, *_):
        work_tab = self.tabs.currentIndex() >= 2
        for widget in (self.connection_actions, self.status, self.output, self.closing_hint):
            widget.setVisible(not work_tab)
        for widget in (self.work.status, self.work.cancel_button, self.work.log):
            widget.setVisible(work_tab)

    def endpoint(self, layout, title):
        card = QWidget()
        card.setObjectName('FormCard')
        form = QFormLayout(card)
        form.setContentsMargins(12, 10, 12, 10)
        layout.addWidget(card, 1)
        label = QLabel(title)
        label.setObjectName('SectionTitle')
        form.addRow(label)
        host, user, key, alias = QLineEdit(), QLineEdit(), QLineEdit(), QLineEdit()
        port = QSpinBox()
        port.setRange(1, 65535)
        port.setValue(22)
        line = QWidget()
        row = QHBoxLayout(line)
        row.setContentsMargins(0, 0, 0, 0)
        row.addWidget(host, 1)
        row.addWidget(QLabel('Port'))
        row.addWidget(port)
        form.addRow('Szervercím', line)
        form.addRow('Felhasználó', user)
        auth = QComboBox()
        auth.addItem('SSH-kulcs / ssh-agent', 'key')
        auth.addItem('Jelszó', 'password')
        form.addRow('Belépés módja', auth)
        key_row = self.file_row(key)
        key.setPlaceholderText('Opcionális: alapértelmezett kulcs / agent')
        form.addRow('SSH-kulcs', key_row)
        password = QLineEdit()
        password.setEchoMode(QLineEdit.EchoMode.Password)
        password.setPlaceholderText('A szerverfelhasználó jelszava')
        password.setToolTip('A fenti felhasználónévhez tartozó belépési jelszó.')
        remember_password = QCheckBox('Jelszó megjegyzése')
        remember_password.setToolTip('Mentés a Windows Hitelesítőadat-kezelőjébe. A jelölés törlése és a Profil mentése eltávolítja a tárolt jelszót.')
        remember_password.setEnabled(os.name == 'nt')
        password_row = QWidget()
        password_layout = QHBoxLayout(password_row)
        password_layout.setContentsMargins(0, 0, 0, 0)
        password_layout.addWidget(password, 1)
        password_layout.addWidget(remember_password)
        form.addRow('Jelszó', password_row)
        alias.setPlaceholderText('Opcionális')
        form.addRow('Hostkulcs neve', alias)
        return dict(host=host, port=port, user=user, key_file=key, host_key_alias=alias, auth=auth,
                    password=password, password_row=password_row, remember_password=remember_password,
                    key_row=key_row, widgets=[card], form=form)

    def update_auth(self, *_):
        for endpoint in (self.target, self.jump):
            password = endpoint['auth'].currentData() == 'password'
            endpoint['key_row'].setEnabled(not password)
            endpoint['form'].setRowVisible(endpoint['key_row'], not password)
            endpoint['host_key_alias'].setEnabled(not password)
            endpoint['form'].setRowVisible(endpoint['host_key_alias'], not password)
            endpoint['form'].setRowVisible(endpoint['password_row'], password)
            endpoint['password_row'].setEnabled(password)
        active = (self.target, self.jump) if self.mode.currentData() == 'proxyjump' else (self.target,)
        manual_identity = any(endpoint['auth'].currentData() == 'key' for endpoint in active)
        self.identity_row.setEnabled(manual_identity)
        self.connection_form.setRowVisible(self.identity_row, manual_identity)

    def file_row(self, edit):
        widget = QWidget()
        row = QHBoxLayout(widget)
        row.setContentsMargins(0, 0, 0, 0)
        row.addWidget(edit, 1)
        def browse():
            filename, _ = QFileDialog.getOpenFileName(self, 'SSH-fájl kiválasztása', edit.text())
            if filename:
                edit.setText(filename)
        row.addWidget(self.make_button('Tallózás', browse))
        return widget

    def changed(self, *_):
        if not self.loading:
            self.dirty = True
            self.save_button.setText('Profil mentése •')
            self.work.packages.invalidate()

    def update_mode(self, *_):
        via_jump = self.mode.currentData() == 'proxyjump'
        for widget in self.jump['widgets']:
            widget.setEnabled(via_jump)
            widget.setVisible(via_jump)
        self.auth_hint.setText(
            ('A két géphez külön belépési mód és külön jelszó vagy privátkulcs adható meg. '
             if via_jump else
             'A FreeBSD célgép SSH-címét, portját, felhasználóját, majd a belépés módját add meg. '
             'Jelszavas módban privátkulcsot nem kell megadni. ')
            + 'A „Jelszó megjegyzése” bejelölésével a Profil mentése a Windows jelszótárába ment; jelölés nélkül csak az app bezárásáig őrizzük meg. '
              'A szerver azonosságát az app a belépési módtól függetlenül ellenőrzi.')
        self.update_auth()
        if not self.active():
            self.status.setText('Nincs aktív portalagút. ' + (
                'A ProxyJump-kapcsolat minden művelethez automatikusan felépül.' if via_jump else
                'Közvetlenül a FreeBSD célgép SSH-portjára kapcsolódunk. A porttovábbítás opcionális.'))

    def may_discard(self):
        return not self.dirty or QMessageBox.question(self, 'SSH-profil', 'Eldobod a profil nem mentett módosításait?',
                    QMessageBox.StandardButton.Discard | QMessageBox.StandardButton.Cancel,
                    QMessageBox.StandardButton.Cancel) == QMessageBox.StandardButton.Discard

    def reload(self, _=False, initial=False):
        if not initial and (self.active() or not self.may_discard()):
            return
        try:
            self.profiles = self.store.load()
            self.refill()
            self.load(self.profiles[0] if self.profiles else ssh.new_profile())
            self.notify_profiles()
        except (EditorError, OSError, UnicodeError) as error:
            self.output.setPlainText(str(error))
            if initial:
                self.load(ssh.new_profile())

    def refill(self):
        self.choice.blockSignals(True)
        self.choice.clear()
        for profile in self.profiles:
            self.choice.addItem(profile['name'], profile['id'])
        self.choice.blockSignals(False)

    def load(self, profile):
        self.loading = True
        self.loaded_profile = copy.deepcopy(profile)
        self.current_id = profile['id']
        self.name.setText(profile['name'])
        self.mode.setCurrentIndex(self.mode.findData(profile['mode']))
        self.remote_root.setText(profile['remote_root'])
        self.work.load(profile['work'])
        self.known_hosts.setText(profile['known_hosts'])
        for role in ('target', 'jump'):
            for key, value in profile[role].items():
                control = getattr(self, role)[key]
                if key == 'port': control.setValue(value)
                elif key == 'auth': control.setCurrentIndex(control.findData(value))
                elif key == 'remember_password': control.setChecked(value)
                else: control.setText(value)
            try:
                getattr(self, role)['password'].setText(ssh_password.recalled(profile, role))
            except EditorError as error:
                getattr(self, role)['password'].clear()
                self.output.setPlainText(str(error))
        self.forward_table.setRowCount(0)
        for row in profile['forwards']:
            self.add_forward(row)
        self.choice.blockSignals(True)
        self.choice.setCurrentIndex(self.choice.findData(profile['id']))
        self.choice.blockSignals(False)
        self.update_mode()
        self.update_auth()
        self.dirty, self.loading = False, False
        self.save_button.setText('Profil mentése')

    def select_profile(self, index):
        identity = self.choice.itemData(index)
        if identity and identity != self.current_id:
            if self.active() or not self.may_discard():
                self.choice.blockSignals(True)
                self.choice.setCurrentIndex(self.choice.findData(self.current_id))
                self.choice.blockSignals(False)
            else:
                self.load(next(p for p in self.profiles if p['id'] == identity))

    def new(self):
        if not self.active() and self.may_discard():
            self.load(ssh.new_profile())

    def current(self):
        # Force the active table delegate to commit before reading its item.
        self.name.setFocus()
        baseline = self.loaded_profile
        active = (self.target, self.jump) if self.mode.currentData() == 'proxyjump' else (self.target,)
        only_password = all(endpoint['auth'].currentData() == 'password' for endpoint in active)
        identity_file = baseline['known_hosts'] if only_password else self.known_hosts.text()
        profile = dict(id=self.current_id, name=self.name.text().strip(), mode=self.mode.currentData(),
                       known_hosts=identity_file or str(Path.home() / '.ssh/known_hosts'), remote_root=self.remote_root.text(), forwards=[],
                       work=self.work.current())
        for role in ('target', 'jump'):
            fields = getattr(self, role)
            profile[role] = {key: fields[key].value() if key == 'port' else
                             fields[key].currentData() if key == 'auth' else fields[key].text()
                             for key in ('host', 'port', 'user', 'key_file', 'host_key_alias', 'auth')}
            profile[role]['remember_password'] = fields['remember_password'].isChecked() and fields['auth'].currentData() == 'password'
            if profile[role]['auth'] == 'password':
                profile[role]['key_file'] = ''  # Inactive private-key drafts cannot block saving.
                same_endpoint = all(profile[role][key] == baseline[role][key] for key in ('host', 'port', 'user'))
                profile[role]['host_key_alias'] = baseline[role]['host_key_alias'] if same_endpoint else ''
        for row in range(self.forward_table.rowCount()):
            values = [self.forward_table.item(row, col).text().strip() if self.forward_table.item(row, col) else '' for col in range(3)]
            try:
                profile['forwards'].append(dict(local_port=int(values[0]), remote_host=values[1], remote_port=int(values[2])))
            except ValueError:
                raise EditorError('A porttovábbítások portjai egész számok legyenek.') from None
        profile = ssh.validate_profile(profile)
        for role in ('target', 'jump'):
            ssh_password.remember(profile, role, getattr(self, role)['password'].text())
        return profile

    def save(self):
        try:
            profile = self.current()
            updated = [p for p in self.profiles if p['id'] != profile['id']] + [profile]
            previous = next((p for p in self.profiles if p['id'] == profile['id']), None)
            self.store.save(updated, transaction=ssh_password.saved_preferences(profile, previous))
            self.profiles = updated
            self.refill()
            self.load(profile)
            self.status.setText('Profil mentve. Kapcsolódáshoz használd a Kapcsolat ellenőrzése vagy a felső Újracsatlakozás gombot.')
            self.notify_profiles()
        except (EditorError, OSError, UnicodeError) as error:
            self.owner.error(str(error))

    def remove(self):
        if self.active() or not any(p['id'] == self.current_id for p in self.profiles):
            return
        if QMessageBox.question(self, 'SSH-profil', 'Törlöd ezt az SSH-profilt? A kulcsfájlok megmaradnak.') != QMessageBox.StandardButton.Yes:
            return
        selected = getattr(self.owner, 'remote_profile', None)
        if selected and selected['id'] == self.current_id and not self.owner.set_local():
            return
        try:
            removed_id = self.current_id
            removed_profile = next(p for p in self.profiles if p['id'] == removed_id)
            updated = [p for p in self.profiles if p['id'] != self.current_id]
            self.store.save(updated, transaction=ssh_password.saved_preferences(previous=removed_profile))
            self.profiles = updated
            self.refill()
            self.load(updated[0] if updated else ssh.new_profile())
            ssh_password.forget(removed_id)
            self.notify_profiles()
        except (EditorError, OSError) as error:
            self.owner.error(str(error))

    def add_forward(self, value=None):
        value = value if isinstance(value, dict) else dict(local_port=13307, remote_host='127.0.0.1', remote_port=3306)
        row = self.forward_table.rowCount()
        self.forward_table.insertRow(row)
        for col, key in enumerate(('local_port', 'remote_host', 'remote_port')):
            self.forward_table.setItem(row, col, QTableWidgetItem(str(value[key])))
        self.changed()

    def remove_forward(self):
        for row in sorted({item.row() for item in self.forward_table.selectedItems()}, reverse=True):
            self.forward_table.removeRow(row)
            self.changed()

    def active(self):
        return self.probe_process is not None or self.tunnel_process is not None or self.work.active()

    def notify_profiles(self):
        if hasattr(self.owner, 'remote_page') and hasattr(self.owner, 'profiles_changed'):
            self.owner.profiles_changed()

    def target_allowed(self):
        return not hasattr(self.owner, 'is_remote') or (
            self.owner.is_remote() and self.owner.remote_profile['id'] == self.current_id)

    def owner_busy(self):
        client = getattr(self.owner, 'remote_client', None)
        process = getattr(self.owner, 'process', None)
        return bool(client and client.busy()) or bool(process and process.state() != QProcess.ProcessState.NotRunning) or \
            bool(hasattr(self.owner,'backup_page') and self.owner.backup_page.active())

    def operation_profile(self, server=False):
        profile = self.current()
        if not self.target_allowed():
            raise EditorError('A felső szerverválasztóban válaszd ki ezt a távoli profilt a szerverművelethez.')
        if server and hasattr(self.owner, 'remote_online') and not self.owner.remote_online:
            raise EditorError('Előbb csatlakozz a távoli szervercsomaghoz a felső Újracsatlakozás gombbal.')
        active = getattr(self.owner, 'remote_profile', None)
        if active and ssh.connection_route(profile) != ssh.connection_route(active):
            raise EditorError('Az SSH-kapcsolat módosult. Mentsd a profilt, majd csatlakozz újra a felső szerverválasztóval.')
        return profile

    def controls(self):
        active = self.active() or self.owner_busy()
        for widget in (self.choice, self.new_button, self.save_button, self.reload_button,
                       self.delete_button, self.check_button, self.tunnel_button):
            widget.setEnabled(not active)
        self.tabs.setEnabled(self.probe_process is None and not self.owner_busy())
        self.tabs.setTabEnabled(0, not active)
        self.tabs.setTabEnabled(1, not active)
        self.work.controls()
        self.quest_reload_button.setEnabled(self.probe_process is None and not self.work.active() and not self.owner_busy() and self.target_allowed()
            and (not hasattr(self.owner, 'remote_online') or self.owner.remote_online))
        self.stop_button.setEnabled(self.tunnel_process is not None)

    def launch(self, role, session, command):
        process = QProcess(self)
        ssh.configure_process(process, session, self.probe_timer if role == 'probe' else self.tunnel_timer)
        process.setProcessChannelMode(QProcess.ProcessChannelMode.MergedChannels)
        setattr(self, role + '_process', process)
        setattr(self, role + '_session', session)
        setattr(self, role + '_output', '')
        setattr(self, role + '_failure', '')
        process.readyReadStandardOutput.connect(lambda: self.read_output(role, process))
        process.finished.connect(lambda code, status: self.finished(role, process, code, status))
        process.errorOccurred.connect(lambda error: self.failed(role, process, error))
        self.controls()
        process.start(*command)

    def check(self):
        if self.active():
            return
        session = None
        try:
            session = ssh.Session(self.current(), parent=self)
            command = session.probe()
            self.probe_action = 'check'
            self.output.setPlainText('Kapcsolatellenőrzés… Az ugrógép és a célgép hostkulcsát is ellenőrizzük.')
            self.launch('probe', session, command)
            self.probe_timer.start(45000)
        except (EditorError, OSError) as error:
            if session:
                session.close()
            self.owner.error(str(error))

    def reload_quests(self, offline=False):
        if self.probe_process is not None or self.work.active():
            return
        session = None
        try:
            profile = self.operation_profile(server=True)
            root = profile['remote_root']
            if not root:
                raise EditorError('Add meg a távoli szervercsomag mappáját az SSH-profilban.')
            session = ssh.Session(profile, parent=self)
            action = 'apply-quests' if offline else 'reload-quests'
            command = session.ssh(shlex.join(['python3', '-B', root.rstrip('/') + '/manage.py', action]))
            self.probe_action = action
            self.output.setPlainText('A következő indulás questcsomagjának ellenőrzése…' if offline else
                'Távoli questfrissítés: ellenőrzés és közös aktiválás minden futó core-on…')
            self.launch('probe', session, command)
            self.probe_timer.start(150000)
        except (EditorError, OSError) as error:
            if session:
                session.close()
            self.owner.error(str(error))

    def start_tunnel(self):
        if self.active():
            return
        session = None
        try:
            profile = self.current()
            for row in profile['forwards']:
                with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as listener:
                    listener.bind(('127.0.0.1', row['local_port']))
            session = ssh.Session(profile, parent=self)
            command = session.tunnel()
            self.output.setPlainText('SSH-portalagút indítása…')
            self.status.setText('Kapcsolódás az ugrógéphez és a FreeBSD célgéphez…')
            self.launch('tunnel', session, command)
            self.timer.start(500)
            self.tunnel_timer.start(35000)
        except (EditorError, OSError) as error:
            if session:
                session.close()
            self.owner.error('Az alagút nem indult: ' + str(error))

    def read_output(self, role, process):
        text = bytes(process.readAllStandardOutput()).decode('utf-8', errors='replace')
        value = (getattr(self, role + '_output') + text)[-64000:]
        setattr(self, role + '_output', value)
        self.output.setPlainText(value)

    def failed(self, role, process, error):
        if error == QProcess.ProcessError.FailedToStart:
            setattr(self, role + '_failure', 'Az OpenSSH kliens nem indítható: ' + process.errorString())
            self.finished(role, process, 1, QProcess.ExitStatus.CrashExit)

    def finished(self, role, process, code, exit_status):
        if getattr(self, role + '_process') is not process:
            return
        self.read_output(role, process)
        session = getattr(self, role + '_session')
        setattr(self, role + '_process', None)
        setattr(self, role + '_session', None)
        getattr(self, role + '_timer').stop()
        failure = getattr(self, role + '_failure')
        if role == 'probe' and getattr(self, 'probe_action', 'check') in ('reload-quests', 'apply-quests'):
            ok = not failure and code == 0 and exit_status == QProcess.ExitStatus.NormalExit
            self.output.appendPlainText(('A következő indulás questcsomagja elkészült.' if self.probe_action == 'apply-quests' else 'A távoli questek frissítése kész.') if ok else
                (failure or 'A frissítés nem fejeződött be. A napló mutatja, hogy elutasítás vagy függő aktiválás történt; függő aktiválásnál indítsd újra ezt a műveletet.'))
        elif role == 'probe':
            try:
                if failure or code != 0 or exit_status != QProcess.ExitStatus.NormalExit:
                    raise EditorError(failure or 'Az SSH-kapcsolat nem sikerült. Ellenőrizd a fenti hibát, a belépési módot, a jelszót vagy kulcsot és a known_hosts fájlt.')
                result = ssh.probe_result(self.probe_output)
                lines = ['Az SSH-kapcsolat működik.', 'Rendszer: ' + result['OS'] + ' ' + result['RELEASE'],
                         'Felhasználó: ' + result['USER'], 'Python 3: ' + (result['PYTHON'] or 'nem található'),
                         'Szervermappa: ' + {'yes': 'létezik', 'no': 'nem található', 'unset': 'még nincs megadva'}.get(result['DIRECTORY'], 'ismeretlen'),
                         'Új szervercsomag jelölői: ' + {'yes': 'megtalálhatók', 'no': 'hiányoznak', 'unset': 'nincs ellenőrizve'}.get(result['PACKAGE'], 'ismeretlen')]
                if result['OS'] != 'FreeBSD' or not result['RELEASE'].startswith('15.1-'):
                    lines += ['A rendszer eltér a tervezett FreeBSD 15.1 környezettől.']
                self.output.setPlainText('\n'.join(lines))
            except EditorError as error:
                self.output.appendPlainText(str(error))
        else:
            self.timer.stop()
            self.status.setText(failure or ('A portalagút megszakadt; részletek a naplóban.' if code else 'A portalagút bezárult.'))
        session.close()
        process.deleteLater()
        self.controls()

    def probe_timeout(self):
        if self.probe_process:
            self.probe_failure = ('A questfrissítés időkorlátja lejárt. A távoli döntés megmarad; a művelet megismétlésével ellenőrizhető és befejezhető.'
                if getattr(self, 'probe_action', 'check') in ('reload-quests', 'apply-quests') else 'A kapcsolatellenőrzés túllépte a 45 másodperces időkorlátot.')
            self.probe_process.kill()

    def tunnel_timeout(self):
        if self.tunnel_process:
            self.tunnel_failure = 'Az alagút 35 másodperc alatt nem nyitotta meg a helyi portokat.'
            self.tunnel_process.kill()

    def poll_tunnel(self):
        if not self.tunnel_process or self.tunnel_process.state() != QProcess.ProcessState.Running:
            return
        for row in self.tunnel_session.profile['forwards']:
            try:
                with socket.create_connection(('127.0.0.1', row['local_port']), timeout=0.05):
                    pass
            except OSError:
                return
        self.timer.stop()
        self.tunnel_timer.stop()
        self.status.setText('A helyi porttovábbítások figyelnek. A céloldali szolgáltatások működését külön kell ellenőrizni.')

    def stop_tunnel(self):
        if self.tunnel_process:
            self.tunnel_failure = 'A portalagutat bontottad.'
            self.tunnel_process.kill()

    def shutdown(self):
        self.work.shutdown()
        self.probe_timer.stop()
        self.tunnel_timer.stop()
        self.timer.stop()
        for role in ('probe', 'tunnel'):
            process = getattr(self, role + '_process')
            if process:
                process.kill()
                process.waitForFinished(3000)
