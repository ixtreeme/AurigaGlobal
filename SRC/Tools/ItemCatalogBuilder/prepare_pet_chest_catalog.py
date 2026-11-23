"""Prepare an additive catalog export for the eleven pet-chest eggs, offline."""

import argparse
import json
from pathlib import Path

from legacy_item_migration import MigrationError, strict_json
from upgrade_exported_catalog import native


RULES = Path(__file__).resolve().parents[3] / 'config/item_combat/pet_chest_hatch_profiles.json'
TRAINING_CODE = 'pet.training.chest_growth'
EVOLUTION_TABLE = 'growth_pet_evolution_ingredient'
EVOLUTION_COLUMNS = ['profile_id', 'stage', 'category_id', 'local_id', 'amount']


def upgrade_pet_catalog(catalog):
    tables, columns = catalog['tables'], catalog['columns']
    if tables['catalog_format'] == [['1', '55', 'auriga.item_catalog.v55']]:
        tables['catalog_format'] = [['1', '56', 'auriga.item_catalog.v56']]
        columns[EVOLUTION_TABLE] = EVOLUTION_COLUMNS
        tables[EVOLUTION_TABLE] = []
    elif (tables['catalog_format'] != [['1', '56', 'auriga.item_catalog.v56']] or
          columns.get(EVOLUTION_TABLE) != EVOLUTION_COLUMNS or EVOLUTION_TABLE not in tables):
        raise MigrationError('Expected a v55 or v56 pet catalog export')
    return catalog


def append(catalog, table, values):
    columns = catalog['columns'][table]
    if set(columns) != values.keys():
        raise MigrationError(f'Unexpected columns in {table}')
    catalog['tables'][table].append([None if values[name] is None else str(values[name]) for name in columns])


def next_profile(tables, table):
    return 1 + max((int(row[0]) for row in tables[table]), default=0)


def original_training(catalog):
    tables = catalog['tables']
    training_id = next_profile(tables, 'pet_training_profile')
    skills, ranks = {}, {}
    for _, skill_id, code, effect in tables['pet_skill_definition']:
        if not code.startswith('pet.original_'):
            continue
        if skills.setdefault(skill_id, (code, effect)) != (code, effect):
            raise MigrationError(f'Conflicting original pet skill: {code}')
    for _, skill_id, rank, value in tables['pet_skill_rank']:
        if skill_id in skills and ranks.setdefault((skill_id, rank), value) != value:
            raise MigrationError(f'Conflicting original pet skill rank: {skill_id}/{rank}')
    if len(skills) != 12 or any((skill_id, str(rank)) not in ranks for skill_id in skills for rank in range(1, 11)):
        raise MigrationError('The catalog is missing the original pet skills')

    append(catalog, 'pet_training_profile', dict(profile_id=training_id, code=TRAINING_CODE,
        hp_evolution_period=5, monster_evolution_period=7, pve_evolution_period=4, evolution_min=1, evolution_max=6))
    append(catalog, 'pet_age_tier', dict(profile_id=training_id, minimum_age_ms=0, max_hp_flat=0,
        monster_resistance_bp=0, human_resistance_bp=0))
    # The three evolution gates each unlock a slot. The fourth stays locked.
    for slot, stage in enumerate((1, 2, 3, 15)):
        append(catalog, 'pet_slot_rule', dict(profile_id=training_id, slot=slot, minimum_stage=stage, minimum_age_ms=0))
    for skill_id, (code, effect) in sorted(skills.items(), key=lambda row: int(row[0])):
        append(catalog, 'pet_skill_definition', dict(profile_id=training_id, skill_id=skill_id, code=code, effect_kind=effect))
        for rank in range(1, 11):
            append(catalog, 'pet_skill_rank', dict(profile_id=training_id, skill_id=skill_id,
                skill_rank=rank, value_scaled=ranks[(skill_id, str(rank))]))
    return training_id


def add_pet_chest(catalog, rules):
    tables, columns = catalog['tables'], catalog['columns']
    bases = {row[2]: dict(zip(columns['item_base'], row, strict=True)) for row in tables['item_base']}
    quest_rows = {tuple(row[:2]): row for row in tables['item_quest']}
    pets = rules['pets']
    experience = rules['experience_to_next_level']
    if rules['version'] != 1 or len(pets) != 11 or len(experience) != 119 or min(experience) < 10:
        raise MigrationError('Incomplete pet chest rules')
    if any(row[1] == TRAINING_CODE for row in tables['pet_training_profile']):
        raise MigrationError('The pet chest hatch profiles are already present')

    def quest_item(code):
        definition = bases.get(code)
        if not definition or definition['category_id'] != '10' or definition['retired'] != '0':
            raise MigrationError(f'Missing quest item: {code}')
        key = (definition['category_id'], definition['local_id'])
        if key not in quest_rows or quest_rows[key][2:4] != ['2', '']:
            raise MigrationError(f'Expected a scripted quest item: {code}')
        return definition

    quest_item(rules['player_choice_chest'])
    training_id = original_training(catalog)
    first_form = next_profile(tables, 'companion_form')
    first_growth = next_profile(tables, 'growth_pet_profile')
    first_hatch = next_profile(tables, 'pet_hatch_profile')
    first_seal = int(catalog['sequences']['15'])
    if first_seal <= max((int(row[1]) for row in tables['item_base'] if row[0] == '15'), default=0):
        raise MigrationError('The companion allocator would reuse an item key')

    for offset, pet in enumerate(pets):
        family = pet['family']
        egg = quest_item(pet['egg'])
        seal = quest_item(pet['seal'])
        form, growth, hatch, local = first_form + offset * 2, first_growth + offset, first_hatch + offset, first_seal + offset
        definition = dict(seal, category_id=15, local_id=local, content_code=f'companion.growth.{family}', max_stack=1)
        append(catalog, 'item_base', definition)
        for form_id, field in ((form, 'base_race'), (form + 1, 'evolved_race')):
            race = pet[field]
            append(catalog, 'companion_form', dict(form_id=form_id, code=f'pet.growth.{family}.{field}',
                mob_template_id=race, model_asset=f'Metin2RacePrefabs/{race}'))
        append(catalog, 'growth_pet_profile', dict(profile_id=growth, code=f'pet.growth.{family}',
            maximum_level=120, experience_policy=2))
        for level, total in enumerate([*experience, 0], 1):
            mob = (total // 10) * 9
            append(catalog, 'growth_pet_level', dict(profile_id=growth, level=level, mob_experience=mob, item_experience=total - mob))
        for stage, cap in enumerate((40, 60, 80, 120)):
            append(catalog, 'growth_pet_stage', dict(profile_id=growth, stage=stage, maximum_level=cap,
                form_id=form if stage < 3 else form + 1))
        append(catalog, 'item_companion', dict(category_id=15, local_id=local, companion_kind=3,
            form_id=form, riding_form_id=None, clock_kind=4, duration_ms=259200000, dungeon_only_bonus=0,
            growth_profile_id=growth, training_profile_id=training_id, flying_mount=0))
        append(catalog, 'pet_hatch_profile', dict(profile_id=hatch, code=f"pet_egg.{egg['content_code']}",
            target_category_id=15, target_local_id=local, gold_cost=100000, hp_min=30, hp_max=40, hp_step=5,
            monster_min=10, monster_max=20, monster_step=5, pve_normal_min=10, pve_normal_max=20, pve_normal_step=5))

    catalog['sequences']['15'] = str(first_seal + len(pets))
    return catalog


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True, help='Approved catalog export')
    parser.add_argument('--helper', type=Path, required=True, help='CatalogEditorTool built from the same revision')
    parser.add_argument('--output', type=Path, required=True, help='New output file for review')
    args = parser.parse_args()
    if args.output.exists() or args.output.resolve() == args.source.resolve():
        parser.error('Choose a new output path')
    rules = strict_json(RULES.read_bytes())
    catalog = add_pet_chest(upgrade_pet_catalog(strict_json(args.source.read_bytes())), rules)
    result = native(args.helper, dict(action='validate', tables=catalog['tables']))
    catalog['hash'] = result['hash']
    with args.output.open('x', encoding='utf-8', newline='\n') as handle:
        json.dump(catalog, handle, ensure_ascii=False, separators=(',', ':'))
        handle.write('\n')
    print(f"Prepared eleven pet hatch profiles. Catalog hash {catalog['hash']}")


if __name__ == '__main__':
    main()
