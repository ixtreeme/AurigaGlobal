"""Local/SSH crash events and noninteractive core analysis."""
import datetime
import hashlib
from pathlib import Path, PurePosixPath
import time

from PySide6.QtCore import QThread, Signal, Qt
from PySide6.QtGui import QFont
from PySide6.QtWidgets import (QAbstractItemView, QApplication, QComboBox, QFileDialog,
    QFormLayout, QHBoxLayout, QLabel, QLineEdit, QPlainTextEdit, QPushButton,
    QSplitter, QTableWidget, QTableWidgetItem, QVBoxLayout, QWidget)

import backend as api
import crash_tools


class CrashTask(QThread):
    ready = Signal(object)
    failed = Signal(str)

    def __init__(self, callback, parent):
        super().__init__(parent)
        self.callback = callback

    def run(self):
        try: self.ready.emit(self.callback())
        except Exception as error: self.failed.emit(str(error))


class CrashPage(QWidget):
    def __init__(self, owner):
        super().__init__()
        self.owner, self.task = owner, None
        self.target = None
        self.next_poll = 0
        self.data = dict(events=[], cores=[])
        self.rows = []
        self.last_report = ''
        self.report_context = None
        layout = QVBoxLayout(self)
        layout.setContentsMargins(0,0,0,0)
        row = QHBoxLayout()
        title = QLabel('Összeomlások és core fájlok');title.setObjectName('SectionTitle')
        row.addWidget(title,1)
        self.refresh_button = QPushButton('Lista frissítése')
        self.refresh_button.clicked.connect(self.refresh_list)
        row.addWidget(self.refresh_button)
        self.ack_button = QPushButton('Jelzés nyugtázása')
        self.ack_button.clicked.connect(self.acknowledge)
        row.addWidget(self.ack_button)
        layout.addLayout(row)
        self.status = QLabel('Válassz helyi vagy távoli szervert.')
        self.status.setWordWrap(True);layout.addWidget(self.status)
        split = QSplitter(Qt.Orientation.Vertical)
        self.events = QTableWidget(0,5)
        self.events.setHorizontalHeaderLabels(['Időpont','Példány','PID','Kilépés / jel','Core fájl'])
        self.events.horizontalHeader().setStretchLastSection(True)
        for column,width in enumerate((190,150,65,150)):
            self.events.setColumnWidth(column,width)
        self.events.setSelectionBehavior(QAbstractItemView.SelectionBehavior.SelectRows)
        self.events.setEditTriggers(QAbstractItemView.EditTrigger.NoEditTriggers)
        self.events.itemSelectionChanged.connect(self.select_event)
        split.addWidget(self.events)
        lower = QWidget(); lower_layout = QVBoxLayout(lower);lower_layout.setContentsMargins(0,0,0,0)
        self.core_choice = QComboBox()
        self.core_choice.currentIndexChanged.connect(self.select_core)
        lower_layout.addWidget(self.core_choice)
        form = QFormLayout()
        self.core_path = QLineEdit();self.core_path.setPlaceholderText('run/ch1-core1/GameServer.core vagy teljes célgépi útvonal')
        self.binary_path = QLineEdit();self.binary_path.setPlaceholderText('Az összeomláskor használt program; archivált bináris ajánlott')
        for label, edit in (('Core fájl',self.core_path),('Program / bináris',self.binary_path)):
            row=QHBoxLayout();row.addWidget(edit,1)
            browse=QPushButton('Tallózás');browse.clicked.connect(lambda _, control=edit:self.browse(control))
            row.addWidget(browse);form.addRow(label,row)
        lower_layout.addLayout(form)
        row=QHBoxLayout()
        self.analyze_button=QPushButton('Core elemzése');self.analyze_button.setObjectName('Primary')
        self.analyze_button.clicked.connect(self.analyze)
        self.copy_button=QPushButton('Jelentés másolása');self.copy_button.clicked.connect(lambda:QApplication.clipboard().setText(self.last_report))
        self.export_button=QPushButton('Jelentés mentése…');self.export_button.clicked.connect(self.export)
        for button in (self.analyze_button,self.copy_button,self.export_button):row.addWidget(button)
        row.addStretch();lower_layout.addLayout(row)
        self.report=QPlainTextEdit();self.report.setReadOnly(True);self.report.setFont(QFont('Consolas',10))
        self.report.setPlaceholderText('A kiválasztott esemény részletei és a debugger hibaverme itt jelenik meg.')
        lower_layout.addWidget(self.report,1)
        split.addWidget(lower);split.setSizes([180,400]);layout.addWidget(split,1)
        hint=QLabel('A felügyelő az app bezárása után is rögzíti a váratlan kilépéseket. '
                    'A core elemzése a kiválasztott gépen fut. Debug vagy RelWithDebInfo build ad fájlneveket és sorszámokat. '
                    'A jelentés memóriából származó adatot is tartalmazhat; megosztás előtt nézd át.')
        hint.setWordWrap(True);layout.addWidget(hint)
        self.controls()

    def active(self): return self.task is not None

    def key(self):
        target=self.owner.remote_profile['id'] if self.owner.is_remote() else str(self.owner.root)
        return 'crashes/ack/'+hashlib.sha256(target.encode()).hexdigest()

    def acknowledge(self):
        latest=(self.owner.state() or {}).get('crashes',{}).get('latest')
        if latest:self.owner.settings.setValue(self.key(),latest['id'])
        self.owner.refresh()

    def tick(self,state):
        target=self.owner.target_id()
        if target!=self.target:
            self.target=target;self.next_poll=0;self.data=dict(events=[],cores=[]);self.rows=[]
            self.events.setRowCount(0);self.core_choice.clear();self.core_path.clear();self.binary_path.clear()
            self.report.clear();self.last_report='';self.report_context=None
        latest=(state or {}).get('crashes',{}).get('latest')
        unread=bool(latest and self.owner.settings.value(self.key())!=latest['id'])
        self.owner.crash_alert.setVisible(unread)
        if unread:self.owner.crash_alert.setText('Váratlan kilépés: %s — %s. Részletek az Összeomlások oldalon.' % (latest['instance'],latest['reason']))
        self.owner.navigation.item(6).setText('Összeomlások ●' if unread else 'Összeomlások')
        if state and not self.isVisible():
            self.status.setText(('A futó felügyelőben aktív a crashfigyelő.' if state.get('crash_monitor_active') else
                'A figyelő a következő szerverindítástól aktív.' if state.get('crash_monitor_available') else
                'A crashfigyelőhöz frissíteni kell a célgép manage.py fájlját. A meglévő core fájlok elemezhetők.'))
        if self.isVisible() and state and time.monotonic()>=self.next_poll and not self.owner.busy():
            self.refresh_list()
        self.controls()

    def controls(self):
        available=bool(self.owner.root) if not self.owner.is_remote() else self.owner.remote_online
        busy=self.owner.busy() if hasattr(self.owner,'crash_page') else False
        for control in (self.refresh_button,self.analyze_button,self.core_path,self.binary_path,self.core_choice,self.events):
            control.setEnabled(available and not busy)
        self.copy_button.setEnabled(bool(self.last_report));self.export_button.setEnabled(bool(self.last_report))
        self.ack_button.setEnabled(available and not busy)

    def run(self,operation,arguments,callback):
        self.next_poll=time.monotonic()+15
        if self.owner.is_remote():
            self.owner.remote_request(operation,arguments,callback,error_callback=self.failed)
            return
        root=self.owner.root
        function=(lambda:crash_tools.inventory(root)) if operation=='crashes' else (
            lambda:dict(crash_tools.analyze(root,arguments['core'],arguments['binary'],arguments.get('expected'))))
        task=CrashTask(function,self);self.task=task
        task.ready.connect(callback);task.failed.connect(self.failed)
        def finished():
            self.task=None;task.deleteLater();self.owner.refresh()
        task.finished.connect(finished);task.start();self.owner.refresh()

    def refresh_list(self):
        if self.owner.busy():return
        self.status.setText('Összeomlások és core fájlok keresése…')
        self.run('crashes',{},self.received)

    def received(self,value):
        self.data=value
        selected=self.core_path.text()
        self.rows=[dict(event=event,core=None) for event in value['events']]
        listed={path for event in value['events'] for path in event.get('cores',[])}
        for core in value['cores']:
            if core['path'] not in listed:
                self.rows.append(dict(event=None,core=core));listed.add(core['path'])
        self.events.blockSignals(True);self.events.clearSelection();self.events.setRowCount(len(self.rows))
        for row,entry in enumerate(self.rows):
            event,core=entry['event'],entry['core']
            if event:
                try: timestamp=datetime.datetime.fromisoformat(event['timestamp']).astimezone().strftime('%Y-%m-%d %H:%M:%S')
                except ValueError:timestamp=event['timestamp']
                values=(timestamp,event['instance'],event['pid'],event['reason'],str(len(event.get('cores',[])))+' db')
                tip='\n'.join(event.get('cores',[]))
            else:
                timestamp=datetime.datetime.fromtimestamp(core['modified']).astimezone().strftime('%Y-%m-%d %H:%M:%S')
                values=(timestamp,self.core_instance(core),'—','Nincs eseménynapló',core['path'])
                tip='Meglévő core fájl. A felügyelő nem rögzített hozzá eseményt; a PID és a kilépési jel ismeretlen. Az időpont a fájl módosításának ideje.'
            for column,text in enumerate(values):
                item=QTableWidgetItem(str(text));item.setToolTip(tip)
                self.events.setItem(row,column,item)
        self.events.blockSignals(False)
        self.core_choice.blockSignals(True);self.core_choice.clear()
        for core in value['cores']:
            self.core_choice.addItem('%s  (%s, %.1f MiB)' % (core['path'],core['format'],core['size']/1048576),core)
        self.core_choice.setCurrentIndex(next((i for i,row in enumerate(value['cores']) if row['path']==selected),-1))
        self.core_choice.blockSignals(False)
        if not selected and value['cores']:
            self.core_choice.setCurrentIndex(0)
        if self.core_choice.currentIndex()>=0:self.select_core(self.core_choice.currentIndex(),preserve_report=True)
        tools=', '.join(name for name,path in value['debuggers'].items() if path) or 'nincs debugger'
        self.status.setText('%d esemény, %d core fájl. Debugger: %s.%s' % (len(value['events']),len(value['cores']),tools,
                            ' A lista elérte a keresési korlátot.' if value.get('limited') else ''))
        state=self.owner.state() or {}
        if state.get('running') and not state.get('crash_monitor_active'):
            self.status.setText(self.status.text()+' A futó felügyelőben még nincs crashfigyelő; a következő indítástól aktív.')

    def core_instance(self,core):
        path=PurePosixPath(core['path'])
        config=(self.owner.state() or {}).get('config',{})
        instances=dict(config.get('game_servers',{}))
        instances.update({name:config[name] for name in ('database','login') if name in config})
        for name,section in instances.items():
            directory=PurePosixPath(section.get('working_directory','run/'+name).replace('\\','/'))
            if path.parent==directory:return name
        return '—'

    def event_report(self,event):
        return '\n'.join('%s: %s'%(key,event.get(key,'')) for key in
            ('timestamp','instance','pid','reason','phase','program','binary','binary_sha256'))+'\n'+'\n'.join(event.get('errors',[]))

    def select_event(self):
        row=self.events.currentRow()
        if not 0<=row<len(self.rows):return
        entry=self.rows[row]
        if entry['core']:
            index=next((i for i,core in enumerate(self.data['cores']) if core['path']==entry['core']['path']),-1)
            self.core_choice.blockSignals(True);self.core_choice.setCurrentIndex(index);self.core_choice.blockSignals(False)
            self.select_core(index)
            return
        event=entry['event']
        self.last_report=self.event_report(event);self.report.setPlainText(self.last_report)
        core=next(iter(event.get('cores',[])),'')
        self.core_choice.blockSignals(True)
        self.core_choice.setCurrentIndex(next((i for i,item in enumerate(self.data['cores']) if item['path']==core),-1))
        self.core_choice.blockSignals(False)
        self.core_path.setText(core);self.binary_path.setText(event.get('binary') or '')
        item=next((item for item in self.data['cores'] if item['path']==core),None)
        self.report_context=(item['path'],item['size'],item['mtime_ns']) if item else None
        self.controls()

    def select_core(self,index,preserve_report=False):
        core=self.core_choice.itemData(index)
        if not core:return
        context=(core['path'],core['size'],core['mtime_ns'])
        preserving=preserve_report and self.report_context==context
        self.core_path.setText(core['path'])
        if not (preserving and self.binary_path.text().strip()):self.binary_path.setText(core['binary'])
        row=next((i for i,entry in enumerate(self.rows) if
            (entry['core'] and entry['core']['path']==core['path']) or
            (entry['event'] and core['path'] in entry['event'].get('cores',[]))),-1)
        self.events.blockSignals(True)
        if row>=0:self.events.selectRow(row)
        self.events.blockSignals(False)
        if row>=0 and self.rows[row]['event']:
            details=self.event_report(self.rows[row]['event'])
        else:
            timestamp=datetime.datetime.fromtimestamp(core['modified']).astimezone().strftime('%Y-%m-%d %H:%M:%S')
            details=('Core fájl: %s\nPéldány: %s\nMódosítva: %s\nMéret: %.1f MiB\nFormátum: %s\n'
                     'Ehhez a core fájlhoz nincs felügyelő által rögzített esemény. A PID és a kilépési jel ismeretlen.\n'
                     'Az elemzéshez az összeomláskor használt build programját válaszd.' %
                     (core['path'],self.core_instance(core),timestamp,core['size']/1048576,core['format']))
        # A background refresh keeps an already displayed debugger report.
        if not (preserving and self.last_report):
            self.last_report=details;self.report.setPlainText(details)
        self.report_context=context
        self.controls()

    def browse(self,control):
        if self.owner.is_remote():
            self.status.setText('Távoli fájlnál írd be a célgépen érvényes teljes útvonalat, vagy válassz a core listából.')
            return
        path,_=QFileDialog.getOpenFileName(self,'Core fájl / hozzá tartozó program',str(self.owner.root or ''))
        if path:control.setText(path)

    def analyze(self):
        if self.owner.busy():return
        if not self.core_path.text().strip() or not self.binary_path.text().strip():
            self.failed('Válassz core fájlt és az összeomláshoz tartozó programot.');return
        core=self.core_path.text().strip()
        expected=next((dict(size=row['size'],mtime_ns=row['mtime_ns']) for row in self.data['cores'] if row['path']==core),None)
        self.report_context=(core,expected['size'],expected['mtime_ns']) if expected else None
        self.last_report='';self.report.clear()
        self.status.setText('Core elemzése, legfeljebb 90 másodperc…')
        self.run('crash-analyze',dict(core=core,binary=self.binary_path.text().strip(),expected=expected),self.analyzed)

    def analyzed(self,value):
        self.last_report=api.redact(value['report'],self.owner.root) if not self.owner.is_remote() else value['report']
        self.report.setPlainText(self.last_report)
        self.status.setText('Elemzés kész — '+value['debugger']+' — '+value['checked_at']);self.controls()

    def failed(self,message):
        self.status.setText(message);self.report.setPlainText(message)

    def export(self):
        path,_=QFileDialog.getSaveFileName(self,'Hibajelentés mentése','crash-report.txt','Szöveg (*.txt)')
        if path:
            try:Path(path).write_text(self.last_report,encoding='utf-8')
            except OSError as error:self.failed(str(error))
