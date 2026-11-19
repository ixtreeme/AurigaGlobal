"""Ground Yang and trusted, variable-value guild payout pouches."""
from pathlib import Path
from full_catalog_behaviors import unused_values
from legacy_item_migration import MigrationError,file_hash

def convert_native_money(c,row):
    source=row['vnum']
    if source not in (1,50026):return False
    # Ground currency is identified by its original money type as well. An
    # unrelated source using the number1 remains under its own typed converter.
    if source==1 and row['type']!=9:return False
    expected=(9,0) if source==1 else (3,10)
    if (row['type'],row['subtype'])!=expected:raise MigrationError('original_currency_source_kind_changed')
    unused_values(row)
    server=Path(__file__).resolve().parents[2]/'Server'
    for name in ('common/item_length.h','GameServer/ecs/systems/CombatSystem.cpp','GameServer/ecs/systems/ItemSystem_LegacyBridge.cpp','Database/GuildManager.cpp','common/length.h'):c.rule_inputs[name]=file_hash(server/name)
    symbol=(server/'common/item_length.h').read_text(encoding='utf-8')
    import re
    if not re.search(r'\bITEM_ELK_VNUM\s*=\s*50026\b',symbol):raise MigrationError('original_guild_payout_symbol_changed')
    doc,icon=c.base(row,16)
    if doc['bonuses'] or doc['base']['max_stack']!='1':raise MigrationError('unexpected_currency_source_payload')
    ground=source==1;doc['money_policy']=dict(mode_kind='3' if ground else '2')
    doc['base']['content_code']='currency.ground_gold' if ground else 'currency.guild_payout'
    doc['child'].update(wallet_currency='1',amount_per_unit='1' if ground else '0')
    c.finish(row,doc,icon,[],'native_ground_yang_wallet_credit' if ground else 'native_dynamic_guild_payout_pouch')
    return True
