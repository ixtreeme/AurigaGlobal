"""Audited non-bonus effects and costume reset; no legacy runtime dispatch."""
import re
from pathlib import Path
from legacy_item_migration import MigrationError,file_hash
from full_catalog_behaviors import unused_values

def reset_tiers():
    source=(Path(__file__).resolve().parents[2]/'Server/GameServer/core/constants.cpp').read_text(encoding='utf-8')
    result=[]
    for draw,symbol in enumerate(('aiItemMagicAttributePercentHigh','aiItemMagicAttributePercentLow','aiItemMagicAttributePercentLow')):
        match=re.search(r'\b'+symbol+r'\s*\[[^]]+\]\s*=\s*\{([^}]+)\}',source)
        if not match:raise MigrationError('missing_costume_reset_tier_table')
        body=re.sub(r'//[^\n]*|/\*.*?\*/','',match[1],flags=re.S)
        values=[int(v) for v in re.findall(r'-?\d+',body)]
        if len(values)!=5 or any(v<0 for v in values) or sum(values)!=100:raise MigrationError('invalid_costume_reset_tier_table')
        result.extend(dict(draw_no=str(draw),tier=str(tier),chance_bp=str(chance*100)) for tier,chance in enumerate(values,1) if chance)
    return result

def convert_context_services(c,row):
    if row['type']!=3:return False
    sub=row['subtype']
    if 70201<=row['vnum']<=70206:
        if sub!=10:raise MigrationError('original_hair_dye_subtype_changed')
        unused_values(row)
        path=Path(__file__).resolve().parents[2]/'Server/GameServer/ecs/systems/ItemSystem_LegacyBridge.cpp';source=path.read_text(encoding='utf8')
        body=source.split('case 70201:',1)[1].split('case ITEM_NEW_YEAR_GREETING_VNUM:',1)[0]
        for phrase in ('PART_HAIR) < 1001','last_dye_level + 3','GetItemVnum(item) - 70201','dyeing_hair.last_dye_level', 'ConsumeItemEcs(itemEntity);', 'NetworkSyncSystem::UpdatePacket(e);'):
            if phrase not in body:raise MigrationError('original_hair_dye_handler_changed')
        c.rule_inputs['GameServer/ecs/systems/ItemSystem_LegacyBridge.cpp']=file_hash(path)
        doc,icon=c.base(row,5);doc['recovery']=doc['effect']=None
        if doc['bonuses']:raise MigrationError('unexpected_hair_dye_static_bonus')
        doc['character_service']=dict(service_kind='3',units_per_use='1',amount=str(row['vnum']-70200),minimum_value='0',maximum_value='1001')
        c.finish(row,doc,icon,[],'durable_hair_dye');return True
    if row['vnum'] in (27610,71004,76008):
        fish=row['vnum']==27610
        if (fish and sub!=10) or (not fish and (sub!=8 or row['value0']!=511 or row['value3']!=536870911)):
            raise MigrationError('original_timed_utility_prototype_changed')
        unused_values(row,() if fish else (0,3))
        server=Path(__file__).resolve().parents[2]/'Server'
        path=server/'GameServer/ecs/systems/ItemSystem_LegacyBridge.cpp';source=path.read_text(encoding='utf8')
        if fish:
            body=source.split('case fishing::FISH_MIND_PILL_VNUM:',1)[1].split('break;',1)[0]
            if '20 * 60' not in body or 'ConsumeItemEcs(itemEntity);' not in body or 'AffectSystem::FindAffect(e, AFFECT_FISH_MIND_PILL)' not in body:
                raise MigrationError('fish_mind_source_handler_changed')
        else:
            # The earlier USE_AFFECT branch is only for 50801..50820 packages.
            body=source.rsplit('case USE_AFFECT:',1)[1].split('break;',1)[0]
            if 'AffectSystem::FindAffect(e, GetItemValue(item, 0)' not in body or 'AffectSystem::AddAffect(e, GetItemValue(item, 0)' not in body or 'ConsumeItemEcs(itemEntity);' not in body:
                raise MigrationError('no_death_penalty_source_handler_changed')
        c.rule_inputs['GameServer/ecs/systems/ItemSystem_LegacyBridge.cpp']=file_hash(path)
        doc,icon=c.base(row,5);doc['recovery']=doc['effect']=None
        if doc['bonuses']:raise MigrationError('unexpected_timed_utility_bonus')
        doc.update(c.model.timed_defaults(doc['base']['local_id']));doc['timed_bonuses']=[]
        doc['timed'].update(duration_ms='1200000' if fish else '536870911000',utility_kind='1' if fish else '2',allowed_contexts='15',allow_during_pvp='1')
        doc['timed_group'].update(code='effect.original_fish_mind' if fish else 'effect.original_no_death_penalty',reapply_kind='1',clock_kind='1',remove_on_death='1' if fish else '0')
        c.finish(row,doc,icon,[],'timed_fish_prediction' if fish else 'timed_one_shot_death_protection');return True
    if sub==10 and row['vnum']==70014:
        unused_values(row)
        source=(Path(__file__).resolve().parents[2]/'Server/GameServer/ecs/systems/ItemSystem_LegacyBridge.cpp').read_text(encoding='utf-8')
        body=source.split('case ITEM_GIVE_STAT_RESET_COUNT_VNUM:',1)[1].split('break;',1)[0]
        if 'ecs::PointSystem::Change(e, POINT_STAT_RESET_COUNT, 1);' not in body or 'ConsumeItemEcs(itemEntity);' not in body:raise MigrationError('stat_reset_credit_handler_changed')
        doc,icon=c.base(row,5);doc['recovery']=doc['effect']=None
        if doc['bonuses']:raise MigrationError('unexpected_stat_reset_credit_bonus')
        doc['character_service']=dict(service_kind='2',units_per_use='1',amount='1',minimum_value='0',maximum_value='32767')
        c.finish(row,doc,icon,[],'stat_reset_credit_character_service');return True
    if sub==10 and row['vnum']==71109:
        unused_values(row);doc,icon=c.base(row,5);doc['recovery']=doc['effect']=None
        if doc['bonuses']:raise MigrationError('unexpected_extraction_scroll_bonus')
        doc['socket_service']=dict(service_kind='3',units_per_use='1');c.finish(row,doc,icon,[],'extract_last_socket_stone');return True
    if sub==10 and row['vnum'] in (70102,70100,71107,39032,72100,72101):
        identity=row['vnum'];amount,minimum,maximum={70102:(5000,0,2500000),70100:(50000,0,25000000),
            71107:(20000,0,25000000),39032:(20000,0,25000000),72100:(100000,0,25000000),72101:(300000,25000000,50000000)}[identity]
        unused_values(row,(0,1))
        if row['value0']!=(0 if identity==72101 else amount) or row['value1']!=(8 if identity in (71107,39032) else 0):raise MigrationError('alignment_source_values_changed')
        source=(Path(__file__).resolve().parents[2]/'Server/GameServer/ecs/systems/ItemSystem_LegacyBridge.cpp').read_text(encoding='utf-8')
        body=source.split('case '+str(identity)+':',1)[1].split('break;',1)[0]
        if not re.search(r'max_limit\s*=\s*'+str(maximum)+r'\s*;',body) or 'std::min(' not in body or 'CombatSystem::UpdateAlignment(e, real_add)' not in body:raise MigrationError('alignment_source_handler_changed')
        if identity==72101 and (not re.search(r'min_limit\s*=\s*25000000\s*;',body) or not re.search(r'add_value\s*=\s*300000\s*;',body)):raise MigrationError('alignment_source_handler_changed')
        doc,icon=c.base(row,5);doc['recovery']=doc['effect']=None
        if doc['bonuses']:raise MigrationError('unexpected_alignment_static_bonus')
        doc['character_service']=dict(service_kind='1',units_per_use='1',amount=str(amount),minimum_value=str(minimum),maximum_value=str(maximum))
        c.finish(row,doc,icon,[],'alignment_character_service');return True
    if sub==8 and row['value0'] in (512,513):
        header=(Path(__file__).resolve().parents[2]/'Server/GameServer/combat/affect.h').read_text(encoding='utf-8')
        if not all(re.search(r'\b'+name+r'\s*=\s*'+str(value)+r'\b',header) for name,value in [('AFFECT_SKILL_BOOK_BONUS',512),('AFFECT_SKILL_NO_BOOK_DELAY',513)]):raise MigrationError('source_book_aid_effect_contract_changed')
        unused_values(row,(0,3))
        if row['value3'] not in (0,536870911):raise MigrationError('finite_book_aid_lifetime_requires_expiry')
        if row['value3']==0 and row['vnum']!=71094:raise MigrationError('unknown_zero_duration_book_aid')
        doc,icon=c.base(row,5);doc['recovery']=doc['effect']=None
        if doc['bonuses']:raise MigrationError('unexpected_book_aid_static_bonus')
        doc.update(c.model.book_aid_defaults());doc['book_aid']['aid_kind']='1' if row['value0']==512 else '2'
        c.finish(row,doc,icon,[],'one_shot_book_aid')
        if row['value3']==0:c.accepted[row['vnum']]['source_dispatch_repaired']='book_bonus_single_use_instead_of_zero_duration_affect'
        return True
    if sub==32:
        unused_values(row);doc,icon=c.base(row,20)
        if doc['bonuses']:raise MigrationError('unexpected_costume_reset_static_bonus')
        doc['child'].update(operation_kind='3',units='1',parameter='0',bonus_id=None,value_scaled=None)
        doc['special_tiers']=reset_tiers();c.finish(row,doc,icon,[],'costume_normal_bonus_reset');return True
    if sub==9:
        unused_values(row);doc,icon=c.base(row,8)
        if doc['bonuses']:raise MigrationError('unexpected_stone_draw_static_bonus')
        entries=[];absent=[]
        # Source USE_CREATE_STONE draws over all 14 integers, including absent
        # prototypes. AutoGiveItemEcs returns no reward for an absent prototype;
        # the source item is still consumed. Retain this exact distribution.
        for identity in range(28000,28014):
            ref=c.accepted.get(identity)
            if identity in c.source and not ref:raise MigrationError('unresolved_stone_draw_reward:'+str(identity))
            if not ref:absent.append(identity)
            entries.append(dict(entry_no=str(len(entries)+1),reward_kind='1' if ref else '9',item_category_id=str(ref['item']['category_id']) if ref else None,item_local_id=str(ref['item']['local_id']) if ref else None,amount='1' if ref else '0',weight='1',rare_attribute_chance_bp='0',refinement_level=str(ref['refinement_level']) if ref else '0'))
        doc['child'].update(container_kind='1',lock_id='0');doc['loot']=dict(pick_mode='1',entries=entries)
        c.finish(row,doc,icon,[],'uniform_source_stone_draw');c.accepted[row['vnum']]['source_absent_stone_outcomes']=absent;return True
    return False
