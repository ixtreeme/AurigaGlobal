"""Deterministic OFFLINE inventory/mapping for the item catalog cutover.

No SQL connection, game imports, vnum arithmetic, or runtime lookup table.
`analyze` emits a complete *proposal*, never an installable partial catalog.
Every source prototype and instance is accounted for, including retired data.
An explicit reviewed mapping is required before compiling a release bundle.
"""
from __future__ import annotations

import argparse
from collections import Counter, defaultdict
import hashlib
import json
from pathlib import Path
import re
import unicodedata

TOOL_VERSION = 1
MAX_PROTOTYPES = 100_000
MAX_INSTANCES = 5_000_000
INT_TYPES = {'int', 'bigint', 'tinyint', 'smallint', 'mediumint'}
CATEGORY_CODES = {
    1: 'weapon', 2: 'armor', 3: 'accessory', 4: 'costume', 5: 'consumable',
    6: 'material', 7: 'socket_stone', 8: 'container', 9: 'skill_book',
    10: 'quest', 11: 'tool', 12: 'fishing', 13: 'polymorph', 14: 'soul',
    15: 'companion', 16: 'currency_token', 17: 'special', 18: 'container_key',
}
NAME_FIELDS = ('name', 'locale_name', 'locale_name_en', 'locale_name_ro', 'locale_name_it',
               'locale_name_tr', 'locale_name_de', 'locale_name_pl', 'locale_name_pt',
               'locale_name_es', 'locale_name_cz', 'locale_name_hu')
# These may legitimately change by refinement, but every difference is exported
# for the reviewed mapping; this set does NOT authorize dropping any field.
LEVEL_FIELDS = {'refined_vnum', 'refine_set', 'gold', 'shop_buy_price',
                'value0', 'value1', 'value2', 'value3', 'value4', 'value5',
                'limitvalue0', 'limitvalue1', 'magic_pct', 'socket_pct', 'addon_type'}
LEVEL_FIELDS.update('apply' + kind + str(i) for kind in ('type', 'value') for i in range(5))
PROTO_FIELDS = {'vnum', 'type', 'subtype', 'weight', 'size', 'flag', 'wearflag', 'antiflag', 'immuneflag',
                'limittype0', 'limittype1', 'vnum_range'} | LEVEL_FIELDS | set(NAME_FIELDS)


class MigrationError(ValueError):
    pass


def canonical(value):
    return json.dumps(value, ensure_ascii=True, sort_keys=True, separators=(',', ':')).encode('ascii')


def value_hash(value):
    return hashlib.sha256(canonical(value)).hexdigest()


def file_hash(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def strict_json(text):
    def pairs(values):
        result = {}
        for name, value in values:
            if name in result:
                raise MigrationError('Duplicate JSON key: ' + name)
            result[name] = value
        return result
    def invalid(_):
        raise MigrationError('Non-finite JSON number')
    return json.loads(text, object_pairs_hook=pairs, parse_constant=invalid)


def uint(value, bits, field):
    if type(value) is not int or not 0 <= value < 1 << bits:
        raise MigrationError('Invalid unsigned integer: ' + field)
    return value


def decode_table(packet):
    columns = packet['columns']
    names = [column['name'] for column in columns]
    if len(names) != len(set(names)):
        raise MigrationError('Duplicate input column')
    result = []
    for cells in packet['rows']:
        if len(cells) != len(columns):
            raise MigrationError('Input row width mismatch')
        row = {}
        for column, value in zip(columns, cells):
            if value is not None:
                encoding = column['encoding']
                if encoding == 'utf8-hex':
                    value = bytes.fromhex(value).decode('utf-8', errors='strict')
                elif encoding == 'binary-hex':
                    # Opaque bytes are preserved; a later semantic converter
                    # must resolve them before it can mark an instance ready.
                    value = dict(binary_hex=value)
                elif encoding != 'text':
                    raise MigrationError('Unknown input encoding')
                elif column['data_type'] in INT_TYPES:
                    if not re.fullmatch(r'-?(?:0|[1-9][0-9]*)', value):
                        raise MigrationError('Non-canonical SQL integer')
                    value = int(value)
            row[column['name']] = value
        result.append(row)
    return result


def read_checkpoint(source):
    source = Path(source).resolve()
    manifest = strict_json((source / 'manifest.json').read_text(encoding='utf-8'))
    if manifest.get('format_version') != 1 or manifest.get('complete') is not True:
        raise MigrationError('Incomplete or unknown source checkpoint')
    for relative, expected in manifest['files'].items():
        path = (source / relative).resolve()
        if not path.is_relative_to(source) or not path.is_file() or file_hash(path) != expected:
            raise MigrationError('Checkpoint file changed: ' + relative)
    tables = {}
    for qualified, info in manifest['tables'].items():
        path = (source / info['file']).resolve()
        if not path.is_relative_to(source) or file_hash(path) != info['sha256']:
            raise MigrationError('Input table changed: ' + qualified)
        packet = strict_json(path.read_text(encoding='utf-8'))
        if len(packet['rows']) != info['rows']:
            raise MigrationError('Input count mismatch: ' + qualified)
        tables[qualified] = decode_table(packet)
    return tables, manifest


def classification(row):
    """Numeric codes are used ONLY here in the offline source decoder."""
    if row['vnum'] == 0:
        return None, 'quarantine', 'zero_prototype_placeholder'
    kind, subtype = row['type'], row['subtype']
    uint(kind, 8, 'type'); uint(subtype, 8, 'subtype')
    if kind in (29, 30, 31) or kind == 5 and subtype in (5, 6, 7) or kind == 3 and subtype in (27, 28, 42):
        return None, 'retired', 'retired_dragonsoul_content'
    if kind == 1:
        return (1, 'quarantine', 'unsupported_weapon_subtype') if subtype > 7 else (1, 'candidate', 'weapon_or_ammunition')
    if kind == 2:
        if subtype == 7:
            return 2, 'quarantine', 'pendant_disabled_in_current_build'
        if subtype > 7:
            return 2, 'quarantine', 'unknown_armor_subtype'
        return (3 if subtype in (3, 5, 6) else 2), 'candidate', 'equipment'
    if kind == 5:
        return (6, 'candidate', 'ordinary_material') if subtype <= 4 else (6, 'quarantine', 'unknown_material_subtype')
    if kind == 28:
        return (4, 'candidate', 'costume') if subtype <= 16 else (4, 'quarantine', 'undeclared_costume_subtype')
    simple = {0: 10, 3: 5, 4: 5, 6: 17, 7: 11, 8: 8, 9: 16, 10: 7, 11: 8, 12: 12,
              13: 11, 14: 6, 15: 11, 16: 3, 17: 9, 18: 10, 19: 13, 20: 8, 21: 18,
              22: 9, 23: 8, 24: 11, 25: 4, 26: 11, 27: 5, 32: 16, 33: 3, 34: 3,
              35: 14, 36: 17, 37: 15, 38: 16}
    if kind not in simple:
        return None, 'quarantine', 'unknown_item_type'
    # Some old USE items have a different actual domain (bait/fish/companions).
    category = simple[kind]
    if kind == 3 and subtype in (6, 49): category = 12
    if kind == 3 and subtype in (43, 44): category = 15
    if kind == 3 and subtype in (4, 5): category = 8
    return category, 'candidate', 'quest_exchange_token' if kind == 0 else 'typed_port_required'


def name_parts(text):
    text = unicodedata.normalize('NFC', text or '').strip()
    match = re.fullmatch(r'(.*?)\s*\+\s*([0-9]{1,4})\s*', text)
    if match:
        return match[1].strip().casefold(), int(match[2])
    return text.casefold(), None


def source_level(row):
    levels = {name_parts(row.get(name))[1] for name in NAME_FIELDS if row.get(name)} - {None}
    if len(levels) > 1:
        return None, 'contradictory_localized_levels'
    level = next(iter(levels), None)
    if level is not None and level > 255:
        return None, 'refinement_level_exceeds_255'
    return level, None


def edge_decision(source, target):
    """A conservative family candidate. Explicit review is still mandatory."""
    if source['vnum'] == target['vnum']:
        return False, 'self_reference'
    if classification(source)[:2] != classification(target)[:2] or (source['type'], source['subtype']) != (target['type'], target['subtype']):
        return False, 'different_item_kind'
    invariant = (set(source) | set(target)) - LEVEL_FIELDS - set(NAME_FIELDS) - {'vnum'}
    if any(source.get(field) != target.get(field) for field in invariant):
        return False, 'non_refinement_policy_change'
    left_level, left_error = source_level(source)
    right_level, right_error = source_level(target)
    if left_error or right_error:
        return False, left_error or right_error
    if left_level is not None or right_level is not None:
        if left_level is None or right_level is None or right_level != left_level + 1:
            return False, 'level_reset_or_gap'
    matched, contradicted = [], []
    for field in NAME_FIELDS:
        a, _ = name_parts(source.get(field)); b, _ = name_parts(target.get(field))
        if a and b:
            (matched if a == b else contradicted).append(field)
    if not matched:
        return False, 'different_or_missing_semantic_name'
    # A valid localized name may correct a corrupt old internal name, but the
    # discrepancy is visible in the proposal and cannot auto-publish a family.
    return True, 'matching_name_and_contiguous_chain' if not contradicted else 'localized_match_internal_name_differs'


def allocate_key(category, counters):
    local = counters.setdefault(category, 1)
    uint(category, 16, 'category_id'); uint(local, 32, 'local_id')
    if not category or not local:
        raise MigrationError('Zero ItemKey component')
    counters[category] = local + 1
    return dict(category_id=category, local_id=local)


def content_code(row, category, issued):
    name = next((name_parts(row.get(field))[0] for field in ('locale_name_hu', 'locale_name_en', 'locale_name', 'name') if row.get(field)), '')
    slug = unicodedata.normalize('NFKD', name).encode('ascii', errors='ignore').decode('ascii')
    slug = re.sub(r'[^a-z0-9]+', '_', slug).strip('_')[:60] or 'item'
    if not slug[0].isalpha(): slug = 'item_' + slug
    stem = CATEGORY_CODES[category] + '.' + slug
    code = stem
    if code in issued:
        code = stem + '_' + value_hash(row)[:12]
    if code in issued:
        raise MigrationError('Content code allocation collision')
    issued.add(code)
    return code


def refinement_recipe_drafts(plan, existing=()):
    """Convert ordinary smith economics to the v12 typed shape.

    These drafts still depend on reviewing the family mapping and on a native
    candidate catalog. The runtime part contains NO source IDs or lookup map.
    Dedicated rod/soul/costume logic is recorded separately, never guessed.
    """
    recipes = {r['source_recipe_id']: r for r in plan['recipes']}
    mapping = {r['source_id']: r for r in plan['mapping']}
    result, links, blocked = [], [], []
    issued_ids,issued_codes=set(),set()
    for row in existing:
        recipe_id=uint(row['recipe_id'],32,'issued recipe ID')
        if not recipe_id or recipe_id in issued_ids or row['code'] in issued_codes:
            raise MigrationError('Duplicate/zero issued refinement recipe identity')
        issued_ids.add(recipe_id);issued_codes.add(row['code'])
    first_id=max(issued_ids,default=0)+1
    def key(value): return value['category_id'], value['local_id']
    ordered = sorted(plan['transitions'], key=lambda t: (key(t['source']) if t['source'] else (0,0),
                                                       t['source_level'] if t['source_level'] is not None else -1,t['source_id']))
    for transition in ordered:
        source_id = transition['source_id']
        reason = None
        if transition['status'] != 'candidate' or mapping[transition['target_id']]['status'] != 'candidate':
            reason = 'retired_or_quarantined_content'
        elif transition['kind'] == 'dedicated_behavior': reason = 'dedicated_refinement_behavior_required'
        elif not transition['recipe_found']: reason = 'missing_recipe'
        elif transition['source_level'] is None or transition['target_level'] is None: reason = 'ambiguous_levels'
        elif not 0<=transition['source_level']<=255 or not 0<=transition['target_level']<=255: reason='level_out_of_range'
        if reason:
            blocked.append(dict(source_id=source_id, reason=reason)); continue
        source_recipe = recipes[transition['source_recipe_id']]
        if not 0 <= source_recipe['cost'] <= (1<<63)-1 or not 0 <= source_recipe['success_chance_bp'] <= 10000:
            blocked.append(dict(source_id=source_id, reason='invalid_recipe')); continue
        materials = Counter()
        for material in source_recipe['materials']:
            reference = mapping.get(material['source_id'])
            if reference is None or reference['target'] is None or reference['status'] != 'candidate' or reference['refinement_level'] is None:
                reason = 'missing_or_unavailable_material'; break
            if not 0 < material['amount'] <= 4294967295:
                reason = 'invalid_material_amount'; break
            materials[(*key(reference['target']), reference['refinement_level'])] += material['amount']
        if not reason and (len(materials)>32 or any(count>4294967295 for count in materials.values())):
            reason = 'material_budget_exceeded'
        if reason:
            blocked.append(dict(source_id=source_id,reason=reason)); continue
        recipe_id=uint(first_id+len(result),32,'new recipe ID')
        code='refine.'+mapping[source_id]['content_code']+'.at_'+str(transition['source_level'])+'.smith'
        if len(code)>96:code=code[:79].rstrip('.')+'_'+value_hash(code)[:16]
        if code in issued_codes:code=code[:79].rstrip('.')+'_'+value_hash(transition)[:16]
        if code in issued_codes:raise MigrationError('Refinement recipe code collision')
        issued_codes.add(code)
        result.append(dict(recipe_id=recipe_id,code=code,
                           source=transition['source'],source_level=transition['source_level'],
                           target=transition['target'],target_level=transition['target_level'],
                           gold_cost=source_recipe['cost'],success_chance_bp=source_recipe['success_chance_bp'],
                           failure_kind=2,failure_level=None,  # DoRefine's ordinary smith failure destroys the source.
                           ingredients=[dict(item=dict(category_id=c,local_id=i),refinement_level=l,amount=n)
                                        for (c,i,l),n in sorted(materials.items())]))
        links.append(dict(recipe_id=recipe_id,source_id=source_id,source_recipe_id=transition['source_recipe_id']))
    return dict(schema_version=12,installable=False,recipes=result,audit_source_links=links,blocked=blocked,
                supported_operation='ordinary_smith_only',
                remaining_services=['scroll_variants','guild_fee_and_probability','money_only','dedicated_behaviors'])


def relational_reference_drafts(tables, mapping):
    """Explicitly named item fields; never infer semantics from a column name.

    A guild donation's item0..item4 columns are COUNTS, and object_proto.vnum
    identifies a BUILDING. Neither may be run through the item-ID mapping.
    """
    fields = {
        'player.shop_item': ('item_vnum', *('itemvnum'+str(i) for i in range(1,6))),
        'player.myshop_pricelist': ('item_vnum',),
        'player.item_extra_proto': ('vnum',),
        'player.item_award': ('vnum',),
        'player.daily_reward_items': ('items',),
        'player.quest_item_proto': ('vnum',),
        'player.ishop_data': ('itemVnum',),
        'mission.mission_engine_item_awards': ('vnum',),
    }
    drafts, blockers = {}, []
    for table, columns in fields.items():
        converted = []
        for index, row in enumerate(sorted(tables.get(table, []), key=canonical)):
            references = {}
            for field in columns:
                raw = row.get(field)
                optional = table == 'player.shop_item' and field != 'item_vnum'
                if optional and raw == 0:
                    references[field] = None; continue
                if type(raw) is int: old_id = raw
                elif isinstance(raw,str) and re.fullmatch(r'(?:0|[1-9][0-9]*)',raw): old_id = int(raw)
                else:
                    blockers.append(dict(table=table,row=index,field=field,reason='unparsed_item_reference'))
                    references[field] = None; continue
                reference = mapping.get(old_id)
                if reference is None or reference['target'] is None or reference['status']!='candidate':
                    blockers.append(dict(table=table,row=index,field=field,source_id=old_id,reason='unavailable_item_reference'))
                references[field] = dict(item=reference['target'],refinement_level=reference['refinement_level']) if reference else None
            converted.append(dict(source_row_hash=value_hash(row),references=references,conversion_complete=False))
        drafts[table] = converted
    return dict(tables=drafts,blockers=blockers,
                counter_tables={'player.guild_renewal_kisado_item':len(tables.get('player.guild_renewal_kisado_item',[]))},
                semantic_payloads_requiring_conversion={name:len(tables.get(name,[])) for name in
                    ('player.object_proto','player.quest','player.affect','player.new_petsystem',
                     'player.item_attr','player.item_attr_rare','player.item_attr_rare_8')})


def validate_owners(owner_keys):
    if owner_keys is None:return None
    if set(owner_keys)!={'player','account','guild'}:raise MigrationError('Incomplete owner identity sets')
    result={}
    for kind,keys in owner_keys.items():
        if not isinstance(keys,list):raise MigrationError('Owner identities must be lists')
        result[kind]=set()
        for key in keys:
            uint(key,64,'owner identity')
            if not key or key in result[kind]:raise MigrationError('Zero/duplicate owner identity')
            result[kind].add(key)
    return result


def analyze(tables, source_hash, owner_keys=None):
    owners=validate_owners(owner_keys)
    protos = tables.get('player.item_proto', [])
    instances = tables.get('player.item', [])
    instance_count = len(instances)+sum(len(tables.get(name,[])) for name in ('player.account_mount_inventory','player.guild_renewal_storage'))
    if not protos or len(protos) > MAX_PROTOTYPES or instance_count > MAX_INSTANCES:
        raise MigrationError('Missing/oversized source data')
    by_id = {}
    for row in protos:
        old_id = uint(row['vnum'], 32, 'source item ID')
        if old_id in by_id:
            raise MigrationError('Duplicate source item ID')
        by_id[old_id] = row
    diagnostics = []
    def issue(code, **detail): diagnostics.append(dict(code=code, **detail))
    for row in sorted(protos, key=lambda r: r['vnum']):
        unknown = sorted(set(row) - PROTO_FIELDS)
        if unknown: issue('unaccounted_proto_columns', source_id=row['vnum'], columns=unknown)
        if row.get('vnum_range', 0): issue('range_proto_requires_expansion', source_id=row['vnum'])
    counters = {category: 1 for category in CATEGORY_CODES}
    issued, issued_keys = set(), set()
    preserved = tables.get('protos.item_base', [])
    for row in preserved:
        category, local = int(row['category_id']), int(row['local_id'])
        uint(category, 16, 'existing category'); uint(local, 32, 'existing local ID')
        if category not in counters or local == 0:
            raise MigrationError('Invalid existing catalog key')
        counters[category] = max(counters[category], local + 1)
        if (category,local) in issued_keys: raise MigrationError('Duplicate existing catalog key')
        issued_keys.add((category,local))
        if row['content_code'] in issued: raise MigrationError('Duplicate existing content code')
        issued.add(row['content_code'])
    for row in tables.get('protos.item_local_id_sequence', []):
        category = int(row['category_id'])
        if category not in counters: raise MigrationError('Unknown category sequence')
        counters[category] = max(counters[category], int(row['next_local_id']))
    edges, predecessor = [], defaultdict(list)
    candidate_next = {}
    for old_id, row in sorted(by_id.items()):
        next_id = row.get('refined_vnum', 0)
        if not next_id: continue
        if next_id not in by_id:
            keep, reason = False, 'missing_target'
            issue('missing_refinement_target', source_id=old_id, target_id=next_id)
        else:
            keep, reason = edge_decision(row, by_id[next_id])
        edges.append(dict(source_id=old_id, target_id=next_id, same_prototype_candidate=keep, reason=reason))
        if reason == 'self_reference': issue('self_refinement_reference', source_id=old_id)
        if keep:
            candidate_next[old_id] = next_id
            predecessor[next_id].append(old_id)
    for target, sources in sorted(predecessor.items()):
        if len(sources) > 1:
            issue('merged_refinement_chains', target_id=target, source_ids=sorted(sources))
            for old_id in sources: candidate_next.pop(old_id, None)
    # Find directed cycles without recursion or assuming that each component
    # has a root. Self-loops are already accounted for above.
    done = set()
    for start in sorted(by_id):
        path, position, current = [], {}, start
        while current in candidate_next and current not in done:
            if current in position:
                cycle = path[position[current]:]
                issue('refinement_cycle', source_ids=cycle)
                for old_id in cycle: candidate_next.pop(old_id, None)
                break
            position[current] = len(path); path.append(current); current = candidate_next[current]
        done.update(path)
    incoming = set(candidate_next.values())
    groups, visited, mapping = [], set(), {}
    for start in sorted(old_id for old_id in by_id if old_id not in incoming):
        members, current = [], start
        while current not in visited:
            visited.add(current); members.append(current)
            if current not in candidate_next: break
            current = candidate_next[current]
        root = by_id[start]
        category, status, reason = classification(root)
        levels = [source_level(by_id[old_id])[0] for old_id in members]
        if all(level is None for level in levels): levels = list(range(len(members)))
        if any(level is None for level in levels) or len(set(levels)) != len(levels):
            issue('ambiguous_family_levels', source_ids=members)
        if levels and levels != list(range(len(members))):
            issue('family_missing_level_zero_or_steps', source_ids=members, levels=levels)
        if any(level is not None and level>255 for level in levels):
            issue('family_exceeds_supported_levels',source_ids=members)
        key = allocate_key(category, counters) if category is not None else None
        code = content_code(root, category, issued) if category is not None else None
        fields = sorted(set().union(*(set(by_id[i]) for i in members)) - {'vnum', 'refined_vnum'})
        changes = {field: [by_id[i].get(field) for i in members] for field in fields
                   if len({canonical(by_id[i].get(field)) for i in members}) > 1}
        group = dict(key=key, content_code=code, status=status, reason=reason, source_ids=members,
                     levels=levels, source_rows_hash=value_hash([by_id[i] for i in members]),
                     maximum_candidate=max((level for level in levels if level is not None), default=0),
                     changed_fields=changes, reviewed=False)
        groups.append(group)
        for old_id, level in zip(members, levels):
            mapping[old_id] = dict(source_id=old_id, target=key, refinement_level=level,
                                   content_code=code, status=status)
    if visited != set(by_id) or len(mapping) != len(protos):
        raise MigrationError('Internal error: incomplete prototype accounting')
    recipes, recipe_ids = [], set()
    for row in sorted(tables.get('player.refine_proto', []), key=lambda r: r['id']):
        recipe_id = uint(row['id'], 32, 'recipe id')
        if recipe_id in recipe_ids: raise MigrationError('Duplicate source recipe')
        recipe_ids.add(recipe_id)
        materials = []
        cost, probability = row['cost'], row['prob']
        if not 0 <= cost <= (1 << 63) - 1 or not 0 <= probability <= 100:
            issue('invalid_recipe_cost_or_probability', recipe_id=recipe_id)
        gap = False
        for slot in range(5):
            old_id, amount = row['vnum' + str(slot)], row['count' + str(slot)]
            if not old_id:
                gap = True
                if amount: issue('recipe_count_without_material', recipe_id=recipe_id, slot=slot)
                continue
            if gap: issue('recipe_material_after_zero_slot', recipe_id=recipe_id, slot=slot)
            if not 0 < amount <= 4294967295: issue('invalid_recipe_material_amount', recipe_id=recipe_id, slot=slot)
            reference = mapping.get(old_id)
            if reference is None:
                issue('missing_recipe_material', recipe_id=recipe_id, source_id=old_id)
            materials.append(dict(slot=slot, source_id=old_id, target=reference['target'] if reference else None,
                                  refinement_level=reference['refinement_level'] if reference else None, amount=amount))
        recipes.append(dict(source_recipe_id=recipe_id, cost=cost, success_chance_bp=probability * 100,
                            materials=materials, source_row_hash=value_hash(row)))
    transitions = []
    for edge in edges:
        old_id, next_id = edge['source_id'], edge['target_id']
        if next_id not in mapping or next_id == old_id: continue
        source, target, row = mapping[old_id], mapping[next_id], by_id[old_id]
        recipe_id = row['refine_set']
        # Rod/pick, soul and costume combinations have dedicated logic. A
        # missing generic recipe is NOT evidence that these operations are free.
        dedicated = row['type'] in (13, 24, 28, 35) or row['type'] == 16 and row['subtype'] in (2, 3)
        kind = 'dedicated_behavior' if dedicated else 'same_prototype_level' if source['target'] == target['target'] else 'item_transformation'
        if recipe_id not in recipe_ids and not dedicated:
            issue('missing_used_refinement_recipe', source_id=old_id, recipe_id=recipe_id)
        transitions.append(dict(source_id=old_id, target_id=next_id, kind=kind, source=source['target'],
                                source_level=source['refinement_level'], target=target['target'],
                                target_level=target['refinement_level'], source_recipe_id=recipe_id,
                                recipe_found=recipe_id in recipe_ids, status=source['status']))
    instance_map, instance_ids = [], set()
    quantities_before, quantities_after = Counter(), Counter()
    all_instances = [('player.item',row,row['id'],row['owner_id'],row['window'],row['pos'])
                     for row in sorted(instances,key=lambda r:r['id'])]
    next_instance_id = max((row['id'] for row in instances),default=0)+1
    for table, owner_field, window in (('player.account_mount_inventory','account_id','ACCOUNT_MOUNT'),
                                        ('player.guild_renewal_storage','guild_id','GUILD_STORAGE')):
        auxiliary = sorted(tables.get(table,[]),key=lambda row:(row[owner_field],row['slot'],row.get('id',0)))
        seen_locations = set()
        for row in auxiliary:
            location=(row[owner_field],row['slot'])
            if location in seen_locations: raise MigrationError('Duplicate auxiliary item location: '+table)
            seen_locations.add(location)
            all_instances.append((table,row,next_instance_id,row[owner_field],window,row['slot']))
            next_instance_id += 1
    if len(all_instances)>MAX_INSTANCES:raise MigrationError('Too many source item instances')
    for table, row, instance_id, owner, window, position in all_instances:
        instance_id = uint(instance_id, 64, 'instance id')
        if not instance_id or instance_id in instance_ids: raise MigrationError('Duplicate or zero item instance ID')
        instance_ids.add(instance_id)
        reference = mapping.get(row['vnum'])
        if reference is None: issue('orphan_instance', instance_id=instance_id, source_id=row['vnum'])
        quantity = uint(row['count'], 32, 'item count')
        if not quantity: issue('zero_instance_count', instance_id=instance_id)
        location = (table, owner, window)
        quantities_before[location] += quantity
        owner_kind='guild' if window=='GUILD_STORAGE' else 'account' if window in ('ACCOUNT_MOUNT','MALL','SAFEBOX') else \
            'player' if window in ('INVENTORY','EQUIPMENT','EXTRA_INVENTORY','MOUNT_INVENTORY','BELT_INVENTORY','SWITCHBOT') else None
        if owner_kind is None:issue('unknown_instance_owner_kind',instance_id=instance_id,source_store=table,window=window)
        owner_valid=None if owners is None or owner_kind is None else owner in owners[owner_kind]
        if owner_valid is False:issue('missing_instance_owner',instance_id=instance_id,source_store=table,owner_kind=owner_kind)
        # Source state is retained by hash and instance ID, not normalized by
        # guessing what an overloaded socket means. Such a row is not ready.
        instance_map.append(dict(instance_id=instance_id, source_store=table, target=reference['target'] if reference else None,
                                 refinement_level=reference['refinement_level'] if reference else None,
                                 count=quantity, owner_id=owner, window=window, position=position,
                                 owner_kind=owner_kind,owner_valid=owner_valid,
                                 status='owner_quarantine' if owner_valid is False else reference['status'] if reference else 'orphan', source_row_hash=value_hash(row),
                                 state_conversion_complete=False))
        quantities_after[location] += quantity
    if quantities_before != quantities_after: raise MigrationError('Item quantity was lost during mapping')
    summary = dict(source_prototypes=len(protos), proposed_prototypes=len(groups),
                   merged_source_rows=len(protos)-len(groups), prototype_status=dict(sorted(Counter(m['status'] for m in mapping.values()).items())),
                   source_instances=len(all_instances), mapped_instance_count=len(instance_map),
                   instance_stores=dict(sorted(Counter(row[0] for row in all_instances).items())),
                   owner_status=dict(sorted(Counter('unchecked' if r['owner_valid'] is None else 'valid' if r['owner_valid'] else 'missing' for r in instance_map).items())),
                   instance_status=dict(sorted(Counter(m['status'] for m in instance_map).items())),
                   source_recipes=len(recipes), refinement_edges=len(edges), transitions=len(transitions),
                   preserved_catalog_items=len(preserved), diagnostics=dict(sorted(Counter(d['code'] for d in diagnostics).items())),
                   category_proposals={str(k): sum(g['key'] is not None and g['key']['category_id']==k for g in groups) for k in CATEGORY_CODES},
                   source_total_quantity=sum(quantities_before.values()), mapped_total_quantity=sum(quantities_after.values()))
    result = dict(format_version=1, tool_version=TOOL_VERSION, source_hash=source_hash,
                installable=False, summary=summary, next_local_ids={str(k):v for k,v in sorted(counters.items())},
                preserved_catalog_hash=value_hash({name: sorted(rows,key=canonical) for name,rows in sorted(tables.items()) if name.startswith('protos.')}),
                families=groups, mapping=[mapping[i] for i in sorted(mapping)], refinement_edges=edges,
                recipes=recipes, transitions=transitions, instances=instance_map, diagnostics=diagnostics,
                remaining_gates=['reviewed_explicit_mapping', 'complete_typed_content_conversion',
                                 'complete_instance_state_conversion', 'all_content_references_converted',
                                 'native_catalog_validation', 'new_server_database_and_unity_runtime',
                                 'local_closed_smoke'])
    result['refinement_recipe_draft'] = refinement_recipe_drafts(result,tables.get('protos.refinement_recipe',[]))
    result['relational_reference_drafts'] = relational_reference_drafts(tables,mapping)
    result['summary']['ordinary_smith_recipe_drafts'] = len(result['refinement_recipe_draft']['recipes'])
    result['summary']['refinement_draft_blockers'] = dict(sorted(Counter(r['reason'] for r in result['refinement_recipe_draft']['blocked']).items()))
    result['summary']['relational_reference_rows'] = {name:len(rows) for name,rows in result['relational_reference_drafts']['tables'].items()}
    result['summary']['relational_reference_blockers'] = len(result['relational_reference_drafts']['blockers'])
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=['analyze'])
    parser.add_argument('--source', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--ownership-supplement', type=Path)
    args = parser.parse_args()
    tables, manifest = read_checkpoint(args.source)
    owners=manifest.get('owner_keys')
    identity=dict(source_manifest=value_hash(manifest))
    if args.ownership_supplement:
        supplement=strict_json(args.ownership_supplement.read_text(encoding='utf-8'))
        if supplement.get('format_version')!=1 or supplement.get('checkpoint_sha256')!=manifest['files']['checkpoint.sql']:
            raise MigrationError('Owner identities do not belong to this checkpoint')
        if owners is not None and owners!=supplement.get('owner_keys'):
            raise MigrationError('Conflicting checkpoint owner identities')
        owners=supplement['owner_keys'];identity['ownership_supplement']=value_hash(supplement)
    result = analyze(tables, value_hash(identity),owners)
    with args.output.open('x', encoding='utf-8', newline='\n') as stream:
        json.dump(result, stream, ensure_ascii=True, sort_keys=True, indent=2)
        stream.write('\n')
    print(json.dumps(dict(installable=result['installable'], summary=result['summary'], output_hash=file_hash(args.output)), sort_keys=True))


if __name__ == '__main__':
    main()
