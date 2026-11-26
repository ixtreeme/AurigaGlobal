"""Audited original pet table -> native skill definitions and study supplies."""
from pathlib import Path
import json
from full_catalog_behaviors import unused_values
from legacy_item_migration import MigrationError,file_hash
SERVER=Path(__file__).resolve().parents[2]/'Server'
# Source APPLY values are checked only here. Runtime uses named native effects.
SKILLS=((78,19,'warrior_resistance'),(80,20,'sura_resistance'),(79,21,'ninja_resistance'),(81,22,'shaman_resistance'),
 (120,23,'pve_critical_chance'),(116,24,'stone_damage'),(117,25,'boss_damage'),(66,26,'experience'),
 (68,27,'gold_drop'),(64,28,'attack_bonus'),(27,29,'melee_block'),(39,30,'melee_reflection'))
def original_table(c):
    # Offline migration evidence survives retirement of the executable legacy motor.
    # These are source data, never a second runtime progression or skill implementation.
    contract=Path(__file__).with_name('original_pet_study_contract.json')
    try:
        data=json.loads(contract.read_text(encoding='utf-8'))
        rows=data['rows']
        if (data['schema']!=1 or data['max_rank']!=10 or data['known_success_bp']!=7100 or
            data['known_cooldown_ms']!=10800000 or len(rows)!=12 or
            any(row!=[n+1,SKILLS[n][0]]+[rank*(2 if n in (7,8) else 1) for rank in range(1,21)]
                for n,row in enumerate(rows)) or
            any(len(data[key])!=64 or any(ch not in '0123456789abcdef' for ch in data[key])
                for key in ('constants_sha256','runtime_sha256'))):
            raise ValueError('changed contract')
    except (OSError,ValueError,KeyError,TypeError) as error:
        raise MigrationError('original_pet_study_contract_changed') from error
    c.rule_inputs['native_pet_character_projection']=file_hash(SERVER/'GameServer/item/TypedEquipment.cpp')
    c.rule_inputs['native_pet_character_properties']=file_hash(SERVER/'common/item/PetLifecycle.hpp')
    c.rule_inputs['original_pet_skill_table']=data['constants_sha256']
    c.rule_inputs['original_pet_study_runtime']=data['runtime_sha256']
    c.rule_inputs['original_pet_study_contract']=file_hash(contract)
    return rows

def skill_ids(c):
    definitions=c.model.tables['pet_skill_definition'];next_id=max((int(r['skill_id']) for r in definitions),default=0)+1;out={}
    for index,(_,_,name) in enumerate(SKILLS,1):
        code='pet.original_'+name;existing={int(r['skill_id']) for r in definitions if r['code']==code}
        if len(existing)>1:raise MigrationError('original_pet_skill_identity_conflict:'+name)
        if existing:out[index]=next(iter(existing))
        else:out[index]=next_id;next_id+=1
    return out

def convert_pet_book(c,row):
    if row['type']!=37 or row['vnum'] not in range(55010,55022):return False
    unused_values(row,(0,));index=row['value0']
    if not 1<=index<=12 or index!=row['vnum']-55009:raise MigrationError('invalid_original_pet_book_skill')
    original_table(c)
    if not c.model.tables['pet_training_profile']:raise MigrationError('missing_native_growth_pet_training_profile')
    doc,icon=c.base(row,17)
    if doc['bonuses']:raise MigrationError('unexpected_original_pet_book_bonus')
    identity=skill_ids(c)[index];doc['supply']['supply_kind']='1'
    doc['pet_study'].update(skill_id=str(identity),selected_allowed='1',known_allowed='1',known_success_bp='7100',known_cooldown_ms='10800000')
    c.finish(row,doc,icon,[],'original_growth_pet_skill_book')
    c.accepted[row['vnum']].update(native_pet_skill=identity,native_pet_effect=SKILLS[index-1][1],source_pet_skill=index,
        native_pet_bonus={1: 105, 2: 107, 3: 106, 4: 108, 5: 139, 6: 135, 7: 136, 8: 94, 9: 96, 10: 92, 11: 57, 12: 69}[index],native_pet_step=original_table(c)[index-1][2]*100,
        original_table_index_repaired='apply_column1_rank_column1_plus_rank',source_max_rank=10)
    return True

def bind_original_pet_skills(c):
    refs=[r for r in c.accepted.values() if r.get('semantics')=='original_growth_pet_skill_book']
    if not refs:return dict(profiles=0,skills=0,ranks=0)
    table=original_table(c);ids=skill_ids(c)
    for ref in refs:
        if ref['native_pet_skill']!=ids[ref['source_pet_skill']]:raise MigrationError('original_pet_book_identity_changed')
    profiles=c.model.tables['pet_training_profile'];definitions=c.model.tables['pet_skill_definition'];ranks=c.model.tables['pet_skill_rank']
    for profile in profiles:
        pid=profile['profile_id']
        for index,(_,effect,name) in enumerate(SKILLS,1):
            definition=dict(profile_id=pid,skill_id=str(ids[index]),code='pet.original_'+name,effect_kind=str(effect))
            before=next((r for r in definitions if r['profile_id']==pid and int(r['skill_id'])==ids[index]),None)
            expected=[dict(profile_id=pid,skill_id=str(ids[index]),skill_rank=str(rank),value_scaled=str(table[index-1][rank+1]*100)) for rank in range(1,11)]
            if before:
                actual=[r for r in ranks if r['profile_id']==pid and int(r['skill_id'])==ids[index]]
                if before!=definition or sorted(actual,key=lambda r:int(r['skill_rank']))!=expected:raise MigrationError('issued_original_pet_skill_changed:'+name)
            else:definitions.append(definition);ranks.extend(expected)
    return dict(profiles=len(profiles),skills=len(profiles)*12,ranks=len(profiles)*120)
