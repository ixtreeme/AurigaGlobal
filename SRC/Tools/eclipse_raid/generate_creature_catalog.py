"""Generate numeric creature stats using the upstream class/level formulas."""
import argparse,json,math
from pathlib import Path


def generate(source,output):
    rows=json.loads(source.read_text())['creatures'];rows.sort(key=lambda r:int(r['entry']))
    lines=['// Generated numeric source data; no original models or names.',
           '// Health: CreatureBaseStats::GenerateHealth; damage: Creature::CalculateMinMaxDamage.',
           '#pragma once','#include "CreatureCatalog.hpp"','namespace EclipseRaid::ICC {',
           f'inline constexpr std::array<CreatureRecord,{len(rows)}> CreatureRecords={{{{']
    for row in rows:
        stats=row['base_statistics'];exp=int(row['exp']);period=int(row['BaseAttackTime']) or 2000
        base_damage=float(stats[['damage_base','damage_exp1','damage_exp2'][exp]])
        base_value=float(stats['attackpower'])/14*float(row['BaseVariance'])
        multiplier=float(row['DamageModifier'])*period/1000
        health=max(1,math.ceil(float(stats[f'basehp{exp}'])*float(row['HealthModifier'])))
        minimum=(base_damage+base_value)*multiplier;maximum=(base_damage*1.5+base_value)*multiplier
        ids=','.join(row[f'difficulty_entry_{i}'] for i in (1,2,3))
        lines.append(' {'+','.join([row['entry'],'{{'+ids+'}}',str(health),repr(minimum)+'f',repr(maximum)+'f',str(period),repr(float(row['speed_run'])*7)+'f',row['minlevel']])+'},')
    lines+=['}};','inline const CreatureCatalog& Creatures(){static const CreatureCatalog data(CreatureRecords.data(),CreatureRecords.size());return data;}','}']
    output.write_text('\n'.join(lines)+'\n');print('ICC_CREATURE_CATALOG',len(rows),'source templates')


if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('input',type=Path);p.add_argument('output',type=Path);a=p.parse_args();generate(a.input,a.output)
