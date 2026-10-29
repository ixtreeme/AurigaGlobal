"""Upgrade a CAPTURED v7 catalog to the current native format, offline only.

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
    if len(version) != 1 or version[0].get('schema_version') != 7 or version[0].get('canonical_domain') != 'auriga.item_catalog.v7':
        raise MigrationError('Only the audited captured v7 catalog upgrade is supported')
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
    additions = {'item_consumable': {'timed_effect_profile_id': None, 'toggle_effect_profile_id': None},
                 'item_container': {'hatch_profile_id': None}}
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
                source_version=7,target_version=target_version, validated=True, installable_release=False)
    with args.output.open('x',encoding='utf-8',newline='\n') as stream:
        json.dump(output,stream,ensure_ascii=True,sort_keys=True,indent=2);stream.write('\n')
    print(json.dumps(dict(validated=True,target_version=target_version,preserved_items=counts.get('item_base',0),hash=candidate['hash'])))


if __name__=='__main__':main()
