"""Preserve a CAPTURED current catalog, or upgrade audited v7, offline only.

Keeps issued keys, counters, profiles and every user content cell. Registry
changes must be additive and existing bonus semantics must match. A native
validation failure never produces an installable candidate or modifies SQL.
"""
import argparse
import copy
import json
from pathlib import Path
import subprocess

from legacy_item_migration import MigrationError, read_checkpoint, strict_json, value_hash


def native(helper, payload):
    result = subprocess.run([str(Path(helper).resolve(strict=True))], input=json.dumps(payload).encode('utf-8'),
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=120)
    replies = [line[len(b'AURIGA_CATALOG_RESULT '):] for line in result.stdout.splitlines() if line.startswith(b'AURIGA_CATALOG_RESULT ')]
    if len(replies) != 1:
        raise MigrationError('Native catalog tool returned no unambiguous result')
    reply = strict_json(replies[0])
    if result.returncode or not reply.get('ok'):
        # No connection/credentials are sent. Structured content rejection only.
        raise MigrationError('Native catalog rejection: ' + json.dumps(reply.get('detail', reply.get('error','unknown'))))
    return reply['result']


def upgrade(tables, blank):
    version = tables.get('protos.catalog_format', [])
    target_format = dict(zip(blank['columns']['catalog_format'], blank['tables']['catalog_format'][0]))
    supported = {7, int(target_format['schema_version'])}
    if int(target_format['schema_version'])>=46:supported.add(45)
    if int(target_format['schema_version'])>=47:supported.add(46)
    if int(target_format['schema_version'])>=48:supported.add(47)
    if int(target_format['schema_version'])>=49:supported.add(48)
    if int(target_format['schema_version'])>=50:supported.add(49)
    if int(target_format['schema_version'])>=51:supported.add(50)
    if int(target_format['schema_version'])>=52:supported.add(51)
    if int(target_format['schema_version'])>=53:supported.add(52)
    if int(target_format['schema_version'])>=54:supported.add(53)
    if int(target_format['schema_version'])>=55:supported.add(54)
    if int(target_format['schema_version'])>=45:supported.add(44)
    if int(target_format['schema_version'])>=44:supported.add(43)
    if int(target_format['schema_version'])>=43:supported.add(42)
    if int(target_format['schema_version'])>=42:supported.add(41)
    if int(target_format['schema_version'])>=41:supported.add(40)
    if int(target_format['schema_version'])>=40:supported.add(39)
    if int(target_format['schema_version'])>=39:supported.add(38)
    if int(target_format['schema_version'])>=38:supported.add(37)
    if int(target_format['schema_version'])>=37:supported.add(36)
    if int(target_format['schema_version'])>=36:supported.add(35)
    if int(target_format['schema_version'])>=31:supported.add(30)
    if int(target_format['schema_version'])>=32:supported.add(31)
    if int(target_format['schema_version'])>=33:supported.add(32)
    if int(target_format['schema_version'])>=34:supported.add(33)
    if int(target_format['schema_version'])>=35:supported.add(34)
    if len(version) != 1 or version[0].get('schema_version') not in supported or version[0].get('canonical_domain') != 'auriga.item_catalog.v' + str(version[0].get('schema_version')):
        raise MigrationError('Only the captured current catalog or audited v7 upgrade is supported')
    candidate = copy.deepcopy(blank)
    registries = {'item_category': 'category_id', 'bonus_definition': 'bonus_id'}
    for table, key in registries.items():
        target = {int(row[candidate['columns'][table].index(key)]): dict(zip(candidate['columns'][table],row))
                  for row in candidate['tables'][table]}
        for old in tables.get('protos.' + table, []):
            new = target.get(int(old[key]))
            if new is None: raise MigrationError('An issued registry identity was removed: '+table)
            for field, value in old.items():
                if field not in new: raise MigrationError('An issued registry field was removed: '+table+'.'+field)
                if field == 'allowed_sources':
                    if int(value) <= 0 or int(value) & ~int(new[field]):
                        raise MigrationError('Existing bonus sources are no longer allowed')
                elif (None if value is None else str(value)) != new[field]:
                    raise MigrationError('Existing registry meaning changed: '+table+'.'+field)
    additions = {'timed_effect_group':{'family_kind':'0'},'timed_effect_profile':{'utility_kind':'0','conflict_bonus_id':None},'item_soul':{'next_category_id':None,'next_local_id':None},'item_costume': {'companion_form_id':None,'body_effect_id':'0','weapon_sword_effect_id':'0','weapon_dagger_effect_id':'0','weapon_bow_effect_id':'0','weapon_two_handed_effect_id':'0','weapon_bell_effect_id':'0','weapon_fan_effect_id':'0','weapon_dagger_alternate_effect_id':'0','weapon_fan_alternate_effect_id':'0'}, 'item_consumable': {'character_service':'0','timed_effect_profile_id': None, 'toggle_effect_profile_id': None, 'socket_service_profile_id': None, 'attribute_service_profile_id': None},
                 'item_container': {'hatch_profile_id': None}, 'item_companion': {'training_profile_id': None},
                 'refinement_level': {'minimum_level': None,'buy_amount': None,'sell_amount': None},
                 'accessory_socket_grade': {'truncate_bonus': '0'},'tool_level':dict(success_category_id=None,success_local_id=None,success_level='0',failure_category_id=None,failure_local_id=None,display_grade='65535'),'item_tool':{'all_water_pools':'0','maximum_uses':'0'},'loot_entry': {'refinement_level': '0'},'attribute_service_profile': {'target_reroll_group':'0'},'item_definition_rules': {'attribute_reroll_group':'1','weapon_effect_alternate':'0','birth_open_stone_sockets':'0','birth_attribute_profile_id':None}}
    counts = {}
    for qualified, rows in sorted(tables.items()):
        if not qualified.startswith('protos.'): continue
        table = qualified[len('protos.'):]
        if table in registries or table in ('catalog_format','item_local_id_sequence'): continue
        if table not in candidate['tables']:
            if rows: raise MigrationError('Nonempty catalog table has no target: '+table)
            continue
        columns = candidate['columns'][table]
        converted = []
        for old in rows:
            if set(old)-set(columns): raise MigrationError('Catalog content columns would be dropped: '+table)
            missing = set(columns)-set(old)
            if missing-set(additions.get(table,{})): raise MigrationError('Catalog column needs an explicit migration: '+table)
            values = dict(additions.get(table,{}), **old)
            converted.append([None if values[name] is None else str(values[name]) for name in columns])
        candidate['tables'][table] = converted
        counts[table] = len(converted)
    for row in tables.get('protos.item_local_id_sequence',[]):
        category = str(row['category_id'])
        if category not in candidate['sequences']: raise MigrationError('Unknown existing category sequence')
        candidate['sequences'][category] = str(row['next_local_id'])
    # Hash is recomputed by the native builder, never copied from the empty seed.
    candidate.pop('hash',None)
    return candidate, counts


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source',required=True,type=Path)
    parser.add_argument('--helper',required=True,type=Path)
    parser.add_argument('--output',required=True,type=Path)
    args=parser.parse_args()
    tables,manifest=read_checkpoint(args.source)
    blank=native(args.helper,dict(action='blank'))
    candidate,counts=upgrade(tables,blank)
    candidate['hash']=native(args.helper,dict(action='validate',tables=candidate['tables']))['hash']
    target_version=int(blank['tables']['catalog_format'][0][blank['columns']['catalog_format'].index('schema_version')])
    output=dict(packet=candidate, source_manifest_hash=value_hash(manifest), preserved_table_counts=counts,
                source_version=tables['protos.catalog_format'][0]['schema_version'],target_version=target_version, validated=True, installable_release=False)
    with args.output.open('x',encoding='utf-8',newline='\n') as stream:
        json.dump(output,stream,ensure_ascii=True,sort_keys=True,indent=2);stream.write('\n')
    print(json.dumps(dict(validated=True,target_version=target_version,preserved_items=counts.get('item_base',0),hash=candidate['hash'])))


if __name__=='__main__':main()
