"""OFFLINE conversion of the legacy item shop's costume, mount and stole families (C46).

Legacy ITEM_COSTUME (28) subtypes and every field the old server reads:
  0 body   -> costume Body    value3 body shape (value0 has no server reader; recorded)
  1 hair   -> costume Hair    value3 hair shape (input_login hair check)
  4 weapon -> costume Weapon  value3 legacy weapon type
  2 mount  -> companion Mount value1 mount race; value5==1 flight (MountFlightSystem::CanFly)
  5 stole  -> regalia Stole   value0 grade 1..4; six bonuses drawn from stoleInfoTable
Limits: LEVEL, REAL_TIME, REAL_TIME_START_FIRST_USE, TIMER_BASED_ON_WEAR, UNIQUE_UNLIMITED.
Static APPLY bonuses use the C43 decoder. The ordinary costume attribute pool comes
from item_attr's costume columns and lv*_costume tiers (rare costume slots are filled
by separate bonus items, converted elsewhere). Every model is a verified Unity file or
catalog entry. Anything else is blocked with a reason; nothing is guessed. No source
item number enters the playable catalog.
"""
from __future__ import annotations

import argparse
import copy
import hashlib
import json
from pathlib import Path
import re

from legacy_item_migration import MigrationError, value_hash, file_hash, strict_json, NAME_FIELDS
from convert_quest_tokens import Catalog, translation_for, verified_asset, read_visuals, read_locales
from convert_legacy_equipment import asset, effects
from legacy_equipment_rules import bonus, APPLY_NAMES

TOOL_VERSION = 1
RACE_BY_CLASS = ((0, 4), (5, 1), (2, 6), (7, 3))  # (male, female) race of warrior, assassin, sura, shaman
WEAPON = {0: 1, 1: 2, 2: 3, 3: 4, 4: 5, 5: 6, 7: 7}  # legacy weapon type -> native WeaponSubtype
FAMILIES = {0: 'costume_body', 1: 'costume_hair', 4: 'costume_weapon', 2: 'mount', 5: 'stole'}
CATEGORY = {'costume_body': 4, 'costume_hair': 4, 'costume_weapon': 4, 'mount': 15, 'stole': 19}
PREFIX = {4: 'costume', 15: 'companion', 19: 'regalia'}
ATTRIBUTE_COLUMN = {'costume_body': 'costume_body', 'costume_hair': 'costume_hair', 'costume_weapon': 'costume_weapon'}
EQUIPMENT = 'Assets/Metin2/AddressableContent/Equipment/'
RACES = 'Assets/Metin2/AddressableContent/Races/Prefabs/'
SASH_CATALOG = 'Assets/Metin2/Resources/Metin2SashCatalog.json'
# Source restrictions the native item already enforces by construction (recorded per item):
# PK death drop only walks legacy inventory entities; native items are created with max_stack 1.
SATISFIED_ANTI = (1 << 14) | (1 << 15)


def satisfied(row, family):
    notes = []
    if row['antiflag'] & (1 << 14):
        notes.append('anti_pkdrop: native items never drop on PK death (CombatSystem walks legacy inventory only)')
    if row['antiflag'] & (1 << 15):
        notes.append('anti_stack: native max_stack is 1')
    if family == 'mount' and row['magic_pct']:
        notes.append('magic_pct: costume mounts have no attribute set; AlterItemToMagicItem returns false')
    if family == 'costume_body' and row['value0']:
        notes.append('value0=' + str(row['value0']) + ': no server reader; appearance comes from value3')
    return notes


# Values each family must leave empty; a set value would be silently dropped.
EMPTY_VALUES = {'costume_body': (1, 2, 4, 5), 'costume_hair': (0, 1, 2, 4, 5), 'costume_weapon': (0, 1, 2, 4, 5),
                'mount': (0, 2, 3, 4), 'stole': (1, 2, 3, 4, 5)}


def actors(row):
    anti = row['antiflag']
    classes = 15 & ~(anti >> 2)
    sexes = 3 & ~((anti & 1) << 1 | (anti & 2) >> 1)
    return classes, sexes


def clock(row):
    """(min_level, clock_kind, duration_ms) from both legacy limit slots."""
    level, kind, duration, seen = 0, 1, 0, set()
    for i in (0, 1):
        t, v = row[f'limittype{i}'], row[f'limitvalue{i}']
        if t == 0 and v == 0:
            continue
        if t in seen:
            raise MigrationError('duplicate_limit')
        seen.add(t)
        if t == 1 and 0 <= v <= 65535:
            level = v
        elif t in (7, 8, 9):
            if v <= 0:
                raise MigrationError('zero_duration_limit')
            if kind != 1:
                raise MigrationError('two_clock_limits')
            kind, duration = {7: 2, 8: 3, 9: 4}[t], v * 1000
        elif t == 10:
            pass  # LIMIT_UNIQUE_UNLIMITED: permanent
        else:
            raise MigrationError('unsupported_limit')
    return level, kind, duration


def family_policy(row, family, extra):
    if row['vnum'] in extra:
        raise MigrationError('extra_proto_conversion_required')
    if row['weight'] or row['vnum_range']:
        raise MigrationError('unsupported_weight_or_range')
    if not 1 <= row['size'] <= 3:
        raise MigrationError('inventory_height')
    if row['flag'] & ~(1 | 2 | 4 | 8192):
        raise MigrationError('unsupported_item_flags')
    if row['antiflag'] & ~(0x3f | 128 | 256 | 8192 | 262144 | SATISFIED_ANTI):
        raise MigrationError('unsupported_anti_flags')
    classes, sexes = actors(row)
    if not classes or not sexes:
        raise MigrationError('no_allowed_actor')
    if row['wearflag']:
        raise MigrationError('unmapped_wearflag')
    if row['refined_vnum'] or row['refine_set']:
        raise MigrationError('refinement_chain_requires_recipes')
    if row['addon_type'] or row['socket_pct'] or row['magic_pct'] and family != 'mount':
        raise MigrationError('birth_attribute_policy_not_native')
    if any(not isinstance(row[n], str) or '�' in row[n] or any(ord(c) < 32 for c in row[n]) for n in NAME_FIELDS):
        raise MigrationError('invalid_source_name')
    if not row['locale_name_hu'] and not row['locale_name_en']:
        raise MigrationError('missing_name')
    if any(not 0 <= row[n] <= 2 ** 63 - 1 for n in ('gold', 'shop_buy_price')):
        raise MigrationError('invalid_price')
    if any(row['value' + str(i)] for i in EMPTY_VALUES[family]):
        raise MigrationError('unmapped_family_value')
    if family in ('costume_body', 'costume_hair') and row['value3'] <= 0:
        raise MigrationError('missing_shape')
    if family == 'costume_weapon' and row['value3'] not in WEAPON:
        raise MigrationError('unsupported_costume_weapon_type')
    if family == 'mount' and (row['value1'] <= 0 or row['value5'] not in (0, 1)):
        raise MigrationError('invalid_mount_race_or_flight')
    if family == 'stole' and not 1 <= row['value0'] <= 4:
        raise MigrationError('invalid_stole_grade')
    clock(row)
    effects(row)


def stole_table(server: Path):
    """stoleInfoTable with the build's #ifdef choices resolved from common/service.h."""
    # Build switches live in both headers (ENABLE_STRONG_METIN is in CommonDefines.h).
    defines = set()
    for header in ('common/service.h', 'common/CommonDefines.h'):
        defines |= set(re.findall(r'^\s*#define\s+(\w+)', (server / header).read_text(encoding='utf-8', errors='replace'), re.M))
    text = (server / 'common/stole_length.h').read_text(encoding='utf-8', errors='replace')
    start = text.index('stoleInfoTable')
    body = text[text.index('{', start):text.index('};', start)]
    active, stack, tokens = True, [], []
    for line in body.splitlines():
        directive = re.match(r'\s*#\s*(ifdef|ifndef|else|endif)\b\s*(\w*)', line)
        if directive:
            word, name = directive.groups()
            if word in ('ifdef', 'ifndef'):
                stack.append(active)
                active = active and ((name in defines) == (word == 'ifdef'))
            elif word == 'else':
                active = stack[-1] and not active
            else:
                active = stack.pop()
            continue
        if active:
            tokens.extend(re.findall(r'APPLY_\w+|-?\d+', line.split('//')[0]))
    if len(tokens) != 6 * 17:
        raise MigrationError('stole_table_shape')
    rows = [tokens[i * 17:(i + 1) * 17] for i in range(6)]
    table = []
    for row in rows:
        name = row[0].removeprefix('APPLY_')
        if name not in APPLY_NAMES:
            raise MigrationError('unported_stole_apply:' + row[0])
        table.append((APPLY_NAMES[name], [int(v) for v in row[1:]]))
    return table, dict(stole_length=file_hash(server / 'common/stole_length.h'), service=file_hash(server / 'common/service.h'),
                       common_defines=file_hash(server / 'common/CommonDefines.h'))


def stole_rules(table, grade):
    rules = []
    for index, (source, values) in enumerate(table):
        for variant in range(4):
            entry = bonus(source, values[(grade - 1) * 4 + variant])
            if entry is None:
                raise MigrationError('zero_stole_bonus')
            rules.append(dict(bonus_index=str(index), variant_no=str(variant), minimum_charge_bp='0',
                              bonus_id=entry['bonus_id'], value_scaled=entry['value_scaled']))
    return rules


def costume_attributes(tables, column):
    """Ordinary costume pool: the item_attr rows eligible for this costume part."""
    candidates, values, seen = [], [], set()
    for source in tables['player.item_attr']:
        maximum = source.get(column, 0)
        if not source['prob'] or not maximum:
            continue
        if not 1 <= maximum <= 255:
            raise MigrationError('unsupported_attribute_tiers')
        maximum = min(maximum, 5)
        identity = source.get('apply_index', APPLY_NAMES.get(source['apply']))
        if identity is None:
            raise MigrationError('unported_attribute_name:' + str(source['apply']))
        if identity in seen:
            raise MigrationError('ambiguous_attribute_pool')
        seen.add(identity)
        entries = [bonus(identity, source[f'lv{i}_costume']) for i in range(1, maximum + 1)]
        if any(e is None or int(e['value_scaled']) == 0 for e in entries):
            raise MigrationError('zero_attribute_tier')
        key = entries[0]['bonus_id']
        candidates.append(dict(pool_kind='1', bonus_id=key, weight=str(source['prob'])))
        values.extend(dict(pool_kind='1', bonus_id=key, tier=str(i), value_scaled=e['value_scaled']) for i, e in enumerate(entries, 1))
    if not candidates:
        return dict(attribute_profile=None, attribute_groups=[], attribute_candidates=[], attribute_values=[])
    return dict(attribute_profile=dict(allow_lock='1'), attribute_groups=[dict(pool_kind='1', capacity='5', exclude_static='1')],
                attribute_candidates=candidates, attribute_values=values)


def visuals_for(project, row, family):
    classes, sexes = actors(row)
    rows, proofs = [], []
    for c in range(4):
        for sex in range(2):
            if not (classes >> c & 1 and sexes >> sex & 1):
                continue
            race = RACE_BY_CLASS[c][sex]
            name = {'costume_body': f'a{race}_{row["value3"]}', 'costume_hair': f'h{race}_{row["value3"]}',
                    'costume_weapon': f'w{row["vnum"]}'}[family]
            proofs.append(asset(project, EQUIPMENT + name + '.asset'))
            rows.append(dict(character_class=str(c + 1), sex=str(sex + 1), model_asset='Metin2Equipment/' + name))
    return rows, proofs


def native_code(proposal, row, category):
    """Reviewed proposal code, re-prefixed to the native category; unique by construction."""
    entry = proposal.get(row['vnum'])
    if not entry or not entry.get('content_code'):
        raise MigrationError('missing_proposal_code')
    slug = entry['content_code'].split('.', 1)[1]
    if not re.fullmatch(r'[a-z0-9_]{1,80}', slug):
        raise MigrationError('invalid_code_slug')
    return PREFIX[category] + '.' + slug


def compile_shop_content(tables, preserved, project, visuals, locales, proposal, vnums, server):
    source = {r['vnum']: r for r in tables['player.item_proto']}
    extra = {r['vnum'] for r in tables.get('player.item_extra_proto', [])}
    model = Catalog(copy.deepcopy(preserved))
    codes = {r['content_code'] for r in model.tables['item_base']}
    forms = {r['mob_template_id']: r['form_id'] for r in model.tables['companion_form']}
    stoles, stole_inputs = stole_table(server)
    sash = strict_json((Path(project) / SASH_CATALOG).read_text(encoding='utf-8-sig'))
    sash_entries = {e['vnum']: e for e in sash['entries']}
    accepted, blocked, localization, assets, attribute_profiles = [], [], {}, {}, {}
    for vnum in sorted(set(vnums)):
        row = source.get(vnum)
        checkpoint = None
        try:
            if row is None:
                raise MigrationError('missing_source_prototype')
            if row['type'] != 28:
                raise MigrationError('family_not_in_this_converter')
            family = FAMILIES.get(row['subtype'])
            if family is None:
                raise MigrationError({6: 'pet_skin_rules_required', 7: 'mount_skin_rules_required', 8: 'costume_effect_not_native',
                                      9: 'costume_effect_not_native', 3: 'acce_absorption_not_in_shop_scope'}.get(row['subtype'], 'unknown_costume_subtype'))
            family_policy(row, family, extra)
            category = CATEGORY[family]
            code = native_code(proposal, row, category)
            if code in codes:
                raise MigrationError('content_code_already_issued')
            icon = visuals.get(vnum)
            if not icon or not icon.get('iconPath', '').startswith('icon/item/') or not icon['iconPath'].endswith('.tga'):
                raise MigrationError('missing_icon')
            icon_proof = verified_asset(project, 'Assets/Metin2/Generated/ItemIcons/' + icon['iconPath'], icon.get('icon'))
            level, clock_kind, duration = clock(row)
            proofs = []
            checkpoint = {name: list(rows) for name, rows in model.tables.items()}
            sequences = dict(model.packet['sequences'])
            document = model.new(category)
            base = document['base']
            base.update(content_code=code, name_key='item.' + code + '.name', description_key='item.' + code + '.description',
                        icon_asset=icon['iconPath'], inventory_height=str(row['size']), min_level=str(level), max_stack='1',
                        buy_amount=str(row['gold']), sell_amount=str(row['shop_buy_price']))
            document['child']['local_id'] = base['local_id']
            classes, sexes = actors(row)
            anti = row['antiflag']
            document['use'].update(allowed_classes=str(classes), allowed_sexes=str(sexes))
            document['trade'].update(allow_trade=str(int(not anti & 8192)), allow_drop=str(int(not anti & 128)), allow_sell=str(int(not anti & 256)))
            original = effects(row)
            document['bonuses'] = [dict(bonus_id=str(k), value_scaled=str(v)) for k, v in sorted(original.items()) if v]
            no_attributes = dict(attribute_profile=None, attribute_groups=[], attribute_candidates=[], attribute_values=[])
            if category == 4:
                document['child'].update(costume_kind={'costume_body': '1', 'costume_hair': '2', 'costume_weapon': '3'}[family],
                                         weapon_subtype=str(WEAPON[row['value3']]) if family == 'costume_weapon' else None,
                                         clock_kind=str(clock_kind), duration_ms=str(duration))
                document['visuals'], proofs = visuals_for(project, row, family)
                document.update(costume_attributes(tables, ATTRIBUTE_COLUMN[family]) if family not in attribute_profiles else no_attributes)
            elif category == 15:
                mob = row['value1']
                proofs = [asset(project, RACES + str(mob) + '.asset')]
                document['child'].update(companion_kind='2', clock_kind=str(clock_kind), duration_ms=str(duration),
                                         dungeon_only_bonus='0', flying_mount=str(row['value5']))
                if mob in forms:
                    document['child']['form_id'] = str(forms[mob])
                    document['new_companion_form'] = None
                else:
                    document['new_companion_form'] = dict(code='companion.form_race_' + str(mob), mob_template_id=str(mob),
                                                          model_asset='Metin2RacePrefabs/' + str(mob))
                document['riding_same_form'] = True
                document['new_riding_form'] = None
                document.update(no_attributes)
            else:
                if clock_kind != 1:
                    raise MigrationError('timed_stole_not_native')
                entry = sash_entries.get(vnum)
                if not entry or not entry.get('address', '').startswith('Metin2Sash/'):
                    raise MigrationError('missing_sash_appearance')
                document['child'].update(regalia_kind='1', rune_slot='0', stole_grade=str(row['value0']), capacity_ms='0', initial_ms='0')
                document['appearance'] = dict(model_asset=entry['address'])
                document['regalia_rules'] = stole_rules(stoles, row['value0'])
                document.update(no_attributes)
                proofs = [dict(path=SASH_CATALOG, entry_sha256=value_hash(entry))]
            model.apply(document, encode_result=False)
            local = base['local_id']
            model.packet['sequences'][str(category)] = str(int(local) + 1)  # new() does not advance it
            if category == 15 and document.get('new_companion_form'):
                forms[mob] = int(next(r for r in model.tables['companion_form'] if r['code'] == document['new_companion_form']['code'])['form_id'])
            if category == 4:
                if family in attribute_profiles:
                    model.replace('item_attribute_binding', {'category_id': '4', 'local_id': local},
                                  [dict(category_id='4', local_id=local, profile_id=attribute_profiles[family])])
                else:
                    binding = next((r for r in model.tables['item_attribute_binding'] if r['category_id'] == '4' and r['local_id'] == local), None)
                    if binding:
                        attribute_profiles[family] = binding['profile_id']
            codes.add(code)
            localization[code] = translation_for(row, code, locales)
            assets[code] = dict(icon=icon_proof, models=proofs)
            key = dict(category_id=category, local_id=int(local))
            accepted.append(dict(source_id=vnum, item=key, refinement_level=0, source_row_hash=value_hash(row), semantics=family,
                                 satisfied_by_native=satisfied(row, family)))
        except (MigrationError, ValueError, KeyError) as error:
            if checkpoint is not None:
                model.tables = checkpoint
                model.packet['sequences'] = sequences
            blocked.append(dict(source_id=vnum, source_row_hash=value_hash(row) if row else None, reasons=[str(error)]))
    packet = copy.deepcopy(preserved)
    packet['tables'] = model.encode()
    packet.pop('hash', None)
    for category in ('4', '15', '19'):
        used = [int(r['local_id']) for r in model.tables['item_base'] if r['category_id'] == category]
        packet['sequences'][category] = str(max([int(packet['sequences'].get(category, 1))] + [n + 1 for n in used]))
    return dict(tool_version=TOOL_VERSION, packet=packet, accepted=accepted, blocked=blocked, localization=localization,
                assets=assets, stole_inputs=stole_inputs, sash_catalog=file_hash(Path(project) / SASH_CATALOG))


def main(argv=None):
    from upgrade_exported_catalog import native
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument('--source', required=True, type=Path, help='checkpoint JSON with tables (c43-checkpoint.private.json)')
    parser.add_argument('--preserved', required=True, type=Path, help='installed catalog packet JSON (CatalogEditorTool read)')
    parser.add_argument('--proposal', required=True, type=Path, help='reviewed conversion proposal with mapping codes')
    parser.add_argument('--vnums', required=True, type=Path, help='JSON list of legacy item numbers to convert')
    parser.add_argument('--project', required=True, type=Path, help='Unity project')
    parser.add_argument('--server', required=True, type=Path, help='SRC/Server (stole table and build flags)')
    parser.add_argument('--helper', required=True, type=Path, help='CatalogEditorTool executable')
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args(argv)
    checkpoint = strict_json(args.source.read_text(encoding='utf-8'))
    preserved = strict_json(args.preserved.read_text(encoding='utf-8'))
    verified = native(args.helper, dict(action='validate', tables=preserved['tables']))
    if verified['hash'] != preserved['hash']:
        raise MigrationError('Preserved catalog hash mismatch')
    proposal = {r['source_id']: r for r in strict_json(args.proposal.read_text(encoding='utf-8'))['mapping'] if r['status'] == 'candidate'}
    visuals = read_visuals(args.project / 'Assets/Metin2/GeneratedData/Metin2ItemCatalog.asset')
    locales, _ = read_locales(args.project)
    vnums = [int(v) for v in strict_json(args.vnums.read_text(encoding='utf-8'))]
    result = compile_shop_content(checkpoint['tables'], preserved, args.project, visuals, locales, proposal, vnums, args.server)
    result['packet']['hash'] = native(args.helper, dict(action='validate', tables=result['packet']['tables']))['hash']
    result['preserved_hash'] = preserved['hash']
    result['input_hashes'] = dict(source=file_hash(args.source), proposal=file_hash(args.proposal), vnums=file_hash(args.vnums))
    with args.output.open('x', encoding='utf-8') as f:
        json.dump(result, f, sort_keys=True, ensure_ascii=True)
        f.write('\n')
    from collections import Counter
    print(json.dumps(dict(accepted=len(result['accepted']), blocked=len(result['blocked']), by_family=dict(Counter(a['semantics'] for a in result['accepted'])),
                          blocked_reasons=dict(Counter(b['reasons'][0] for b in result['blocked'])), catalog_hash=result['packet']['hash']), sort_keys=True))


if __name__ == '__main__':
    main()
