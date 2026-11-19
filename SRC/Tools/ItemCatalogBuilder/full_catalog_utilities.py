"""Actual compiled world utility handlers, not inert quest placeholders."""
from pathlib import Path
import re
from legacy_item_migration import MigrationError
from full_catalog_behaviors import unused_values

def convert_utility(c,row):
    if row['type']!=3:return False
    identity=row['vnum'];group=None
    if row['subtype']==12:
        unused_values(row);group='cleanse'
    elif row['subtype']==13 and identity!=70026:
        unused_values(row,(0,1));group='invisibility'
    elif row['subtype']==10:
        group={70008:'white_flag',70057:'courage_combined',71013:'firework_random',50107:'china_firework',50108:'spinning_top',50020:'restore_hp_full',50021:'restore_sp_full',50022:'restore_stamina_full',27996:'self_poison',71092:'polymorph_cancel',70302:'spouse_warp'}.get(identity)
        if 50100<=identity<=50106:group='firework_'+str(identity-50100+1)
        if group:unused_values(row,(0,1) if identity in (70008,70057) else ())
    if group is None:return False
    server=Path(__file__).resolve().parents[2]/'Server'
    source=(server/'GameServer/ecs/systems/ItemSystem_LegacyBridge.cpp').read_text(encoding='utf8')
    if group=='cleanse' and 'AffectSystem::RemoveBadAffects(e);' not in source.split('case USE_CLEAR:',1)[1].split('case USE_INVISIBILITY:',1)[0]:raise MigrationError('source_cleanse_handler_changed')
    if group=='invisibility' and not re.search(r'AFFECT_INVISIBILITY, POINT_NONE, 0, AFF_INVISIBILITY, 300, 0, true',source.split('case USE_INVISIBILITY:',1)[1].split('case USE_POTION_NODELAY:',1)[0]):raise MigrationError('source_invisibility_handler_changed')
    if group=='courage_combined':
        defines=(server/'common/service.h').read_text(encoding='utf8');body=source.split('case 70057:',1)[1].split('case UNIQUE_ITEM_WHITE_FLAG:',1)[0]
        if not all(re.search(r'^\s*#define\s+'+name+r'\b',defines,re.M) for name in ('__EFFETTO_MANTELLO__','ENABLE_AGGREGATE_MONSTER_PLUS_RAZOR93')) or 'CombatSystem::AggregateMonster(e);' not in body or 'CombatSystem::AggregateMonsterPlus(e);' not in body:raise MigrationError('source_combined_courage_handler_changed')
    if group.startswith('firework_') and ('GetItemVnum(item) - 50100 + FLY_FIREWORK1' not in source or 'number(FLY_FIREWORK1, FLY_FIREWORK6)' not in source):raise MigrationError('source_firework_handler_changed')
    if group=='white_flag' and 'CombatSystem::ForgetMyAttacker(e);' not in source:raise MigrationError('source_white_flag_handler_changed')
    if group=='self_poison':
        body=source.split('case 27996:',1)[1].split('break;',1)[0]
        if 'AffectSystem::ApplyPoison(e, entt::null);' not in body or 'ConsumeItemEcs(itemEntity);' not in body:raise MigrationError('source_self_poison_handler_changed')
    if group=='polymorph_cancel':
        body=source.split('case 71092:',1)[1].split('case 30617:',1)[0]
        if not all(token in body for token in ('CombatSystem::GetSelectedTarget(e)','AffectSystem::SetPolymorph(selectedTarget, 0, false)','AffectSystem::RemoveAffect(selectedTarget, AFFECT_POLYMORPH)','AffectSystem::SetPolymorph(e, 0)','AffectSystem::RemoveAffect(e, AFFECT_POLYMORPH)')) or 'ConsumeItem' in body:raise MigrationError('source_reusable_polymorph_cancel_changed')
    if group=='spouse_warp':
        body=source.split('case ITEM_MARRIAGE_RING:',2)[2].split('case UNIQUE_ITEM_CAPE_OF_COURAGE:',1)[0]
        sp=source.split('int64_t CalculateConsumeSP(',1)[1].split('// #define ENABLE_FIREWORK_STUN',1)[0]
        if not all(token in body for token in ('MarriageSystem::State(couple)','CArenaManager::instance().IsArenaMap','CalculateConsumeSP(e)','POINT_SP, -consumeSP','WarpToPID(e, marriage::MarriageSystem::GetOther')) or 'ConsumeItem' in body or 'NEED_WARP_SP_PERCENT = 30' not in sp:raise MigrationError('source_reusable_marriage_ring_changed')
    if group.startswith('restore_'):
        symbol,call={50020:('ITEM_WONSO_BEAN_VNUM','ecs::PointSystem::GetMaxHP(e) - ecs::PlayerRuntime::GetHP(e)'),50021:('ITEM_WONSO_SUGAR_VNUM','ecs::PointSystem::GetMaxSP(e) - ecs::PlayerRuntime::GetSP(e)'),50022:('ITEM_WONSO_FRUIT_VNUM','ecs::PlayerRuntime::GetMaxStamina(e) - ecs::PlayerRuntime::GetStamina(e)')}[identity]
        body=source.split('case '+symbol+':',1)[1].split('break;',1)[0]
        if call not in body or 'ConsumeItemEcs(itemEntity);' not in body:raise MigrationError('source_full_restoration_handler_changed')
    doc,icon=c.base(row,10)
    if doc['bonuses']:raise MigrationError('unexpected_utility_static_bonus')
    doc['child'].update(dispatch_kind='2',group_code='utility.'+group,allowed_contexts='11' if group in ('china_firework','spinning_top') else '15',allow_observer='0')
    c.finish(row,doc,icon,[],'compiled_world_utility')
    if group=='invisibility':c.accepted[identity]['source_fixed_duration_seconds']=300
    if identity in (70008,70057) or group=='invisibility':c.accepted[identity]['source_ignored_utility_values']=[row['value0'],row['value1']]
    return True
