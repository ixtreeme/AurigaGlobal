"""Prepare typed mining sites from the local map and reviewed item migration data."""

import argparse
import json
from pathlib import Path
import re

from legacy_item_migration import MigrationError, strict_json

ROOT = Path(__file__).resolve().parents[3]
MINING_SOURCE = ROOT / 'SRC/Server/GameServer/world/mining.cpp'
BASE_CHANCE = ROOT / 'SRC/Server/common/service.h'
MOB_NAMES = ROOT / 'config/server_manager/content_names_hu.json'


def ore_spawns(data, races):
    groups = {
        group['vnum']: ({group['leader']['vnum']} | {member['vnum'] for member in group['members']}) & races
        for group in strict_json((data / 'group.json').read_bytes())['groups']
    }
    group_groups = {
        group['vnum']: set().union(*(groups.get(member['vnum'], set()) for member in group['members']))
        for group in strict_json((data / 'group_group.json').read_bytes())['groups']
    }
    spawned = set()
    for map_row in strict_json((data / 'map/index.json').read_bytes())['maps']:
        for path in (data / 'map' / map_row['name']).glob('*.json'):
            for row in strict_json(path.read_bytes()).get('regens', []):
                kind, vnum = row['type'], row['vnum']
                if kind == 'mob':
                    ores = {vnum} & races
                elif kind in ('group', 'group_aggressive'):
                    ores = groups.get(vnum, ())
                elif kind == 'group_group':
                    ores = group_groups.get(vnum, ())
                else:
                    continue
                spawned.update((map_row['index'], ore) for ore in ores)
    return spawned


def prepare(data, packet, conversion, deferred):
    raw_ores = {
        int(race): int(raw)
        for race, raw in re.findall(r'\{\s*((?:200[45]\d|3030[1-6]))\s*,\s*(506\d\d)\s*,\s*506\d\d\s*\}',
                                    MINING_SOURCE.read_text(encoding='utf-8'))
    }
    if len(raw_ores) != 19 or len(set(raw_ores.values())) != len(raw_ores):
        raise MigrationError('Expected the original nineteen distinct ore veins')
    if set(raw_ores.values()) & {row['source_id'] for row in deferred['items']}:
        raise MigrationError('A mining reward is on the deferred item list')
    catalog = {
        (int(row[0]), int(row[1])): dict(zip(packet['columns']['item_base'], row))
        for row in packet['tables']['item_base']
    }
    accepted = {row['source_id']: row for row in conversion['accepted'] if row['source_id'] in raw_ores.values()}
    keys = {}
    for raw in raw_ores.values():
        row = accepted.get(raw)
        if not row or row['refinement_level'] != 0:
            raise MigrationError(f'Missing reviewed ore conversion: {raw}')
        key = (row['item']['category_id'], row['item']['local_id'])
        base = catalog.get(key)
        if (not base or base['retired'] != '0' or int(base['max_stack']) < 25
                or not base['content_code'].endswith(row['source_row_hash'][:12])):
            raise MigrationError(f'Ore must match its active stackable typed definition: {raw}')
        keys[raw] = key
    source = strict_json((data / 'typed_gathering.json').read_bytes())
    if source['version'] not in (1, 2, 3):
        raise MigrationError('Unknown gathering configuration version')
    pairs = ore_spawns(data, raw_ores.keys())
    if not pairs:
        raise MigrationError('No ore spawns were found in the map data')
    existing = {(s['map'], s['race']): s for s in source['sites'] if s['kind'] == 1}
    sites = [s for s in source['sites'] if s['kind'] != 1 or (s['map'], s['race']) not in pairs]
    next_id = max((s['id'] for s in source['sites']), default=0) + 1
    names = strict_json(MOB_NAMES.read_bytes())['mobs']
    chance = int(re.search(r'^#define NAGYFASZU_MINING_CHANCE\s+(\d+)', BASE_CHANCE.read_text(encoding='utf-8'), re.M)[1])
    for map_id, race in sorted(pairs):
        previous = existing.get((map_id, race))
        raw = raw_ores[race]
        category, local = keys[raw]
        sites.append(dict(id=previous['id'] if previous else next_id, map=map_id, race=race,
                          name=names[str(race)]['name'], kind=1, pool=1, range=1000, duration_ms=10000,
                          minimum_power=0, power_step=1, base_chance_bp=chance * 100,
                          power_chance_bp=0, bait_chance_bp=0,
                          rewards=[dict(category=category, local=local, count=25, weight=1)],
                          mining=dict(mission_subject=raw)))
        if not previous:
            next_id += 1
    return dict(version=3, sites=sites), len(pairs)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('data', 'catalog', 'conversion', 'deferred', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    packet = strict_json(args.catalog.read_bytes())
    config, count = prepare(args.data, packet, strict_json(args.conversion.read_bytes()),
                            strict_json(args.deferred.read_bytes()))
    with args.output.open('x', encoding='utf-8', newline='\n') as output:
        json.dump(config, output, ensure_ascii=False, indent=2)
        output.write('\n')
    print(f'Prepared {count} mining sites.')


if __name__ == '__main__':
    main()
