"""Rune dungeon economic contracts and the passive green-orc key."""
from pathlib import Path
from full_catalog_behaviors import unused_values
from legacy_item_migration import MigrationError,file_hash

def convert_dungeon_key(c,row):
    source=row['vnum']
    if source not in (89102,89103,89104) or row['type']!=3 or row['subtype']!=10:return False
    unused_values(row)
    path=Path(__file__).resolve().parents[2]/'Server/GameServer/dungeon/RuneDungeon.cpp'
    text=path.read_text(encoding='utf-8')
    if not all(token in text for token in ('kKeyFragment = 89102','kFloorKey = 89103','kKeyNpcVnum = 20507','number(1, 100) <= 5','number(1, 100) <= 3','opened >= 5')):raise MigrationError('original_rune_key_contract_changed')
    c.rule_inputs['original_rune_dungeon']=file_hash(path)
    doc,icon=c.base(row,10)
    if doc['bonuses']:raise MigrationError('unexpected_dungeon_key_bonus')
    code={89102:'dungeon.rune.fragment',89103:'dungeon.rune.floor_key',89104:'dungeon.orcs.key'}[source]
    doc['base']['content_code']=code
    if source!=89104:doc['child'].update(dispatch_kind='2',group_code=code,allowed_contexts='1',allow_observer='0')
    c.finish(row,doc,icon,[],'original_rune_key_fragment' if source==89102 else 'original_rune_floor_key' if source==89103 else 'original_passive_dungeon_key')
    c.accepted[source]['native_dungeon_role']=code
    return True
