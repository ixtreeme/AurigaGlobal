"""The original consumed bundle and finite silk bundle, not passive tokens."""
from pathlib import Path
from full_catalog_behaviors import unused_values
from legacy_item_migration import MigrationError,file_hash
from convert_quest_tokens import SOURCE_STACK_LIMIT

def convert_private_shop_bundle(c,row):
    source=row['vnum']
    if source not in (50200,71049):return False
    silk=source==71049
    if (row['type'],row['subtype'])!=((16,10) if silk else (3,10)):raise MigrationError('original_private_shop_bundle_kind_changed')
    unused_values(row,(0,2) if silk else ())
    if silk and (row['value0'],row['value2'])!=(864000,1):raise MigrationError('original_silk_bundle_lifetime_changed')
    server=Path(__file__).resolve().parents[2]/'Server'
    for name in ('common/length.h','common/service.h','GameServer/ecs/systems/SocialSystem.cpp','GameServer/ecs/systems/ItemSystem_LegacyBridge.cpp','GameServer/social/shop.cpp'):
        c.rule_inputs[name]=file_hash(server/name)
    doc,icon=c.base(row,10)
    if doc['bonuses'] or doc['base']['max_stack']!=('1' if silk else str(SOURCE_STACK_LIMIT)):raise MigrationError('unexpected_private_shop_bundle_payload')
    code='shop.silk_bundle' if silk else 'shop.bundle'
    doc['base']['content_code']=code
    doc['child'].update(dispatch_kind='2',group_code=code,allowed_contexts='1',allow_observer='0')
    c.finish(row,doc,icon,[],'original_silk_private_shop_bundle' if silk else 'original_consumed_private_shop_bundle')
    c.accepted[source]['private_shop_role']=2 if silk else 1
    return True
