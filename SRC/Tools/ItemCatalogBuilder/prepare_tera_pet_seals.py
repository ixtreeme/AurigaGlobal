"""Prepare the TERA pet cutover against an exported catalog, without writing to a server."""

import argparse
import json
from pathlib import Path
import re
import sys

from legacy_item_migration import MigrationError, strict_json

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'SRC/Tools/ServerManager'))
from item_catalog import Catalog, run_tool

LEGACY_QUEST = ROOT / 'integration/tera_companions/tera_companions.lua'
LOCALES = Path('Assets/Metin2/Resources/Metin2/Locales')
RACES = Path('Assets/Metin2/AddressableContent/Races/Prefabs')
PET_ICON = 'Metin2/ItemIcons/tera_pet_default.tga'


def pet_seal_mapping(conversion, deferred):
    pairs = [(int(source), int(race)) for source, race in
             re.findall(r'\[(\d+)\]\s*=\s*(\d+)', LEGACY_QUEST.read_text(encoding='utf-8'))]
    if len(pairs) != 69 or len({source for source, _ in pairs}) != 69 or len({race for _, race in pairs}) != 69:
        raise MigrationError('The original TERA release must define 69 distinct pet seals and models')
    source_ids = {source for source, _ in pairs}
    if source_ids & {row['source_id'] for row in deferred['items']}:
        raise MigrationError('A TERA pet seal is on the deferred conversion list')
    accepted = [row for row in conversion['accepted'] if row['source_id'] in source_ids]
    if len(accepted) != 69 or {row['source_id'] for row in accepted} != source_ids:
        raise MigrationError('The accepted migration must identify every original TERA pet seal once')
    by_source = {row['source_id']: row for row in accepted}
    return [(source, race, by_source[source]) for source, race in pairs]


def prepare(packet, mapping, project):
    model = Catalog(packet)
    if not (project / 'Assets/Metin2/Resources' / PET_ICON).is_file():
        raise MigrationError('Missing the Unity TERA pet seal icon')
    translations = {}
    source_ids = {source for source, _, _ in mapping}
    for path in sorted((project / LOCALES).glob('*_items.json')):
        language = path.name.removesuffix('_items.json')
        translations[language] = {row['vnum']: row for row in strict_json(path.read_bytes())['entries']
                                  if row['vnum'] in source_ids}
    if any(len(translations.get(language, {})) != 69 for language in ('en', 'hu')):
        raise MigrationError('The original English or Hungarian pet-seal names are incomplete')

    documents = []
    destinations = {}
    localization = {}
    next_id = int(packet['sequences']['15'])
    claimed_codes = {row['content_code'] for row in model.tables['item_base']}
    for source, race, migrated in mapping:
        key = migrated['item']
        old = model.item((key['category_id'], key['local_id']))
        base = old['base']
        code = base['content_code']
        if (base['category_id'] != '10' or base['retired'] != '0' or base['max_stack'] != '1'
                or migrated['refinement_level'] != 0 or old['refinement']['max_level'] != '0'
                or old['child']['dispatch_kind'] != '2' or old['child']['group_code']
                or not code.endswith(migrated['source_row_hash'][:12])):
            raise MigrationError(f'The original TERA definition has changed: {code}')
        if not (project / RACES / f'{race}.asset').is_file():
            raise MigrationError(f'Missing Unity reference for TERA model {race}')

        target = 'companion.tera.' + code.removeprefix('catalog.10.')
        if code in destinations or target in claimed_codes:
            raise MigrationError(f'The pet seal is already defined: {target}')
        destinations[code] = target
        claimed_codes.add(target)

        document = model.new(15)
        document['base'].update(base, category_id='15', local_id=str(next_id), content_code=target, icon_asset=PET_ICON)
        document['child']['local_id'] = str(next_id)
        for field in ('use', 'trade', 'appearance', 'bonuses', 'levels'):
            document[field] = old[field]
        form = next((row for row in model.tables['companion_form'] if row['mob_template_id'] == str(race)
                     and row['model_asset'] == f'Metin2RacePrefabs/{race}'), None)
        if form:
            document['child']['form_id'] = form['form_id']
        else:
            document['new_companion_form'] = dict(code=f'companion.form_race_{race}', mob_template_id=str(race),
                                                 model_asset=f'Metin2RacePrefabs/{race}')
        model.apply(document, encode_result=False)
        documents.append(document)
        next_id += 1

        localization[target] = dict(name_key=base['name_key'], description_key=base['description_key'],
                                    texts={language: dict(name=rows[source]['name'], description=rows[source]['description'])
                                           for language, rows in translations.items() if source in rows})

    return model.encode(), documents, destinations, localization


def quest_source(destinations):
    lines = ['quest tera_companions begin', '    state start begin', '        function exchange(code)',
             '            local category, local_id', '            if game.typed_item_key then',
             '                category, local_id = game.typed_item_key(code)', '            end',
             '            if category ~= 15 then',
             '                syschat("A petpecsét cseréje még nem érhető el. / Pet seal exchange is not available yet.")',
             '                return', '            end',
             '            local ok, reason = pc.typed_item_redeem({{category, local_id, 0, 1}}, 0)',
             '            if ok then',
             '                syschat("A petpecsét cseréje sikerült. Az új pecséttel megidézheted a petet. / Use the new seal to summon your pet.")',
             '            elseif reason == "inventory_full" then',
             '                syschat("Szabadíts fel helyet a leltárban. / Make room in your inventory.")',
             '            else',
             '                syschat("A petpecsét most nem cserélhető. / The pet seal cannot be exchanged now.")',
             '            end', '        end', '']
    for code, target in destinations.items():
        lines.extend((f'        when "{code}".typed_use begin',
                      f'            tera_companions.exchange("{target}")', '        end'))
    return '\n'.join([*lines, '    end', 'end', ''])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('catalog', 'conversion', 'deferred', 'unity', 'helper', 'output'):
        parser.add_argument('--' + name, required=True, type=Path)
    args = parser.parse_args()
    packet = strict_json(args.catalog.read_bytes())
    mapping = pet_seal_mapping(strict_json(args.conversion.read_bytes()), strict_json(args.deferred.read_bytes()))
    tables, documents, destinations, localization = prepare(packet, mapping, args.unity)
    support = run_tool(dict(action='runtime_support', tables=tables), packet['columns'], args.helper)
    by_key = {(entry['category'], entry['local']): entry for entry in support['items']}
    for document in documents:
        key = (15, int(document['base']['local_id']))
        if not by_key[key]['grant'] or not by_key[key]['persist']:
            raise MigrationError(f'The new TERA pet cannot be granted and stored: {document["base"]["content_code"]}')

    args.output.mkdir()
    (args.output / 'tera_companions.lua').write_text(quest_source(destinations), encoding='utf-8', newline='\n')
    with (args.output / 'catalog-review.json').open('x', encoding='utf-8', newline='\n') as review:
        json.dump(dict(expected_hash=packet['hash'], candidate_hash=support['catalog_hash'],
                       documents=documents, localization=localization), review, ensure_ascii=False, indent=2)
        review.write('\n')
    print(f'Prepared {len(documents)} TERA pet seals for review.')


if __name__ == '__main__':
    main()
