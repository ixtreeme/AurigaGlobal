"""Stateful original world items; runtime uses native groups/components only."""
import re
from pathlib import Path
from full_catalog_behaviors import unused_values
from legacy_item_migration import MigrationError,file_hash

SERVER=Path(__file__).resolve().parents[2]/'Server'
RECALL={22000,22001,22010,22011,22020,22030,39043}
SUMMON_GATES={22000,22010,22011,22020}

def convert_stateful_utility(c,row):
    source=row['vnum'];kind=row['type'];sub=row['subtype']
    recall=source in RECALL and kind==3 and sub==1
    detector=source in (27989,76006) and kind==3 and sub==10
    campfire=source==27600 and kind==15
    if not (recall or detector or campfire):return False
    path=SERVER/'GameServer/ecs/systems/ItemSystem_LegacyBridge.cpp';text=path.read_text(encoding='utf-8')
    if recall:
        unused_values(row,(0,))
        if row['value0'] not in (1,2) or not all(token in text for token in ('WARP_NEED_LIFE_PERCENT = 30','WARP_MIN_LIFE_PERCENT = 10','const double nDistant = 5000.0','case 301:','case 304:','ProcessRecallItem(e, item);')):raise MigrationError('original_recall_rules_changed')
        doc,icon=c.base(row,10)
        if doc['bonuses']:raise MigrationError('unexpected_recall_static_bonus')
        group='utility.recall_'+('town' if row['value0']==1 else 'memory')+('_restricted' if source in SUMMON_GATES else '')
        doc['child'].update(dispatch_kind='2',group_code=group,allowed_contexts='11',allow_observer='0')
        c.finish(row,doc,icon,[],'original_stateful_recall_scroll')
        c.accepted[source].update(requires_native_recall_component=True,source_summon_gates=source in SUMMON_GATES)
        if source in (22011,22020):c.accepted[source]['source_distance_branch_repaired']='use_saved_memory_destination_instead_of_always_zero_distance'
        return True
    unused_values(row)
    if detector:
        body=text.split('case 27989:',1)[1].split('case 27996:',1)[0]
        if not all(token in body for token in ('GetItemSocket(item, 0) + 1','GetItemSocket(item, 0) >= 6','FFindStone','max < 10000','max < 70000','StoneDetect %u 0 0')):raise MigrationError('original_stone_detector_rules_changed')
        doc,icon=c.base(row,11)
        if doc['bonuses']:raise MigrationError('unexpected_detector_static_bonus')
        doc['base']['max_stack']='1';doc['refinement'].update(max_level='0',profile_kind='1');doc['levels']=[doc['levels'][0]]
        doc['child'].update(tool_kind='5',fishing_pool='1',all_water_pools='0',maximum_uses='6')
        doc['tool_levels']=[c.model.tool_level(0,True)]
        c.finish(row,doc,icon,[],'original_stone_detector');c.accepted[source]['source_use_capacity']=6
        return True
    body=text.split('case ITEM_CAMPFIRE:',1)[1].split('case ITEM_UNIQUE:',1)[0]
    if not all(token in body for token in ('100.0f','ATTR_WATER','"kamp.spawned") < 60','fishing::CAMPFIRE_MOB','PASSES_PER_SEC(40)','campfire == entt::null','ConsumeItemEcs(itemEntity)')):raise MigrationError('original_campfire_rules_changed')
    defines='\n'.join((SERVER/p).read_text(encoding='utf-8') for p in ('common/service.h','common/CommonDefines.h'))
    if not re.search(r'^\s*#define\s+ENABLE_BUG_FIXES\b',defines,re.M):raise MigrationError('original_campfire_cooldown_not_enabled')
    binding=c.project/'Assets/Metin2/AddressableContent/Races/Prefabs/12000.asset'
    if not binding.is_file() or not re.search(r'^  prefab: \{fileID: -?[1-9][0-9]*, guid: [a-f0-9]{32}, type: 3\}$',binding.read_text(encoding='utf-8'),re.M):raise MigrationError('missing_original_campfire_model')
    c.rule_inputs['unity.campfire_original_binding']=file_hash(binding)
    cd=next((r for r in c.model.tables['consumable_cooldown_profile'] if r['code']=='utility.campfire'),None)
    if cd:
        if cd['duration_ms']!='60000':raise MigrationError('issued_original_campfire_cooldown_changed')
    else:c.model.tables['consumable_cooldown_profile'].append(dict(profile_id=c.model.allocate('consumable_cooldown_profile','profile_id'),code='utility.campfire',duration_ms='60000'))
    doc,icon=c.base(row,10)
    if doc['bonuses']:raise MigrationError('unexpected_campfire_static_bonus')
    doc['child'].update(dispatch_kind='2',group_code='utility.campfire',allowed_contexts='15',allow_observer='0')
    c.finish(row,doc,icon,[],'original_stateful_campfire');c.accepted[source].update(source_lifetime_seconds=40,source_reuse_utc_ms=60000,source_creature=12000)
    return True
