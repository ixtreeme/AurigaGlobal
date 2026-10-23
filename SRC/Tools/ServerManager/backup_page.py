"""Target-bound backup center with verified SFTP import/export."""
import json
import datetime
import os
from pathlib import Path
import secrets
import shutil
import tempfile
import time

from PySide6.QtCore import QProcess,QThread,Signal,Qt
from PySide6.QtWidgets import (QCheckBox,QFileDialog,QFormLayout,QHBoxLayout,QHeaderView,
    QLabel,QLineEdit,QPlainTextEdit,QPushButton,QScrollArea,QSpinBox,QSplitter,QTableWidget,
    QTableWidgetItem,QAbstractItemView,QVBoxLayout,QWidget)

import backend as api
import backup_tools as backups
import server_targets as targets
import ssh_connection as ssh


class BackupTask(QThread):
    ready=Signal(object);failed=Signal(str)
    def __init__(self,callback,parent):super().__init__(parent);self.callback=callback
    def run(self):
        try:self.ready.emit(self.callback())
        except Exception as error:self.failed.emit(str(error))


class BackupPage(QWidget):
    def __init__(self,owner):
        super().__init__();self.owner=owner;self.task=self.transfer=self.session=None
        self.temporary=None;self.plan=None;self.data={};self.target=None;self.next_poll=0;self.loaded_policy=False
        layout=QVBoxLayout(self);layout.setContentsMargins(0,0,0,0)
        bar=QHBoxLayout();title=QLabel('Mentési központ');title.setObjectName('SectionTitle');bar.addWidget(title,1)
        self.reload=QPushButton('Lista frissítése');self.reload.clicked.connect(self.refresh_list);bar.addWidget(self.reload)
        layout.addLayout(bar)
        self.status=QLabel('Válassz helyi vagy távoli szervert.');self.status.setWordWrap(True);layout.addWidget(self.status)
        split=QSplitter(Qt.Orientation.Vertical)
        self.table=QTableWidget(0,5);self.table.setHorizontalHeaderLabels(['Időpont','Típus','Tartalom','Méret','Megjegyzés'])
        self.table.horizontalHeader().setStretchLastSection(True)
        for column,width in enumerate((205,135,240,110)):self.table.setColumnWidth(column,width)
        self.table.setSelectionBehavior(QAbstractItemView.SelectionBehavior.SelectRows)
        self.table.setSelectionMode(QAbstractItemView.SelectionMode.SingleSelection)
        self.table.setEditTriggers(QAbstractItemView.EditTrigger.NoEditTriggers)
        self.table.itemSelectionChanged.connect(self.selected);split.addWidget(self.table)
        body=QWidget();inner=QVBoxLayout(body);self.checks={}
        row=QHBoxLayout()
        for key,label in (('config','Konfiguráció'),('data','Játékadatok és questek'),('tls','TLS-tanúsítványok és kulcsok'),('sql','Adatbázisok')):
            check=QCheckBox(label);check.setChecked(True);check.toggled.connect(self.invalidate);self.checks[key]=check;row.addWidget(check)
        row.addStretch();inner.addLayout(row)
        self.comment=QLineEdit();self.comment.setPlaceholderText('Opcionális megjegyzés a mentéshez');inner.addWidget(self.comment)
        row=QHBoxLayout()
        self.create_button=QPushButton('Új mentés');self.create_button.setObjectName('Primary');self.create_button.clicked.connect(self.create)
        self.download_button=QPushButton('Letöltés / másolat mentése…');self.download_button.clicked.connect(self.download)
        self.import_button=QPushButton('Mentési archívum betöltése…');self.import_button.clicked.connect(self.import_file)
        for button in (self.create_button,self.download_button,self.import_button):row.addWidget(button)
        row.addStretch();inner.addLayout(row)
        row=QHBoxLayout()
        self.plan_button=QPushButton('Visszaállítási terv');self.plan_button.clicked.connect(self.prepare_restore)
        self.preserve=QCheckBox('Jelenlegi SQL-kapcsolatok megtartása');self.preserve.setChecked(True);self.preserve.toggled.connect(self.invalidate)
        self.confirm=QCheckBox('Felülírom a kiválasztott összetevőket');self.confirm.toggled.connect(self.controls)
        row.addWidget(self.plan_button);row.addWidget(self.preserve);row.addWidget(self.confirm);row.addStretch();inner.addLayout(row)
        self.preview=QPlainTextEdit();self.preview.setReadOnly(True);self.preview.setMinimumHeight(100);self.preview.setMaximumHeight(150)
        self.preview.setPlaceholderText('A terv megmutatja a fájlcseréket, törléseket és az érintett SQL-adatbázisokat.');inner.addWidget(self.preview)
        row=QHBoxLayout()
        self.restore_button=QPushButton('Terv szerinti visszaállítás');self.restore_button.setObjectName('Primary');self.restore_button.clicked.connect(self.restore)
        row.addWidget(self.restore_button)
        row.addWidget(QLabel('Előtte automatikus védőmentés készül. A játékszervert állítsd le.'),1);inner.addLayout(row)
        policy=QHBoxLayout()
        self.daily=QCheckBox('Napi automatikus mentés')
        self.hour=QSpinBox();self.hour.setRange(0,23);self.hour.setSuffix(' óra')
        self.minute=QSpinBox();self.minute.setRange(0,59);self.minute.setSuffix(' perc')
        self.keep=QSpinBox();self.keep.setRange(0,365);self.keep.setSuffix(' ütemezett mentés');self.keep.setSpecialValueText('Mindet megtartja')
        self.auto_deploy=QCheckBox('Automatikus mentés buildtelepítés előtt')
        for control in (self.daily,self.hour,self.minute,QLabel('Megőrzés:'),self.keep):policy.addWidget(control)
        policy.addStretch();inner.addLayout(policy)
        row=QHBoxLayout();row.addWidget(self.auto_deploy)
        self.policy_button=QPushButton('Mentési beállítások mentése');self.policy_button.clicked.connect(self.save_policy);row.addWidget(self.policy_button);row.addStretch();inner.addLayout(row)
        hint=QLabel('Az SQL-mentés idején az írások átmenetileg várakozhatnak. A csomag jelszavakat és privát kulcsokat is tartalmazhat. '
            'Az ütemezés a célgép idejét követi, az app bezárása után is fut; Windowson bejelentkezett felhasználó szükséges.')
        hint.setWordWrap(True);inner.addWidget(hint)
        scroll=QScrollArea();scroll.setWidgetResizable(True);scroll.setWidget(body);split.addWidget(scroll);split.setSizes([200,450]);layout.addWidget(split,1)
        self.note=QLabel();self.note.setWordWrap(True);layout.addWidget(self.note)
        self.cancel_transfer=QPushButton('Archívum átvitelének megszakítása')
        self.cancel_transfer.clicked.connect(lambda:self.transfer.kill() if self.transfer else None)
        self.cancel_transfer.hide();layout.addWidget(self.cancel_transfer)

    def active(self):return self.task is not None or self.transfer is not None

    def current_id(self):
        row=self.table.currentRow();items=self.data.get('backups',[])
        return items[row]['id'] if 0<=row<len(items) else None

    def selected_scopes(self):return [name for name,control in self.checks.items() if control.isChecked()]

    def invalidate(self,*_):
        self.plan=None
        if hasattr(self,'preview'):self.preview.clear()
        if hasattr(self,'confirm'):self.confirm.setChecked(False)
        if hasattr(self,'restore_button'):self.restore_button.setEnabled(False)

    def tick(self,state):
        if self.target!=self.owner.target_id():
            self.target=self.owner.target_id();self.next_poll=0;self.loaded_policy=False;self.data={};self.invalidate();self.table.setRowCount(0)
        if self.isVisible() and state and time.monotonic()>=self.next_poll and not self.owner.busy():self.refresh_list()
        self.controls()

    def controls(self,*_):
        available=self.owner.remote_online if self.owner.is_remote() else bool(self.owner.root)
        busy=self.owner.busy() if hasattr(self.owner,'backup_page') else False
        enabled=available and not busy
        self.cancel_transfer.setVisible(self.transfer is not None)
        for button in (self.reload,self.create_button,self.import_button,self.policy_button,self.table,self.comment,self.preserve,self.confirm,
                       self.daily,self.hour,self.minute,self.keep,self.auto_deploy,*self.checks.values()):button.setEnabled(enabled)
        self.download_button.setEnabled(enabled and bool(self.current_id()))
        running=bool((self.owner.state() or {}).get('running')) if available else False
        self.plan_button.setEnabled(enabled and bool(self.current_id()) and not running)
        self.restore_button.setEnabled(enabled and self.plan is not None and self.confirm.isChecked() and not running)

    def failed(self,text):
        if self.task is None and self.transfer is None and self.temporary:
            self.temporary.cleanup();self.temporary=None
        self.status.setText(text);self.invalidate()

    def run_local(self,function,callback):
        task=BackupTask(function,self);self.task=task
        outcome={}
        task.ready.connect(lambda value:outcome.update(value=value))
        task.failed.connect(lambda text:outcome.update(error=text))
        def finished():
            if self.task is task:self.task=None
            task.deleteLater()
            try:
                if 'error' in outcome:self.failed(outcome['error'])
                elif 'value' in outcome:callback(outcome['value'])
            except Exception as error:self.failed(str(error))
            self.owner.refresh()
        task.finished.connect(finished);task.start();self.owner.refresh()

    def run(self,operation,args,callback):
        self.next_poll=time.monotonic()+30
        if self.owner.is_remote():
            return self.owner.remote_request(operation,args,callback,error_callback=self.failed)
        root=self.owner.root
        actions={
            'backup-list':lambda:backups.inventory(root,api),
            'backup-create':lambda:backups.create(root,api,args['scopes'],comment=args.get('comment','')),
            'backup-plan':lambda:backups.restore_plan(root,api,args['id'],args['scopes']),
            'backup-restore':lambda:backups.restore(root,api,args['id'],args['scopes'],args['fingerprint'],args['preserve_connections']),
            'backup-policy':lambda:backups.save_policy(root,api,args['policy']),
            'backup-info':lambda:backups.archive_info(root,args['id']),
            'backup-import':lambda:backups.import_archive(root,api,args['path'],args['sha256'])}
        self.run_local(actions[operation],callback)

    def refresh_list(self):
        if self.owner.busy():return
        self.status.setText('Mentések keresése…');self.run('backup-list',{},self.received)

    def received(self,value):
        selected=self.current_id();self.data=value;self.table.blockSignals(True);self.table.setRowCount(len(value['backups']))
        labels={'manual':'Kézi','scheduled':'Ütemezett','before-restore':'Védőmentés','before-deploy':'Build előtt'}
        scope_labels={'config':'Beállítások','data':'Játékadatok','tls':'TLS','sql':'SQL'}
        for row,item in enumerate(value['backups']):
            timestamp=datetime.datetime.fromisoformat(item['created']).astimezone().strftime('%Y-%m-%d %H:%M:%S')
            size='%.1f KiB'%(item['archive_size']/1024) if item['archive_size']<1048576 else '%.1f MiB'%(item['archive_size']/1048576)
            for column,text in enumerate((timestamp,labels.get(item['reason'],item['reason']),', '.join(scope_labels[name] for name in item['scopes']),
                                         size,item['comment'])):
                self.table.setItem(row,column,QTableWidgetItem(str(text)))
        match=next((i for i,item in enumerate(value['backups']) if item['id']==selected),-1)
        if match>=0:self.table.selectRow(match)
        self.table.blockSignals(False)
        if not self.loaded_policy:
            settings=value['policy'];self.daily.setChecked(settings['enabled']);self.hour.setValue(settings['hour']);self.minute.setValue(settings['minute'])
            self.keep.setValue(settings['keep']);self.auto_deploy.setChecked(settings['auto_before_deploy']);self.loaded_policy=True
            for name,control in self.checks.items():control.setChecked(name in settings['scopes'])
        self.status.setText('%d mentés. Szabad hely: %.1f GiB.'%(len(value['backups']),value['free_bytes']/1073741824))
        self.note.setText('Folyamatban: '+value['active'] if value.get('active') else
            'Befejezetlen helyreállítás! Válassz ellenőrzött mentést, és készíts visszaállítási tervet.' if value.get('pending') else '')
        if value.get('schedule_last'):
            last=value['schedule_last']
            state={'success':'sikeres','failed':'sikertelen'}.get(last.get('status'),last.get('status',''))
            self.note.setText(self.note.text()+'\nUtolsó ütemezett mentés: %s — %s. %s'%(last.get('date',''),state,last.get('error','')))
        self.controls()

    def selected(self):
        self.invalidate();identity=self.current_id()
        item=next((row for row in self.data.get('backups',[]) if row['id']==identity),None)
        if item:
            for name,control in self.checks.items():control.setChecked(name in item['scopes'])
        self.controls()

    def create(self):
        if self.owner.busy():return
        if any(editor.dirty for editor in self.owner.editors):self.failed('Mentés előtt mentsd a megnyitott szerkesztők módosításait.');return
        self.status.setText('Mentés készül a kiválasztott gépen. Az adatbázis méretétől függően több percig tarthat…')
        self.run('backup-create',dict(scopes=self.selected_scopes(),comment=self.comment.text()),lambda value:self.done('Mentés elkészült: '+value['id']))

    def done(self,text):
        self.status.setText(text);self.note.setText(text);self.invalidate();self.next_poll=0

    def prepare_restore(self):
        if self.owner.busy() or not self.current_id():return
        if any(editor.dirty for editor in self.owner.editors):self.failed('Visszaállítás előtt mentsd vagy vesd el a megnyitott szerkesztők módosításait.');return
        self.invalidate();self.status.setText('A teljes archívum és a visszaállítási cél ellenőrzése…')
        self.run('backup-plan',dict(id=self.current_id(),scopes=self.selected_scopes()),self.planned)

    def planned(self,value):
        self.plan=value
        sql=['%s:%s — %s'%(row['host'],row['port'],', '.join(row['databases'])) for row in value['sql']]
        self.preview.setPlainText('Mentés: '+value['id']+'\nVisszaállítandó fájlok: %d\nEltávolítandó többletfájlok: %d\n'%(value['files'],len(value['remove']))+
            '\n'.join(value['remove'][:30])+('\nSQL-adatbázisok teljes felülírása:\n'+'\n'.join(sql) if sql else '')+
            '\nA jelenlegi állapotról automatikus védőmentés készül.')
        self.status.setText('A terv elkészült. Ellenőrizd, majd jelöld be a felülírást.');self.controls()

    def restore(self):
        if self.owner.busy() or not self.plan or not self.confirm.isChecked():return
        if any(editor.dirty for editor in self.owner.editors):
            self.failed('Visszaállítás előtt mentsd vagy vesd el a megnyitott szerkesztők módosításait.');return
        plan=self.plan;self.invalidate();self.status.setText('Védőmentés és visszaállítás folyamatban. Hagyd nyitva az alkalmazást…')
        args=dict(id=plan['id'],scopes=plan['scopes'],fingerprint=plan['fingerprint'],preserve_connections=self.preserve.isChecked())
        def restored(value):
            self.done('Visszaállítás kész. Védőmentés: '+value['safety_backup'])
            self.owner.clear_target_pages()
            if self.owner.is_remote():
                def connected(packet):
                    self.owner.remote_state=packet['state'];self.owner.remote_inventory=packet['inventory']
                    self.owner.config_editor.display(targets.document(packet['document']))
                    for page in (self.owner.data_page,self.owner.quest_page):page.refresh()
                    self.owner.update_channels();self.owner.update_logs()
                self.owner.remote_request('connect',{},connected)
            else:
                self.owner.config_editor.load('server.json',force=True);self.owner.refresh_inventory();self.owner.update_channels()
        self.run('backup-restore',args,restored)

    def save_policy(self):
        if self.owner.busy():return
        value=dict(version=1,enabled=self.daily.isChecked(),hour=self.hour.value(),minute=self.minute.value(),keep=self.keep.value(),
                   scopes=self.selected_scopes(),auto_before_deploy=self.auto_deploy.isChecked())
        args=dict(policy=value)
        if self.owner.is_remote():
            profile=self.owner.remote_profile
            request=dict(schema_version=1,id=secrets.token_hex(12),target=profile['id'],root=profile['remote_root'],operation='backup-scheduled',arguments={})
            args['runner']=targets.worker_input(request).decode()
        self.status.setText('Mentési beállítások és ütemezés mentése…')
        self.run('backup-policy',args,lambda value:self.done('Mentési beállítások elmentve.'))

    def download(self):
        if self.owner.busy() or not self.current_id():return
        destination,_=QFileDialog.getSaveFileName(self,'Mentési másolat mentése','Auriga-'+self.current_id()+'.tar.gz','Auriga mentés (*.tar.gz)')
        if not destination:return
        self.status.setText('Archívum ellenőrzése a másolás előtt…')
        def ready(value):
            if self.owner.is_remote():
                self.temporary=tempfile.TemporaryDirectory(prefix='auriga-backup-download-')
                path=Path(self.temporary.name)/'backup.tar.gz'
                def transferred():self.run_local(lambda:self.publish_download(path,Path(destination),value),lambda _:self.transfer_done('A mentési másolat elkészült.'))
                self.start_sftp('get',value['path'],path,transferred)
            else:self.run_local(lambda:self.publish_download(Path(value['path']),Path(destination),value),lambda _:self.done('A mentési másolat elkészült.'))
        self.run('backup-info',dict(id=self.current_id()),ready)

    @staticmethod
    def publish_download(source,destination,info):
        temporary=destination.with_name(destination.name+'.part-'+secrets.token_hex(6))
        try:
            shutil.copyfile(source,temporary)
            if temporary.stat().st_size!=info['size'] or backups.digest(temporary)!=info['sha256']:raise ValueError('A letöltött mentés ellenőrzőösszege eltér.')
            temporary.chmod(0o600);os.replace(temporary,destination)
        finally:temporary.unlink(missing_ok=True)
        return True

    def import_file(self):
        if self.owner.busy():return
        source,_=QFileDialog.getOpenFileName(self,'Mentési archívum betöltése','','Auriga mentés (*.tar.gz)')
        if not source:return
        self.temporary=tempfile.TemporaryDirectory(prefix='auriga-backup-upload-');path=Path(self.temporary.name)/'backup.tar.gz'
        def snapshot():shutil.copyfile(source,path);return dict(sha256=backups.digest(path),path=str(path))
        def ready(value):
            if not self.owner.is_remote():
                self.run('backup-import',value,lambda result:self.transfer_done('A mentés betöltve: '+result['id']));return
            identity=secrets.token_hex(12)
            def prepared(packet):
                def uploaded():self.run('backup-import',dict(transfer=identity,sha256=value['sha256']),lambda result:self.transfer_done('A mentés feltöltve és ellenőrizve: '+result['id']))
                self.start_sftp('put',packet['path'],path,uploaded)
            self.run('backup-upload-prepare',dict(transfer=identity),prepared)
        self.status.setText('Feltöltési pillanatkép és ellenőrzőösszeg készítése…');self.run_local(snapshot,ready)

    def start_sftp(self,operation,remote,local,callback):
        try:
            self.session=ssh.Session(self.owner.remote_profile,parent=self)
            quote=lambda value:'"'+str(value).replace('\\','/').replace('"','\\"')+'"'
            batch=Path(self.temporary.name)/'batch'
            line=operation+' '+(quote(remote)+' '+quote(local) if operation=='get' else quote(local)+' '+quote(remote))
            batch.write_text(line+'\nbye\n',encoding='utf-8')
            process=QProcess(self);self.transfer=process;ssh.configure_process(process,self.session)
            process.setProcessChannelMode(QProcess.ProcessChannelMode.MergedChannels);output=[]
            process.readyReadStandardOutput.connect(lambda:output.append(bytes(process.readAllStandardOutput()).decode('utf-8',errors='replace')))
            process.started.connect(process.closeWriteChannel)
            def finished(code,status):
                if self.transfer is not process:return
                self.transfer=None;process.deleteLater();self.session.close();self.session=None
                if code or status!=QProcess.ExitStatus.NormalExit:self.transfer_failed('SFTP-hiba: '+''.join(output)[-3000:])
                else:callback()
                self.owner.refresh()
            process.finished.connect(finished)
            process.errorOccurred.connect(lambda error:finished(1,QProcess.ExitStatus.CrashExit) if error==QProcess.ProcessError.FailedToStart else None)
            self.status.setText('Mentési archívum átvitele SFTP-vel…');process.start(*self.session.sftp(batch));self.owner.refresh()
        except Exception as error:self.transfer_failed(str(error))

    def transfer_failed(self,text):
        if self.session:self.session.close();self.session=None
        self.transfer=None
        if self.temporary:self.temporary.cleanup();self.temporary=None
        self.failed(text)

    def transfer_done(self,text):
        if self.temporary:self.temporary.cleanup();self.temporary=None
        self.done(text)
