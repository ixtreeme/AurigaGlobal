"""Faithful local gear clones plus explicitly labelled state edge cases.

Never edits the source owner. The caller supplies a NEW dedicated owner and
unused legacy IDs while holding the owned, stopped package's supervisor lock.
"""
import copy,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'SRC/Tools/ItemCatalogBuilder'))
from convert_legacy_equipment import convert_instances
from legacy_equipment_rules import APPLY

def clone_rows(tables,compiled,owner,first):
    proto={r['vnum']:r for r in tables['player.item_proto']}
    originals=[r for r in tables['player.item'] if r['owner_id']==3 and proto[r['vnum']]['type'] in (1,2,10)]
    if len(originals)!=52:raise ValueError('Source gear inventory changed')
    rows=copy.deepcopy(originals)
    weapon=next(r for r in originals if r['window']=='EQUIPMENT' and r['pos']==4)
    wrist=next(r for r in originals if r['window']=='EQUIPMENT' and r['pos']==3)
    edge=copy.deepcopy(weapon);edge.update(window='EXTRA_INVENTORY',pos=2010,lockedattr=2,socket0=1,socket1=28960,socket2=0)
    packet=compiled['packet'];native={name:[dict(zip(packet['columns'][name],r)) for r in values] for name,values in packet['tables'].items()}
    target=next(r['item'] for r in compiled['accepted'] if r['source_id']==weapon['vnum'])
    binding=next(r['profile_id'] for r in native['item_attribute_binding'] if int(r['category_id'])==target['category_id'] and int(r['local_id'])==target['local_id'])
    selected=[];rare=set()
    for pool,count in ((1,5),(2,2),(3,1)):
        candidates=[r for r in native['attribute_candidate'] if r['profile_id']==binding and int(r['pool_kind'])==pool and int(r['weight'])>0 and (pool==1 or r['bonus_id'] not in rare)]
        for candidate in candidates[:count]:
            identity=int(candidate['bonus_id']);source,scale=next((source,scale) for source,(bonus,scale) in APPLY.items() if bonus==identity)
            value=next(int(r['value_scaled']) for r in native['attribute_value'] if r['profile_id']==binding and r['pool_kind']==str(pool) and int(r['bonus_id'])==identity and r['tier']=='1')
            if value%scale:raise ValueError('Nonintegral source attribute')
            selected.append((source,value//scale))
            if pool!=1:rare.add(candidate['bonus_id'])
    if len(selected)!=8:raise ValueError('Eight source slots unavailable')
    for i,(kind,value) in enumerate(selected):edge[f'attrtype{i}']=kind;edge[f'attrvalue{i}']=value
    timed=copy.deepcopy(wrist);timed.update(window='EXTRA_INVENTORY',pos=2011,socket0=2,socket1=3,socket2=700)
    permanent=copy.deepcopy(wrist);permanent.update(window='EXTRA_INVENTORY',pos=2012,socket0=3,socket1=3,socket2=86410)
    rows.extend((edge,timed,permanent))
    if first+len(rows)>=1<<32:raise ValueError('Legacy ID bound')
    for i,row in enumerate(rows):row.update(id=first+i,owner_id=owner)
    return rows

def fingerprint(item):
    key=lambda k:str(k['category_id'])+':'+str(k['local_id']) if k else '0:0'
    pos=item['placement'];a=item['accessory'];attributes=','.join(f"{i}:{b['bonus_id']}:{b['value_scaled']}" for i,b in enumerate(item['attributes']) if b)
    stones=','.join(f"{s['state']}:{s['grade']}:{s['level']}:{key(s['stone'])}" for s in item['stones'])
    accessory=f"{a['capacity']}:{a['grade']}:{a['remaining_ms']}:{int(a['permanent'])}" if a else '-'
    return f"{item['instance_id']}|{key(item)}|{item['refinement_level']}|{item['count']}|{pos['kind']}:{pos['storage_id']}:{pos['anchor']}|{'' if item['locked_slot'] is None else item['locked_slot']}|{attributes}|{stones}|{accessory}"
