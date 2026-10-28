"""Compile representable passive quest tokens from an immutable checkpoint.

OFFLINE ONLY: no database endpoint or runtime legacy lookup is emitted. This
is a partial conversion rehearsal, never an installable release. Scripted
quest items remain blocked until their handlers and state are converted.
"""
import argparse
from collections import Counter
import copy
import json
from pathlib import Path, PurePosixPath
import re
import sys

from legacy_item_migration import (MigrationError, NAME_FIELDS, PROTO_FIELDS, analyze,
                                   file_hash, read_checkpoint, strict_json, value_hash)
from upgrade_exported_catalog import native, upgrade

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'SRC/Tools/ServerManager'))
from item_catalog import Catalog

STACK_FLAG = 1 << 2
SOURCE_STACK_LIMIT = 50000  # common/length.h::ITEM_MAX_COUNT, audited source.
SUPPORTED_ANTI = 0x3f | (1 << 7) | (1 << 8)
SQL_LOCALES = {'en': 'locale_name_en', 'ro': 'locale_name_ro', 'it': 'locale_name_it',
               'tr': 'locale_name_tr', 'de': 'locale_name_de', 'pl': 'locale_name_pl',
               'pt': 'locale_name_pt', 'es': 'locale_name_es', 'cz': 'locale_name_cz', 'hu': 'locale_name_hu'}


def plain_token_errors(row, extra_ids):
    """Every source column is either mapped, preserved as text, or zero-gated."""
    errors = []
    if set(row) != PROTO_FIELDS:
        return ['unexpected_source_columns']
    if row['vnum'] == 0: errors.append('zero_placeholder')
    if row['type'] != 0: errors.append('script_handler_conversion_required')
    mapped = {'vnum', 'type', 'size', 'flag', 'antiflag', 'limittype0', 'limitvalue0',
              'limittype1', 'limitvalue1'} | set(NAME_FIELDS)
    for name in sorted(PROTO_FIELDS - mapped):
        if row[name] != 0: errors.append('unmapped_nonzero:' + name)
    if row['flag'] < 0 or row['flag'] & ~STACK_FLAG:
        errors.append('unmapped_item_flags')
    if row['antiflag'] < 0 or row['antiflag'] & ~SUPPORTED_ANTI:
        errors.append('unmapped_anti_flags')
    if not 1 <= row['size'] <= 3: errors.append('invalid_inventory_height')
    levels = []
    for i in range(2):
        kind, value = row['limittype' + str(i)], row['limitvalue' + str(i)]
        if kind == 1 and 0 <= value <= 65535: levels.append(value)
        elif kind != 0 or value != 0: errors.append('unmapped_limit:' + str(i))
    if len(levels) > 1: errors.append('duplicate_level_limit')
    if row['antiflag'] & 0x3c == 0x3c: errors.append('all_classes_forbidden')
    if row['antiflag'] & 3 == 3: errors.append('all_sexes_forbidden')
    if row['vnum'] in extra_ids: errors.append('extra_proto_conversion_required')
    if not row['locale_name_hu'] and not row['locale_name_en']: errors.append('missing_display_name')
    if any('\ufffd' in row[name] or any(ord(c) < 32 for c in row[name]) for name in NAME_FIELDS):
        errors.append('invalid_source_name')
    return sorted(set(errors))


def scalar(value):
    value = value.strip()
    if value.startswith('"'):
        decoded = strict_json(value)
        if not isinstance(decoded, str): raise MigrationError('Expected a quoted asset string')
        return decoded
    if any(c in value for c in ('"', "'", '\\', '{', '}', '#', '\x00')):
        raise MigrationError('Unsupported asset scalar')
    return value


def read_visuals(asset):
    """Read only four fields of the actual Unity YAML; never evaluate YAML tags."""
    result, current = {}, None
    for line in Path(asset).read_text(encoding='utf-8-sig').splitlines():
        match = re.fullmatch(r'  - vnum: ([0-9]+)', line)
        if match:
            old = int(match[1])
            if old in result: raise MigrationError('Duplicate Unity item entry')
            current = result[old] = {}
        elif current is not None:
            match = re.fullmatch(r'    (iconPath|modelPath):\s*(.*)', line)
            if match: current[match[1]] = scalar(match[2])
            match = re.fullmatch(r'    (icon|model): \{fileID: (-?[0-9]+)(?:, guid: ([a-f0-9]{32}), type: 3)?\}', line)
            if match: current[match[1]] = dict(file_id=match[2], guid=match[3])
    if not result: raise MigrationError('No Unity item entries')
    return result


def verified_asset(project, relative, reference):
    if not reference or not reference.get('guid') or reference.get('file_id') == '0':
        raise MigrationError('missing_unity_asset_reference')
    pure = PurePosixPath(relative)
    if pure.is_absolute() or '..' in pure.parts or '\\' in relative or ':' in relative:
        raise MigrationError('unsafe_asset_path')
    project = Path(project).resolve(strict=True)
    path = (project / relative).resolve()
    if not path.is_relative_to(project) or not path.is_file(): raise MigrationError('missing_asset_file')
    meta = path.with_name(path.name + '.meta')
    if not meta.is_file(): raise MigrationError('missing_asset_meta')
    guids = re.findall(r'^guid: ([a-f0-9]{32})$', meta.read_text(encoding='utf-8-sig'), re.MULTILINE)
    if guids != [reference['guid']]: raise MigrationError('asset_guid_mismatch')
    return dict(path=pure.as_posix(), guid=reference['guid'], file_id=reference['file_id'],
                sha256=file_hash(path), meta_sha256=file_hash(meta))


def visual_for(project, visual):
    if not visual: raise MigrationError('missing_unity_entry')
    icon = visual.get('iconPath', '')
    model = visual.get('modelPath', '')
    if not icon.startswith('icon/item/') or not icon.endswith('.tga'):
        raise MigrationError('unsupported_icon_path')
    if not model.startswith('item/') or not model.endswith('.gr2'):
        raise MigrationError('unsupported_ground_model_path')
    return dict(icon=verified_asset(project, 'Assets/Metin2/Generated/ItemIcons/' + icon, visual.get('icon')),
                ground_model=verified_asset(project, 'Assets/Metin2/Converted/' + model[:-4] + '.fbx', visual.get('model')))


def read_locales(project):
    root = Path(project) / 'Assets/Metin2/Resources/Metin2/Locales'
    locales, hashes = {}, {}
    for path in sorted(root.glob('*_items.json')):
        rows = strict_json(path.read_text(encoding='utf-8-sig'))['entries']
        entries = {}
        for row in rows:
            # Explicit ranges need semantic conversion; do not expand by ID arithmetic.
            if type(row['vnum']) is not int or row['vnum'] in entries:
                raise MigrationError('Duplicate/invalid localization identity')
            entries[row['vnum']] = row
        locales[path.name[:-11]] = entries
        hashes[path.relative_to(project).as_posix()] = file_hash(path)
    return locales, hashes


def translation_for(row, code, locales):
    texts, issues = {}, []
    for language in sorted(set(locales) | set(SQL_LOCALES)):
        old = locales.get(language, {}).get(row['vnum'], {})
        if old.get('range', 0): issues.append(dict(language=language, reason='locale_range_requires_conversion'))
        name = row[SQL_LOCALES[language]] if language in SQL_LOCALES else old.get('name', '')
        # Preserve source descriptions exactly. A damaged text is reported,
        # never "fixed" by discarding bytes or substituted client garbage.
        texts[language] = dict(name=name, summary=old.get('summary', ''), description=old.get('description', ''))
        for field, value in texts[language].items():
            if not isinstance(value, str): raise MigrationError('Invalid localization text')
            if '\ufffd' in value: issues.append(dict(language=language, field=field, reason='replacement_character'))
    return dict(name_key='item.' + code + '.name', description_key='item.' + code + '.description',
                texts=texts, review_issues=issues)


def convert(tables, plan, preserved, project, visuals, locales):
    source = tables['player.item_proto']
    if len({row['vnum'] for row in source}) != len(source): raise MigrationError('Duplicate source identity')
    mapping = {r['source_id']: r for r in plan['mapping']}
    if set(mapping) != {r['vnum'] for r in source} or len(mapping) != len(plan['mapping']):
        raise MigrationError('Incomplete/duplicate full mapping')
    extra_ids = {r['vnum'] for r in tables.get('player.item_extra_proto', [])}
    packet = copy.deepcopy(preserved)
    model = Catalog(packet)
    accepted, blocked, translations, assets = [], [], {}, {}
    for row in sorted(source, key=lambda r: r['vnum']):
        if row['type'] not in (0, 18): continue
        errors = plain_token_errors(row, extra_ids)
        ref = mapping[row['vnum']]
        if ref['status'] != 'candidate' or ref['refinement_level'] != 0 or not ref['target'] or ref['target']['category_id'] != 10:
            errors.append('incompatible_family')
        if not errors:
            try: visual = visual_for(project, visuals.get(row['vnum']))
            except MigrationError as error: errors.append(str(error))
        if errors:
            blocked.append(dict(source_id=row['vnum'], source_row_hash=value_hash(row), reasons=sorted(set(errors))))
            continue
        key, code = ref['target'], ref['content_code']
        document = model.new(10)
        base = document['base']
        base.update(local_id=str(key['local_id']), content_code=code, name_key='item.' + code + '.name',
                    description_key='item.' + code + '.description', icon_asset=visual['icon']['path'],
                    inventory_height=str(row['size']), max_stack=str(SOURCE_STACK_LIMIT if row['flag'] & STACK_FLAG else 1),
                    min_level=str(max([row['limitvalue' + str(i)] for i in range(2) if row['limittype' + str(i)] == 1], default=0)))
        document['child']['local_id'] = base['local_id']
        document['use'].update(allowed_classes=str(15 & ~(row['antiflag'] >> 2)),
                               allowed_sexes=str(3 & ~((row['antiflag'] & 1) << 1 | (row['antiflag'] & 2) >> 1)))
        document['trade'].update(allow_drop=str(int(not row['antiflag'] & (1 << 7))),
                                 allow_sell=str(int(not row['antiflag'] & (1 << 8))))
        document['appearance'] = dict(model_asset=visual['ground_model']['path'])
        model.apply(document)
        translations[code] = translation_for(row, code, locales)
        assets[code] = visual
        accepted.append(dict(source_id=row['vnum'], item=key, source_row_hash=value_hash(row),
                             semantics='passive_token', source_names={k: row[k] for k in NAME_FIELDS}, reviewed=False))
    packet['tables'] = model.encode()
    for category, counter in plan['next_local_ids'].items():
        packet['sequences'][category] = str(max(int(counter), int(packet['sequences'][category])))
    packet.pop('hash', None)
    return dict(packet=packet, accepted=accepted, blocked=blocked, localization=translations, assets=assets,
                installable_release=False, scope='passive_quest_tokens_only',
                summary=dict(source_quest_rows=len(accepted) + len(blocked), converted=len(accepted), blocked=len(blocked),
                             blocked_reasons=dict(sorted(Counter(r for item in blocked for r in item['reasons']).items())),
                             localization_review_items=sum(bool(t['review_issues']) for t in translations.values())))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('source', 'ownership-supplement', 'mapping', 'preserved', 'project', 'helper', 'output'):
        parser.add_argument('--' + name, required=True, type=Path)
    args = parser.parse_args()
    tables, manifest = read_checkpoint(args.source)
    supplement = strict_json(args.ownership_supplement.read_text(encoding='utf-8'))
    if supplement.get('format_version') != 1 or supplement.get('checkpoint_sha256') != manifest['files']['checkpoint.sql']:
        raise MigrationError('Ownership supplement belongs to a different checkpoint')
    identity = dict(source_manifest=value_hash(manifest), ownership_supplement=value_hash(supplement))
    plan = strict_json(args.mapping.read_text(encoding='utf-8'))
    if plan != analyze(tables, value_hash(identity), supplement['owner_keys']):
        raise MigrationError('Mapping is stale or edited; explicit family review is a separate release gate')
    preserved = strict_json(args.preserved.read_text(encoding='utf-8'))
    if preserved['source_manifest_hash'] != value_hash(manifest) or not preserved['validated']:
        raise MigrationError('Preserved catalog belongs to a different checkpoint')
    expected, _ = upgrade(tables, native(args.helper, dict(action='blank')))
    supplied = copy.deepcopy(preserved['packet']); supplied.pop('hash', None)
    if supplied != expected:
        raise MigrationError('Preserved catalog content/counters changed after capture')
    if native(args.helper, dict(action='validate', tables=supplied['tables']))['hash'] != preserved['packet']['hash']:
        raise MigrationError('Preserved catalog hash mismatch')
    unity_catalog = args.project / 'Assets/Metin2/GeneratedData/Metin2ItemCatalog.asset'
    locales, locale_hashes = read_locales(args.project)
    result = convert(tables, plan, preserved['packet'], args.project, read_visuals(unity_catalog), locales)
    result['packet']['hash'] = native(args.helper, dict(action='validate', tables=result['packet']['tables']))['hash']
    result['source_identity'] = identity
    result['input_hashes'] = dict(mapping=file_hash(args.mapping), preserved=file_hash(args.preserved),
                                  unity_catalog=file_hash(unity_catalog), locales=locale_hashes)
    with args.output.open('x', encoding='utf-8', newline='\n') as stream:
        json.dump(result, stream, ensure_ascii=True, sort_keys=True, indent=2); stream.write('\n')
    print(json.dumps(dict(result['summary'], catalog_hash=result['packet']['hash'], installable_release=False)))


if __name__ == '__main__': main()
