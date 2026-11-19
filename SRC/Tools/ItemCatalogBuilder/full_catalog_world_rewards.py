"""Gate existing summon definitions on their actual original creature assets.

Issued native keys remain reserved. Missing source models make the existing
definition unavailable until the teammate merge, never a fabricated creature.
"""
from pathlib import Path
import re
import struct
from legacy_item_migration import MigrationError,file_hash

def bind_world_reward_availability(result,project,groups_path):
    import json
    p=result['packet'];rows=lambda t:[dict(zip(p['columns'][t],r)) for r in p['tables'][t]]
    world={}
    for r in rows('loot_entry'):
        if r['reward_kind'] in ('4','5'):world.setdefault(r['profile_id'],[]).append(r)
    if not world:return dict(checked=0,deferred=[])
    if not groups_path.is_file():raise MigrationError('world_reward_groups_input_required')
    groups={r['vnum']:r for r in json.loads(groups_path.read_text(encoding='utf8'))['groups']}
    # The authoring catalog is deliberately smaller than the per-race runtime
    # assets. Either real source binding is sufficient; never substitute a race.
    text=(project/'Assets/Metin2/GeneratedData/Metin2RaceCatalog.asset').read_text(encoding='utf8')
    authored={int(m[1]) for m in re.finditer(r'^  - race: (\d+)\s*\n    folder: [^\n]*\n    prefab: \{fileID: -?[1-9][0-9]*, guid: [a-f0-9]{32}, type: 3\}',text,re.M)}
    index=project/'Assets/Metin2/Resources/Metin2RacePrefabIndex.asset'
    encoded=re.search(r'^  races: ([0-9a-f]+)$',index.read_text(encoding='utf8'),re.M)
    if not encoded:raise MigrationError('world_reward_runtime_race_index_required')
    data=bytes.fromhex(encoded[1]);indexed=set(struct.unpack('<'+str(len(data)//4)+'I',data))
    references={}
    for r in result['accepted']:references.setdefault((str(r['item']['category_id']),str(r['item']['local_id'])),[]).append(r['source_id'])
    rules=p['tables']['item_definition_rules'];columns=p['columns']['item_definition_rules'];available=columns.index('available');deferred=[];assets={};checked=0
    for box in rows('item_container'):
        if box['container_kind'] not in ('1','2') or box['loot_profile_id'] not in world:continue
        checked+=1;missing=set();members=set()
        for reward in world[box['loot_profile_id']]:
            definition=int(reward['amount'])
            if reward['reward_kind']=='4':members.add(definition)
            elif definition not in groups:raise MigrationError('missing_world_reward_group:'+str(definition))
            else:
                group=groups[definition];members.add(int(group['leader']['vnum']));members.update(int(m['vnum']) for m in group.get('members',[]))
        for race in members:
            if race not in indexed:missing.add(race);continue
            if race in authored:continue
            path=project/'Assets/Metin2/AddressableContent/Races/Prefabs'/f'{race}.asset'
            if not path.is_file():missing.add(race);continue
            binding=path.read_text(encoding='utf8')
            if not re.search(r'^  race: '+str(race)+r'$',binding,re.M) or not re.search(r'^  prefab: \{fileID: -?[1-9][0-9]*, guid: [a-f0-9]{32}, type: 3\}$',binding,re.M):missing.add(race);continue
            assets[str(race)]=file_hash(path)
        if missing:
            key=box['category_id'],box['local_id'];matched=[r for r in rules if tuple(r[:2])==key]
            if not matched:raise MigrationError('world_reward_definition_rules_missing')
            for row in matched:row[available]='0'
            deferred.append(dict(item=dict(category_id=int(key[0]),local_id=int(key[1])),source_ids=references.get(key,[]),missing_creature_assets=sorted(missing),reason='missing_original_world_creature_asset',available=False))
    return dict(checked=checked,usable=checked-len(deferred),deferred=deferred,groups_sha256=file_hash(groups_path),runtime_prefab_index_sha256=file_hash(index),authoring_race_catalog_sha256=file_hash(project/'Assets/Metin2/GeneratedData/Metin2RaceCatalog.asset'),per_race_assets=assets)
