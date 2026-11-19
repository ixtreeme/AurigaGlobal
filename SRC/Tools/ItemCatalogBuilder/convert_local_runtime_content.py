"""Offline native starter-pack and ordinary mob-drop conversion.

Never reads player instances. Runtime output contains native keys only;
source IDs and exclusion reasons live in the separate audit document.
"""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import re
import struct

from legacy_equipment_rules import bonus
from upgrade_exported_catalog import native


def digest(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, separators=(',', ':')).encode()).hexdigest()


def chance_ppb(value):
    # Original reader: float percent; uint32_t(10000.0f * percent).
    # Its baseline range is 4,000,000, not 1,000,000.
    f32 = lambda v: struct.unpack('f', struct.pack('f', v))[0]
    if isinstance(value, bool) or not isinstance(value, (int, float)) or not 0 <= value <= 400:
        raise ValueError('invalid original drop chance')
    return int(f32(f32(value) * f32(10000))) * 250


def convert_drops(catalog, document):
    result = copy.deepcopy(catalog)
    tables = result['packet']['tables']
    if tables['mob_drop_group'] or tables['mob_drop_entry']:
        raise ValueError('refusing to overwrite existing native drops')
    accepted = {r['source_id']: r for r in result['accepted']}
    deferred = {r['source_id'] for r in result['blocked']}
    retired = {r['source_id'] for r in result['retired']}
    bases = {(int(r[0]), int(r[1])): dict(zip(result['packet']['columns']['item_base'], r)) for r in tables['item_base']}
    audit = dict(groups=0, entries=0, excluded=[], ignored_regions=[], disabled_entries=0, duplicate_groups_preserved=[])
    seen = set()
    for source_ordinal, group in enumerate(document['groups'], 1):
        if group['type'].lower() != 'drop':
            raise ValueError('unsupported original group type: ' + group['type'])
        identity = (group['name'], group['mob'])
        if not isinstance(group['mob'], int) or not 0 < group['mob'] <= 0xffffffff:
            raise ValueError('invalid original drop group')
        if identity in seen:
            # The original reader appends these entries to the same mob's
            # independent rolls. Preserve them, including identical entries.
            audit['duplicate_groups_preserved'].append(dict(name=group['name'], mob=group['mob'], source_ordinal=source_ordinal))
        seen.add(identity)
        if group['name'].startswith('kr_'):
            audit['ignored_regions'].append(group['name'])
            continue
        group_id = len(tables['mob_drop_group']) + 1
        entries = []
        for ordinal, entry in enumerate(group['items'], 1):
            if not re.fullmatch(r'[0-9]+', str(entry['item'])):
                raise ValueError('unresolved named source drop')
            source = int(entry['item'])
            if source not in accepted:
                audit['excluded'].append(dict(group=group['name'], entry=ordinal, source_id=source,
                    reason='merge_deferred' if source in deferred else 'retired' if source in retired else 'missing_original_prototype'))
                continue
            ref = accepted[source]
            key = ref['item']
            count = entry['count']
            base = bases[key['category_id'], key['local_id']]
            if isinstance(count, bool) or not isinstance(count, int) or not 0 < count <= int(base['max_stack']):
                raise ValueError('source drop exceeds native stack: ' + str(source))
            chance = chance_ppb(entry['chance'])
            if not chance:
                audit['disabled_entries'] += 1
                continue
            entries.append([str(v) for v in (group_id, len(entries)+1, key['category_id'], key['local_id'],
                ref['refinement_level'], count, chance, 0, 0, 1, 65535)])
        if not entries:
            continue
        if len(entries) > 256:
            raise ValueError('too many original drops in one group')
        tables['mob_drop_group'].append([str(group_id), 'original.mob_drop.'+digest((source_ordinal,*identity))[:24], '1', str(group['mob']), '1', '0', '0'])
        tables['mob_drop_entry'].extend(entries)
    audit.update(groups=len(tables['mob_drop_group']), entries=len(tables['mob_drop_entry']))
    return result, audit


def original_starter_rows(source):
    text = re.sub(r'//[^\n]*|/\*.*?\*/', '', source, flags=re.S)
    text = text.split('static SInitialItem initialItems[MAIN_RACE_MAX_NUM][13]', 1)[1]
    text = text[text.index('{')+1:]
    groups = []
    depth = 0
    begin = 0
    for offset, ch in enumerate(text):
        if ch == '{':
            if depth == 0:
                begin = offset
            depth += 1
        elif ch == '}':
            depth -= 1
            if depth < 0:
                break
            if depth == 0:
                groups.append(text[begin:offset+1])
    if len(groups) != 8:
        raise ValueError('original race starter array changed')
    pattern = r'\{INVENTORY,\s*\d+,\s*(\d+),\s*(\d+),\s*\{([^}]*)\}\s*\}'
    return [[(int(count), int(item), [v.strip() for v in attrs.split(',')])
        for count, item, attrs in re.findall(pattern, group)] for group in groups]


def convert_starters(catalog, source, length_header, creation_header):
    # These functions are the existing character-choice contract. Fail if its
    # shape changes rather than silently publishing a different starting pack.
    compact = re.sub(r'\s+', '', creation_header)
    for expected in ('case0:returngroup==2?3009:19;', 'case1:returngroup==2?2009:1009;',
                     'case2:return19;default:returngroup==2?5009:7009;',
                     'return11209+(race%4)*200;', 'race==0||race==2||race==5||race==7'):
        if expected not in compact:
            raise ValueError('original character-choice contract changed')
    enum = length_header.split('enum EApplyTypes', 1)[1].split('#ifdef', 1)[0]
    enum = re.sub(r'//[^\n]*|/\*.*?\*/', '', enum, flags=re.S)
    applies = {symbol: index for index, symbol in enumerate(re.findall(r'\bAPPLY_[A-Z_0-9]+\b', enum))}
    accepted = {r['source_id']: r for r in catalog['accepted']}
    rows = original_starter_rows(source)
    initial_slots = {12209:3,12349:3,12489:3,12629:3,13009:4,14009:6,15009:5,16009:7,17009:8}
    costumes = {19:40129,1009:40130,2009:40131,3009:40132,5009:40133,7009:40134}
    alternatives = ((19,3009),(1009,2009,19),(19,),(5009,7009))
    profiles = []
    source_audit = []
    for race in range(8):
        for skill in (1, 2):
            grants = []
            sources = []

            def grant(item, count=1, equipped=None, attributes=None, magic=False):
                ref = accepted[item]  # Missing starter content is a hard error.
                value = dict(category=ref['item']['category_id'], local=ref['item']['local_id'],
                    level=ref['refinement_level'], count=count, try_magic=magic)
                if equipped is not None:
                    value['equipped'] = equipped
                if attributes is not None:
                    value['attributes'] = []
                    for apply, amount in zip(attributes[::2], attributes[1::2]):
                        # An empty legacy APPLY slot may contain stale numeric
                        # data (50187 has 0,10); it contributed no bonus.
                        if apply == '0':
                            continue
                        converted = bonus(applies[apply], int(amount))
                        if converted:
                            value['attributes'].append(dict(bonus=int(converted['bonus_id']), value=int(converted['value_scaled'])))
                grants.append(value)
                sources.append(item)

            for count, item, attributes in rows[race]:
                if item not in (0,50188,11409):
                    grant(item, count, initial_slots.get(item), attributes)
            job = race % 4
            weapon = ((19,3009),(1009,2009),(19,19),(7009,5009))[job][skill-1]
            grant(weapon, equipped=1)
            grant(11209+job*200, equipped=2)
            for item in alternatives[job]:
                if item != weapon:
                    grant(item)
            # This source is a real mount seal in the native catalog, rather
            # than a costume skin. It remains usable in inventory; never force
            # a summonable companion into a skin equipment slot.
            grant(71231)
            grant(88222, equipped=28)
            grant(88224, equipped=27)
            male = race in (0,2,5,7)
            grant(41311 if male else 41312, equipped=9, magic=True)
            grant(45147 if male else 45148, equipped=10, magic=True)
            grant(costumes[weapon], equipped=11, magic=True)
            for item in alternatives[job]:
                if costumes[item] != costumes[weapon]:
                    grant(costumes[item], magic=True)
            grant(76016, 40)
            profile = dict(race=race, skill_group=skill, grants=grants)
            profile['contract'] = digest(profile)
            profiles.append(profile)
            source_audit.append(dict(race=race, skill_group=skill, source_ids=sources))
    return dict(version=1, profiles=profiles), source_audit


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('catalog', 'data', 'server', 'helper', 'output'):
        p.add_argument('--'+name, type=Path, required=True)
    args = p.parse_args()
    if args.output.exists():
        raise ValueError('output must be a new directory')
    catalog = json.loads(args.catalog.read_text(encoding='utf-8'))
    for name, key in (('common_drop_item.json','groups'), ('drop_item_group.json','groups'), ('etc_drop_item.json',None)):
        value = json.loads((args.data/name).read_text(encoding='utf-8-sig'))
        if (value[key] if key else value):
            raise ValueError('additional drop format requires an explicit converter: '+name)
    converted, drops = convert_drops(catalog, json.loads((args.data/'mob_drop_item.json').read_text(encoding='utf-8-sig')))
    starter, birth = convert_starters(catalog,
        (Path(__file__).resolve().parents[3]/'config/item_combat/original_character_starters.source.cpp').read_text(encoding='utf-8'),
        (args.server/'common/length.h').read_text(encoding='utf-8'),
        (args.server/'common/character_creation.h').read_text(encoding='utf-8'))
    converted['packet']['hash'] = native(args.helper, dict(action='validate', tables=converted['packet']['tables']))['hash']
    inputs = [args.catalog, args.data/'mob_drop_item.json', Path(__file__).resolve().parents[3]/'config/item_combat/original_character_starters.source.cpp', args.server/'common/length.h', args.server/'common/character_creation.h']
    evidence = dict(inputs={str(path):hashlib.sha256(path.read_bytes()).hexdigest() for path in inputs},
        catalog_hash=converted['packet']['hash'], drops=drops, starter_sources=birth)
    args.output.mkdir(parents=True)
    for name, value in (('catalog.json',converted),('typed_character_creation.json',starter),('audit.json',evidence)):
        (args.output/name).write_text(json.dumps(value, ensure_ascii=True, sort_keys=True)+'\n', encoding='utf-8')
    print(json.dumps(dict(groups=drops['groups'], entries=drops['entries'], excluded=len(drops['excluded']), starter_profiles=len(starter['profiles']), catalog_hash=evidence['catalog_hash'])))


if __name__ == '__main__':
    main()
