"""Prepare quest pet seal definitions for review without a server connection."""

import argparse
import json
from pathlib import Path
import sys

from legacy_item_migration import MigrationError, strict_json

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'SRC/Tools/ServerManager'))
from item_catalog import Catalog

RULES = ROOT / 'config/item_combat/permanent_quest_pet_seals.json'
RACES = Path('Assets/Metin2/AddressableContent/Races/Prefabs')


def prepare(packet, seals, deferred, project):
    model = Catalog(packet)
    excluded = {row['source_id'] for name in ('items', 'runtime_deferred_native')
                for row in deferred.get(name, [])}
    existing = {row['content_code']: row for row in model.tables['item_base']}
    definitions = []
    source_edits = []
    next_id = int(packet['sequences']['15'])
    for seal in seals:
        source = seal['source_code']
        race = seal['race']
        base = existing.get(source)
        clock = str(seal.get('clock', 1))
        duration = str(seal.get('duration_ms', 0))
        timed = clock in ('2', '3')
        target = ('companion.timed.' if timed else 'companion.fixed.') + source.removeprefix('catalog.10.')
        if seal['source_id'] in excluded:
            raise MigrationError(f'A deferred pet seal was selected: {source}')
        if base is None or not source.startswith('catalog.10.') or target in existing:
            raise MigrationError(f'Cannot allocate a typed pet seal for {source}')
        key = int(base['category_id']), int(base['local_id'])
        old = model.item(key)
        if (key[0] != 10 or base['retired'] != '0' or base['max_stack'] != '1'
                or old['refinement']['max_level'] != '0' or old['child']['dispatch_kind'] != '2'
                or old['child']['group_code']):
            raise MigrationError(f'The source is no longer an active quest pet seal: {source}')
        rules = next((row for row in model.tables['item_definition_rules']
                      if (row['category_id'], row['local_id'], row['refinement_level'])
                      == (base['category_id'], base['local_id'], '0')), None)
        if not rules or (rules['clock_kind'], rules['duration_ms']) != (clock, duration) or (
                rules['available'] != '1' or rules['allow_acquire'] != '1'):
            raise MigrationError(f'The source clock or acquisition policy has changed: {source}')
        if not (project / RACES / f'{race}.asset').is_file():
            raise MigrationError(f'Missing Unity reference for pet model {race}')

        document = model.new(15)
        document['base'].update(base, category_id='15', local_id=str(next_id), content_code=target)
        document['child'].update(local_id=str(next_id), clock_kind=clock, duration_ms=duration,
                                 dungeon_only_bonus='1' if race in (34004, 34009) else '0')
        for name in ('use', 'trade', 'appearance', 'levels', 'definition_rules'):
            document[name] = old[name]
        document['bonuses'] = [dict(bonus_id=bonus, value_scaled=value)
                              for bonus, value in seal['bonuses'].items()]
        asset = f'Metin2RacePrefabs/{race}'
        form = next((row for row in model.tables['companion_form']
                     if row['mob_template_id'] == str(race) and row['model_asset'] == asset), None)
        if form:
            document['child']['form_id'] = form['form_id']
        else:
            document['new_companion_form'] = dict(code=f'companion.form_race_{race}',
                                                  mob_template_id=str(race), model_asset=asset)
        model.apply(document, encode_result=False)
        definitions.append(document)
        existing[target] = document['base']
        if timed:
            old['child']['group_code'] = target
            source_edits.append(dict(original_key=key, document=old))
        next_id += 1
    return definitions, source_edits


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('catalog', 'deferred', 'unity', 'output'):
        parser.add_argument('--' + name, required=True, type=Path)
    parser.add_argument('--rules', type=Path, default=RULES)
    args = parser.parse_args()
    packet = strict_json(args.catalog.read_bytes())
    rules = strict_json(args.rules.read_bytes())
    if rules['version'] != 1:
        raise MigrationError('Unsupported quest pet seal rules')
    definitions, source_edits = prepare(packet, rules['seals'], strict_json(args.deferred.read_bytes()), args.unity)
    review = dict(expected_hash=packet['hash'], documents=definitions)
    if source_edits:
        review['source_edits'] = source_edits
    with args.output.open('x', encoding='utf-8', newline='\n') as output:
        json.dump(review, output, ensure_ascii=False, indent=2)
        output.write('\n')
    print(f'Prepared {len(definitions)} pet seals and {len(source_edits)} source updates for review.')


if __name__ == '__main__':
    main()
