"""Durable pending edits, target-bound overlays and combined catalog publication."""
import base64,contextlib,copy,gzip,json,os
from pathlib import Path
from PySide6.QtCore import QLockFile,QObject,Signal
import backend as api
import change_review as review

CATALOG_KINDS={'catalog_edit','catalog_socket_edit'}
SQL_KINDS={'item','container','bulk_rewards','mob_drop','mob_drop_delete'} | CATALOG_KINDS
PENDING={'staged','approved','publishing','unconfirmed'}
_models={}


def read_model(packet):
    import item_catalog as catalog
    key=(id(packet['tables']),id(packet.get('live_drops',{}).get('tables')))
    found=_models.get(key)
    if found is None:
        found=(packet,catalog.Catalog(packet,live_drops=True));_models[key]=found
        if len(_models)>4:_models.pop(next(iter(_models)))
    return found[1]


def drop_model(packet):
    source=read_model(packet);model=copy.copy(source);model.tables=dict(source.tables)
    for name in ('mob_drop_group','mob_drop_entry'):model.tables[name]=list(source.tables[name])
    return model


def merged_rows(old,new):
    rows={row[1]:list(row) for row in old}
    for row in new:
        value=list(row)
        if value[1] in rows:value[2]=rows[value[1]][2]
        rows[value[1]]=value
    return [row for row in rows.values() if row[2]!=row[3]]


def rebind_profiles(document,current):
    """Internal profile allocations are recomputed when earlier drafts change."""
    result=copy.deepcopy(document)
    def visit(value,previous):
        if isinstance(value,dict):
            for key,child in value.items():
                category=document['base']['category_id']
                explicit=(key=='ammo_policy_id' and category=='21') or (key=='socket_profile_id' and category=='3')
                if (key.endswith('_profile_id') or key in ('profile_id','policy_id')) and not explicit:
                    value[key]=previous.get(key) if isinstance(previous,dict) else None
                else:visit(child,previous.get(key) if isinstance(previous,dict) else None)
        elif isinstance(value,list):
            for i,child in enumerate(value):visit(child,previous[i] if isinstance(previous,list) and i<len(previous) else None)
    visit(result,current);return result


def compile_catalog(before,operations):
    import item_catalog as catalog
    from reward_catalog import Rewards
    model=catalog.Catalog(before) if any(op['type']=='item' for op in operations) else None
    for op in operations:
        if op['type']!='item':continue
        key=tuple(op['key'])
        if op.get('socket_profile_only'):
            local=str(key[1])
            child=next((row for row in model.tables['item_accessory'] if row['local_id']==local and row['category_id']==str(key[0])),None)
            expected=op['document']['socket_profile']['id']
            if child is None or child['socket_profile_id']!=expected:
                raise api.EditorError('Az ékszer foglalatprofilja megváltozott. Készíts új tervezetet.')
            model.apply_accessory_socket(op['document'],child)
            continue
        exists=any((int(r['category_id']),int(r['local_id']))==key for r in model.tables['item_base'])
        document=rebind_profiles(op['document'],model.item(key) if exists else {})
        model.apply(document,key if exists else None,encode_result=False)
    packet=copy.copy(before);packet['tables']=model.encode() if model is not None else before['tables'];packet['live_drops']=copy.deepcopy(before.get('live_drops',{}))
    if model is not None:
        packet['sequences']=dict(before['sequences'])
        for base in model.tables['item_base']:
            category=base['category_id'];packet['sequences'][category]=str(max(int(packet['sequences'][category]),int(base['local_id'])+1))
    source=read_model(before) if model is None else catalog.Catalog(packet,live_drops=True)
    live=copy.copy(source);live.tables=dict(source.tables)
    for name in ('mob_drop_group','mob_drop_entry'):live.tables[name]=list(source.tables[name])
    for op in operations:
        if op['type']!='mob':continue
        key=str(op['key']);live.replace('mob_drop_group',{'group_id':key},[op['group']] if op['group'] else [])
        live.replace('mob_drop_entry',{'group_id':key},op['entries'])
    packet['live_drops']['tables']={name:[[row[column] for column in before['columns'][name]] for row in live.tables[name]] for name in ('mob_drop_group','mob_drop_entry')}
    rewards=Rewards(packet,model=live)
    for op in operations:
        if op['type']=='container':rewards.set_loot(tuple(op['key']),op['document'])
    keys=set(rewards.bases)
    if any((int(row['category_id']),int(row['local_id'])) not in keys for row in live.tables['mob_drop_entry']):raise api.EditorError('Egy függő mobdrop még hivatkozik a visszavonni kívánt tárgyra.')
    for loot in rewards.overrides.values():
        if any(row['reward_kind']=='1' and (int(row['item_category_id']),int(row['item_local_id'])) not in keys for row in loot['entries']):raise api.EditorError('Egy függő ládatartalom még hivatkozik a visszavonni kívánt tárgyra.')
    packet['live_drops'].update(rewards.container_rows());packet['_pending_overlay']=True;packet['_pending_base']=before
    _models[(id(packet['tables']),id(packet['live_drops']['tables']))]=(packet,live)
    if len(_models)>4:_models.pop(next(iter(_models)))
    return packet


class PendingChanges(QObject):
    changed=Signal()
    def __init__(self,owner):
        super().__init__(owner);self.owner=owner;self.active=False;self.revision=0;self._loaded_root=None;self.index=[];self.cache={};self.overlay_cache={};self.baselines={};self.baseline_refs={}

    @property
    def root(self):return review.directory(self.owner)/'pending'

    def load(self):
        if self._loaded_root==self.root:return
        self._loaded_root=self.root;self.cache={};self.overlay_cache={};self.baselines={};self.baseline_refs={};path=self.root/'index.json'
        if path.is_file():
            raw=path.read_bytes();value=api.strict_json(raw.decode('utf-8'))
            if value.get('version')!=1 or not isinstance(value.get('entries'),list):raise api.EditorError('A függő módosítások jegyzéke hibás.')
            self.index=value['entries'];self.index_digest=review.digest(raw)
        else:self.index=[];self.index_digest=None

    def save_index(self):
        self.root.mkdir(parents=True,exist_ok=True,mode=0o700)
        lock=QLockFile(str(self.root/'index.lock'))
        if not lock.tryLock(500):raise api.EditorError('Másik kezelőablak ment tervezetet. Próbáld újra a lista frissítése után.')
        try:
            path=self.root/'index.json';actual=review.digest(path.read_bytes()) if path.exists() else None
            if actual!=self.index_digest:raise api.EditorError('Másik kezelőablak módosította a függő listát. Frissítsd a listát az új mentés előtt.')
            raw=review.encoded(dict(version=1,entries=self.index));review.atomic(path,raw);self.index_digest=review.digest(raw)
        finally:lock.unlock()
        self.revision+=1;self.overlay_cache={};self.changed.emit()

    @contextlib.contextmanager
    def write_guard(self):
        self.root.mkdir(parents=True,exist_ok=True,mode=0o700);lock=QLockFile(str(self.root/'publication.lock'))
        if not lock.tryLock(0):raise api.EditorError('Másik kezelőablakban mentés vagy csomagjóváhagyás van folyamatban. Várd meg a befejezését.')
        try:yield
        finally:lock.unlock()

    def entries(self,*,current=True):
        self.load();result=[]
        for record in self.index:
            if current and record.get('scope')!=review.scope(self.owner):continue
            name=record['id']
            if Path(name).name!=name:raise api.EditorError('Hibás ideiglenes mentési útvonal.')
            path=self.root/name;stage=review.Stage(path,record['checksum'])
            status=api.strict_json((path/'status.json').read_text(encoding='utf-8'))
            if status.get('state') not in PENDING:continue
            cached=self.cache.get(name)
            if cached is None:
                body,_=stage.read();self.cache[name]=body
            else:body=cached
            state='unconfirmed' if status['state'] in ('approved','publishing') and not self.active else status['state']
            result.append(dict(record,stage=stage,body=body,state=state,created=status.get('created_utc','')))
        return result

    def baseline(self,entries=None):
        sql=[e for e in (self.entries() if entries is None else entries) if e['body']['kind'] in CATALOG_KINDS]
        if not sql:return None
        payload=sql[0]['body']['payload']
        if 'before' in payload:return payload['before']
        ref=payload['before_ref'];key=ref['checksum']
        if key not in self.baselines:
            if not isinstance(key,str) or len(key)!=64 or any(c not in '0123456789abcdef' for c in key):raise api.EditorError('Hibás katalógustervezet.')
            with gzip.open(self.root/'baselines'/(key+'.json.gz'),'rb') as file:raw=file.read(256*1024*1024+1)
            if len(raw)>256*1024*1024 or review.digest(raw)!=key:raise api.EditorError('A tervezet eredeti katalógusa megsérült.')
            self.baselines[key]=api.strict_json(raw.decode('utf-8'))
        return self.baselines[key]

    def store_baseline(self,packet):
        known=self.baseline_refs.get(id(packet))
        if known is not None:return known
        raw=review.encoded(packet);key=review.digest(raw);path=self.root/'baselines'/(key+'.json.gz');path.parent.mkdir(parents=True,exist_ok=True,mode=0o700)
        if not path.exists():review.atomic(path,gzip.compress(raw,compresslevel=1,mtime=0))
        ref=dict(checksum=key);self.baseline_refs[id(packet)]=ref;self.baselines[key]=packet;return ref

    def slot(self,kind,payload):
        if kind=='file':return 'file:'+payload['relative']
        if kind in CATALOG_KINDS:return payload['operation']['slot']
        if kind in ('ssh_profile','ssh_profile_delete'):return 'profile:'+(payload.get('profile') or payload['removed'])['id']
        if kind=='backup-policy':return kind
        if kind=='bench' and payload['action']=='wallet':return 'wallet:'+str(payload['owner'])
        if kind=='server_action':return 'action:'+payload['argv'][0]
        return None

    def put(self,title,kind,payload,rows,files=None):
        self.load();slot=self.slot(kind,payload);scope=review.scope(self.owner)
        if kind in CATALOG_KINDS and 'before' in payload:
            payload=dict(payload);payload['before_ref']=self.store_baseline(payload.pop('before'))
        previous=next((e for e in self.entries() if slot and e.get('slot')==slot),None)
        if previous and previous['state']!='staged':raise api.EditorError('Ennek a módosításnak a korábbi feltöltése bizonytalan. Előbb ellenőrizd a szerver állapotát.')
        if kind=='catalog_edit' and previous and previous['body']['kind']=='catalog_socket_edit':
            kind='catalog_socket_edit'
        if previous and kind=='file':
            original=previous['body']['payload']
            base_encoding=original.get('base_encoding') or payload.get('base_encoding') or original.get('encoding','utf-8')
            payload=dict(payload,expected_hash=original['expected_hash'],
                         base_data=original.get('base_data',payload.get('base_data')),base_encoding=base_encoding)
            if payload.get('base_data'):
                before=base64.b64decode(payload['base_data']).decode(base_encoding)
                after=base64.b64decode(payload['data']).decode(payload.get('encoding','utf-8'))
                rows=review.text_rows(Path(payload['relative']).name,before,after,payload['relative'].endswith('.json'))
            else:
                rows=merged_rows(previous['body']['rows'],rows)
        elif kind in ('ssh_profile','ssh_profile_delete'):
            old=previous['body']['payload'] if previous else payload
            original=old.get('previous',old.get('removed'));profile=payload.get('profile') or payload['removed']
            payload=dict(payload,previous=original);rows=review.dict_rows(profile['name'],original or {},payload.get('profile') or {})
            for role in payload.get('password_fingerprints',{}):rows.append((profile['name'],'SSH-jelszó · '+role,'Korábbi állapot','Új, rejtett jelszó'))
        elif kind=='backup-policy':
            original=(previous['body']['payload'] if previous else payload).get('before_policy')
            if original is not None:payload=dict(payload,before_policy=original);rows=review.dict_rows('Mentési beállítások',original,payload['arguments']['policy'])
        elif previous and kind=='bench' and payload['action']=='wallet':
            old=next((r[2] for r in previous['body']['rows'] if r[1]=='Yang'),None)
            if old is not None:rows=[(previous['body']['rows'][0][0],'Yang',old,str(payload['fields']['gold']))];rows=[r for r in rows if r[2]!=r[3]]
        elif previous and not (kind in CATALOG_KINDS and payload['operation']['type']!='item'):rows=merged_rows(previous['body']['rows'],rows)
        stage=review.Stage.create(self.owner,title,kind,payload,rows,files,root=self.root,persistent=True)
        record=dict(id=stage.path.name,checksum=stage.checksum,scope=scope,slot=slot)
        updated=[e for e in self.index if previous is None or e['id']!=previous['id']]
        if rows:updated.append(record)
        old_index=self.index;self.index=updated
        try:
            if not getattr(self,'_editing',False):self.save_index()
        except Exception:self.index=old_index;raise
        if previous:
            if getattr(self,'_editing',False):self._superseded.append(previous)
            else:previous['stage'].state('superseded');self.cache.pop(previous['id'],None)
        if not rows:stage.state('discarded')
        return stage

    def enqueue(self,title,kind,payload,rows,files=None):
        with self.write_guard():return self._enqueue(title,kind,payload,rows,files)

    def _enqueue(self,title,kind,payload,rows,files=None):
        if self.active:raise api.EditorError('Feltöltés közben nem módosítható a függő csomag.')
        if kind not in SQL_KINDS:return self.put(title,kind,copy.deepcopy(payload),rows,files)
        import item_catalog as catalog
        from reward_catalog import Rewards,target_text
        from content_names import item_name
        before=payload['before'];baseline=self.baseline() or before.get('_pending_base',before)
        operations=[]
        if kind=='item':
            doc=payload['document'];key=[int(doc['base']['category_id']),int(doc['base']['local_id'])]
            operations=[dict(type='item',key=key,document=doc,slot='item:%d:%d'%tuple(key),name=item_name(doc['base']))]
        elif kind=='container':
            key=list(map(int,payload['key']));operations=[dict(type='container',key=key,document=payload['document'],slot='container:%d:%d'%tuple(key),name=item_name(Rewards(before).bases[tuple(key)]))]
        else:
            old=read_model(before)
            if kind=='bulk_rewards':tables=payload['plan']['tables'];containers=payload['plan']['containers']
            elif kind=='mob_drop':tables=drop_model(before).apply_drops(payload['document'],payload['original_id']);containers=None
            elif kind=='mob_drop_delete':tables=drop_model(before).delete_drop_group(payload['group_id']);containers=None
            else:raise api.EditorError('Ismeretlen katalógusmódosítás.')
            after=dict(before);after['live_drops']=dict(before['live_drops'],tables={name:tables[name] for name in ('mob_drop_group','mob_drop_entry')})
            if containers is not None:after['live_drops'].update(containers)
            fresh=read_model(after)
            left={g['group_id']:old.drop_group(g['group_id']) for g in old.drop_groups()};right={g['group_id']:fresh.drop_group(g['group_id']) for g in fresh.drop_groups()}
            for key in dict.fromkeys([*left,*right]):
                if left.get(key)==right.get(key):continue
                group=fresh.row('mob_drop_group',key,'group_id');entries=[r for r in fresh.tables['mob_drop_entry'] if r['group_id']==key]
                operations.append(dict(type='mob',key=key,group=group,entries=entries,slot='mob:'+key,name=target_text(group or old.row('mob_drop_group',key,'group_id'))))
            if containers is not None:
                a,b=Rewards(before),Rewards(after)
                for base in a.containers():
                    key=(int(base['category_id']),int(base['local_id']))
                    if a.loot(key)!=b.loot(key):operations.append(dict(type='container',key=list(key),document=b.loot(key),slot='container:%d:%d'%key,name=item_name(base)))
        target_rows={}
        if operations and all(op['type']=='item' for op in operations):
            combined={e['body']['payload']['operation']['slot']:e['body']['payload']['operation'] for e in self.catalog_entries()}
            combined.update({op['slot']:op for op in operations});compile_catalog(baseline,list(combined.values()))
        if any(op['type']!='item' for op in operations):
            combined={e['body']['payload']['operation']['slot']:e['body']['payload']['operation'] for e in self.catalog_entries()}
            combined.update({op['slot']:op for op in operations})
            item_ops=[op for op in combined.values() if op['type']=='item'];reference=compile_catalog(baseline,item_ops) if item_ops else baseline
            effective=compile_catalog(baseline,list(combined.values()))
            review.reward_rows(reference,effective['live_drops']['tables'],{key:effective['live_drops'][key] for key in ('container_groups','container_entries')},by_target=target_rows,before_model=read_model(reference),after_model=read_model(effective))
        result=None;old_index=self.index;self._editing=True;self._superseded=[]
        # Each destination is replaceable/discardable independently; a bulk save is durable before returning.
        try:
            for op in operations:
                op_rows=rows if op['type']=='item' else target_rows.get((op['type'],tuple(op['key']) if op['type']=='container' else str(op['key'])),[])
                kind='catalog_edit'
                if op['type']=='item' and op['key'][0]==3:
                    document=op['document']
                    profile=document.get('socket_profile') or {}
                    if profile.get('id')!=document['child'].get('socket_profile_id'):
                        kind='catalog_socket_edit'
                result=self.put(('Tárgy · ' if op['type']=='item' else 'Láda · ' if op['type']=='container' else 'Mobdrop · ')+op['name'],
                                kind,dict(before=baseline,operation=op),op_rows)
            self.save_index()
        except Exception:self.index=old_index;self.overlay_cache={};raise
        finally:self._editing=False
        for previous in self._superseded:previous['stage'].state('superseded');self.cache.pop(previous['id'],None)
        if result is None:raise api.EditorError('Nincs ideiglenesen mentendő módosítás.')
        return result

    def catalog_entries(self):return [e for e in self.entries() if e['body']['kind'] in CATALOG_KINDS]

    def overlay_packet(self,packet):
        entries=self.catalog_entries()
        if not entries:return packet.get('_pending_base',packet)
        key=(review.encoded(review.scope(self.owner)),self.revision)
        if key not in self.overlay_cache:
            self.overlay_cache[key]=compile_catalog(self.baseline(entries),[e['body']['payload']['operation'] for e in entries])
        return self.overlay_cache[key]

    def overlay_document(self,document):
        entry=next((e for e in self.entries() if e['body']['kind']=='file' and e['body']['payload']['relative']==document.relative),None)
        if entry is None:return document
        payload=entry['body']['payload'];result=api.Document.from_bytes(document.relative,base64.b64decode(payload['data']),allow_invalid_json=True)
        result.staged_base=api.Document.from_bytes(document.relative,base64.b64decode(payload.get('base_data') or base64.b64encode(document.original)),allow_invalid_json=True)
        result.encoding=payload.get('encoding',result.staged_base.encoding);result.newline=result.staged_base.newline
        result.digest=payload['expected_hash'];result.staged=True;return result

    def discard(self,identities):
        with self.write_guard():return self._discard(identities)

    def _discard(self,identities):
        if self.active:raise api.EditorError('Feltöltés közben nem vonható vissza módosítás.')
        entries=[e for e in self.entries() if e['id'] in identities]
        if any(e['state']!='staged' for e in entries):raise api.EditorError('Bizonytalan feltöltést előbb ellenőrizni kell a szerveren. Nem törölhető függő szerkesztésként.')
        # Verify remaining dependent drafts before removing their prerequisites.
        remaining=[e for e in self.catalog_entries() if e['id'] not in identities]
        if remaining:compile_catalog(self.baseline(remaining),[e['body']['payload']['operation'] for e in remaining])
        old=self.index;self.index=[r for r in self.index if r['id'] not in identities]
        try:self.save_index()
        except Exception:self.index=old;raise
        for entry in entries:entry['stage'].state('discarded');self.cache.pop(entry['id'],None)

    def summary(self,entries):
        result=[]
        for e in entries:result.extend(e['body']['rows'])
        return result

    def rebase_remaining(self,packet):
        entries=self.catalog_entries()
        if not entries:return
        compile_catalog(packet,[e['body']['payload']['operation'] for e in entries])
        for e in entries:
            if e['state']!='staged':continue
            payload=dict(e['body']['payload'],before=packet)
            self.put(e['body']['title'],e['body']['kind'],payload,e['body']['rows'])

    def publish_catalog(self,entries):
        import item_catalog as catalog
        from reward_catalog import Rewards
        before=self.baseline(entries);current=catalog.request(self.owner.root)
        if current['hash']!=before['hash'] or current.get('live_drops',{}).get('release')!=before.get('live_drops',{}).get('release'):
            raise api.EditorError('Az éles katalógus vagy dropverzió megváltozott a tervezés óta. A függő csomag nem tölthető fel erre az állapotra.')
        ops=[e['body']['payload']['operation'] for e in entries];after=compile_catalog(before,ops)
        if any(op['type']=='item' for op in ops) and any(op['type']!='item' for op in ops):
            import secrets
            tables=dict(after['tables']);tables.update(after['live_drops']['tables']);live=before['live_drops']
            publish=dict(expected_release=live['release'],expected_drop_hash=live['hash'],release_id=secrets.token_hex(16),
                **{key:after['live_drops'][key] for key in ('container_groups','container_entries')})
            return catalog.commit_tables(self.owner.root,before,tables,dict(pending_items_and_rewards=len(ops)),batch_publish=publish)
        if any(op['type']=='item' for op in ops):
            current=catalog.commit_tables(self.owner.root,before,after['tables'],dict(pending_items=len(ops)))
        if any(op['type']!='item' for op in ops):
            tables=dict(current['tables']);tables.update(after['live_drops']['tables'])
            current=catalog.commit_drop_tables(self.owner.root,current,tables,dict(pending_destinations=len(ops)),
                containers={key:after['live_drops'][key] for key in ('container_groups','container_entries')})
        return current
