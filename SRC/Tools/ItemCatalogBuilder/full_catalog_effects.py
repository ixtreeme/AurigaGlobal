"""Original food bundles and their asymmetric, last-point fish admission rule."""
import re
from pathlib import Path
from legacy_item_migration import MigrationError,file_hash
from legacy_equipment_rules import bonus

def bind_original_fish_conflicts(c):
    children={(int(r['category_id']),int(r['local_id'])):r for r in c.model.tables['item_consumable']}
    profiles={r['profile_id']:r for r in c.model.tables['timed_effect_profile']}
    groups={r['group_id']:r for r in c.model.tables['timed_effect_group']}
    bindings=[];fish_groups=set()
    for ref in c.accepted.values():
        if ref.get('semantics')!='cooked_fish_effect':continue
        row=c.source[ref['source_id']]
        if (row['type'],row['subtype'])!=(3,49):raise MigrationError('issued_fish_source_dispatch_changed')
        applies=[bonus(row['applytype'+str(n)],row['applyvalue'+str(n)]) for n in range(5) if row['applytype'+str(n)]]
        if not applies or not applies[-1] or not int(applies[-1]['value_scaled']):raise MigrationError('invalid_fish_admission_point')
        key=ref['item']['category_id'],ref['item']['local_id'];profile=profiles[children[key]['timed_effect_profile_id']];group=groups[profile['group_id']]
        actual={r['bonus_id']:r['value_scaled'] for r in c.model.tables['bonus_profile_entry'] if r['profile_id']==profile['bonus_profile_id']}
        if actual!={r['bonus_id']:r['value_scaled'] for r in applies}:raise MigrationError('issued_fish_bonus_bundle_changed')
        conflict=applies[-1]['bonus_id']
        if (group['code'],group['reapply_kind'],group['clock_kind'],group['remove_on_death'])!=('catalog.fish_effect_'+conflict,'1','1','1') or profile['duration_ms']!=str(row['value0']*1000):raise MigrationError('issued_fish_policy_changed')
        if profile.get('conflict_bonus_id') not in (None,conflict) or group.get('family_kind','0') not in ('0','1'):raise MigrationError('issued_fish_conflict_changed')
        profile['conflict_bonus_id']=conflict;group['family_kind']='1';fish_groups.add(group['group_id'])
        bindings.append(dict(source_id=row['vnum'],profile=profile['profile_id'],group=group['group_id'],conflict_bonus_id=int(conflict),all_bonuses_unchanged=True,duration_ms=int(profile['duration_ms'])))
    if any(r['group_id'] in fish_groups and r.get('conflict_bonus_id') is None for r in profiles.values()):raise MigrationError('fish_group_contains_unaudited_profile')
    return bindings

def convert_original_food_bundle(c,row):
    if row['vnum'] not in (50183,50216):return False
    if (row['type'],row['subtype'])!=(3,10) or any(row['value'+str(i)] for i in (4,5)):raise MigrationError('original_food_bundle_prototype_changed')
    seconds,move,attack,experience=[row['value'+str(i)] for i in range(4)]
    if seconds<=0 or min(move,attack,experience)<=0:raise MigrationError('invalid_original_food_bundle')
    server=Path(__file__).resolve().parents[2]/'Server';path=server/'GameServer/ecs/systems/ItemSystem_LegacyBridge.cpp';symbols=server/'GameServer/item/unique_item.h';source=path.read_text(encoding='utf8');symbol='ITEM_RAMADAN_CANDY' if row['vnum']==50183 else 'ITEM_NOG_POCKET'
    if not re.search(r'\b'+symbol+r'\s*=\s*'+str(row['vnum'])+r'\b',symbols.read_text(encoding='utf8')):raise MigrationError('original_food_bundle_symbol_changed')
    body=source.split('case '+symbol+':',1)[1].split('case ',1)[0]
    for phrase in ('POINT_MOV_SPEED, moveSpeedPer','POINT_MALL_ATTBONUS, attPer','POINT_MALL_EXPBONUS, expPer','ConsumeItemEcs(itemEntity);','AffectSystem::FindAffect'):
        if phrase not in body:raise MigrationError('original_food_bundle_handler_changed')
    c.rule_inputs['GameServer/ecs/systems/ItemSystem_LegacyBridge.cpp']=file_hash(path);c.rule_inputs['GameServer/item/unique_item.h']=file_hash(symbols)
    for name in ('GameServer/ecs/systems/CombatSystem.cpp','GameServer/ecs/systems/PointSystem.cpp','common/item/PremiumAttack.hpp'):c.rule_inputs[name]=file_hash(server/name)
    doc,icon=c.base(row,5);doc['bonuses']=[];doc['recovery']=doc['effect']=None;doc.update(c.model.timed_defaults(doc['base']['local_id']))
    doc['timed'].update(duration_ms=str(seconds*1000));doc['timed_group'].update(code='effect.festival_candy' if row['vnum']==50183 else 'effect.winter_drink',family_kind='3' if row['vnum']==50183 else '2')
    doc['timed_bonuses']=[bonus(8,move),dict(bonus_id='144',value_scaled=str(attack*100)),bonus(66,experience)]
    c.finish(row,doc,icon,[],'original_food_bonus_bundle');return True
