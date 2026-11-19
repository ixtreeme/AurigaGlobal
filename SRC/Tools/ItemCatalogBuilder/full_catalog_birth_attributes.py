"""Capture source initial magic attribute trials as explicit native policies."""
from pathlib import Path
import re
from legacy_item_migration import MigrationError,value_hash
from full_catalog_context_services import reset_tiers

def bind_birth_attributes(c):
    server=Path(__file__).resolve().parents[2]/'Server'
    source=(server/'GameServer/ecs/systems/ItemAttributeSystem.cpp').read_text(encoding='utf-8')
    body=source.split('bool AlterItemToMagicItem(entt::entity item)',1)[1].split('\nvoid AttrLog(',1)[0]
    if ('case ITEM_WEAPON: second = 20; third = 5; break;' not in body or
        'case ITEM_ARMOR: second = 10; third = GetItemSubType(item) == ARMOR_BODY ? 2 : 1; break;' not in body or
        'second = third = 100;' not in body):raise MigrationError('source_birth_magic_trial_contract_changed')
    defines=set(re.findall(r'^\s*#define\s+(\w+)', '\n'.join((server/p).read_text(encoding='utf-8') for p in ('common/service.h','common/CommonDefines.h')),re.M))
    tiers=reset_tiers();wanted={1:('weapon',2000,500),2:('body',1000,200),3:('armor',1000,100),4:('costume',10000,10000)};policies={}
    bindings={(int(r['category_id']),int(r['local_id'])):r['profile_id'] for r in c.model.tables['item_attribute_binding']}
    normal={r['profile_id'] for r in c.model.tables['attribute_group'] if r['pool_kind']=='1'}
    rules={(int(r['category_id']),int(r['local_id']),int(r['refinement_level'])):r for r in c.model.tables['item_definition_rules']}
    selected=[]
    for ref in c.accepted.values():
        row=c.source[ref['source_id']];key=ref['item']['category_id'],ref['item']['local_id'];kind=None
        if row['type']==1 and key[0]==1:kind=1
        elif row['type']==2 and key[0] in (2,3):kind=2 if row['subtype']==0 else 3
        elif row['type']==28 and key[0]==4 and row['subtype'] in (0,1,4) and 'ENABLE_ATTR_COSTUMES' in defines and (row['subtype']!=4 or 'ENABLE_WEAPON_COSTUME_SYSTEM' in defines):kind=4
        if kind is None:continue
        if bindings.get(key) not in normal:
            if row['magic_pct'] or row['addon_type']:raise MigrationError('source_birth_magic_normal_profile_missing:'+str(ref['source_id']))
            continue # Attribute-free zero-chance content has no initial magic policy.
        selected.append((key,ref['refinement_level'],kind))
    for kind in sorted({r[2] for r in selected}):
        name,second,third=wanted[kind]
        code='catalog.birth_'+name+'_'+value_hash(dict(second=second,third=third,tiers=tiers))[:12]
        profile=next((r for r in c.model.tables['attribute_birth_profile'] if r['code']==code),None)
        if profile is None:
            identity=str(max((int(r['profile_id']) for r in c.model.tables['attribute_birth_profile']),default=0)+1)
            profile=dict(profile_id=identity,code=code,second_chance_bp=str(second),third_chance_bp=str(third));c.model.tables['attribute_birth_profile'].append(profile)
            c.model.tables['attribute_birth_tier'].extend(dict(t,profile_id=identity) for t in tiers)
        policies[kind]=profile['profile_id']
    for key,level,kind in selected:rules[*key,level]['birth_attribute_profile_id']=policies[kind]
    return dict(source_states=len(selected),profiles=len(policies),tiers=len(tiers))
