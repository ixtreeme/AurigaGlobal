"""Import a candidate catalog into a NEW private MariaDB and read it natively.

There is deliberately no --host, --port, --password, existing-database, or
production option. This is an offline migration test, not an editor bypass.
"""
import argparse
import json
from pathlib import Path
import re
import sys

ROOT=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(ROOT/'db/tools'))
from backup_sandbox import Sandbox
from mariadb_client import statements
from legacy_item_migration import MigrationError, file_hash, strict_json
from upgrade_exported_catalog import native

# Cross-table FK insertion order. Source SQL identifiers are never accepted.
INSERT_ORDER = (
    'item_use_policy','item_trade_policy','item_appearance_profile','item_attack_profile',
    'item_ammo_policy','item_resistance_profile','skill_book_profile','skill_book_target','skill_book_step',
    'bonus_profile','bonus_profile_entry','refinement_profile','refinement_level',
    'accessory_socket_profile','accessory_socket_grade','attribute_profile','attribute_group','attribute_candidate','attribute_value','attribute_service_profile','attribute_tier_chance','socket_service_profile','consumable_cooldown_profile',
    'timed_effect_group','timed_effect_profile','toggle_effect_group','toggle_effect_profile',
    'recovery_profile','instant_recovery_profile','reserve_recovery_profile','periodic_recovery_profile','automatic_recovery_profile',
    'loot_profile','item_base','pet_hatch_profile','item_weapon','item_armor','item_material','item_accessory','item_consumable',
    'item_attribute_binding','item_skill_book','item_container','item_container_key','item_currency_token','loot_entry','accessory_socket_material',
    'pet_feed_rule','companion_form','pet_training_profile','pet_age_tier','pet_slot_rule','pet_skill_definition','pet_skill_rank','pet_supply_profile','pet_study_rule','pet_extension_rule','pet_enchant_rule','pet_skin_rule','item_special','growth_pet_profile','growth_pet_level','growth_pet_stage','item_companion','soul_profile','soul_level','item_soul','polymorph_form','polymorph_profile','polymorph_skill_rule','polymorph_mastery_rule','item_polymorph','tool_profile','tool_level','item_tool','item_fishing','item_quest','item_socket_stone','item_costume','item_costume_visual','refinement_recipe','refinement_ingredient',
)
REGISTRIES=('catalog_format','item_category','bonus_definition')


def identifier(name):
    if not re.fullmatch('[a-z][a-z0-9_]*',name):raise MigrationError('Invalid compiled schema identifier')
    return '`'+name+'`'


def literal(value):
    # Offline dump import only. Hex encodes ALL bytes; a source cell can never
    # become SQL syntax. The game/editor continue using prepared statements.
    if value is None:return 'NULL'
    if not isinstance(value,str):raise MigrationError('Candidate cells must be strings or NULL')
    return "CONVERT(X'"+value.encode('utf-8').hex()+"' USING utf8mb4)"


def validate_candidate(candidate,blank):
    if candidate['columns']!=blank['columns'] or set(INSERT_ORDER)|set(REGISTRIES)!=set(blank['tables']):
        raise MigrationError('Candidate columns or compiled table coverage mismatch')
    for table in REGISTRIES:
        if sorted(candidate['tables'][table])!=sorted(blank['tables'][table]):
            raise MigrationError('Candidate registries differ from the native schema')
    if set(candidate['sequences'])!=set(blank['sequences']):raise MigrationError('Incomplete category counters')
    base_columns=blank['columns']['item_base']
    maxima={key:0 for key in candidate['sequences']}
    for row in candidate['tables']['item_base']:
        category=row[base_columns.index('category_id')];local=int(row[base_columns.index('local_id')])
        maxima[category]=max(maxima[category],local)
    for category,value in candidate['sequences'].items():
        if not isinstance(value,str) or not re.fullmatch('[1-9][0-9]*',value) or not maxima[category]<int(value)<=1<<32:
            raise MigrationError('Invalid or reused issued category counter')


def stage(candidate_file,helper,work,binaries):
    candidate_file=Path(candidate_file).resolve(strict=True)
    document=strict_json(candidate_file.read_text(encoding='utf-8'))
    candidate=document['packet']
    blank=native(helper,dict(action='blank'))
    validate_candidate(candidate,blank)
    expected=native(helper,dict(action='validate',tables=candidate['tables']))['hash']
    if expected!=candidate['hash']:raise MigrationError('Candidate hash mismatch')
    sandbox=Sandbox(work,binaries)
    migrations=sorted((ROOT/'db/migrations/item_combat').glob('[0-9][0-9][0-9]_*.sql'))
    if [int(p.name[:3]) for p in migrations]!=list(range(31)):raise MigrationError('Unexpected catalog migration chain')
    try:
        sandbox.start();sandbox.verify()
        for path in migrations:
            for sql in statements(path.read_text(encoding='utf-8-sig')):sandbox.client.query(sql)
        sandbox.client.query('START TRANSACTION')
        try:
            for table in INSERT_ORDER:
                columns=blank['columns'][table];rows=candidate['tables'][table]
                for offset in range(0,len(rows),100):
                    batch=rows[offset:offset+100]
                    if any(len(row)!=len(columns) for row in batch):raise MigrationError('Invalid row width')
                    sandbox.client.query('INSERT INTO protos.'+identifier(table)+' ('+','.join(map(identifier,columns))+') VALUES '+
                                         ','.join('('+','.join(map(literal,row))+')' for row in batch))
            for category,counter in candidate['sequences'].items():
                sandbox.client.query('UPDATE protos.item_local_id_sequence SET next_local_id='+counter+' WHERE category_id='+category)
            sandbox.client.query('COMMIT')
        except BaseException:
            sandbox.client.query('ROLLBACK');raise
        saved=native(helper,dict(action='read',connection=dict(port=sandbox.port,user='root',password=sandbox.password)))
        if saved['hash']!=expected or saved['sequences']!=candidate['sequences']:
            raise MigrationError('Restored catalog content/counters differ from candidate')
        result=dict(validated=True,installed_to_game=False,candidate_sha256=file_hash(candidate_file),catalog_hash=expected,
                    tables={name:len(rows) for name,rows in candidate['tables'].items()},
                    migrations={p.name:file_hash(p) for p in migrations})
        with (Path(work)/'catalog-stage-result.json').open('x',encoding='utf-8') as stream:
            json.dump(result,stream,ensure_ascii=True,sort_keys=True,indent=2);stream.write('\n')
        return result
    finally:
        sandbox.stop()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--candidate',required=True,type=Path)
    parser.add_argument('--helper',required=True,type=Path)
    parser.add_argument('--work',required=True,type=Path)
    parser.add_argument('--mariadb-bin',required=True,type=Path)
    args=parser.parse_args()
    result=stage(args.candidate,args.helper,args.work,args.mariadb_bin)
    print(json.dumps(dict(validated=result['validated'],installed_to_game=False,catalog_hash=result['catalog_hash'],items=result['tables']['item_base'])))


if __name__=='__main__':main()
