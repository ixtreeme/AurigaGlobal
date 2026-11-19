"""Offline source rules for secondary books and charged recovery elixirs."""
from legacy_item_migration import MigrationError, value_hash, file_hash
from pathlib import Path
import re
from full_catalog_behaviors import unused_values
from legacy_equipment_rules import APPLY_NAMES,bonus

# ItemSystem_LegacyBridge USE_SPECIAL + unique_item.h. Item identities stop here;
# the playable catalog contains explicit skill/profile/pool data instead.
SECONDARY_BOOKS = {50304: ('combo', 122, 2), 50305: ('combo', 122, 2),
    50306: ('combo', 122, 2), 50311: ('language', 126, 20),
    50312: ('language', 127, 20), 50313: ('language', 128, 20),
    50061: ('summoning', 131, 10), 50902: ('crafting', 123, 40),
    50903: ('crafting', 123, 40), 50904: ('crafting', 123, 40),
    50600: ('mining', 124, 40)}
AUTO_POOLS = {72723: 1, 72724: 1, 72725: 1, 72726: 1,
    72727: 2, 72728: 2, 72729: 2, 72730: 2,
    76004: 2, 76005: 2, 76021: 1, 76022: 1}
RARE_SERVICES = {30617: (1, 2), 30618: (2, 2), 30619: (1, 3), 30620: (2, 3), 71052: (2, 2)}

def convert_services(c, row):
    if row['type'] != 3 or row['subtype'] != 10:
        return False
    identity = row['vnum']
    if identity==50060:
        unused_values(row,(0,1))
        if row['value0']!=130 or not 1<=row['value1']<=100:raise MigrationError('invalid_horse_point_book')
        doc,icon=c.base(row,9);doc.update(c.model.book_defaults('horse'))
        chance=str(row['value1']*100)
        for step in doc['book_steps']:step.update(success_chance_bp=chance,boosted_chance_bp=chance,delay_on_failure='0')
        c.finish(row,doc,icon,[],'horse_point_book');return True
    if identity==70251:
        unused_values(row)
        doc,icon=c.base(row,20);doc['child'].update(operation_kind='6',units='1',parameter='0',bonus_id=None,value_scaled=None)
        if doc['bonuses']:raise MigrationError('unexpected_paired_addon_material_bonus')
        c.finish(row,doc,icon,[],'paired_weapon_addon');return True
    if identity in RARE_SERVICES:
        unused_values(row)
        operation, pool = RARE_SERVICES[identity]
        table = 'player.item_attr_rare' + ('_8' if pool == 3 else '')
        rows = c.tables.get(table+'.effective', c.tables.get(table, []))
        # Source RollRare/RollRare8 choose uniformly and use the highest
        # eligible tier, capped at five. Existing native profiles use the
        # captured probability weights; equal positive weights are equivalent.
        # Refuse another dataset rather than silently changing its distribution.
        if not rows or len({r['prob'] for r in rows}) != 1 or rows[0]['prob'] <= 0:
            raise MigrationError('rare_source_pool_requires_uniform_native_weights')
        doc, icon = c.base(row, 5)
        if doc['bonuses']:
            raise MigrationError('unexpected_rare_service_bonus')
        doc['recovery'] = doc['effect'] = None
        doc.update(c.model.attribute_service_defaults())
        capacity = 2 if pool == 2 else 1
        doc['attribute_service'].update(operation_kind=str(operation), pool_kind=str(pool),
            target_mask='23', minimum_count='0' if operation == 1 else '1',
            maximum_count=str(capacity-1 if operation == 1 else capacity),
            add_count='1' if operation == 1 else '0')
        doc['attribute_tiers'] = [dict(tier='5', chance_bp='10000')]
        if identity==71052:
            original=Path('F:/SF_V3_5/SOURCE_SERVER_9999/Server/game/src/char_item.cpp')
            attributes=original.with_name('item_attribute.cpp')
            if not original.is_file() or not attributes.is_file():raise MigrationError('missing_original_reusable_rare_handler')
            body=original.read_text(encoding='utf8',errors='replace').split('case 71052 :',1)[1].split('case ITEM_AUTO_HP_RECOVERY_S:',1)[0]
            rare=attributes.read_text(encoding='utf8',errors='replace').split('bool CItem::ChangeRareAttribute()',1)[1].split('void CItem::AddRareAttribute2',1)[0]
            if 'ChangeRareAttribute()' not in body or 'SetCount' in body or 'ConsumeItem' in body or 'int nAttrLevel = 5;' not in rare or 'avail[number (0, avail.size() - 1)]' not in rare:raise MigrationError('original_reusable_rare_contract_changed')
            c.rule_inputs[str(original)]=file_hash(original);c.rule_inputs[str(attributes)]=file_hash(attributes)
            doc['attribute_service'].update(units_per_use='0',target_mask='7')
        c.finish(row, doc, icon, [], 'rare_attribute_service')
        if identity==71052:c.accepted[identity]['source_reusable_tool']=True
        c.accepted[identity]['source_rare_pool'] = dict(table=table, rows_hash=value_hash(rows),
            uniform_selection=True, maximum_tier=5)
        return True
    if identity in SECONDARY_BOOKS:
        kind, skill, maximum = SECONDARY_BOOKS[identity]
        definition = c.secondary_skills.get(skill)
        if not definition:
            raise MigrationError('missing_secondary_skill_definition:'+str(skill))
        if definition['type'] != 0 or definition['max_level'] < maximum:
            raise MigrationError('secondary_book_exceeds_skill_definition:'+str(skill))
        unused_values(row, (0,) if kind == 'combo' else (0, 1, 2) if kind == 'crafting' else (0, 1))
        chance = row['value0'] if kind == 'combo' else row['value1']
        if not 1 <= chance <= 100:
            raise MigrationError('secondary_book_chance_out_of_range')
        if kind in ('language', 'summoning') and row['value0'] != skill:
            raise MigrationError('secondary_book_skill_disagrees_with_source_handler')
        doc, icon = c.base(row, 9)
        if doc['bonuses']:
            raise MigrationError('unexpected_secondary_book_bonus')
        doc['book'].update(allowed_masteries='1' if maximum <= 20 else '7')
        doc['book_targets'] = [dict(skill_id=str(skill), refund_pool='0',
            forbidden_empire=str(skill - 125 if kind == 'language' else 0))]
        doc['book_steps'] = [dict(current_level=str(n), next_level=str(n + 1),
            min_character_level=str((30, 50)[n] if kind == 'combo' else 0),
            required_successes='1', success_chance_bp=str(chance * 100),
            boosted_chance_bp=str(min(100, chance + chance // 2) * 100),
            experience_cost='0', delay_minimum_ms='64800000',
            delay_maximum_ms='108000000', delay_on_failure='1') for n in range(maximum)]
        c.finish(row, doc, icon, [], 'secondary_skill_book')
        if kind == 'mining':
            c.accepted[identity]['source_book_dispatch_repaired'] = 'explicit_mining_handler_uses_skill_124_not_value0'
        if kind == 'crafting':
            c.accepted[identity]['source_unused_book_values'] = dict(value0=row['value0'], value2=row['value2'])
        return True
    if identity in AUTO_POOLS:
        unused_values(row, (0,))
        if row['value0'] <= 0:
            raise MigrationError('automatic_recovery_capacity_required')
        doc, icon = c.base(row, 5)
        if doc['bonuses']:
            raise MigrationError('unexpected_automatic_recovery_bonus')
        doc['base']['max_stack'] = '1'
        doc['recovery'].update(delivery_kind='4', allowed_contexts='11', allow_during_pvp='0')
        doc['effect'] = dict(delivery_kind='4', pool_kind=str(AUTO_POOLS[identity]),
            supply_kind='1', capacity_raw=str(row['value0'] * 1000), tick_interval_ms='1000',
            max_pool_per_tick_bp='700', active_max_pool_bonus_bp='400' if identity in (72726, 72730) else '0')
        c.finish(row, doc, icon, [], 'automatic_recovery')
        return True
    return False
