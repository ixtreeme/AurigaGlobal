"""Server-scoped local item templates; applying one always returns copied rows."""
import copy,os,re,secrets
from pathlib import Path
from types import SimpleNamespace
from PySide6.QtCore import QLockFile
import backend as api
import change_review as review
from content_names import item_name,search_text

MAX_ROWS=256
MAX_WEIGHT=2**32-1
CHANCE_SCALE=10**9
BOX_CHANCE_FACTOR=100000
FIELDS={'category_id':(1,65535),'local_id':(1,2**32-1),'refinement_level':(0,255),'amount':(1,2**32-1),
        'chance_ppb':(1,CHANCE_SCALE),'weight':(1,MAX_WEIGHT),'rare_attribute_chance_bp':(0,10000),
        'killer_level_min':(1,65535),'killer_level_max':(1,65535)}
DEFAULTS=dict(refinement_level='0',amount='1',chance_ppb='10000000',weight='1',rare_attribute_chance_bp='0',killer_level_min='1',killer_level_max='65535')


def owner_of(widget):
    if widget is not None:return getattr(widget,'owner',widget)
    return SimpleNamespace(root='unbound-catalog')


def normalize_entries(rows):
    if not isinstance(rows,list) or not 1<=len(rows)<=MAX_ROWS:raise api.EditorError('A csoport 1–256 tárgyat tartalmazhat.')
    result=[]
    for row in rows:
        if not isinstance(row,dict):raise api.EditorError('Hibás csoportbejegyzés.')
        value={}
        for field,(low,high) in FIELDS.items():
            text=str(row.get(field,DEFAULTS.get(field,'')))
            if not re.fullmatch('[0-9]{1,10}',text) or not low<=int(text)<=high:raise api.EditorError('A csoport egyik értéke hibás: '+field)
            value[field]=str(int(text))
        if int(value['killer_level_min'])>int(value['killer_level_max']):raise api.EditorError('A csoport egyik szintkorlátja fordított.')
        code=row.get('content_code');name=row.get('name','Tárgy')
        if not isinstance(code,str) or not 1<=len(code)<=96 or not isinstance(name,str) or len(name)>512:raise api.EditorError('Hibás tárgyhivatkozás a csoportban.')
        value.update(content_code=code,name=name);result.append(value)
    if sum(int(row['weight']) for row in result)>MAX_WEIGHT:raise api.EditorError('A csoport súlyainak összege túl nagy.')
    return result


def bases_of(model):return {(int(r['category_id']),int(r['local_id'])):r for r in model.items()}


def make_entry(model,key,values=None,bases=None):
    base=(bases if bases is not None else bases_of(model)).get(tuple(map(int,key)))
    if base is None:raise api.EditorError('A csoport egyik tárgya már nem szerepel a katalógusban.')
    row=dict(DEFAULTS);row.update(values or {});row.update(category_id=base['category_id'],local_id=base['local_id'],content_code=base['content_code'],name=item_name(base));return row


def from_keys(model,keys):
    bases=bases_of(model);return [make_entry(model,key.split(':'),bases=bases) for key in keys]


def from_mob(model,rows,weighted=False):
    bases=bases_of(model);return normalize_entries([make_entry(model,(r['category_id'],r['local_id']),dict(r,chance_ppb=DEFAULTS['chance_ppb'] if weighted else r['chance_ppb'],weight=r['weight'] if weighted else '1'),bases) for r in rows])


def from_chest(model,rows,weighted=False):
    bases=bases_of(model);return normalize_entries([make_entry(model,(r['item_category_id'],r['item_local_id']),dict(r,chance_ppb=DEFAULTS['chance_ppb'] if weighted else str(int(r['weight'])*BOX_CHANCE_FACTOR),weight=r['weight'] if weighted else '1'),bases) for r in rows if r['reward_kind']=='1'])


def validate_items(rows,model):
    rows=normalize_entries(rows);bases=bases_of(model)
    for row in rows:
        key=(int(row['category_id']),int(row['local_id']));base=bases.get(key)
        if base is None or base['retired']=='1' or base['content_code']!=row['content_code']:
            raise api.EditorError(row['name']+': a mentett tárgyhivatkozás már nem használható ezen a szerveren.')
        if int(row['amount'])>max(1,int(base['max_stack'])):raise api.EditorError(item_name(base)+': a mentett darabszám meghaladja a megengedettet.')
        if int(row['refinement_level'])>(model.target_levels(key) or 0):raise api.EditorError(item_name(base)+': a mentett fejlesztési szint már nem használható.')
    return rows


def check_capacity(rows, *, weighted, existing_rows, existing_weight):
    if existing_rows+len(rows)>MAX_ROWS:raise api.EditorError('A teljes csoport nem fér el: legfeljebb 256 sor lehet egy tartalomban.')
    if weighted and existing_weight+sum(int(r['weight']) for r in rows)>MAX_WEIGHT:raise api.EditorError('A csoporttal együtt a súlyok összege túl nagy lenne.')


def prepare(rows,model,*,target,weighted,existing_rows=0,existing_weight=0):
    rows=validate_items(rows,model)
    check_capacity(rows, weighted=weighted, existing_rows=existing_rows, existing_weight=existing_weight)
    return destination_entries(rows, target=target, weighted=weighted)


def destination_entries(rows, *, target, weighted):
    """Convert validated template rows to the target's active roll format."""
    result=[]
    for row in rows:
        common={key:row[key] for key in ('refinement_level','amount','rare_attribute_chance_bp')}
        if target=='mob':
            result.append(dict(common,category_id=row['category_id'],local_id=row['local_id'],chance_ppb='0' if weighted else row['chance_ppb'],weight=row['weight'] if weighted else '0',killer_level_min=row['killer_level_min'],killer_level_max=row['killer_level_max']))
        elif target=='box':
            if not weighted and int(row['chance_ppb'])%BOX_CHANCE_FACTOR:raise api.EditorError(row['name']+': a ládában az esély legfeljebb két tizedesjegyű lehet. Igazítsd a csoport esélyét.')
            result.append(dict(common,reward_kind='1',item_category_id=row['category_id'],item_local_id=row['local_id'],weight=row['weight'] if weighted else str(int(row['chance_ppb'])//BOX_CHANCE_FACTOR)))
        else:raise ValueError('Unknown group destination')
    return result


def revision(group):return review.digest(review.encoded(group))


class GroupStore:
    def __init__(self,owner):
        self.owner=owner;self.scope=review.digest(review.encoded(review.scope(owner)))
        override=getattr(owner,'reward_group_root',None) or getattr(owner,'change_review_root',None)
        root=Path(override)/'reward-groups' if override else Path(os.environ.get('LOCALAPPDATA',str(Path.home()/'.local/share')))/'AurigaServerManager/reward-groups'
        self.root=root/self.scope;self.path=self.root/'groups.json'

    def read(self):
        if not self.path.is_file():return dict(version=1,scope=self.scope,groups=[])
        try:
            if self.path.stat().st_size>16*1024*1024:raise ValueError()
            value=api.strict_json(self.path.read_text(encoding='utf-8'))
            if value['version']!=1 or value['scope']!=self.scope or not isinstance(value['groups'],list) or len(value['groups'])>1000:raise ValueError()
            ids=set();names=set()
            for group in value['groups']:
                if not re.fullmatch('[0-9a-f]{32}',group['id']) or group['id'] in ids or not isinstance(group['name'],str) or not 1<=len(group['name'])<=96 or search_text(group['name']) in names:raise ValueError()
                ids.add(group['id']);names.add(search_text(group['name']));group['entries']=normalize_entries(group['entries'])
            return value
        except (ValueError,KeyError,TypeError,OSError):raise api.EditorError('A helyi tárgycsoportok fájlja nem olvasható. Az eredeti fájl megmaradt.') from None

    def groups(self):return copy.deepcopy(sorted(self.read()['groups'],key=lambda group:search_text(group['name'])))

    def get(self,identity):
        group=next((g for g in self.groups() if g['id']==identity),None)
        if group is None:raise api.EditorError('A tárgycsoport már nem található. Frissítsd a csoportlistát.')
        return group

    def save(self,name,entries,*,identity=None,expected=None):
        name=name.strip()
        if not name or len(name)>96:raise api.EditorError('Adj meg legfeljebb 96 karakteres csoportnevet.')
        entries=normalize_entries(entries)
        return self.mutate(identity,expected,name,entries)

    def delete(self,identity,expected):return self.mutate(identity,expected,None,None)

    def mutate(self,identity,expected,name,entries):
        try:return self._mutate(identity,expected,name,entries)
        except OSError as error:raise api.EditorError('A helyi tárgycsoportok mentése nem sikerült: '+str(error)) from None

    def _mutate(self,identity,expected,name,entries):
        self.root.mkdir(parents=True,exist_ok=True,mode=0o700);lock=QLockFile(str(self.root/'groups.lock'))
        if not lock.tryLock(0):raise api.EditorError('Másik ablak menti a tárgycsoportokat. Próbáld újra.')
        try:
            value=self.read();old=next((g for g in value['groups'] if g['id']==identity),None)
            if identity is not None and (old is None or revision(old)!=expected):raise api.EditorError('A csoportot egy másik ablak módosította. Nyisd meg újra a friss változatot.')
            if name is not None and any(g['id']!=identity and search_text(g['name'])==search_text(name) for g in value['groups']):raise api.EditorError('Ilyen nevű tárgycsoport már létezik.')
            if old is None and len(value['groups'])>=1000:raise api.EditorError('Legfeljebb 1000 helyi tárgycsoport hozható létre.')
            result=None
            if name is not None:result=dict(id=identity or secrets.token_hex(16),name=name,entries=copy.deepcopy(entries),updated_utc=review.now())
            value['groups']=[g for g in value['groups'] if g['id']!=identity]
            if result:value['groups'].append(result)
            if self.path.is_file():
                history=self.root/'history';history.mkdir(exist_ok=True,mode=0o700);review.atomic(history/(secrets.token_hex(16)+'.json'),self.path.read_bytes())
            review.atomic(self.path,review.encoded(value));return copy.deepcopy(result)
        finally:lock.unlock()
