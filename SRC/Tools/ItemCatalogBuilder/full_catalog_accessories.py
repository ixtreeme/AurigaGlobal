"""Offline port of ItemSystem::CanPutInto/CanPutInto2 and socket terms.

Source identities are used only while compiling native compatibility sets.
The game's runtime receives native material keys and explicit grade rules.
"""
from legacy_item_migration import MigrationError
from pathlib import Path
import re

TIMED_SPECIAL={
    50634:((14220,14233),(16220,16233),(17220,17233)),
    50640:((14580,14589),(15010,15013),(16580,16593),(17570,17583)),
    50641:((8210,8223),(8250,8263),(8270,8283)),
    50645:((8780,8789),(8760,8769),(8790,8799)),
    50646:((8730,8739),(8700,8709),(8780,8789)),
    50643:((1740,1753),(1780,1793),(1800,1813)),
}
PERMANENT_SPECIAL={50684:TIMED_SPECIAL[50634],50690:TIMED_SPECIAL[50640],50642:TIMED_SPECIAL[50641],50644:TIMED_SPECIAL[50643]}
TIMED_JEWELS=((50634,14220,16220,17220),(50635,14500,16500,17500),(50636,14520,16520,17520),
    (50637,14540,16540,17540),(50638,14560,16560,17560),(50639,14570,16570,17570),
    (50641,8210,8250,8270),(50645,8780,8760,8790),(50643,1740,1780,1800),(50646,8730,8720,8730))
PERMANENT_JEWELS=((50684,14220,16220,17220),(50685,14500,16500,17500),(50686,14520,16520,17520),
    (50687,14540,16540,17540),(50688,14560,16560,17560),(50689,14570,16570,17570),
    (50642,8210,8250,8270),(50644,1740,1780,1800))

def source_compatible(material,target,permanent=False):
    # Rings explicitly return false in CanPutIntoRing. Belts have no ore slots.
    if target['type']!=2:return False
    identity=target['vnum'];sub=target['subtype']
    special=PERMANENT_SPECIAL if permanent else TIMED_SPECIAL
    if material in special:return any(a<=identity<=b for a,b in special[material])
    column={3:1,5:2,6:3}.get(sub)
    if column is None:return False
    family=identity//10*10
    for jewel in PERMANENT_JEWELS if permanent else TIMED_JEWELS:
        if jewel[column]==family:return material==jewel[0]
    # Preserve the source's uint32 subtraction, including unusual prototypes.
    relative=(identity-{3:14000,5:16000,6:17000}[sub])&0xffffffff
    kind=relative//20;start=50673 if permanent else 50623
    if kind>11:kind=((relative-170)&0xffffffff)//20
    elif 16210<=identity<=16219:return material==start+2
    elif 16230<=identity<=16239:return material==start+3
    return material==start+kind

def bind_source_accessories(c):
    server=Path(__file__).resolve().parents[2]/'Server'
    flags='\n'.join((server/p).read_text(encoding='utf-8') for p in ('common/service.h','common/CommonDefines.h'))
    defines=set(re.findall(r'^\s*#define\s+(\w+)',flags,re.M))
    unlock_chance=5000 if 'ENABLE_ADDSTONE_FAILURE' in defines else 10000
    original={tuple(r[:2]) for r in c.original['tables']['item_base']}
    refs={}
    for ref in c.accepted.values():
        row=c.source[ref['source_id']];key=(str(ref['item']['category_id']),str(ref['item']['local_id']))
        if row['type']==2 and row['subtype'] in (3,5,6) and key not in original:refs.setdefault(key,[]).append(row)
    materials=[r for r in c.source.values() if r['type']==3 and r['subtype']==21 and r['vnum'] in c.accepted]
    unlock=[r for r in c.source.values() if r['type']==3 and r['subtype']==20 and r['vnum'] in c.accepted]
    native=lambda row:(str(c.accepted[row['vnum']]['item']['category_id']),str(c.accepted[row['vnum']]['item']['local_id']))
    profiles={};bindings=0
    for key,states in sorted(refs.items()):
        sets=[]
        for state in states:
            timed=[];permanent=[]
            for material in materials:
                if source_compatible(material['vnum'],state):timed.append(native(material))
                elif 'ENABLE_INFINITE_RAFINES' in defines and source_compatible(material['vnum'],state,True):
                    (permanent if material['value0']==1 else timed).append(native(material))
            sets.append((tuple(sorted(native(m) for m in unlock)),tuple(sorted(timed)),tuple(sorted(permanent))))
        if len(set(sets))!=1:raise MigrationError('native_accessory_family_changes_material_compatibility')
        signature=sets[0]
        if signature not in profiles:
            identity=str(max((int(r['profile_id']) for r in c.model.tables['accessory_socket_profile']),default=0)+1)
            c.model.tables['accessory_socket_profile'].append(dict(profile_id=identity,code='catalog.accessory_socket_'+identity,max_grade='3',unlock_units_per_attempt='1',charge_units_per_attempt='1'))
            for grade,(seconds,pct) in enumerate(zip((0,86400,43200,21600),(0,1000,2000,4000))):
                c.model.tables['accessory_socket_grade'].append(dict(profile_id=identity,grade=str(grade),duration_ms=str(seconds*1000),bonus_increase_bp=str(pct),minimum_raw=str(grade*1000),minimum_bp=str(grade*100),minimum_integer=str(grade),unlock_chance_bp=str(unlock_chance) if grade<3 and signature[0] else '0',charge_chance_bp='10000' if grade<3 and (signature[1] or signature[2]) else '0',truncate_bonus='1'))
            for operation,keys in enumerate(signature,1):
                for category,local in keys:c.model.tables['accessory_socket_material'].append(dict(profile_id=identity,operation_kind=str(operation),category_id=category,local_id=local))
            profiles[signature]=identity
        for child in c.model.tables['item_accessory']:
            if (child['category_id'],child['local_id'])==key:child['socket_profile_id']=profiles[signature];bindings+=1
    return dict(new_profiles=len(profiles),new_accessory_bindings=bindings,source_unlock_chance_bp=unlock_chance,source_charge_chance_bp=10000)
