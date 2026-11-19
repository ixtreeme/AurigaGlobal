"""Energy crystals retain the original single birth roll and online lifetime."""
from pathlib import Path
from legacy_item_migration import MigrationError,file_hash
from full_catalog_behaviors import unused_values

def convert_energy(c,row):
    if row['type']!=27 or row['subtype']!=0:return False
    original=c.blends.get(row['vnum'])
    if not original or original.get('apply_type')!='ENERGY':return False
    unused_values(row)
    values=original.get('apply_value');durations=original.get('apply_duration')
    if values!=[1,3,5,7,10] or durations!=[1800]*5:raise MigrationError('original_energy_crystal_distribution_changed')
    server=Path(__file__).resolve().parents[2]/'Server'
    birth=server/'GameServer/item/blend_item.cpp';body=birth.read_text(encoding='utf-8').split('static int FN_ECS_random_index()',1)[1].split('bool\tBlend_Item_set_value',1)[0]
    if not all(token in body for token in ('percent<=5','percent<=15','percent<=60','percent<=85')):raise MigrationError('original_energy_birth_weights_changed')
    for name in ('GameServer/item/blend_item.cpp','GameServer/combat/buff_on_attributes.cpp','GameServer/ecs/systems/AffectSystem.cpp'):c.rule_inputs[name]=file_hash(server/name)
    doc,icon=c.base(row,5);doc['bonuses']=[];doc['recovery']=doc['effect']=None;doc.update(c.model.timed_defaults(doc['base']['local_id']))
    doc['timed'].update(duration_ms='1800000',units_per_use='1',utility_kind='3')
    doc['timed_group'].update(code='effect.energy_crystal',reapply_kind='1',clock_kind='1',remove_on_death='0')
    doc['timed_bonuses']=[];doc['energy_tiers']=[dict(percent=str(value),chance_bp=str(chance)) for value,chance in zip(values,(500,1000,4500,2500,1500))]
    c.finish(row,doc,icon,[],'original_energy_crystal_birth_roll');return True
