"""Rehearse native passive content conversion with a COMPLETE source ledger.

Offline only. The output deliberately remains non-installable until the other
semantic compilers, instance state and reference closure are complete. Source
identities live only in the private audit ledger, never in the runtime packet.
"""
import argparse
from collections import Counter,defaultdict
import json
from pathlib import Path

from legacy_item_migration import MigrationError,analyze,file_hash,read_checkpoint,strict_json,value_hash
from upgrade_exported_catalog import native,upgrade
from convert_quest_tokens import read_visuals,read_locales,convert as quests
from convert_plain_materials import convert as materials


def ledger(tables,plan,accepted,blocked):
    compiled={r['source_id']:r for r in accepted}
    rejected={r['source_id']:r['reasons'] for r in blocked}
    if len(compiled)!=len(accepted) or len(rejected)!=len(blocked) or compiled.keys() & rejected.keys():
        raise MigrationError('Duplicate or contradictory source compilation')
    source={r['vnum']:r for r in tables['player.item_proto']}
    rows=[]
    for row in plan['mapping']:
        identity=row['source_id']
        if identity not in source:raise MigrationError('Mapping references an absent source')
        found=compiled.get(identity)
        if found and (found['item']!=row['target'] or found['source_row_hash']!=value_hash(source[identity])):
            raise MigrationError('Compiled item differs from the full mapping/source')
        rows.append(dict(source_id=identity,item=row['target'],level=row['refinement_level'],source_row_hash=value_hash(source[identity]),
                         compiled=found is not None,source_status=row['status'],
                         blockers=[] if found else rejected.get(identity,[row['status'] if row['status']!='candidate' else 'semantic_converter_required'])))
    if len(rows)!=len(source) or {r['source_id'] for r in rows}!=set(source) or not set(compiled|rejected)<=set(source):
        raise MigrationError('Incomplete source prototype ledger')
    # Do not mistake content conversion for a converted, playable character.
    # Sockets, affixes, clocks, placement and owner receipts remain separate.
    owners=defaultdict(lambda:dict(instances=0,units=0,missing_prototypes=set(),state_conversion_complete=False))
    source_rows={(table,value_hash(r)):r for table in ('player.item','player.account_mount_inventory','player.guild_renewal_storage') for r in tables.get(table,[])}
    for item in plan['instances']:
        owner=owners[(item['owner_kind'] or 'unknown',item['owner_id'])]
        owner['instances']+=1;owner['units']+=item['count']
        # Resolve the exact captured row, including auxiliary stores and any
        # remapped instance ID; a family's other levels are not this instance.
        identities={source_rows[(item['source_store'],item['source_row_hash'])]['vnum']}
        owner['missing_prototypes'].update(identities-set(compiled))
    owner_rows=[dict(owner_kind=k,owner_id=i,**{**v,'missing_prototypes':sorted(v['missing_prototypes'])}) for (k,i),v in sorted(owners.items())]
    return rows,owner_rows


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('source','mapping','project','helper','output'):p.add_argument('--'+name,required=True,type=Path)
    a=p.parse_args();tables,manifest=read_checkpoint(a.source)
    plan=strict_json(a.mapping.read_text(encoding='utf-8'))
    if plan!=analyze(tables,value_hash(dict(source_manifest=value_hash(manifest))),manifest.get('owner_keys')):
        raise MigrationError('Full source mapping is stale or edited')
    preserved,_=upgrade(tables,native(a.helper,dict(action='blank')))
    native(a.helper,dict(action='validate',tables=preserved['tables']))
    unity=a.project/'Assets/Metin2/GeneratedData/Metin2ItemCatalog.asset'
    visuals=read_visuals(unity);locales,locale_hashes=read_locales(a.project)
    material=materials(tables,plan,preserved,a.project,visuals,locales)
    quest=quests(tables,plan,material['packet'],a.project,visuals,locales)
    packet=quest['packet'];packet['hash']=native(a.helper,dict(action='validate',tables=packet['tables']))['hash']
    accepted=material['accepted']+quest['accepted'];blocked=material['blocked']+quest['blocked']
    prototypes,owners=ledger(tables,plan,accepted,blocked)
    result=dict(packet=packet,accepted=accepted,blocked=blocked,prototype_ledger=prototypes,owner_readiness=owners,
                localization={**material['localization'],**quest['localization']},assets={**material['assets'],**quest['assets']},
                installable_release=False,scope='passive_materials_and_quest_tokens',remaining_gates=plan['remaining_gates'],
                input_hashes=dict(source_manifest=value_hash(manifest),mapping=file_hash(a.mapping),unity_catalog=file_hash(unity),locales=locale_hashes),
                summary=dict(source_prototypes=len(prototypes),compiled_source_rows=len(accepted),pending_source_rows=len(prototypes)-len(accepted),
                             native_items=len(packet['tables']['item_base']),preserved_native_items=len(preserved['tables']['item_base']),
                             source_instances=plan['summary']['source_instances'],converted_instances=0,
                             compiled_categories=dict(sorted(Counter(str(r['item']['category_id']) for r in accepted).items())),
                             material=material['summary'],quest=quest['summary']))
    with a.output.open('x',encoding='utf-8',newline='\n') as f:
        json.dump(result,f,ensure_ascii=True,sort_keys=True,indent=2);f.write('\n')
    print(json.dumps(dict(result['summary'],catalog_hash=packet['hash'],installable_release=False)))


if __name__=='__main__':main()
