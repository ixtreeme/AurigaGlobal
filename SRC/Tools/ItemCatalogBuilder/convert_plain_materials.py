"""Compile plain legacy materials into native catalogue documents, offline only.

Every source column is either mapped explicitly or required to be zero. This
rehearsal preserves the existing native catalogue and emits no live SQL writer
or runtime legacy-ID lookup. Unknown semantics remain visible release blockers.
"""
import argparse
from collections import Counter
import copy
import json
from pathlib import Path

from legacy_item_migration import (MigrationError, NAME_FIELDS, PROTO_FIELDS, analyze,
                                   file_hash, read_checkpoint, strict_json, value_hash)
from convert_quest_tokens import (Catalog, SOURCE_STACK_LIMIT, read_visuals, read_locales,
                                 translation_for, verified_asset)
from upgrade_exported_catalog import native, upgrade

STACK = 1 << 2
SAVE = 1 << 1  # all native persistent items are saved, independent of a source flag
LOG = 1 << 13  # native mutations always have durable operation receipts
SUPPORTED_ANTI = 0x3f | (1 << 7) | (1 << 8) | (1 << 13)


def material_errors(row, extra_ids):
    if set(row) != PROTO_FIELDS:
        return ['unexpected_source_columns']
    errors = []
    if not row['vnum']: errors.append('zero_placeholder')
    if row['type'] != 5 or not 0 <= row['subtype'] <= 4:
        errors.append('not_plain_material')
    mapped = set(NAME_FIELDS) | {'vnum','type','subtype','size','flag','antiflag',
                                'gold','shop_buy_price','limittype0','limitvalue0','limittype1','limitvalue1'}
    for field in sorted(PROTO_FIELDS - mapped):
        if row[field] != 0: errors.append('unmapped_nonzero:' + field)
    if row['flag'] < 0 or row['flag'] & ~(SAVE | STACK | LOG): errors.append('unmapped_item_flags')
    if row['antiflag'] < 0 or row['antiflag'] & ~SUPPORTED_ANTI: errors.append('unmapped_anti_flags')
    if row['antiflag'] & 0x3c == 0x3c: errors.append('all_classes_forbidden')
    if row['antiflag'] & 3 == 3: errors.append('all_sexes_forbidden')
    if not 1 <= row['size'] <= 3: errors.append('invalid_inventory_height')
    for field in ('gold','shop_buy_price'):
        if not 0 <= row[field] <= (1 << 63)-1: errors.append('invalid_price:' + field)
    limits = []
    for i in range(2):
        kind, value = row['limittype'+str(i)], row['limitvalue'+str(i)]
        if kind == 1 and 0 <= value <= 65535: limits.append(value)
        elif kind or value: errors.append('unmapped_limit:' + str(i))
    if len(limits) > 1: errors.append('duplicate_level_limit')
    if row['vnum'] in extra_ids: errors.append('extra_proto_conversion_required')
    if not row['locale_name_hu'] and not row['locale_name_en']: errors.append('missing_display_name')
    if any('\ufffd' in row[n] or any(ord(c) < 32 for c in row[n]) for n in NAME_FIELDS):
        errors.append('invalid_source_name')
    return sorted(set(errors))


def convert(tables, plan, preserved, project, visuals, locales):
    sources = tables['player.item_proto']
    mapping = {r['source_id']:r for r in plan['mapping']}
    if len(mapping) != len(plan['mapping']) or set(mapping) != {r['vnum'] for r in sources} or len(mapping) != len(sources):
        raise MigrationError('Incomplete/duplicate full mapping')
    extra_ids = {r['vnum'] for r in tables.get('player.item_extra_proto', [])}
    packet = copy.deepcopy(preserved); model = Catalog(packet)
    accepted, blocked, localization, assets = [], [], {}, {}
    for row in sorted(sources,key=lambda r:r['vnum']):
        if row['type'] != 5: continue
        ref = mapping[row['vnum']]; errors = material_errors(row,extra_ids)
        if ref['status'] != 'candidate' or ref['refinement_level'] != 0 or not ref['target'] or ref['target']['category_id'] != 6:
            errors.append('incompatible_family')
        visual = visuals.get(row['vnum'])
        if not errors:
            try:
                if not visual or not visual.get('iconPath','').startswith('icon/item/') or not visual['iconPath'].endswith('.tga'):
                    raise MigrationError('unsupported_icon_path')
                icon = verified_asset(project,'Assets/Metin2/Generated/ItemIcons/'+visual['iconPath'],visual.get('icon'))
            except MigrationError as error: errors.append(str(error))
        if errors:
            blocked.append(dict(source_id=row['vnum'],source_row_hash=value_hash(row),reasons=sorted(set(errors))))
            continue
        key,code = ref['target'],ref['content_code']
        document = model.new(6); base = document['base']
        base.update(local_id=str(key['local_id']),content_code=code,name_key='item.'+code+'.name',description_key='item.'+code+'.description',
                    icon_asset=visual['iconPath'],inventory_height=str(row['size']),
                    max_stack=str(SOURCE_STACK_LIMIT if row['flag'] & STACK else 1),
                    min_level=str(max((row['limitvalue'+str(i)] for i in range(2) if row['limittype'+str(i)]==1),default=0)),
                    buy_amount=str(row['gold']),sell_amount=str(row['shop_buy_price']))
        document['child'].update(local_id=base['local_id'],material_kind=str(row['subtype']+1))
        document['use'].update(allowed_classes=str(15 & ~(row['antiflag'] >> 2)),
                               allowed_sexes=str(3 & ~((row['antiflag'] & 1)<<1 | (row['antiflag'] & 2)>>1)))
        document['trade'].update(allow_trade=str(int(not row['antiflag'] & (1<<13))),
                                 allow_drop=str(int(not row['antiflag'] & (1<<7))),allow_sell=str(int(not row['antiflag'] & (1<<8))))
        model.apply(document,encode_result=False)
        localization[code] = translation_for(row,code,locales)
        assets[code] = dict(icon=icon)
        accepted.append(dict(source_id=row['vnum'],item=key,refinement_level=0,source_row_hash=value_hash(row),semantics='plain_material',
                             infrastructure_flags=dict(save='native_persistence',log='durable_operation_receipt'),reviewed=False))
    packet['tables'] = model.encode()
    for category,counter in plan['next_local_ids'].items():
        packet['sequences'][category] = str(max(int(counter),int(packet['sequences'][category])))
    packet.pop('hash',None)
    return dict(packet=packet,accepted=accepted,blocked=blocked,localization=localization,assets=assets,
                scope='plain_materials_only',installable_release=False,
                summary=dict(source_material_rows=len(accepted)+len(blocked),converted=len(accepted),blocked=len(blocked),
                             blocked_reasons=dict(sorted(Counter(r for item in blocked for r in item['reasons']).items()))))


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('source','mapping','project','helper','output'):parser.add_argument('--'+name,required=True,type=Path)
    args=parser.parse_args();tables,manifest=read_checkpoint(args.source)
    plan=strict_json(args.mapping.read_text(encoding='utf-8'))
    if plan != analyze(tables,value_hash(dict(source_manifest=value_hash(manifest))),manifest.get('owner_keys')):
        raise MigrationError('Full mapping is stale or edited')
    preserved,_=upgrade(tables,native(args.helper,dict(action='blank')))
    native(args.helper,dict(action='validate',tables=preserved['tables']))
    unity=args.project/'Assets/Metin2/GeneratedData/Metin2ItemCatalog.asset'
    locales,locale_hashes=read_locales(args.project)
    result=convert(tables,plan,preserved,args.project,read_visuals(unity),locales)
    result['packet']['hash']=native(args.helper,dict(action='validate',tables=result['packet']['tables']))['hash']
    result['input_hashes']=dict(source_manifest=value_hash(manifest),mapping=file_hash(args.mapping),unity_catalog=file_hash(unity),locales=locale_hashes)
    with args.output.open('x',encoding='utf-8',newline='\n') as stream:
        json.dump(result,stream,ensure_ascii=True,sort_keys=True,indent=2);stream.write('\n')
    print(json.dumps(dict(result['summary'],catalog_hash=result['packet']['hash'],installable_release=False)))


if __name__=='__main__':main()
