"""Compile reviewed evolution ingredients into catalog format 56."""

import backend as api
from pet_chest_catalog import append


TABLE = 'growth_pet_evolution_ingredient'
COLUMNS = ['profile_id', 'stage', 'category_id', 'local_id', 'amount']
V55 = [['1', '55', 'auriga.item_catalog.v55']]
V56 = [['1', '56', 'auriga.item_catalog.v56']]
GATES = ('40', '60', '80', '120')


def add_pet_evolution(packet, families, recipes):
    tables = packet['tables']
    columns = packet['columns']
    if tables['catalog_format'] == V55:
        tables['catalog_format'] = V56
        tables[TABLE] = []
        columns[TABLE] = COLUMNS
    elif tables['catalog_format'] != V56 or columns.get(TABLE) != COLUMNS or TABLE not in tables:
        raise api.EditorError('A petfejlesztéshez 56-os tárgykatalógus szükséges.')

    if len(families) != 11 or len(set(families)) != 11:
        raise api.EditorError('A petláda 11 fejlődési profilja szükséges.')
    if len(recipes) != 3 or [recipe['level'] for recipe in recipes] != [40, 60, 80]:
        raise api.EditorError('A 40., 60. és 80. szinthez egy-egy recept szükséges.')
    profiles_by_code = {row[1]: row[0] for row in tables['growth_pet_profile']}
    stages = {(profile, stage): cap for profile, stage, cap, _ in tables['growth_pet_stage']}
    profiles = [profiles_by_code.get('pet.growth.' + family) for family in families]
    if any(not profile or tuple(stages.get((profile, str(stage))) for stage in range(4)) != GATES
           for profile in profiles):
        raise api.EditorError('A petláda típusos fejlődési profiljait előbb jóvá kell hagyni.')
    if any(row[0] in profiles for row in tables[TABLE]):
        raise api.EditorError('Ezekhez a petekhez már van fejlődési recept.')

    materials = {tuple(row[:2]) for row in tables['item_material']}
    available_column = columns['item_definition_rules'].index('available')
    unavailable = {tuple(row[:2]) for row in tables['item_definition_rules']
                   if row[2] == '0' and row[available_column] != '1'}
    keys = {row[2]: tuple(row[:2]) for row in tables['item_base'] if row[-1] == '0'}

    for stage, recipe in enumerate(recipes):
        ingredients = recipe['ingredients']
        if not isinstance(ingredients, dict) or sorted(ingredients.values()) != [3, 10, 10]:
            raise api.EditorError('Egy evolúcióhoz három különböző alapanyag kell, 10, 10 és 3 darab.')
        for code, amount in sorted(ingredients.items()):
            key = keys.get(code)
            if key not in materials or key in unavailable:
                raise api.EditorError('Az evolúció egyik alapanyaga nem érhető el a tárgykatalógusban.')
            for profile in profiles:
                append(packet, TABLE, dict(profile_id=profile, stage=stage, category_id=key[0],
                                           local_id=key[1], amount=amount))


def is_empty_schema_upgrade(before, current):
    if before['tables']['catalog_format'] != V55 or current['tables']['catalog_format'] != V56:
        return False
    if current['tables'].get(TABLE) != [] or current['columns'].get(TABLE) != COLUMNS:
        return False
    if set(current['tables']) != set(before['tables']) | {TABLE} or current['sequences'] != before['sequences']:
        return False
    def rows(values):
        return sorted(tuple((cell is not None, cell or '') for cell in row) for row in values)
    for name, values in before['tables'].items():
        if current['columns'][name] != before['columns'][name]:
            return False
        if name != 'catalog_format' and rows(current['tables'][name]) != rows(values):
            return False
    old_live, new_live = before['live_drops'], current['live_drops']
    # The schema upgrade changes the base hash, so it can issue a new live release for the same drops.
    if set(old_live) != set(new_live) or any(old_live[name] != new_live[name]
                                           for name in ('supported', 'containers_supported')):
        return False
    for name in ('container_groups', 'container_entries'):
        if rows(old_live[name]) != rows(new_live[name]):
            return False
    old_drops, new_drops = old_live['tables'], new_live['tables']
    return (set(old_drops) == set(new_drops)
            and all(rows(old_drops[name]) == rows(new_drops[name]) for name in old_drops))
