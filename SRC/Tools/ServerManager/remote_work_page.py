"""Build and upload UI sharing RemotePage's verified direct/ProxyJump session."""
import codecs
import json
from pathlib import Path
import secrets
import shlex
import threading

from PySide6.QtCore import QProcess, QThread, Signal, QStandardPaths, QTimer
from PySide6.QtGui import QColor
from PySide6.QtWidgets import (QCheckBox, QComboBox, QFileDialog, QFormLayout,
    QHBoxLayout, QLabel, QLineEdit, QPlainTextEdit, QPushButton, QSpinBox,
    QTableWidget, QTableWidgetItem, QHeaderView, QScrollArea, QVBoxLayout, QWidget)

from backend import EditorError
import remote_jobs as jobs
import ssh_connection as ssh
from remote_packages_page import RemotePackages


class PrepareUpload(QThread):
    ready = Signal(object)
    failed = Signal(str)
    progress = Signal(str)

    def __init__(self, sources, destination, excludes, contents, parent):
        super().__init__(parent)
        self.arguments = sources, destination, excludes, contents
        self.cancel = threading.Event()

    def run(self):
        try:
            snapshot = jobs.UploadSnapshot(*self.arguments, cancelled=self.cancel.is_set, progress=self.progress.emit)
            self.ready.emit(snapshot)
        except Exception as error:
            self.failed.emit(str(error))


class RemoteWork:
    def __init__(self, page):
        self.page = page
        self.process = self.session = self.planner = self.snapshot = None
        self.cancel_process = None
        self.request = None
        self.phase = ''
        self.controls_list = []
        self.log_file = None
        self.settings = {}
        self.build_page = QWidget()
        build_layout = QVBoxLayout(self.build_page)
        hint = QLabel('A forrásnak már a távoli gépen kell lennie; a Feltöltés fülön is felmásolhatod. '
                      'A fordítás külön buildmappába kerül. A Build telepítése gomb a négy kész programot '
                      'a leállított szerver bin mappájába másolja, a korábbi fájlok mentésével.')
        hint.setWordWrap(True)
        build_layout.addWidget(hint)
        form = QFormLayout()
        for key, label, placeholder in (
            ('source_dir', 'Távoli forrásmappa', '/usr/home/auriga/source'),
            ('build_dir', 'Távoli buildmappa', '/usr/home/auriga/build-release'),
            ('cmake', 'CMake program', 'cmake vagy /usr/local/bin/cmake'),
            ('targets', 'Buildcélok', 'GameServer Database LoginServer ServerPackageCheck'),
            ('configure_args', 'További CMake-beállítások', '-DAURIGA_USE_VCPKG=OFF')):
            edit = QLineEdit()
            edit.setPlaceholderText(placeholder)
            self.settings[key] = edit
            form.addRow(label, edit)
        self.settings['configuration'] = QComboBox()
        self.settings['configuration'].addItems(['Release', 'Debug', 'RelWithDebInfo', 'MinSizeRel'])
        form.addRow('Konfiguráció', self.settings['configuration'])
        self.settings['generator'] = QComboBox()
        self.settings['generator'].addItems(['Ninja', 'Unix Makefiles'])
        form.addRow('Generátor', self.settings['generator'])
        self.settings['parallel'] = QSpinBox()
        self.settings['parallel'].setRange(1, 128)
        form.addRow('Párhuzamos feladatok', self.settings['parallel'])
        build_layout.addLayout(form)
        self.preview = QPlainTextEdit()
        self.preview.setReadOnly(True)
        self.preview.setMaximumHeight(105)
        build_layout.addWidget(self.preview)
        self.destination = QLabel()
        self.destination.setWordWrap(True)
        buttons = QHBoxLayout()
        self.configure_button = self.button('Csak konfigurálás', lambda: self.start_build('configure'))
        self.build_button = self.button('Konfigurálás és build', lambda: self.start_build('build'))
        self.build_button.setObjectName('Primary')
        self.install_button = self.button('Build telepítése', self.install)
        self.install_button.setToolTip('GameServer, Database, LoginServer és ServerPackageCheck telepítése. '
                                       'A szervernek leállítva kell lennie. Indítani utána az Áttekintés oldalon lehet.')
        buttons.addWidget(self.configure_button)
        buttons.addWidget(self.build_button)
        buttons.addWidget(self.install_button)
        buttons.addStretch()
        build_layout.addStretch()
        build_scroll = QScrollArea()
        build_scroll.setWidgetResizable(True)
        build_scroll.setWidget(self.build_page)
        build_container = QWidget()
        container_layout = QVBoxLayout(build_container)
        container_layout.setContentsMargins(0, 0, 0, 0)
        container_layout.addWidget(build_scroll, 1)
        container_layout.addWidget(self.destination)
        container_layout.addLayout(buttons)
        page.tabs.addTab(build_container, 'Távoli build')

        self.upload_page = QWidget()
        layout = QVBoxLayout(self.upload_page)
        hint = QLabel('Válassz fájlokat vagy mappát, majd készíts feltöltési listát. '
                      'SHA-256 alapján csak a helyi és távoli tartalom eltérései jelennek meg. '
                      'A csak távolin lévő fájlok törléséhez külön kapcsoló szükséges.')
        hint.setWordWrap(True)
        layout.addWidget(hint)
        upload_form = QFormLayout()
        for key, label in (('upload_dir', 'Távoli célmappa'), ('excludes', 'Kizárt nevek / minták (; elválasztással)')):
            self.settings[key] = QLineEdit()
            upload_form.addRow(label, self.settings[key])
        layout.addLayout(upload_form)
        select = QHBoxLayout()
        select.addWidget(self.button('Fájlok kiválasztása', self.choose_files))
        select.addWidget(self.button('Mappa kiválasztása', self.choose_folder))
        self.contents = QCheckBox('Csak a mappa tartalmát másolja a célba')
        self.controls_list.append(self.contents)
        select.addWidget(self.contents)
        select.addStretch()
        layout.addLayout(select)
        self.sources = []
        self.selection = QLabel('Nincs kiválasztott fájl vagy mappa.')
        self.selection.setWordWrap(True)
        self.selection.setMaximumHeight(64)
        layout.addWidget(self.selection)
        self.table = QTableWidget(0, 3)
        self.table.setHorizontalHeaderLabels(['Változás', 'Célhoz viszonyított útvonal', 'Méret (bájt)'])
        self.table.horizontalHeader().setSectionResizeMode(0, QHeaderView.ResizeMode.ResizeToContents)
        self.table.horizontalHeader().setSectionResizeMode(1, QHeaderView.ResizeMode.Stretch)
        self.table.setEditTriggers(QTableWidget.EditTrigger.NoEditTriggers)
        layout.addWidget(self.table, 1)
        self.overwrite = QCheckBox('Meglévő fájlok felülírása, előtte távoli biztonsági mentéssel')
        self.controls_list.append(self.overwrite)
        layout.addWidget(self.overwrite)
        self.delete = QCheckBox('A „Törölt / csak távolin” fájlok eltávolítása is, távoli biztonsági mentéssel')
        self.delete.setToolTip('Csak a kijelölt mappán belül listázott fájlokra vonatkozik. '
            'A kizárt fájlokat és a kiválasztáson kívüli tartalmat nem törli.')
        self.controls_list.append(self.delete)
        layout.addWidget(self.delete)
        row = QHBoxLayout()
        self.plan_button = self.button('Eltérések keresése', self.prepare)
        self.upload_button = self.button('Feltöltés indítása', self.upload)
        self.upload_button.setObjectName('Primary')
        self.upload_button.setEnabled(False)
        row.addWidget(self.plan_button)
        row.addWidget(self.upload_button)
        row.addStretch()
        layout.addLayout(row)
        page.tabs.addTab(self.upload_page, 'Feltöltés')
        self.packages = RemotePackages(self)

        self.cancel_button = QPushButton('Művelet megszakítása')
        self.cancel_button.clicked.connect(self.cancel)
        self.cancel_button.setEnabled(False)
        self.log = QPlainTextEdit()
        self.log.setReadOnly(True)
        self.log.setMinimumHeight(100)
        self.log.setMaximumHeight(180)
        self.log.document().setMaximumBlockCount(1800)
        self.status = QLabel('Nincs folyamatban távoli művelet.')
        self.status.setWordWrap(True)
        # Shared job status remains accessible when switching between work tabs.
        page.layout().addWidget(self.status)
        page.layout().addWidget(self.cancel_button)
        page.layout().addWidget(self.log)
        for control in self.settings.values():
            self.controls_list.append(control)
            if isinstance(control, QLineEdit):
                control.textChanged.connect(self.changed)
            elif isinstance(control, QComboBox):
                control.currentTextChanged.connect(self.changed)
            else:
                control.valueChanged.connect(self.changed)
        self.contents.toggled.connect(self.invalidate)
        self.delete.toggled.connect(self.controls)
        self.overwrite.toggled.connect(self.controls)
        self.load(jobs.defaults())

    def button(self, label, callback):
        button = QPushButton(label)
        button.clicked.connect(callback)
        self.controls_list.append(button)
        return button

    def load(self, settings):
        self.invalidate()
        self.packages.reset()
        self.sources = []
        self.selection.setText('Nincs kiválasztott fájl vagy mappa.')
        self.overwrite.setChecked(False)
        self.delete.setChecked(False)
        for key, value in settings.items():
            control = self.settings[key]
            control.blockSignals(True)
            if isinstance(control, QLineEdit):
                control.setText(value)
            elif isinstance(control, QComboBox):
                control.setCurrentText(value)
            else:
                control.setValue(value)
            control.blockSignals(False)
        self.refresh_preview()

    def current(self):
        return {key: control.text() if isinstance(control, QLineEdit) else
                control.currentText() if isinstance(control, QComboBox) else control.value()
                for key, control in self.settings.items()}

    def changed(self, *_):
        self.page.changed()
        self.invalidate()
        self.refresh_preview()

    def refresh_preview(self):
        self.update_destination()
        try:
            request = jobs.build_request(self.current())
            self.preview.setPlainText(shlex.join(request['configure']) + '\n' + shlex.join(request['compile']))
        except EditorError as error:
            self.preview.setPlainText(str(error))

    def update_destination(self, *_):
        root = self.page.remote_root.text().rstrip('/')
        self.destination.setText('Telepítés helye: ' + (root + '/bin' if root else 'Add meg a profil távoli szervermappáját.'))

    def invalidate(self, *_):
        if self.snapshot:
            self.snapshot.close()
            self.snapshot = None
        self.upload_button.setEnabled(False)
        self.table.setRowCount(0)

    def choose_files(self):
        paths, _ = QFileDialog.getOpenFileNames(self.page, 'Feltöltendő fájlok')
        if paths:
            self.choose(paths)

    def choose_folder(self):
        path = QFileDialog.getExistingDirectory(self.page, 'Feltöltendő mappa')
        if path:
            self.choose([path])

    def choose(self, paths):
        self.invalidate()
        self.sources = paths
        self.selection.setText('\n'.join(paths))
        self.selection.setToolTip('\n'.join(paths))

    def active(self):
        return self.process is not None or self.planner is not None or self.cancel_process is not None

    def controls(self):
        busy = self.active() or self.page.probe_process is not None or self.page.owner_busy()
        for control in self.controls_list:
            control.setEnabled(not busy)
        self.build_button.setEnabled(not busy and self.page.target_allowed())
        self.configure_button.setEnabled(not busy and self.page.target_allowed())
        state = getattr(self.page.owner, 'remote_state', None) or {}
        self.install_button.setEnabled(not busy and self.page.target_allowed()
            and getattr(self.page.owner, 'remote_online', True)
            and not state.get('running') and not state.get('starting') and not state.get('deployment_pending'))
        ready = self.snapshot is not None and self.snapshot.compared
        if ready:
            ready = not any(row['change'] == 'conflict' for row in self.snapshot.changes) and bool(
                self.snapshot.request['files'] or self.snapshot.request['directories'] or
                self.delete.isChecked() and self.snapshot.request.get('deletions'))
        self.upload_button.setEnabled(not busy and ready and self.page.target_allowed())
        self.plan_button.setEnabled(not busy and self.page.target_allowed())
        self.cancel_button.setEnabled(self.planner is not None or (self.process is not None and self.phase not in ('commit', 'install')))
        self.packages.controls(not busy)
        if self.phase == 'packages' and self.request and self.request['operation'] in ('install', 'update'):
            self.cancel_button.setEnabled(False)

    def append(self, text):
        self.log.moveCursor(self.log.textCursor().MoveOperation.End)
        self.log.insertPlainText(text)
        self.log.ensureCursorVisible()
        if self.log_file:
            self.log_file.write(text)
            self.log_file.flush()

    def begin_log(self):
        if self.log_file:
            self.log_file.close()
        root = Path(QStandardPaths.writableLocation(QStandardPaths.StandardLocation.AppLocalDataLocation)) / 'remote-jobs'
        root.mkdir(parents=True, exist_ok=True)
        path = root / (self.request['id'] + '.log')
        self.log_file = path.open('x', encoding='utf-8')
        self.log.clear()
        self.append('Naplófájl: ' + str(path) + '\n')

    def prepare(self):
        if self.page.probe_process is not None or self.active() or self.page.owner_busy():
            return
        try:
            self.invalidate()
            self.page.operation_profile()
            settings = jobs.validate_settings(self.current())
            self.planner = PrepareUpload(list(self.sources), settings['upload_dir'], settings['excludes'], self.contents.isChecked(), self.page)
            self.planner.ready.connect(self.prepared)
            self.planner.failed.connect(self.preparation_failed)
            self.planner.progress.connect(self.status.setText)
            self.planner.finished.connect(self.preparation_finished)
            self.status.setText('Feltöltési pillanatkép és ellenőrzőösszegek készítése…')
            self.page.controls()
            self.planner.start()
        except (EditorError, OSError) as error:
            self.page.owner.error(str(error))

    def prepared(self, snapshot):
        self.snapshot = snapshot

    def display_comparison(self):
        labels = dict(new='Új', modified='Módosított', deleted='Törölt / csak távolin',
                      directory='Új mappa', conflict='Ütközés')
        colors = dict(new='#81d4ad', modified='#edc278', deleted='#ee9c9c', directory='#81d4ad', conflict='#ee9c9c')
        rows = self.snapshot.changes
        self.table.setRowCount(len(rows))
        for index, row in enumerate(rows):
            self.table.setItem(index, 0, QTableWidgetItem(labels[row['change']]))
            self.table.item(index, 0).setForeground(QColor(colors[row['change']]))
            self.table.setItem(index, 1, QTableWidgetItem(row['path'] + ('/' if row['change'] == 'directory' else '')))
            self.table.item(index, 1).setToolTip(row['path'])
            self.table.setItem(index, 2, QTableWidgetItem(str(row['size']) if row['change'] != 'directory' else 'mappa'))
            if row.get('detail'):
                self.table.item(index, 0).setToolTip(row['detail'])
        counts = {kind: sum(row['change'] == kind for row in rows) for kind in labels}
        self.status.setText(('%d új, %d módosított, %d törölt / csak távolin, %d új mappa, %d ütközés. '
            '%d változatlan fájl kihagyva. Cél: %s') % (counts['new'], counts['modified'], counts['deleted'],
            counts['directory'], counts['conflict'], self.snapshot.unchanged, self.snapshot.request['destination']))

    def preparation_failed(self, message):
        self.status.setText(message)

    def preparation_finished(self):
        cancelled = self.planner.cancel.is_set()
        self.planner.deleteLater()
        self.planner = None
        if cancelled:
            self.invalidate()
            self.status.setText('Az összehasonlítást megszakítottad.')
        elif self.snapshot is not None:
            try:
                self.session = ssh.Session(self.page.operation_profile(), parent=self.page)
                self.request = dict(self.snapshot.request, action='compare')
                self.begin_log()
                self.start_worker(self.request, 'compare')
            except (EditorError, OSError) as error:
                self.cleanup()
                self.invalidate()
                self.status.setText(str(error))
        self.page.controls()

    def start_build(self, action):
        if self.page.probe_process is not None or self.active():
            return
        try:
            request = jobs.build_request(self.current(), action)
            self.session = ssh.Session(self.page.operation_profile(), parent=self.page)
            self.request = request
            self.begin_log()
            self.start_worker(request, action)
        except (EditorError, OSError) as error:
            self.cleanup()
            self.page.owner.error(str(error))

    def upload(self):
        if self.page.probe_process is not None or self.active() or self.snapshot is None:
            return
        try:
            self.request = self.snapshot.upload_request(self.overwrite.isChecked(), self.delete.isChecked())
            self.session = ssh.Session(self.page.operation_profile(), parent=self.page)
            self.begin_log()
            self.start_worker(self.request, 'prepare')
        except (EditorError, OSError) as error:
            self.cleanup()
            self.page.owner.error(str(error))

    def install(self):
        if self.page.probe_process is not None or self.active():
            return
        try:
            profile = self.page.operation_profile(server=True)
            state = getattr(self.page.owner, 'remote_state', None) or {}
            if state.get('running') or state.get('starting'):
                raise EditorError('Telepítés előtt állítsd le a kiválasztott szervert az Áttekintés oldalon.')
            self.request = jobs.install_request(self.current(), profile['remote_root'])
            self.session = ssh.Session(profile, parent=self.page)
            self.begin_log()
            self.start_worker(self.request, 'install')
        except (EditorError, OSError) as error:
            self.cleanup()
            self.page.owner.error(str(error))

    def start_worker(self, request, phase):
        self.start_process(self.session.ssh('python3 -u -'), phase, jobs.worker_input(request))

    def start_process(self, command, phase, data=None):
        self.phase = phase
        self.phase_output = ''
        self.display_buffer = ''
        process = QProcess(self.page)
        ssh.configure_process(process, self.session)
        self.process = process
        decoder = codecs.getincrementaldecoder('utf-8')(errors='replace')
        process.setProcessChannelMode(QProcess.ProcessChannelMode.MergedChannels)
        process.readyReadStandardOutput.connect(lambda: self.read(process, decoder))
        process.started.connect(lambda: self.feed(process, data))
        process.finished.connect(lambda code, status: self.finished(process, code, status, decoder))
        process.errorOccurred.connect(lambda error: self.failed(process, error, decoder))
        self.status.setText({'compare': 'A helyi és távoli fájlok összehasonlítása SHA-256 alapján…',
                             'prepare': 'Távoli feltöltési terület előkészítése…', 'transfer': 'Fájlok feltöltése SFTP-vel…',
                             'commit': 'Ellenőrzés, mentés és fájlcsere…', 'build': 'Távoli konfigurálás és build…',
                             'configure': 'Távoli CMake-konfigurálás…',
                             'packages': 'Távoli csomagművelet fut…',
                             'install': 'Build ellenőrzése, mentése és telepítése a szerver bin mappájába…'}[phase])
        self.page.controls()
        process.start(*command)

    @staticmethod
    def feed(process, data):
        if data is not None:
            process.write(data)
        process.closeWriteChannel()

    def read(self, process, decoder, final=False):
        text = decoder.decode(bytes(process.readAllStandardOutput()), final=final)
        limit = 16 * 1024 * 1024 if self.phase == 'compare' else 2 * 1024 * 1024 if self.phase == 'packages' else 64000
        self.phase_output = (self.phase_output + text)[-limit:]
        if self.phase == 'packages':
            self.packages.display(text, final)
        elif self.phase == 'compare':
            self.display_buffer += text
            lines = self.display_buffer.split('\n')
            self.display_buffer = '' if final else lines.pop()
            for line in lines:
                if not line.startswith('AURIGA_UPLOAD_COMPARISON ') and line:
                    self.append(line + '\n')
        else:
            self.append(text)

    def failed(self, process, error, decoder):
        if error == QProcess.ProcessError.FailedToStart:
            self.append('A program nem indítható: ' + process.errorString() + '\n')
            self.finished(process, 1, QProcess.ExitStatus.CrashExit, decoder)

    def finished(self, process, code, status, decoder):
        if self.process is not process:
            return
        self.read(process, decoder, final=True)
        self.process = None
        process.deleteLater()
        marker = {'compare': 'AURIGA_UPLOAD_COMPARISON ', 'prepare': 'AURIGA_UPLOAD_PREPARED ', 'commit': 'AURIGA_UPLOAD_SUCCESS ',
                  'build': 'AURIGA_BUILD_SUCCESS ', 'configure': 'AURIGA_BUILD_SUCCESS ',
                  'install': 'AURIGA_INSTALL_SUCCESS ', 'packages': 'AURIGA_PACKAGES_RESULT '}.get(self.phase)
        verified = not marker or any(line.startswith(marker) for line in self.phase_output.splitlines())
        failure = next((line.removeprefix('AURIGA_JOB_FAILED ') for line in reversed(self.phase_output.splitlines())
                        if line.startswith('AURIGA_JOB_FAILED ')), '')
        if not verified and not failure:
            self.append('A távoli siker-visszajelzés hiányzik.\n')
        if code == 0 and verified and not failure and status == QProcess.ExitStatus.NormalExit and not self.cancelling:
            if self.phase == 'compare':
                try:
                    packets = [line.removeprefix(marker) for line in self.phase_output.splitlines() if line.startswith(marker)]
                    if len(packets) != 1:
                        raise EditorError('Nem egyértelmű távoli összehasonlítási válasz.')
                    self.snapshot.apply_comparison(json.loads(packets[0]))
                    self.display_comparison()
                    self.cleanup()
                    self.page.controls()
                    return
                except (EditorError, ValueError, TypeError) as error:
                    self.append(str(error) + '\n')
                    failure = 'Az összehasonlítás nem sikerült: ' + str(error)
            elif self.phase == 'prepare':
                try:
                    if self.request['files']:
                        self.start_process(self.session.sftp(self.snapshot.batch()), 'transfer')
                    else:
                        self.start_worker(dict(self.request, action='commit'), 'commit')
                    return
                except (EditorError, OSError) as error:
                    self.append(str(error) + '\n')
            elif self.phase == 'transfer':
                self.start_worker(dict(self.request, action='commit'), 'commit')
                return
            else:
                if self.phase == 'packages':
                    try:
                        self.status.setText(self.packages.completed(self.phase_output))
                    except (KeyError, TypeError, ValueError) as error:
                        self.status.setText('Hibás csomagválasz: ' + str(error))
                    self.cleanup()
                    self.page.controls()
                    return
                self.status.setText('A négy program telepítése sikeresen befejeződött. Mentési útvonal a naplóban. '
                    'A szervert az Áttekintés oldalon indíthatod.' if self.phase == 'install' else
                    'A távoli művelet sikeresen befejeződött. Részletek és mentési útvonal a naplóban.')
                self.cleanup()
                self.invalidate()
                self.page.controls()
                return
        self.status.setText(failure or 'A művelet megszakadt vagy hibával leállt. A napló tartalmazza a részleteket; '
                            'kapcsolatszakadásnál a távoli build állapotát és a megmaradt feltöltési területet ellenőrizd.')
        self.cleanup()
        if self.phase in ('compare', 'prepare', 'transfer', 'commit'):
            self.invalidate()
        self.page.controls()

    @property
    def cancelling(self):
        return getattr(self, '_cancelling', False)

    def cancel(self):
        if self.phase == 'packages' and self.request and self.request['operation'] in ('install', 'update'):
            return
        if self.planner:
            self.planner.cancel.set()
        elif self.process is not None and self.phase in ('build', 'configure'):
            if self.cancel_process:
                return
            process = QProcess(self.page)
            ssh.configure_process(process, self.session)
            self.cancel_process = process
            data = jobs.worker_input(dict(action='cancel', id=self.request['id'], build=self.request['build']))
            process.setProcessChannelMode(QProcess.ProcessChannelMode.MergedChannels)
            process.started.connect(lambda: self.feed(process, data))
            process.readyReadStandardOutput.connect(lambda: self.append(bytes(process.readAllStandardOutput()).decode('utf-8', errors='replace')))
            process.finished.connect(lambda *_: self.cancel_finished(process))
            process.errorOccurred.connect(lambda e: self.cancel_finished(process) if e == QProcess.ProcessError.FailedToStart else None)
            process.start(*self.session.ssh('python3 -u -'))
            self.status.setText('Távoli build megszakításának kérése…')
            self.page.controls()
        elif self.process is not None and self.phase not in ('commit', 'install'):
            self._cancelling = True
            self.process.kill()

    def cancel_finished(self, process):
        if self.cancel_process is process:
            self.cancel_process = None
            process.deleteLater()
            self.page.controls()

    def cleanup(self):
        if self.session:
            # A cancellation SSH process may still be reading the same config.
            if self.cancel_process:
                self.cancel_process.kill()
                self.cancel_process.waitForFinished(1000)
            self.session.close()
            self.session = None
        if self.log_file:
            self.log_file.close()
            self.log_file = None
        self._cancelling = False

    def shutdown(self):
        self.invalidate()
        self.cleanup()
