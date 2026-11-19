"""A single explicit approval, preflight, and acknowledged sequential publication."""
import base64,copy,json
from pathlib import Path
from PySide6.QtCore import QLockFile,QObject,QTimer
from PySide6.QtWidgets import QDialog
import backend as api
import change_review as review
from pending_changes import compile_catalog


class PendingPublisher(QObject):
    def __init__(self,owner,queue,page):
        super().__init__(page);self.owner=owner;self.queue=queue;self.page=page;self.task=None;self.entries=[];self.groups=[];self.batch=None;self.begun=[];self.applied=[];self.catalog_result=None;self.lock=None

    def background(self,function,done,failed):
        from item_page import Task
        task=Task(function,self);self.task=task;outcome={}
        task.ready.connect(lambda value:outcome.update(value=value));task.failed.connect(lambda error:outcome.update(error=error))
        def finished():
            self.task=None;task.deleteLater()
            if 'error' in outcome:failed(outcome['error'])
            else:
                try:done(outcome.get('value'))
                except Exception as error:failed(str(error))
        task.finished.connect(finished);task.start()

    def start(self,entries):
        if self.queue.active:return
        try:
            self.queue.root.mkdir(parents=True,exist_ok=True,mode=0o700);self.lock=QLockFile(str(self.queue.root/'publication.lock'))
            if not self.lock.tryLock(0):self.lock=None;raise api.EditorError('Másik kezelőablakban csomagjóváhagyás vagy feltöltés van folyamatban.')
            if any(e['state']!='staged' for e in entries):raise api.EditorError('Előbb ellenőrizd a bizonytalan korábbi feltöltés eredményét.')
            for e in entries:
                body,_=e['stage'].read()
                if not review.Stage.matches(self.owner,body):raise api.EditorError('A csomag másik célgéphez tartozik.')
            self.batch=review.Stage.create(self.owner,'%d függő szerkesztés közös jóváhagyása'%len(entries),'pending_batch',
                dict(entries=[dict(id=e['id'],checksum=e['checksum']) for e in entries]),self.queue.summary(entries),root=self.queue.root/'batches',persistent=True)
            dialog=review.ChangeReviewDialog(self.owner,self.batch)
            accepted=dialog.exec()==QDialog.DialogCode.Accepted;dialog.deleteLater()
            if not accepted:self.lock.unlock();self.lock=None;self.page.status.setText('A szerkesztések továbbra is függőben vannak. Nem történt feltöltés.');return
            self.batch.approve(self.owner);self.entries=entries;self.applied=[];self.begun=[];self.catalog_result=None
            self.queue.active=True;self.owner.refresh();self.page.selection_changed();self.page.status.setText('A teljes jóváhagyott csomag ellenőrzése feltöltés előtt…')
            self.background(self.preflight,lambda _:self.remote_preflight(),lambda error:self.finish(False,error,preflight=True))
        except (api.EditorError,OSError,ValueError) as error:
            if self.lock:self.lock.unlock();self.lock=None
            self.owner.error(str(error))

    def preflight(self):
        import item_catalog as catalog
        from reward_catalog import Rewards
        entries=self.entries;state=self.owner.state() or {}
        if self.owner.is_remote() and not self.owner.remote_online:raise api.EditorError('A feltöltéshez csatlakozz a kiválasztott távoli szerverhez.')
        if any(e['body']['kind']=='backup-restore' for e in entries) and len(entries)>1:
            raise api.EditorError('A teljes visszaállítást külön hagyd jóvá, mert felülírhatja a többi szerkesztés célját. Jelöld ki csak a visszaállítást.')
        sql=[e for e in entries if e['body']['kind']=='catalog_edit']
        if sql:
            before=self.queue.baseline(sql);ops=[e['body']['payload']['operation'] for e in sql]
            if state.get('running') and any(op['type']=='item' for op in ops):raise api.EditorError('A tervezetek megmaradtak. A tárgydefiníciók feltöltése előtt állítsd le a játékszervert; a dropok önállóan élőben feltölthetők.')
            current=catalog.request(self.owner.root)
            if current['hash']!=before['hash'] or current['live_drops']['hash']!=before['live_drops']['hash'] or current['live_drops']['release']!=before['live_drops']['release']:
                raise api.EditorError('Az éles tárgy- vagy dropadatok megváltoztak a tervezet óta. Egyetlen változtatást sem töltöttünk fel.')
            after=compile_catalog(before,ops);model=catalog.Catalog(after,live_drops=True);rewards=Rewards(after)
            for base in rewards.containers():
                key=(int(base['category_id']),int(base['local_id']))
                if key in rewards.overrides:
                    doc=model.item(key);doc['loot']=rewards.loot(key);model.apply(doc,key,encode_result=False)
            catalog.run_tool(dict(action='validate',tables=model.encode()),before['columns'])
        for e in entries:
            body,status=e['stage'].read();data=body['payload'];kind=body['kind']
            if status['state']!='staged' or not review.Stage.matches(self.owner,body):raise api.EditorError('A jóváhagyott csomag időközben megváltozott.')
            if kind=='file' and not self.owner.is_remote():
                doc=api.Document(self.owner.root,data['relative'],allow_invalid_json=True)
                if doc.digest!=data['expected_hash']:raise api.EditorError('Az éles fájl megváltozott: '+data['relative'])
            if kind in ('ssh_profile','ssh_profile_delete'):
                import ssh_password
                profiles=self.owner.remote_page.store.load();profile=data.get('profile') or data['removed'];previous=next((p for p in profiles if p['id']==profile['id']),None)
                expected=data.get('previous',data.get('removed'))
                if previous!=expected:raise api.EditorError('Az SSH-profilt másik szerkesztés módosította. Készíts új tervezetet.')
                for role,expected_hash in data.get('password_fingerprints',{}).items():
                    if review.digest(ssh_password.recalled(profile,role).encode())!=expected_hash:raise api.EditorError('A jelszót újra meg kell adni a függő SSH-profil szerkesztőjében.')
            if kind=='backup-restore' and state.get('running'):raise api.EditorError('Visszaállítás előtt állítsd le a játékszervert.')
            if kind=='backup-policy' and data.get('before_policy') is not None and not self.owner.is_remote():
                import backup_tools
                if backup_tools.policy(self.owner.root)!=data['before_policy']:raise api.EditorError('A mentési beállítások közben megváltoztak. Készíts új tervezetet.')
            if kind not in {'catalog_edit','file','bench','bench_install','bench_setup','ssh_profile','ssh_profile_delete',
                            'backup-create','backup-policy','backup-restore','backup-import','server_action','remote_operation','remote_quest_activation'}:
                raise api.EditorError('Ez a régebbi művelet új szerkesztést igényel: '+body['title'])
        return True

    def remote_preflight(self):
        files=[e for e in self.entries if e['body']['kind']=='file'] if self.owner.is_remote() else []
        def next_file():
            if not files:
                try:self.prepared()
                except Exception as error:self.finish(False,str(error),preflight=True)
                return
            entry=files.pop(0);data=entry['body']['payload'];completed=[False]
            def received(packet):
                import server_targets
                actual=server_targets.document(packet)
                if actual.relative!=data['relative'] or actual.digest!=data['expected_hash']:
                    failed('Az éles távoli fájl megváltozott: '+data['relative']);return
                completed[0]=True;QTimer.singleShot(0,next_file)
            def failed(error):
                if completed[0]:return
                completed[0]=True;self.finish(False,error,preflight=True)
            if not self.owner.remote_request('read',dict(relative=data['relative']),received,error_callback=failed):failed('A távoli ellenőrzés nem indult el.')
        next_file()

    def prepared(self):
        self.batch.begin(self.owner)
        # The batch approval binds every child checksum. A child starts only at its own turn.
        for e in self.entries:e['stage'].approve(self.owner)
        sql=[e for e in self.entries if e['body']['kind']=='catalog_edit']
        rest=[e for e in self.entries if e['body']['kind']!='catalog_edit']
        rest.sort(key=lambda e:2 if e['body']['kind'].startswith('ssh_profile') else 1 if e['body']['kind'] in ('server_action','remote_quest_activation') else 0)
        self.groups=([sql] if sql else [])+[[e] for e in rest];self.advance()

    def advance(self):
        if not self.groups:self.finish(True,'A jóváhagyott csomag feltöltése befejeződött.');return
        group=self.groups.pop(0);self.begun=group
        try:
            for e in group:e['stage'].begin(self.owner)
            self.page.fill()
            self.page.status.setText('Feltöltés: '+group[0]['body']['title'])
            if group[0]['body']['kind']=='catalog_edit':
                def done(packet):self.catalog_result=packet;self.completed(group,True)
                self.background(lambda:self.queue.publish_catalog(group),done,lambda error:self.completed(group,False,error))
            else:self.dispatch(group[0],lambda ok,error='':self.completed(group,ok,error))
        except Exception as error:self.completed(group,False,str(error))

    def completed(self,group,ok,error=''):
        if group is not self.begun or not self.queue.active:return
        for e in group:e['stage'].finish(ok)
        if not ok:self.finish(False,error or 'A feltöltés nem igazolt. Az eredményt ellenőrizni kell.');return
        self.applied.extend(group);self.begun=[];QTimer.singleShot(0,self.advance)

    def dispatch(self,entry,done):
        body=entry['body'];data=body['payload'];kind=body['kind'];stage=entry['stage'];owner=self.owner
        if kind=='file':
            args={key:data[key] for key in ('relative','data','expected_hash')}
            if owner.is_remote():
                started=owner.remote_request('save',args,lambda result:done(True),error_callback=lambda error:done(False,error))
                if not started:QTimer.singleShot(0,lambda:done(False,'A távoli mentés nem indult el.'))
            else:
                path=stage.path/'upload-content';review.atomic(path,base64.b64decode(data['data'],validate=True))
                owner.job(['save',data['relative'],str(path),data['expected_hash']],lambda ok:done(ok,'' if ok else owner.console.toPlainText()[-2000:]),_pending=True)
        elif kind.startswith('bench'):
            import item_bench as bench
            def write():
                if kind=='bench_install':return bench.install(owner.root)
                if kind=='bench_setup':return bench.call(owner.root,'setup',owner=data['owner'],reset=True,gold=data['gold'])
                return bench.call(owner.root,data['action'],owner=data['owner'],**data['fields'])
            self.background(write,lambda result:done(True),lambda error:done(False,error))
        elif kind.startswith('ssh_profile'):
            import ssh_password
            store=owner.remote_page.store;profiles=store.load();profile=data.get('profile') or data['removed'];previous=next((p for p in profiles if p['id']==profile['id']),None)
            updated=[p for p in profiles if p['id']!=profile['id']]
            if kind=='ssh_profile':updated.append(data['profile'])
            store.save(updated,transaction=ssh_password.saved_preferences(data.get('profile'),previous))
            owner.remote_page.profiles=updated;owner.remote_page.refill();QTimer.singleShot(0,lambda:done(True))
        elif kind.startswith('backup-'):
            page=owner.backup_page
            if kind=='backup-import':page.execute_pending_import(stage,data,done)
            else:
                page.pending_callback=done
                page.run(kind,data['arguments'],lambda value:page.pending_done(True),stage=stage)
        elif kind in ('server_action','remote_quest_activation'):
            argv=data['argv'] if kind=='server_action' else [data['action']]
            owner.job(argv,lambda ok:done(ok,'' if ok else 'A szerverművelet sikertelen.'),_pending=True)
        elif kind=='remote_operation':owner.remote_page.work.execute_pending(stage,data,done)
        else:raise api.EditorError('Ismeretlen függő művelet.')

    def finish(self,ok,message,preflight=False):
        for e in self.entries:
            try:
                _,status=e['stage'].read()
                if status['state']=='approved':e['stage'].state('staged')
            except (api.EditorError,OSError,ValueError):pass
        if self.batch:
            try:self.batch.state('applied' if ok else 'validation_failed' if preflight else 'unconfirmed')
            except (api.EditorError,OSError,ValueError):pass
        self.queue.active=False;self.queue.revision+=1;self.queue.overlay_cache={};self.queue.changed.emit()
        if self.catalog_result:
            self.queue.rebase_remaining(self.catalog_result);self.owner.item_page.loaded(self.catalog_result)
        if self.lock:self.lock.unlock();self.lock=None
        self.owner.remote_page.notify_profiles();self.owner.refresh_pending_views();self.owner.refresh();self.page.fill()
        prefix=('%d szerkesztés már feltöltve. '%len(self.applied)) if not ok and self.applied else ''
        self.page.status.setText(prefix+message)
        if not ok:self.owner.error(prefix+message)

    def reconcile(self,entries):
        if not entries or self.owner.busy():return
        if any(e['scope']!=review.scope(self.owner) for e in entries):self.owner.error('Az ellenőrzéshez válaszd ki az eredeti célgépet.');return
        def check():
            import item_catalog as catalog
            result=[]
            for e in entries:
                data=e['body']['payload'];kind=e['body']['kind']
                if kind=='file' and not self.owner.is_remote():
                    actual=api.Document(self.owner.root,data['relative'],allow_invalid_json=True).digest
                    state='applied' if actual==review.digest(base64.b64decode(data['data'])) else 'staged' if actual==data['expected_hash'] else None
                elif kind=='catalog_edit':
                    actual=catalog.request(self.owner.root);base=self.queue.baseline([e])
                    state='staged' if actual['hash']==base['hash'] and actual['live_drops']['hash']==base['live_drops']['hash'] else None
                    if state is None:
                        wanted=compile_catalog(base,[x['body']['payload']['operation'] for x in self.queue.catalog_entries()]);sort=lambda rows:sorted(review.encoded(row) for row in rows)
                        same=all(sort(actual['tables'][k])==sort(wanted['tables'][k]) for k in actual['tables']) and all(sort(actual['live_drops'][k])==sort(wanted['live_drops'][k]) for k in ('container_groups','container_entries'))
                        same=same and all(sort(actual['live_drops']['tables'][k])==sort(wanted['live_drops']['tables'][k]) for k in ('mob_drop_group','mob_drop_entry'))
                        if same:state='applied'
                else:state=None
                if state is None:raise api.EditorError('Az eredmény nem dönthető el automatikusan: '+e['body']['title']+'. Ellenőrizd az éles adatokat; automatikus újrapróbálás nem történt.')
                result.append((e['stage'],state))
            return result
        def checked(results):
            for stage,state in results:stage.state(state)
            self.queue.revision+=1;self.queue.changed.emit();self.page.fill();self.page.status.setText('Az éles adatok alapján a feltöltési állapotok ellenőrizve.')
        self.background(check,checked,self.owner.error)
