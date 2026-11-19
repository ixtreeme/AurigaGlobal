"""Original absorption equipment and explicit, per-component donor semantics."""
import json
import re
from pathlib import Path
from legacy_item_migration import MigrationError,value_hash,file_hash
from legacy_equipment_rules import bonus
from full_catalog_behaviors import unused_values
from full_catalog_stoles import stole_appearance
from convert_shop_content import SASH_CATALOG


def source_contract(c):
    if getattr(c,'absorption_source_verified',False):return
    server=Path(__file__).resolve().parents[2]/'Server'
    header=(server/'common/item_length.h').read_text(encoding='utf8')
    expected={'ACCE_GRADE_1_ABS':1,'ACCE_GRADE_2_ABS':5,'ACCE_GRADE_3_ABS':10,
        'ACCE_GRADE_4_ABS_MIN':11,'ACCE_GRADE_4_ABS_MAX':25,'ACCE_GRADE_4_ABS_MAX_COMB':19,'ACCE_GRADE_4_ABS_RANGE':5}
    expected.update({'ACCE_GRADE_'+str(i)+'_PRICE':price for i,price in enumerate((1000000,3000000,5000000,8000000),1)})
    expected.update({'ACCE_COMBINE_GRADE_'+str(i):chance for i,chance in enumerate((100,80,70,60),1)})
    if not all(re.search(r'\b'+name+r'\s*=\s*'+str(value)+r'\s*[,}]',header) for name,value in expected.items()):
        raise MigrationError('absorption_source_constants_changed')
    source=(server/'GameServer/ecs/systems/AcceSystem.cpp').read_text(encoding='utf8')
    if 'number(dwMinAbs + 1, dwMaxAbs)' not in source or 'CopyAllAttrToEcs(pkItemMaterial[0], resultItem)' not in source:
        raise MigrationError('absorption_source_combine_changed')
    for name in ('common/item_length.h','GameServer/ecs/systems/AcceSystem.cpp'):
        c.rule_inputs[name]=file_hash(server/name)
    c.absorption_source_verified=True


def convert_absorption_sash(c,row):
    if row['type']==3 and row['subtype']==45:
        source_contract(c)
        unused_values(row)
        doc,icon=c.base(row,20)
        if doc['bonuses']:raise MigrationError('unexpected_sash_clear_bonus')
        doc['child'].update(operation_kind='7',units='1',parameter='0',bonus_id=None,value_scaled=None)
        c.finish(row,doc,icon,[],'absorption_sash_clear');return True
    if row['type']!=28 or row['subtype']!=3:return False
    source_contract(c)
    unused_values(row,(0,));grade=row['value0']
    if grade not in (1,2,3,4):raise MigrationError('invalid_absorption_sash_grade')
    if c.extra.get(row['vnum']) and any(c.extra[row['vnum']].get('bonus_type'+str(i),0) for i in range(2)):
        raise MigrationError('sash_extra_bonus_requires_explicit_domain_support')
    # The active source ignores the sash's own APPLY rows and instead computes
    # donor contributions. In this captured dataset old APPLY97 is stale drain
    # metadata, not a new native sword-damage bonus.
    doc,icon=c.base(row,19,ignore_bonuses=True)
    catalog=json.loads((c.project/SASH_CATALOG).read_text(encoding='utf-8-sig'))
    visual=next((r for r in catalog['entries'] if r['vnum']==row['vnum']),None)
    if visual is None:raise MigrationError('missing_absorption_sash_model')
    appearance,proof=stole_appearance(c.project,visual)
    doc['appearance']=dict(model_asset=appearance)
    doc['child'].update(regalia_kind='3',rune_slot='0',stole_grade=str(grade),capacity_ms='0',initial_ms='0')
    doc['regalia_rules']=[]
    low,high=((1,1),(5,5),(10,10),(11,19))[grade-1]
    next_low,next_high=((5,5),(10,10),(12,19),(1,5))[grade-1]
    doc['absorption_sash']=dict(grade=str(grade),next_category_id='19',next_local_id=doc['base']['local_id'],
        initial_min=str(low),initial_max=str(high),combine_min=str(next_low),combine_max=str(next_high),
        max_percent='25',success_bp=str((10000,8000,7000,6000)[grade-1]),gold_cost=str((1000000,3000000,5000000,8000000)[grade-1]))
    c.finish(row,doc,icon,[dict(path=SASH_CATALOG,entry_sha256=value_hash(visual)),*proof],'absorption_sash')
    c.accepted[row['vnum']]['source_inactive_own_applies']=[(row['applytype'+str(i)],row['applyvalue'+str(i)]) for i in range(5)]
    c.accepted[row['vnum']]['source_donor_apply_type_repaired']=True
    return True


def bind_absorption_sashes(c):
    profiles=c.model.tables['absorption_sash_profile']
    if not profiles:return dict(sashes=0,donors=0)
    by_key={(int(p['category_id']),int(p['local_id'])):p for p in profiles}
    source_sashes=[]
    for ref in c.accepted.values():
        row=c.source[ref['source_id']]
        if row['type']!=28 or row['subtype']!=3:continue
        key=ref['item']['category_id'],ref['item']['local_id'];p=by_key[key]
        identity=row['vnum'] if row['value0']==4 else row['vnum']+1
        next_ref=c.accepted.get(identity)
        if not next_ref or next_ref['item']['category_id']!=19 or c.source[identity]['value0']!=min(4,row['value0']+1):
            raise MigrationError('unresolved_absorption_sash_next_grade:'+str(identity))
        p['next_local_id']=str(next_ref['item']['local_id']);source_sashes.append(row['vnum'])
    original_profiles={(int(r['category_id']),int(r['local_id']),int(r['refinement_level'])):r for r in c.model.tables['sash_donor_profile']}
    original_components={}
    for r in c.model.tables['sash_donor_component']:
        original_components.setdefault((int(r['category_id']),int(r['local_id']),int(r['refinement_level'])),[]).append(r)
    captured={}
    for ref in c.accepted.values():
        row=c.source[ref['source_id']];category=ref['item']['category_id']
        if not (row['type']==1 and category==1 or row['type']==2 and row['subtype']==0 and category==2):continue
        key=category,ref['item']['local_id'],ref['refinement_level']
        base=dict(category_id=str(key[0]),local_id=str(key[1]),refinement_level=str(key[2]))
        profile=dict(base,physical_max_raw=str((max(row['value3'],row['value4'])+row['value5'])*1000 if category==1 else 0),
            magical_max_raw=str((max(row['value1'],row['value2'])+row['value5'])*1000 if category==1 else 0),
            physical_base_positive=str(int(category==1 and max(row['value3'],row['value4'])>0)),
            magical_base_positive=str(int(category==1 and max(row['value1'],row['value2'])>0)))
        components=[]
        for i in range(5):
            if row['applytype'+str(i)] and row['applyvalue'+str(i)]>0:
                value=bonus(row['applytype'+str(i)],row['applyvalue'+str(i)])
                if value:components.append(dict(base,component_no=str(len(components)),**value))
        if key in captured and captured[key]!=(profile,components):raise MigrationError('ambiguous_native_sash_donor:'+str(key))
        captured[key]=profile,components
    for key,(profile,components) in sorted(captured.items()):
        if key in original_profiles:
            if original_profiles[key]!=profile or sorted(original_components.get(key,[]),key=lambda r:int(r['component_no']))!=components:
                raise MigrationError('preserved_sash_donor_changed:'+str(key))
        else:
            c.model.tables['sash_donor_profile'].append(profile)
            c.model.tables['sash_donor_component'].extend(components)
    return dict(sashes=len(source_sashes),source_ids=sorted(source_sashes),donors=len(captured),
        components=sum(len(v[1]) for v in captured.values()),donor_contract_sha256=value_hash(list(captured.values())),
        donor_apply_type_repaired=True,active_armor_base_absorption=False)
