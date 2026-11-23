"""Prepare a separate pet evolution catalog for review without contacting a database."""

import argparse
import json
from pathlib import Path

from legacy_item_migration import MigrationError, strict_json
from prepare_pet_chest_catalog import EVOLUTION_TABLE, RULES, append, upgrade_pet_catalog
from upgrade_exported_catalog import native


def add_evolution(catalog, chest, recipes):
    tables = catalog['tables']
    codes = {row[2]: (row[0], row[1]) for row in tables['item_base'] if row[-1] == '0'}
    bases = {row[1]: row[0] for row in tables['growth_pet_profile']}
    gates = {(row[0], row[1]): row[2] for row in tables['growth_pet_stage']}
    materials = {tuple(row[:2]) for row in tables['item_material']}
    if len(recipes) != 3:
        raise MigrationError('Supply three evolution recipes')
    profiles = []
    for pet in chest['pets']:
        code = 'pet.growth.' + pet['family']
        profile = bases.get(code)
        if not profile or any(gates.get((profile, str(stage))) != cap for stage, cap in enumerate(('40','60','80','120'))):
            raise MigrationError(f'Missing evolution gates for {code}')
        profiles.append(profile)
    if any(row[0] in profiles for row in tables[EVOLUTION_TABLE]):
        raise MigrationError('An evolution recipe is already present for the chest pets')

    for stage, recipe in enumerate(recipes):
        if not isinstance(recipe, dict) or sorted(recipe.values()) != [3, 10, 10]:
            raise MigrationError('Each recipe requires 10, 10 and 3 units of distinct materials')
        for code, amount in sorted(recipe.items()):
            key = codes.get(code)
            if key not in materials:
                raise MigrationError(f'Missing active evolution material: {code}')
            for profile in profiles:
                append(catalog, EVOLUTION_TABLE, dict(profile_id=profile, stage=stage,
                    category_id=key[0], local_id=key[1], amount=amount))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True, help='Catalog export containing the eleven chest pets')
    parser.add_argument('--recipes', type=Path, required=True, help='Three maps of material content codes to amounts')
    parser.add_argument('--helper', type=Path, required=True, help='CatalogEditorTool built from this source')
    parser.add_argument('--output', type=Path, required=True, help='New file for review')
    args = parser.parse_args()
    if args.output.exists():
        parser.error('Choose a new output path')

    catalog = upgrade_pet_catalog(strict_json(args.source.read_bytes()))
    add_evolution(catalog, strict_json(RULES.read_bytes()), strict_json(args.recipes.read_bytes()))
    catalog['hash'] = native(args.helper, dict(action='validate', tables=catalog['tables']))['hash']
    with args.output.open('x', encoding='utf-8', newline='\n') as handle:
        json.dump(catalog, handle, ensure_ascii=False, separators=(',', ':'))
        handle.write('\n')
    print(f"Prepared pet evolution recipes. Catalog hash {catalog['hash']}")


if __name__ == '__main__':
    main()
