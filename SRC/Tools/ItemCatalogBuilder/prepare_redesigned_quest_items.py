"""Build the two independent quest-item review packages from a catalog snapshot."""

import argparse
import copy
import gzip
import hashlib
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[3]
RULES = ROOT / 'config/item_combat'
sys.path.insert(0, str(ROOT / 'SRC/Tools/ServerManager'))
from item_catalog import Catalog
from quest_costume_voucher_review import vouchers
from quest_item_review import definition, text


def load(path):
    return json.loads(path.read_bytes())


def write(path, value):
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')


def clock_rule(category, local, clock, duration):
    return dict(category_id=str(category), local_id=str(local), refinement_level='0',
                display_refinement_offset='0', rarity='0', allowed_empires='7', clock_kind=str(clock),
                duration_ms=str(duration), allow_account_storage='1', allow_acquire='1', allow_unequip='1',
                minimum_grant_count='1', birth_magic_chance_bp='0', birth_socket_chance_bp='0',
                birth_addon='0', unique_group='', available='1', attribute_reroll_group='1',
                weapon_effect_alternate='0', birth_open_stone_sockets='0', birth_attribute_profile_id=None)


def choice_handler(choices):
    lines = ['        function choose(worn_time)', '            local hu = pc.get_language() == "hu"',
             '            if not pc.can_warp() then return end', '            local pets = {']
    for row, reward in choices:
        fields = [row['hu'], row['en'], row['effect_hu'], row['effect_en'], row['code']]
        lines.append('                {' + ', '.join(map(text, fields)) + '},')
    lines.extend('''            }
            local first = 1
            while true do
                local last = math.min(first + 4, #pets)
                local options = {}
                for i = first, last do
                    options[#options + 1] = hu and pets[i][1] or pets[i][2]
                end
                local next_page = 0
                if last < #pets then
                    next_page = #options + 1
                    options[next_page] = hu and "Tovább" or "Next"
                end
                local prev_page = 0
                if first > 1 then
                    prev_page = #options + 1
                    options[prev_page] = hu and "Vissza" or "Previous"
                end
                options[#options + 1] = hu and "Mégse" or "Cancel"
                local selected = select_table(options)
                if not selected or selected < 1 or selected >= #options then return end
                if selected == next_page then
                    first = last + 1
                elseif selected == prev_page then
                    first = first - 5
                else
                    local pet = pets[first + selected - 1]
                    say(hu and pet[1] or pet[2])
                    say(hu and pet[3] or pet[4])
                    say(hu and "A pecsét meglévő idejét kapja meg." or "Your seal's remaining time carries over.")
                    if select(hu and "Beváltás" or "Redeem", hu and "Mégse" or "Cancel") == 1 then
                        local prefix = worn_time and "companion.timed.inherited." or "companion.timed.redesign."
                        pet_system.exchange_fixed_seal(prefix .. pet[5])
                    end
                    return
                end
            end
        end
        when "quest.pet_choice.deadline".typed_use begin
            pet_system.choose(false)
        end
        when "quest.pet_choice.remaining".typed_use begin
            pet_system.choose(true)
        end'''.splitlines())
    return lines


def pets(model, first, unity, source):
    rows = load(RULES / 'redesigned_quest_pet_seals.json')
    forms = {(r['mob_template_id'], r['model_asset']): r['form_id'] for r in model.tables['companion_form']}
    companion_first = int(model.packet['sequences']['15'])
    documents, labels, handlers, choices, missing = [], {}, [], [], []
    for offset, row in enumerate(rows):
        slug, race = row['code'], row['race']
        seal = 'quest.pet_seal.' + slug
        reward = 'companion.timed.redesign.' + slug
        duration = str(row['days'] * 86400000)
        ref = unity / f'Assets/Metin2/AddressableContent/Races/Prefabs/{race}.asset'
        if not ref.is_file():
            missing.append((row['source_id'], race))
            continue
        doc = definition(model, 10, first + offset, seal, 'icon/item/53001.tga')
        doc['child']['group_code'] = reward
        rule = clock_rule(10, first + offset, 3, duration)
        doc['definition_rules'] = [rule]
        pet = definition(model, 15, companion_first + offset, reward, doc['base']['icon_asset'])
        asset = f'Metin2RacePrefabs/{race}'
        pet['child'].update(companion_kind='1', clock_kind='3', duration_ms=duration)
        form = forms.get((str(race), asset))
        if form:
            pet['child']['form_id'] = form
        else:
            pet['new_companion_form'] = dict(code=f'companion.form_race_{race}', mob_template_id=str(race), model_asset=asset)
        pet['definition_rules'] = [dict(rule, category_id='15', local_id=pet['base']['local_id'])]
        pet['bonuses'] = [dict(bonus_id=k, value_scaled=str(v)) for k, v in row['bonuses'].items()]
        documents.extend((doc, pet))
        labels[seal] = [f'{row["hu"]} pecsétje ({row["days"]} nap)', f'{row["en"]} Seal ({row["days"]} days)']
        labels[reward] = [row['hu'], row['en']]
        handlers.extend([f'        when {text(seal)}.typed_use begin',
                         f'            pet_system.exchange_fixed_seal({text(reward)})', '        end'])
        if all(previous['race'] != race for previous, _ in choices):
            choices.append((row, reward))
            # Worn-time variants may only be born in a validated exchange.
            # The ceiling is the storage limit, never a grant or a new budget.
            alternate = copy.deepcopy(pet)
            inherited = 'companion.timed.inherited.' + slug
            local = str(companion_first + len(rows) + len(choices) - 1)
            alternate['base'].update(content_code=inherited, name_key=pet['base']['name_key'],
                                     description_key=pet['base']['description_key'], local_id=local)
            alternate['child'].update(local_id=local, clock_kind='4', duration_ms=str(2**63 - 1))
            alternate['definition_rules'] = [clock_rule(15, local, 4, 2**63 - 1)]
            documents.append(alternate)
    if missing:
        raise ValueError(f'Missing Unity pet prefabs (legacy seal, race): {missing}')
    for index, (suffix, clock) in enumerate([('deadline', 2), ('remaining', 4)]):
        local = first + len(rows) + index
        code = 'quest.pet_choice.' + suffix
        chooser = definition(model, 10, local, code, 'icon/item/53001.tga')
        chooser['child']['group_code'] = 'companion.timed.choice'
        chooser['definition_rules'] = [dict(clock_rule(10, local, clock, 2**63 - 1), allow_acquire='0')]
        documents.append(chooser)
        labels[code] = ['Választható petcsere', 'Pet Exchange Seal']
    handlers.extend(choice_handler(choices))
    marker = 'function get_pet_info(itemVnum)'
    if source.count(marker) != 1:
        raise ValueError('Unexpected pet quest version')
    prefix = source.split(marker)[0].rstrip()
    return documents, labels, prefix + '\n\n' + '\n'.join(handlers) + '\n    end\nend\n', missing, choices


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for arg in ('catalog', 'data', 'unity', 'output'):
        parser.add_argument('--' + arg, type=Path, required=True)
    args = parser.parse_args()
    data = args.catalog.read_bytes()
    packet = json.loads(gzip.decompress(data) if data.startswith(b'\x1f\x8b') else data)
    model = Catalog(packet)
    first = int(packet['sequences']['10'])
    for group, name, offsets in [('vouchers', 'costume_system', first), ('pets', 'pet_system', first + 4)]:
        target = args.data / f'quest/Quest/system/{name}.lua'
        raw = target.read_bytes()
        digest = hashlib.sha256(raw.replace(b'\r\n', b'\n')).hexdigest()
        hashes = {'vouchers': {'21c6dcd3d1431c3e1c61fdcdd897f1da883471ef6093a4252c4a970eae0dd870'},
                  'pets': {'fd60d860c9f719e8164a30181341bbe247aa01fd7745f65ef164b1e5031f8057',
                           'c607d3c7745242c90ff175709721d13e3b6f64b1879728466c697e0cc50528e9'}}
        if digest not in hashes[group]:
            raise ValueError(f'Unreviewed source: {target}')
        if group == 'vouchers':
            docs, names, body = vouchers(model, offsets)
            missing = []
        else:
            try:
                source = raw.decode('utf-8')
            except UnicodeDecodeError:
                source = raw.decode('cp1250')
            docs, names, body, missing, _ = pets(model, offsets, args.unity, source.replace('\r\n', '\n'))
        folder = args.output / group
        folder.mkdir(parents=True, exist_ok=True)
        docs.sort(key=lambda doc: (int(doc['base']['category_id']), int(doc['base']['local_id'])))
        write(folder / 'catalog.json', dict(expected_hash=packet['hash'], documents=docs))
        write(folder / 'names.json', names)
        write(folder / 'review.json', dict(quest=name, expected_sha256=hashlib.sha256(raw).hexdigest(), missing_models=missing))
        (folder / (name + '.lua')).write_text(body, encoding='utf-8', newline='\n')


if __name__ == '__main__':
    main()
