"""Durable temporary saves and an explicit, payload-bound publication review."""
import base64,copy,datetime,difflib,gzip,hashlib,json,os,re,secrets
from pathlib import Path
from PySide6.QtCore import QAbstractTableModel,QModelIndex,Qt
from PySide6.QtWidgets import QAbstractItemView,QDialog,QDialogButtonBox,QHeaderView,QLabel,QLineEdit,QPlainTextEdit,QTableView,QVBoxLayout
import backend as api


def encoded(value):return json.dumps(value,ensure_ascii=True,sort_keys=True,separators=(',',':')).encode('utf-8')
def digest(value):return hashlib.sha256(value).hexdigest()
def now():return datetime.datetime.now(datetime.timezone.utc).isoformat()


def atomic(path,value):
    temporary=path.with_name(path.name+'.'+secrets.token_hex(8)+'.tmp')
    fd=os.open(temporary,os.O_WRONLY|os.O_CREAT|os.O_EXCL,0o600)
    with os.fdopen(fd,'wb') as file:file.write(value);file.flush();os.fsync(file.fileno())
    os.replace(temporary,path)


def target(owner):
    if hasattr(owner,'target_id'):return json.loads(encoded(owner.target_id()))
    return ['local',str(getattr(owner,'root',''))]


def scope(owner):
    profile=getattr(owner,'remote_profile',None)
    if profile:
        import ssh_connection
        return ['remote',profile['id'],ssh_connection.connection_route(profile)]
    return ['local',os.path.normcase(os.path.abspath(str(getattr(owner,'root',''))))]


def queued(owner):return getattr(owner,'_last_queued_stage',None) is not None


def directory(owner):
    override=getattr(owner,'change_review_root',None)
    return Path(override) if override else Path(os.environ.get('LOCALAPPDATA',str(Path.home()/'.local/share')))/'AurigaServerManager'/'temporary-changes'


class Stage:
    def __init__(self,path,checksum):self.path=path;self.checksum=checksum;self.audit_error=None

    @classmethod
    def create(cls,owner,title,kind,payload,rows,files=None,*,root=None,persistent=False):
        root=Path(root) if root is not None else directory(owner);root.mkdir(parents=True,exist_ok=True,mode=0o700)
        path=root/(datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S-')+secrets.token_hex(12));path.mkdir(mode=0o700)
        attachments={}
        for name,source in (files or {}).items():
            if not name or Path(name).name!=name or name in ('.','..'):raise api.EditorError('Hibás ideiglenes fájlnév.')
            folder=path/'files';folder.mkdir(exist_ok=True,mode=0o700)
            hasher=hashlib.sha256()
            with open(source['path'],'rb') as original,(folder/name).open('xb') as copy:
                while chunk:=original.read(1024*1024):copy.write(chunk);hasher.update(chunk)
                copy.flush();os.fsync(copy.fileno())
            if hasher.hexdigest()!=source['sha256']:raise api.EditorError('Az előkészített fájl megváltozott. Készíts új összesítőt.')
            attachments[name]=hasher.hexdigest()
        profile=getattr(owner,'remote_profile',None)
        destination=(profile['name']+' · '+profile.get('remote_root','')) if profile else 'Helyi szerver · '+str(getattr(owner,'root',''))
        body=encoded(dict(version=1,target=target(owner),scope=scope(owner),persistent=persistent,destination=destination,title=title,kind=kind,payload=payload,rows=rows,files=attachments))
        checksum=digest(body);atomic(path/'snapshot.json.gz',gzip.compress(body,compresslevel=1,mtime=0))
        atomic(path/'status.json',encoded(dict(version=1,checksum=checksum,state='staged',created_utc=now())))
        return cls(path,checksum)

    def read(self):
        status=json.loads((self.path/'status.json').read_text(encoding='utf-8'))
        with gzip.open(self.path/'snapshot.json.gz','rb') as file:raw=file.read(256*1024*1024+1)
        if len(raw)>256*1024*1024 or status.get('checksum')!=self.checksum or digest(raw)!=self.checksum:
            raise api.EditorError('Az ideiglenes mentés megváltozott. Készíts új mentési összesítőt.')
        body=api.strict_json(raw.decode('utf-8'))
        if body.get('version')!=1:raise api.EditorError('Ismeretlen ideiglenes mentés.')
        return body,status

    def state(self,value,**fields):
        _,status=self.read();status.update(state=value,updated_utc=now(),**fields);atomic(self.path/'status.json',encoded(status))

    def approve(self,owner):
        body,status=self.read()
        if not self.matches(owner,body):raise api.EditorError('Az összesítő másik szerverhez tartozik. Válaszd ki az eredeti célgépet.')
        if status['state']!='staged':raise api.EditorError('Ez a mentés már nem vár jóváhagyásra.')
        self.state('approved',approved_utc=now())

    def begin(self,owner):
        body,status=self.read()
        if not self.matches(owner,body) or status['state']!='approved':
            raise api.EditorError('A feltöltéshez az adott szerver teljes mentési összesítőjének jóváhagyása szükséges.')
        for name,expected in body.get('files',{}).items():
            hasher=hashlib.sha256()
            with (self.path/'files'/name).open('rb') as file:
                while chunk:=file.read(1024*1024):hasher.update(chunk)
            if hasher.hexdigest()!=expected:raise api.EditorError('Az ideiglenes fájl megváltozott. Készíts új összesítőt.')
        self.state('publishing');return body['payload']

    @staticmethod
    def matches(owner,body):
        return body.get('scope')==scope(owner) if body.get('persistent') else body['target']==target(owner)

    def finish(self,success):
        try:self.state('applied' if success else 'unconfirmed')
        except (OSError,api.EditorError) as error:self.audit_error=str(error)


def publish(stage,owner,writer):
    payload=stage.begin(owner)
    try:result=writer(payload)
    except Exception:stage.finish(False);raise
    stage.finish(True);return result


class ReviewModel(QAbstractTableModel):
    HEADERS=['Érintett elem','Módosítás','Korábbi érték','Új érték']
    def __init__(self,rows,parent=None):super().__init__(parent);self.rows=rows;self.shown=list(range(len(rows)))
    def rowCount(self,parent=QModelIndex()):return 0 if parent.isValid() else len(self.shown)
    def columnCount(self,parent=QModelIndex()):return 4
    def data(self,index,role=Qt.ItemDataRole.DisplayRole):
        if index.isValid() and role in (Qt.ItemDataRole.DisplayRole,Qt.ItemDataRole.ToolTipRole):return self.rows[self.shown[index.row()]][index.column()]
    def headerData(self,section,orientation,role=Qt.ItemDataRole.DisplayRole):
        if role==Qt.ItemDataRole.DisplayRole and orientation==Qt.Orientation.Horizontal:return self.HEADERS[section]
    def filter(self,text):
        needle=text.casefold().strip();self.beginResetModel();self.shown=[i for i,row in enumerate(self.rows) if not needle or needle in ' '.join(row).casefold()];self.endResetModel()


class ChangeReviewDialog(QDialog):
    def __init__(self,owner,stage):
        super().__init__(owner);self.stage=stage;body,_=stage.read();self.setWindowTitle('Mentési összesítő és jóváhagyás');self.resize(1180,760)
        layout=QVBoxLayout(self);title=QLabel(body['title']);title.setObjectName('SectionTitle');title.setWordWrap(True);layout.addWidget(title)
        destination=QLabel('Cél: '+body.get('destination',''));destination.setWordWrap(True);layout.addWidget(destination)
        note=QLabel('Az ideiglenes mentés elkészült. A szerkesztett adatok még nem kerültek élesbe. Nézd át a teljes változáslistát, majd külön hagyd jóvá a feltöltést.')
        note.setWordWrap(True);note.setObjectName('Subtle');layout.addWidget(note)
        self.search=QLineEdit();self.search.setPlaceholderText('Keresés az összesítőben…');layout.addWidget(self.search)
        self.table=QTableView();self.model=ReviewModel(body['rows'],self);self.table.setModel(self.model);self.table.setWordWrap(True);self.table.setAlternatingRowColors(True)
        self.table.setSelectionBehavior(QAbstractItemView.SelectionBehavior.SelectRows);self.table.setSelectionMode(QAbstractItemView.SelectionMode.SingleSelection)
        self.table.verticalHeader().hide();self.table.verticalHeader().setDefaultSectionSize(76);header=self.table.horizontalHeader()
        for column in range(4):header.setSectionResizeMode(column,QHeaderView.ResizeMode.Stretch if column>=2 else QHeaderView.ResizeMode.Interactive)
        self.table.setColumnWidth(0,220);self.table.setColumnWidth(1,160)
        layout.addWidget(self.table,1);self.count=QLabel();self.count.setObjectName('Subtle');layout.addWidget(self.count)
        self.details=QPlainTextEdit();self.details.setReadOnly(True);self.details.setMaximumHeight(120);self.details.setPlaceholderText('Válassz egy sort a teljes változtatás elolvasásához.');layout.addWidget(self.details)
        def selected(current,previous):
            if current.isValid():
                row=body['rows'][self.model.shown[current.row()]]
                self.details.setPlainText(row[0]+' · '+row[1]+'\nKorábban: '+row[2]+'\nUtána: '+row[3])
            else:self.details.clear()
        self.table.selectionModel().currentChanged.connect(selected)
        def filtered(text):self.model.filter(text);self.count.setText('%d látható / %d módosítás · a jóváhagyás a teljes listára vonatkozik.'%(self.model.rowCount(),len(body['rows'])))
        self.search.textChanged.connect(filtered);filtered('')
        buttons=QDialogButtonBox(QDialogButtonBox.StandardButton.Save|QDialogButtonBox.StandardButton.Cancel)
        self.approve_button=buttons.button(QDialogButtonBox.StandardButton.Save);self.approve_button.setText('Jóváhagyás és feltöltés');self.approve_button.setProperty('role','success');self.approve_button.setAutoDefault(False)
        back=buttons.button(QDialogButtonBox.StandardButton.Cancel);back.setText('Vissza a szerkesztéshez');back.setDefault(True)
        buttons.accepted.connect(self.accept);buttons.rejected.connect(self.reject);layout.addWidget(buttons)


def request(owner,title,kind,payload,rows,files=None,*,defer=True):
    owner._last_queued_stage=None
    queue=getattr(owner,'pending_changes',None)
    if defer and queue is not None and not getattr(owner,'_pending_dispatch',False):
        owner._last_queued_stage=queue.enqueue(title,kind,payload,rows,files)
        if hasattr(owner,'statusBar'):owner.statusBar().showMessage('Ideiglenesen mentve. A Függő módosítások oldalon több szerkesztés együtt jóváhagyható.',12000)
        return None
    if getattr(owner,'_change_review_active',False):raise api.EditorError('Előbb zárd be a megnyitott mentési összesítőt.')
    rows=[tuple(str(value) for value in row) for row in rows]
    if not rows:raise api.EditorError('Nincs feltöltendő módosítás.')
    owner._change_review_active=True
    try:
        stage=Stage.create(owner,title,kind,payload,rows,files)
        dialog=ChangeReviewDialog(owner,stage)
        accepted=dialog.exec()==QDialog.DialogCode.Accepted;dialog.deleteLater()
        if not accepted:return None
        stage.approve(owner);return stage
    finally:owner._change_review_active=False


def value_text(value):
    if value is None:return 'Nincs'
    if isinstance(value,bool):return 'Igen' if value else 'Nem'
    return json.dumps(value,ensure_ascii=False) if isinstance(value,(dict,list)) else str(value)


def dict_rows(name,before,after,labels=None):
    rows=[];labels=labels or {}
    def visit(left,right,path=()):
        if left==right:return
        if isinstance(left,dict) or isinstance(right,dict):
            a=left if isinstance(left,dict) else {};b=right if isinstance(right,dict) else {}
            for key in dict.fromkeys([*a,*b]):visit(a.get(key),b.get(key),(*path,str(key)))
        elif isinstance(left,list) or isinstance(right,list):
            a=left if isinstance(left,list) else [];b=right if isinstance(right,list) else []
            for index in range(max(len(a),len(b))):visit(a[index] if index<len(a) else None,b[index] if index<len(b) else None,(*path,'%d. sor'%(index+1)))
        else:
            secret=any(api.secret_field(part) or part.casefold() in ('passphrase','private_key','api_key') for part in path)
            rows.append((name,' · '.join(labels.get(part,part.replace('_',' ')) for part in path),'••••••' if secret and left is not None else value_text(left),
                         '••••••' if secret and right is not None else value_text(right)))
    visit(before,after);return rows


def text_rows(name,before,after,is_json=False,labels=None):
    if is_json:
        try:return dict_rows(name,api.strict_json(before),api.strict_json(after),labels)
        except api.EditorError:pass
    rows=[];left=before.splitlines();right=after.splitlines()
    scrub=lambda text:re.sub(r'(?i)((?:password|passphrase|secret|token|api_key)\s*[=:]\s*)[^\r\n]+',r'\1••••••',text)
    for operation,i,j,k,l in difflib.SequenceMatcher(None,left,right).get_opcodes():
        if operation!='equal':rows.append((name,'Szöveg · %d. sortól'%(i+1),scrub('\n'.join(left[i:j])) or 'Nincs',scrub('\n'.join(right[k:l])) or 'Nincs'))
    return rows


def reward_rows(before,tables,containers=None,*,by_target=None,before_model=None,after_model=None):
    """Names and complete reward entry changes; SQL identities stay internal."""
    from reward_catalog import Rewards,target_text,roll_text
    from content_names import item_name,mob_name
    import item_catalog as catalog
    packet=dict(before);packet['live_drops']=dict(before.get('live_drops',{}),tables={name:tables[name] for name in ('mob_drop_group','mob_drop_entry')})
    if containers is not None:packet['live_drops'].update(containers)
    old=Rewards(before,model=before_model);new=Rewards(packet,model=after_model);rows=[]
    def entries(name,left,right,box,left_weighted,right_weighted):
        def identity(row):return (row.get('reward_kind','1'),row.get('item_category_id' if box else 'category_id'),row.get('item_local_id' if box else 'local_id'),row.get('refinement_level','0'))
        def label(row,model,weighted):
            if row is None:return 'Nincs'
            category,local=(row.get('item_category_id'),row.get('item_local_id')) if box else (row['category_id'],row['local_id'])
            kind=str(row.get('reward_kind','1'))
            reward=item_name(model.bases.get((int(category),int(local)),{})) if kind=='1' else {2:'Yang',3:'Tapasztalat',4:'Szörnyidézés',5:'Szörnycsoport idézése',6:'Lassítás',7:'HP-vesztés',8:'Mérgezés',9:'Nincs jutalom'}.get(int(kind),'Jutalom')
            rate='súly '+str(row['weight']) if weighted else 'esély '+catalog.display(row['weight'] if box else row['chance_ppb'],100 if box else catalog.DROP_CHANCE_SCALE)+'%'
            if kind=='1':
                bonus=catalog.display(row.get('rare_attribute_chance_bp','0'),100)+'%'
                if not int(row.get('rare_attribute_chance_bp') or 0):bonus='nincs külön bónuszolás' if box else 'tárgy alapbeállítása'
                result='%s +%s · %s db · %s · bónusszal létrejövés esélye: %s'%(reward,row.get('refinement_level','0'),row['amount'],rate,bonus)
            else:
                amount=row['amount']
                if kind=='4':amount=mob_name(amount)
                elif kind=='7':amount=catalog.display(amount,100)+'% max HP'
                elif kind in ('8','9'):amount=''
                else:amount+=' '+{'2':'Yang','3':'TP','5':'csoport','6':'pont'}.get(kind,'')
                result=' · '.join(part for part in (reward,amount,rate) if part)
            if not box:result+=' · szint %s–%s'%(row['killer_level_min'],row['killer_level_max'])
            return result
        matcher=difflib.SequenceMatcher(None,[identity(row) for row in left],[identity(row) for row in right],autojunk=False)
        for _,a,b,c,d in matcher.get_opcodes():
            for index in range(max(b-a,d-c)):
                l=left[a+index] if a+index<b else None;r=right[c+index] if c+index<d else None
                ltext=label(l,old,left_weighted);rtext=label(r,new,right_weighted)
                if ltext!=rtext:rows.append((name,'Jutalombejegyzés',ltext,rtext))
    for base in old.containers():
        start=len(rows)
        key=(int(base['category_id']),int(base['local_id']));left=old.loot(key);right=new.loot(key)
        if left==right:continue
        name=item_name(base)
        modes={'1':'Egy súlyozott jutalom','2':'Minden bejegyzés külön esélyt kap'}
        if left['pick_mode']!=right['pick_mode']:rows.append((name,'Sorsolás',modes[left['pick_mode']],modes[right['pick_mode']]))
        entries(name,left['entries'],right['entries'],True,left['pick_mode']=='1',right['pick_mode']=='1')
        if by_target is not None:by_target[('container',key)]=rows[start:]
    old_groups={g['group_id']:g for g in old.model.drop_groups()};new_groups={g['group_id']:g for g in new.model.drop_groups()}
    for key in dict.fromkeys([*old_groups,*new_groups]):
        start=len(rows)
        l=old_groups.get(key);r=new_groups.get(key);name=target_text(r or l)
        if not l or not r:rows.append((name,'Droptábla','Meglévő' if l else 'Nincs','Meglévő' if r else 'Törlés'))
        if not l or not r:
            row=r or l
            rules=catalog.DROP_ROLLS[int(row['roll_kind'])]+' · '+roll_text(row)
            rows.append((name,'Sorsolási szabály',rules if l else 'Nincs',rules if r else 'Törlés'))
        if l and r:
            if l['target_kind']!=r['target_kind'] or l['target_value']!=r['target_value']:rows.append((name,'Drop tulajdonosa',target_text(l),target_text(r)))
            if l['roll_kind']!=r['roll_kind']:rows.append((name,'Sorsolás',catalog.DROP_ROLLS[int(l['roll_kind'])],catalog.DROP_ROLLS[int(r['roll_kind'])]))
            for field,title in (('kills_per_drop','Ölések száma jutalmonként'),('minimum_killer_level','Minimum játékosszint')):
                if l[field]!=r[field]:rows.append((name,title,l[field],r[field]))
        left=old.model.drop_group(key)['entries'] if l else [];right=new.model.drop_group(key)['entries'] if r else []
        entries(name,left,right,False,bool(l and l['roll_kind']=='2'),bool(r and r['roll_kind']=='2'))
        if by_target is not None:by_target[('mob',str(key))]=rows[start:]
    return rows
