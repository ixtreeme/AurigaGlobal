"""Editor documents and local administrative transport for the typed catalog.

The native helper validates with the game's catalog builder and commits with
the existing prepared SQL engine. This module never constructs SQL.

Profiles are shared content. Editing one item never changes another item: a
profile used elsewhere is copied first (copy-on-write), and a profile this
edit leaves unused is removed together with its child rows.
"""
import configparser
import copy
import datetime
from decimal import Decimal, InvalidOperation
import json
import os
from pathlib import Path
import secrets
import subprocess
import sys

import backend as api

CATEGORIES = {1: 'Fegyver', 2: 'Páncél', 3: 'Ékszer', 4: 'Kosztüm', 5: 'Fogyasztható', 6: 'Alapanyag', 7: 'Foglalatkő', 8: 'Tároló', 9: 'Képességkönyv', 10: 'Küldetéstárgy', 11: 'Eszköz', 12: 'Hal és csali', 13: 'Átváltozás', 14: 'Lélek és lélektekercs', 15: 'Pet és mount hívó', 16: 'Pénzjegy', 17: 'Különleges petellátmány', 18: 'Kulcs'}
CHILD_TABLES = {1: 'item_weapon', 2: 'item_armor', 3: 'item_accessory', 4: 'item_costume', 5: 'item_consumable', 6: 'item_material',
                7: 'item_socket_stone', 8: 'item_container', 9: 'item_skill_book', 10: 'item_quest', 11: 'item_tool', 12: 'item_fishing', 13: 'item_polymorph', 14: 'item_soul', 15: 'item_companion', 16: 'item_currency_token', 17: 'item_special', 18: 'item_container_key'}
CODE_PREFIX = {1: 'weapon', 2: 'armor', 3: 'accessory', 4: 'costume', 5: 'consumable', 6: 'material', 8: 'container', 9: 'skill_book', 10: 'quest', 11: 'tool', 12: 'fishing', 13: 'polymorph', 14: 'soul', 15: 'companion',
               7: 'socket_stone', 16: 'currency_token', 17: 'special', 18: 'container_key'}
STACKABLE = (5, 6, 7, 8, 9, 10, 12, 13, 14, 16, 17, 18)
LOOT_FIELDS = ['entry_no', 'reward_kind', 'item_category_id', 'item_local_id', 'amount', 'weight', 'rare_attribute_chance_bp']
REFINEMENT_KIND = {1: 2, 2: 3, 3: 4, 4: 6, 7: 5, 11: 7, 15: 9}
EFFECT_TABLES = {1: 'instant_recovery_profile', 2: 'reserve_recovery_profile', 3: 'periodic_recovery_profile', 4: 'automatic_recovery_profile'}
RESISTANCES = ['physical_bp','magical_bp','sword_bp','dagger_bp','bow_bp','two_handed_bp',
               'bell_bp','fan_bp','mount_spear_bp','fire_bp','electric_bp','ice_bp','wind_bp','earth_bp','dark_bp']
GAINS = ['physical_attack_gain_raw','magical_attack_gain_raw','physical_defense_gain_raw','magical_defense_gain_raw']
# Profile table -> (id column, referencing (table, column) pairs, child tables).
PROFILES = {
    'item_use_policy': ('policy_id', [('item_base', 'use_policy_id')], []),
    'item_trade_policy': ('policy_id', [('item_base', 'trade_policy_id')], []),
    'item_appearance_profile': ('profile_id', [('item_base', 'appearance_profile_id')], []),
    'item_attack_profile': ('profile_id', [('item_weapon', 'attack_profile_id')], []),
    'item_ammo_policy': ('policy_id', [('item_weapon', 'ammo_policy_id')], []),
    'item_resistance_profile': ('profile_id', [('item_armor', 'resistance_profile_id'), ('item_accessory', 'resistance_profile_id')], []),
    'bonus_profile': ('profile_id', [('item_base', 'base_bonus_profile_id'), ('refinement_level', 'extra_bonus_profile_id'), ('timed_effect_profile', 'bonus_profile_id'), ('toggle_effect_profile', 'bonus_profile_id')], ['bonus_profile_entry']),
    'refinement_profile': ('profile_id', [('item_base', 'refinement_profile_id')], ['refinement_level']),
    'consumable_cooldown_profile': ('profile_id', [('recovery_profile', 'cooldown_profile_id'), ('timed_effect_profile', 'cooldown_profile_id'), ('toggle_effect_profile', 'cooldown_profile_id')], []),
    'socket_service_profile': ('profile_id',[('item_consumable','socket_service_profile_id')],[]),
    'recovery_profile': ('profile_id', [('item_consumable', 'recovery_profile_id')], list(EFFECT_TABLES.values())),
    'timed_effect_profile': ('profile_id', [('item_consumable', 'timed_effect_profile_id')], []),
    'toggle_effect_profile': ('profile_id', [('item_consumable', 'toggle_effect_profile_id')], []),
    'skill_book_profile': ('profile_id', [('item_skill_book', 'profile_id')], ['skill_book_target', 'skill_book_step']),
    'loot_profile': ('profile_id', [('item_container', 'loot_profile_id'), ('item_fishing', 'use_loot_profile_id')], ['loot_entry']),
    'polymorph_profile': ('profile_id',[('item_polymorph','profile_id')],['polymorph_skill_rule','polymorph_mastery_rule']),
    'pet_training_profile': ('profile_id',[('item_companion','training_profile_id')],['pet_skill_rank','pet_skill_definition','pet_slot_rule','pet_age_tier']),
    'pet_supply_profile': ('profile_id',[('item_special','profile_id')],['pet_study_rule','pet_extension_rule','pet_enchant_rule','pet_skin_rule']),
    'pet_hatch_profile': ('profile_id',[('item_container','hatch_profile_id')],[]),
    'growth_pet_profile': ('profile_id',[('item_companion','growth_profile_id')],['growth_pet_level','growth_pet_stage']),
    'soul_profile': ('profile_id',[('item_soul','profile_id')],['soul_level']),
    'tool_profile': ('profile_id', [('item_tool','profile_id')], ['tool_level']),
}


def scaled(text, scale=1000):
    try:
        value = Decimal(str(text).strip().replace(',', '.')) * scale
        if not value.is_finite() or value != value.to_integral_value(): raise ValueError()
        return int(value)
    except (InvalidOperation, ValueError):
        raise api.EditorError('Érvénytelen szám vagy túl sok tizedesjegy: ' + str(text)) from None


def display(value, scale=1000):
    return format(Decimal(int(value)) / scale, 'f').rstrip('0').rstrip('.') if int(value) % scale else str(int(value) // scale)


def text(value):
    return None if value is None else str(value)


class Catalog:
    def __init__(self, packet):
        self.packet = copy.deepcopy(packet)
        self.tables = {name: [dict(zip(packet['columns'][name], row)) for row in rows]
                       for name, rows in packet['tables'].items()}

    def row(self, table, value, column='profile_id'):
        if value is None: return None
        return next((row for row in self.tables[table] if row[column] == str(value)), None)

    def linked(self, table, base, field, column='profile_id'):
        return copy.deepcopy(self.row(table, base.get(field), column))

    def entries(self, profile_id):
        return [dict(bonus_id=r['bonus_id'], value_scaled=r['value_scaled'])
                for r in self.tables['bonus_profile_entry'] if profile_id is not None and r['profile_id'] == str(profile_id)]

    def items(self):
        return sorted(copy.deepcopy(self.tables['item_base']), key=lambda r: (int(r['category_id']), int(r['local_id'])))

    def choices(self, table, column='profile_id'):
        return sorted(((r[column], r.get('code', r[column])) for r in self.tables[table]), key=lambda pair: int(pair[0]))

    def item(self, key):
        base = next((copy.deepcopy(row) for row in self.tables['item_base'] if
                     (int(row['category_id']), int(row['local_id'])) == tuple(key)), None)
        if base is None: raise api.EditorError('A tárgy már nem található; frissítsd a listát.')
        category = int(base['category_id'])
        child = copy.deepcopy(next(r for r in self.tables[CHILD_TABLES[category]]
                                   if r['category_id'] == base['category_id'] and r['local_id'] == base['local_id']))
        refinement = self.linked('refinement_profile', base, 'refinement_profile_id')
        levels = sorted([copy.deepcopy(r) for r in self.tables['refinement_level'] if r['profile_id'] == refinement['profile_id']],
                        key=lambda r: int(r['level']))
        for level in levels:
            level['bonuses'] = self.entries(level['extra_bonus_profile_id'])
        result = dict(base=base, child=child, use=self.linked('item_use_policy', base, 'use_policy_id', 'policy_id'),
                      trade=self.linked('item_trade_policy', base, 'trade_policy_id', 'policy_id'),
                      appearance=self.linked('item_appearance_profile', base, 'appearance_profile_id'),
                      refinement=refinement, levels=levels, bonuses=self.entries(base['base_bonus_profile_id']))
        result['pet_food_rules']=[{k:v for k,v in r.items() if k not in ('category_id','local_id')} for r in sorted(self.tables['pet_feed_rule'],key=lambda r:int(r['service_kind'])) if (int(r['category_id']),int(r['local_id']))==tuple(key)]
        if category==8:result['hatch']=self.linked('pet_hatch_profile',child,'hatch_profile_id')
        if category == 4:
            result['visuals'] = [{k: r[k] for k in ('character_class', 'sex', 'model_asset')} for r in self.tables['item_costume_visual']
                                 if r['category_id'] == base['category_id'] and r['local_id'] == base['local_id']]
        if category == 1:
            result.update(attack=self.linked('item_attack_profile', child, 'attack_profile_id'),
                          ammo=self.linked('item_ammo_policy', child, 'ammo_policy_id', 'policy_id'))
        if category in (2, 3): result['resistance'] = self.linked('item_resistance_profile', child, 'resistance_profile_id')
        if category == 5:
            result.update(toggle=None, toggle_group=None, toggle_bonuses=[],socket_service=None)
            if child.get('socket_service_profile_id') is not None:
                result.update(socket_service=self.linked('socket_service_profile',child,'socket_service_profile_id'),recovery=None,effect=None,timed=None,timed_group=None,timed_bonuses=[],cooldown=None)
            elif child.get('toggle_effect_profile_id') is not None:
                toggle = self.linked('toggle_effect_profile', child, 'toggle_effect_profile_id')
                result.update(toggle=toggle, toggle_group=self.linked('toggle_effect_group', toggle, 'group_id', 'group_id'),
                              toggle_bonuses=self.entries(toggle['bonus_profile_id']), recovery=None, effect=None,
                              timed=None, timed_group=None, timed_bonuses=[],
                              cooldown=self.linked('consumable_cooldown_profile', toggle, 'cooldown_profile_id'))
            elif child.get('timed_effect_profile_id') is not None:
                timed = self.linked('timed_effect_profile', child, 'timed_effect_profile_id')
                result.update(timed=timed, timed_group=self.linked('timed_effect_group', timed, 'group_id', 'group_id'),
                              timed_bonuses=self.entries(timed['bonus_profile_id']), recovery=None, effect=None,
                              cooldown=self.linked('consumable_cooldown_profile', timed, 'cooldown_profile_id'))
            else:
                recovery = self.linked('recovery_profile', child, 'recovery_profile_id')
                result.update(recovery=recovery, effect=copy.deepcopy(self.row(EFFECT_TABLES[int(recovery['delivery_kind'])], recovery['profile_id'])),
                              timed=None, timed_group=None, timed_bonuses=[],
                              cooldown=self.linked('consumable_cooldown_profile', recovery, 'cooldown_profile_id'))
        if category == 9:
            book = self.linked('skill_book_profile', child, 'profile_id')
            result.update(book=book, book_targets=self.book_rows('skill_book_target', book['profile_id'], 'skill_id'),
                          book_steps=self.book_rows('skill_book_step', book['profile_id'], 'current_level'))
        if category == 17:
            result.update(supply=self.linked('pet_supply_profile',child,'profile_id'))
            for name,table in [('pet_study','pet_study_rule'),('pet_extension','pet_extension_rule'),('pet_enchant','pet_enchant_rule'),('pet_skin','pet_skin_rule')]:result[name]=self.linked(table,child,'profile_id')
        if category == 15:
            result.update(new_companion_form=None,new_riding_form=None,riding_same_form=False)
            result.update(self.growth_defaults())
            result['training']=self.linked('pet_training_profile',child,'training_profile_id')
            if result['training']:
                identity=result['training']['profile_id']
                for name,table,keys in self.training_tables():
                    result[name]=[{k:v for k,v in r.items() if k!='profile_id'} for r in self.tables[table] if r['profile_id']==identity]
                    result[name].sort(key=lambda row:tuple(int(row[k]) for k in keys))
            if child.get('growth_profile_id'):
                result['growth']=self.linked('growth_pet_profile',child,'growth_profile_id')
                result['growth_levels']=[{k:v for k,v in r.items() if k!='profile_id'} for r in self.book_rows('growth_pet_level',child['growth_profile_id'],'level')]
                result['growth_stages']=[{k:v for k,v in r.items() if k!='profile_id'} for r in self.book_rows('growth_pet_stage',child['growth_profile_id'],'stage')]
        if category == 14:
            result['soul_levels']=[{k:v for k,v in r.items() if k!='profile_id'} for r in self.book_rows('soul_level',child['profile_id'],'level')] if child['profile_id'] else [self.soul_level(0)]
        if category == 13:
            result.update(poly=self.linked('polymorph_profile',child,'profile_id'),
                          poly_skills=self.book_rows('polymorph_skill_rule',child['profile_id'],'skill_level'),
                          poly_masteries=self.book_rows('polymorph_mastery_rule',child['profile_id'],'mastery'),new_poly_form=None)
        if category == 11:
            result['tool'] = self.linked('tool_profile',child,'profile_id')
            result['tool_levels'] = [{k:v for k,v in r.items() if k!='profile_id'} for r in self.book_rows('tool_level',child['profile_id'],'level')]
        if category in (8,12):
            loot = self.linked('loot_profile', child, 'loot_profile_id' if category==8 else 'use_loot_profile_id')
            if loot is not None:
                loot['entries'] = self.loot_entries(loot['profile_id'])
            result['loot'] = loot
        return result

    def book_rows(self, table, profile_id, order):
        return [dict(r) for r in sorted(self.tables[table], key=lambda r: int(r[order])) if r['profile_id'] == str(profile_id)]

    def loot_entries(self, profile_id):
        rows = [r for r in self.tables['loot_entry'] if r['profile_id'] == str(profile_id)]
        return [{name: row[name] for name in LOOT_FIELDS} for row in sorted(rows, key=lambda r: int(r['entry_no']))]

    def duplicate(self, key):
        """A new prototype based on a saved item. Its key/code are new; the
        normal copy-on-write path protects every shared source profile."""
        document = self.item(key)
        category = int(document['base']['category_id'])
        fresh = self.new(category)['base']
        local = fresh['local_id']
        existing = {row['content_code'] for row in self.tables['item_base']}
        number = 1
        while True:
            suffix = '_copy_' + local + (('_' + str(number)) if number > 1 else '')
            code = document['base']['content_code'][:96-len(suffix)].rstrip('.') + suffix
            if code not in existing: break
            number += 1
        document['base'].update(local_id=local, content_code=code, retired='0')
        document['child'].update(local_id=local)
        return document

    def new(self, category):
        if category not in CATEGORIES: raise api.EditorError('Ez a kategória még nem szerkeszthető.')
        local = int(self.packet['sequences'][str(category)])
        if not 1 <= local <= 4294967295: raise api.EditorError('A kategória azonosítótartománya elfogyott.')
        code = f'{CODE_PREFIX[category]}.new_item_{local}'
        base = dict(category_id=str(category), local_id=str(local), content_code=code, name_key=f'item.{code}.name',
                    description_key=f'item.{code}.description', icon_asset='icons/item.png', inventory_width='1', inventory_height='1',
                    max_stack='200' if category in STACKABLE else '1', weight_grams='0', min_level='0', binding_policy='1',
                    use_policy_id=None, trade_policy_id=None, refinement_profile_id=None, appearance_profile_id=None, base_bonus_profile_id=None,
                    currency_kind='1', buy_amount='0', sell_amount='0', retired='0')
        item = dict(base=base, child=dict(category_id=str(category), local_id=str(local)),
                    use=dict(allowed_classes='15', allowed_sexes='3', use_during_combat='1'),
                    trade=dict(allow_trade='1', allow_drop='1', allow_sell='1'), appearance=None,
                    refinement=dict(profile_kind=str(REFINEMENT_KIND.get(category, 1)), max_level='0'),
                    levels=[dict(level='0', **{name: '0' for name in GAINS}, extra_bonus_profile_id=None, bonuses=[])], bonuses=[])
        if category == 1:
            item.update(attack=dict(interval_ticks='25', windup_ticks='10', reach_millimeters='3000', max_targets='1'),
                        ammo=dict(ammo_kind='1', units_per_hit='0'))
            item['child'].update(weapon_subtype='1', equip_slot='1', physical_attack_base_raw='0', magical_attack_base_raw='0',
                                 attack_profile_id=None, ammo_policy_id=None)
        if category in (2, 3):
            item['resistance'] = {name: '0' for name in RESISTANCES}
            item['child'].update(equip_slot='2' if category == 2 else '6', resistance_profile_id=None)
        if category == 2: item['child'].update(physical_defense_base_raw='0', magical_defense_base_raw='0')
        if category == 3: item['child']['socket_profile_id'] = None
        if category == 4:
            item['child'].update(costume_kind='1', weapon_subtype=None, clock_kind='1', duration_ms='0')
            item['visuals'] = [dict(character_class=str(c), sex=str(s), model_asset='models/costume_body.model') for c in range(1,5) for s in range(1,3)]
        item['pet_food_rules']=[]
        if category == 6: item['child']['material_kind'] = '1'
        if category == 7: item['child'].update(stone_kind='1', allowed_targets='1', minimum_socket_grade='1', primary_group='', secondary_group='')
        if category == 5:
            item.update(recovery=dict(delivery_kind='1', units_per_use='1', allowed_contexts='15', allow_during_pvp='1', cooldown_profile_id=None, arena_quota_cost='0'),
                        effect=dict(delivery_kind='1', hp_raw='1000', sp_raw='0', max_hp_bp='0', max_sp_bp='0', flat_bonus_cap_bp='10000'),
                        cooldown=None)
            item['child'].update(recovery_profile_id=None, timed_effect_profile_id=None, toggle_effect_profile_id=None,socket_service_profile_id=None)
            item['socket_service']=None
            item.update(timed=None, timed_group=None, timed_bonuses=[], toggle=None, toggle_group=None, toggle_bonuses=[])
        if category == 8:
            # A usable placeholder box: one weighted gold entry until real contents are entered.
            item['child'].update(container_kind='1', loot_profile_id=None, lock_id='0',hatch_profile_id=None)
            item['hatch']=None
            item['loot'] = dict(pick_mode='1', entries=[dict(entry_no='1', reward_kind='2', item_category_id=None, item_local_id=None,
                                                             amount='1', weight='1', rare_attribute_chance_bp='0')])
        if category == 9:
            item['child']['profile_id'] = None
            item.update(self.book_defaults())
        if category == 10:
            item['child'].update(dispatch_kind='1', group_code='', allowed_contexts='0', allow_observer='0')
        if category == 15:
            forms=self.tables['companion_form']
            item['child'].update(companion_kind='1',form_id=forms[0]['form_id'] if forms else None,riding_form_id=None,clock_kind='1',duration_ms='0',dungeon_only_bonus='0',growth_profile_id=None,training_profile_id=None)
            item.update(self.growth_defaults(),training=None)
            identity=self.allocate('companion_form','form_id')
            item.update(new_companion_form=None if forms else dict(code=f'companion.new_form_{identity}',mob_template_id='101',model_asset='models/companion.model'),new_riding_form=None,riding_same_form=False)
        if category == 14:
            item['base']['max_stack']='1';item['child'].update(soul_kind='1',damage_channel='1',profile_id=None,refine_chance_bp='0')
            item['refinement']['profile_kind']='8';item['soul_levels']=[self.soul_level(0)]
        if category == 13:
            item['base']['max_stack']='1'
            item['child'].update(profile_id=None,fixed_form_id=None)
            item.update(self.polymorph_defaults(),new_poly_form=None)
        if category == 11:
            item['child'].update(tool_kind='1',profile_id=None,fishing_pool='1')
            item.update(tool={},tool_levels=[self.tool_level(0,terminal=True)])
        if category == 12:
            item['child'].update(fishing_kind='3',use_loot_profile_id=None,grill_category_id=None,grill_local_id=None,bait_power='1')
            item['loot']=None
        if category == 16:
            item['child'].update(wallet_currency='1', amount_per_unit='1')
        if category == 17:
            item['base']['max_stack']='1';item['child'].update(behavior_kind='1',profile_id=None)
            item.update(self.pet_supply_defaults())
        if category == 18:
            item['child']['lock_id'] = '1'
        return item

    @staticmethod
    def soul_level(level):
        return dict(level=str(level),charge_capacity='60',cycle_charge_cost='60',strikes_per_cycle='500',initial_charge='0',initial_strikes='500',recharge_interval_ms='60000',first_multiplier_bp='12000',second_multiplier_bp='12000',third_multiplier_bp='12000')

    @staticmethod
    def polymorph_defaults():
        # Audited C++ initializers: 40 explicit entries; index 40 is zero-filled.
        # These are authoring defaults, not an approved balance conversion.
        powers=[10,11,11,12,13,13,14,15,16,17,18,19,20,22,23,24,26,27,29,31,33,35,37,39,41,44,46,49,52,55,59,62,66,70,74,79,84,89,94,100,0]
        return dict(poly=dict(polymorph_kind='1',mob_level_grace='20',grace_loss_numerator='3',grace_loss_denominator='10',
                              max_book_grade='0',practice_per_grade='0',book_chance_base_percent='0'),
                    poly_skills=[dict(skill_level=str(i),duration_ms=str((5 if i==0 else 35 if i==40 else 10)*60000),
                                      attack_bonus_bp=str((2+i//40)*10000),power_bp=str(powers[i]*100)) for i in range(41)],poly_masteries=[])

    @staticmethod
    def tool_level(level,terminal=False):
        return dict(level=str(level),power='0',gather_bonus_bp='0',required_points='0' if terminal else '1',
                    practice_numerator='0' if terminal else '1',practice_denominator='1',refine_chance_bp='0' if terminal else '10000',
                    failure_kind='1',failure_level=None,failure_loss_bp='0')

    @staticmethod
    def book_defaults(template='master'):
        kind = {'forget': '2', 'horse': '3'}.get(template, '1')
        book = dict(operation_kind=kind, target_mode='2' if kind == '2' else '3' if kind == '3' else '1',
                    units_per_use='1', allowed_masteries='2', allow_delay_bypass='1', bonus_policy='2')
        targets = [dict(skill_id='1', refund_pool='0', forbidden_empire='0')]
        levels = list(range(20, 30)); delay = (64800000, 108000000)
        required = {}; next_levels = {}; minimum = {}; chance, boosted = 7000, 10000
        if template == 'leadership':
            targets[0]['skill_id'] = '121'; levels = list(range(40)); book['allowed_masteries'] = '7'; delay = (18000000, 18000000)
        elif template == 'combo':
            targets[0]['skill_id'] = '122'; levels = [0, 1]; book['allowed_masteries'] = '1'; minimum = {0: 30, 1: 50}; chance, boosted = 5000, 7500
        elif template == 'language':
            targets[0].update(skill_id='126', forbidden_empire='1'); levels = list(range(20)); book['allowed_masteries'] = '1'; chance, boosted = 5000, 7500
        elif template == 'support':
            targets[0]['skill_id'] = '143'; levels = list(range(10)); book['allowed_masteries'] = '1'; required = {i: i + 2 for i in levels}; chance, boosted = 7000, 9000; delay = (10800000, 10800000)
        elif template == 'passive':
            targets[0]['skill_id'] = '221'; levels = list(range(11)) + list(range(20, 30)); book['allowed_masteries'] = '3'; minimum = {i: 90 for i in levels}
            old = [1,1,1,2,2,2,3,3,3,4,0,0,0,0,0,0,0,0,0,0,4,5,6,7,8,9,10,13,15,19]
            required = {i: old[i] + 1 for i in levels}; next_levels[10] = 20; chance, boosted = 7000, 8000
        elif template == 'forget':
            book.update(allowed_masteries='1', allow_delay_bypass='0', bonus_policy='1'); levels = list(range(1,20)); delay = (0,0); chance = boosted = 10000
            targets[0]['refund_pool'] = '1'; next_levels = {i: i - 1 for i in levels}
        elif template == 'horse':
            book.update(allowed_masteries='0', bonus_policy='1'); targets = []; levels = list(range(20)); minimum = {i: 50 for i in levels}; chance = boosted = 5000
        steps = [dict(current_level=str(i), next_level=str(next_levels.get(i, i + 1)), min_character_level=str(minimum.get(i, 0)),
                      required_successes=str(required.get(i, 1)), success_chance_bp=str(chance), boosted_chance_bp=str(boosted),
                      experience_cost='0', delay_minimum_ms=str(delay[0]), delay_maximum_ms=str(delay[1]),
                      delay_on_failure='0' if kind in ('2','3') else '1') for i in levels]
        return dict(book=book, book_targets=targets, book_steps=steps)

    @staticmethod
    def toggle_defaults(local):
        return dict(toggle=dict(group_id=None, bonus_profile_id=None, variant_id='1', duration_ms='3600000',
                                requirements='0', allowed_contexts='9', allow_during_pvp='0', cooldown_profile_id=None),
                    toggle_group=dict(group_id=None, code=f'toggle.item_{local}', remove_on_death='0'),
                    toggle_bonuses=[dict(bonus_id='26', value_scaled='1500')])

    @staticmethod
    def timed_defaults(local):
        return dict(timed=dict(group_id=None, bonus_profile_id=None, duration_ms='60000', units_per_use='1',
                               allowed_contexts='15', allow_during_pvp='1', cooldown_profile_id=None),
                    timed_group=dict(group_id=None, code=f'effect.item_{local}', reapply_kind='1', clock_kind='1', remove_on_death='1'),
                    timed_bonuses=[dict(bonus_id='26', value_scaled='1500')])

    # --- low-level table edits -------------------------------------------
    def replace(self, table, key, values):
        rows = self.tables[table]
        rows[:] = [row for row in rows if not all(row.get(k) == str(v) for k, v in key.items())]
        for value in values: rows.append({name: text(value.get(name)) for name in self.packet['columns'][table]})

    def allocate(self, table, idcol='profile_id'):
        return str(max((int(row[idcol]) for row in self.tables[table]), default=0) + 1)

    def references(self, table, profile_id):
        _, sources, _ = PROFILES[table]
        return [(source, row) for source, field in sources for row in self.tables[source] if row.get(field) == str(profile_id)]

    def profile(self, table, data, old_id, idcol='profile_id', reuse=True):
        """Return a profile ID holding `data`. An unshared old profile is
        updated in place; a shared one stays untouched (dedupe or new copy)."""
        columns = self.packet['columns'][table]
        fields = [name for name in columns if name not in (idcol, 'code')]
        values = {name: text(data.get(name)) for name in fields}
        old = self.row(table, old_id, idcol)
        if old and all(old[name] == values[name] for name in fields): return old[idcol]
        uses = len(self.references(table, old_id)) if old_id is not None else 0
        if not (reuse and old and uses <= 1):
            for row in self.tables[table]:
                if all(row[name] == values[name] for name in fields): return row[idcol]
            identity = self.allocate(table, idcol)
            old = {idcol: identity, 'code': f'editor.{table}.{identity}'}
        values.update({idcol: old[idcol], 'code': old['code']})
        self.replace(table, {idcol: old[idcol]}, [values])
        return old[idcol]

    def bonus(self, entries, old_id, source, owner):
        """Bonus profile for one owner row ((table, key) of item_base or
        refinement_level). Reused in place only when nothing else uses it."""
        wanted = sorted((str(e['bonus_id']), str(e['value_scaled'])) for e in entries or [])
        if not wanted: return None
        old = self.row('bonus_profile', old_id)
        if old and old['source_kind'] == source:
            current = sorted((e['bonus_id'], e['value_scaled']) for e in self.entries(old_id))
            if current == wanted: return old_id
            users = [(table, self.owner(table, row)) for table, row in self.references('bonus_profile', old_id)]
            if all(user == owner for user in users):
                self.replace('bonus_profile_entry', {'profile_id': old_id},
                             [dict(profile_id=old_id, bonus_id=b, value_scaled=v) for b, v in wanted])
                return old_id
        identity = self.allocate('bonus_profile')
        self.replace('bonus_profile', {'profile_id': identity}, [dict(profile_id=identity, code=f'editor.bonus.{identity}', source_kind=source)])
        self.replace('bonus_profile_entry', {'profile_id': identity},
                     [dict(profile_id=identity, bonus_id=b, value_scaled=v) for b, v in wanted])
        return identity

    def loot(self, loot, old_id, key):
        """Loot profile of one container. Entries are numbered in document
        order; a profile used by another box is copied, never edited in place."""
        if not loot: return None
        mode = str(loot['pick_mode'])
        wanted = [{name: text(entry.get(name)) for name in LOOT_FIELDS} for entry in loot['entries']]
        for number, entry in enumerate(wanted, 1): entry['entry_no'] = str(number)
        old = self.row('loot_profile', old_id)
        if old and old['pick_mode'] == mode and self.loot_entries(old_id) == wanted: return old_id
        users = [(int(r['category_id']), int(r['local_id'])) for _, r in self.references('loot_profile', old_id)] if old else []
        if old and all(user == key for user in users): identity = old_id
        else:
            identity = self.allocate('loot_profile')
            old = dict(code=f'editor.loot.{identity}')
        self.replace('loot_profile', {'profile_id': identity}, [dict(profile_id=identity, code=old['code'], pick_mode=mode)])
        self.replace('loot_entry', {'profile_id': identity}, [dict(entry, profile_id=identity) for entry in wanted])
        return identity

    @staticmethod
    def owner(table, row):
        if table == 'item_base': return row['category_id'], row['local_id']
        if table in ('timed_effect_profile', 'toggle_effect_profile'): return row['profile_id']
        return row['profile_id'], row['level']

    def referenced(self):
        return {(table, row[spec[0]]) for table, spec in PROFILES.items() for row in self.tables[table]
                if self.references(table, row[spec[0]])}

    def prune(self, candidates):
        # Remove profiles this edit left unused; children first, repeated
        # because dropping refinement levels can free their bonus profiles.
        while True:
            unused = [(table, pid) for table, pid in candidates if self.row(table, pid, PROFILES[table][0]) and not self.references(table, pid)]
            if not unused: return
            for table, pid in unused:
                idcol, _, children = PROFILES[table]
                for child in children: self.replace(child, {'profile_id': pid}, [])
                self.replace(table, {idcol: pid}, [])

    # --- document -> tables ------------------------------------------------
    def apply(self, document, original_key=None):
        document = copy.deepcopy(document)
        base, child = document['base'], document['child']
        category = int(base['category_id'])
        key = (int(base['category_id']), int(base['local_id']))
        if original_key is not None and key != tuple(original_key):
            raise api.EditorError('A kategória és a kulcs létrehozás után nem módosítható.')
        if category not in CATEGORIES: raise api.EditorError('Ez a kategória még nem szerkeszthető.')
        exists = any((int(r['category_id']), int(r['local_id'])) == key for r in self.tables['item_base'])
        if original_key is None and exists:
            # A new document must never silently replace an issued item.
            raise api.EditorError('Ez a tárgykulcs már foglalt. Frissítsd a listát, majd hozd létre újra.')
        if original_key is not None and not exists: raise api.EditorError('A tárgy már nem található; frissítsd a listát.')
        if not exists:
            # A copied document may carry the source item's IDs. Its simple
            # profiles have no owner key in profile(), so new items must enter
            # the deduplication path instead of updating the source in place.
            for field in ('use_policy_id', 'trade_policy_id', 'appearance_profile_id'): base[field] = None
            for field in ('attack_profile_id', 'ammo_policy_id', 'resistance_profile_id', 'hatch_profile_id'):
                if field in child: child[field] = None
        before = self.referenced()
        base['use_policy_id'] = self.profile('item_use_policy', document['use'], base.get('use_policy_id'), idcol='policy_id')
        base['trade_policy_id'] = self.profile('item_trade_policy', document['trade'], base.get('trade_policy_id'), idcol='policy_id')
        appearance = document.get('appearance')
        base['appearance_profile_id'] = self.profile('item_appearance_profile', appearance, base.get('appearance_profile_id')) if appearance else None
        owner = ('item_base', (str(key[0]), str(key[1])))
        base['base_bonus_profile_id'] = self.bonus(document.get('bonuses'), base.get('base_bonus_profile_id'), '1', owner)
        self.apply_refinement(document, base, key)
        if category == 1:
            child['attack_profile_id'] = self.profile('item_attack_profile', document['attack'], child.get('attack_profile_id'))
            child['ammo_policy_id'] = self.profile('item_ammo_policy', document['ammo'], child.get('ammo_policy_id'), idcol='policy_id')
        if category in (2, 3):
            child['resistance_profile_id'] = self.profile('item_resistance_profile', document['resistance'], child.get('resistance_profile_id'))
        if category == 5:
            if document.get('socket_service') is not None:
                identity=self.profile('socket_service_profile',document['socket_service'],child.get('socket_service_profile_id') if exists else None)
                child.update(recovery_profile_id=None,timed_effect_profile_id=None,toggle_effect_profile_id=None,socket_service_profile_id=identity)
            elif document.get('toggle') is not None: self.apply_toggle(document, child, key)
            elif document.get('timed') is not None: self.apply_timed(document, child, key)
            else: self.apply_recovery(document, child, key)
        if category == 9: self.apply_book(document, child, key)
        if category == 17:self.apply_pet_supply(document,child,key)
        if category == 15:self.apply_companion(document,child,key)
        if category == 14:self.apply_soul(document,child,key)
        if category == 13:self.apply_polymorph(document,child,key)
        if category == 11: self.apply_tool(document,child,key)
        if category == 12:
            child['use_loot_profile_id'] = self.loot(document.get('loot'),child.get('use_loot_profile_id'),key) if child['fishing_kind']=='1' else None
        if category == 8:
            scripted = str(child.get('container_kind')) in ('3','4')
            child['loot_profile_id'] = None if scripted else self.loot(document.get('loot'), child.get('loot_profile_id'), key)
            if child['container_kind']=='4' and not isinstance(document.get('hatch'),dict):
                raise api.EditorError('A pettojás keltetési beállításai hiányoznak.')
            child['hatch_profile_id']=self.profile('pet_hatch_profile',document.get('hatch'),child.get('hatch_profile_id')) if child['container_kind']=='4' else None
        self.replace('item_base', {'category_id': key[0], 'local_id': key[1]}, [base])
        if category == 4:
            self.replace('item_costume_visual', {'category_id': base['category_id'], 'local_id': base['local_id']},
                         [dict(v, category_id=base['category_id'], local_id=base['local_id']) for v in document['visuals']])
        self.replace(CHILD_TABLES[category], {'category_id': key[0], 'local_id': key[1]}, [child])
        self.replace('pet_feed_rule',{'category_id':key[0],'local_id':key[1]},
                     [dict(r,category_id=key[0],local_id=key[1]) for r in document.get('pet_food_rules',[])])
        self.prune(before)
        return self.encode()

    def apply_refinement(self, document, base, key):
        refinement, old = document['refinement'], base.get('refinement_profile_id')
        # The curve is part of the profile's content: a shared head is never
        # edited in place, even when only a level row changes.
        users = [row for _, row in self.references('refinement_profile', old)] if old else []
        private = bool(old) and all((int(r['category_id']), int(r['local_id'])) == key for r in users)
        profile = self.profile('refinement_profile', refinement, old if private else None)
        if any(r.get('refinement_profile_id') == profile and (int(r['category_id']), int(r['local_id'])) != key for r in self.tables['item_base']):
            profile = self.allocate('refinement_profile')
            self.replace('refinement_profile', {'profile_id': profile}, [dict(refinement, profile_id=profile, code=f'editor.refinement.{profile}')])
        base['refinement_profile_id'] = profile
        levels = []
        for row in document['levels']:
            row = dict(row, profile_id=profile)
            if 'bonuses' in row:
                row['extra_bonus_profile_id'] = self.bonus(row.pop('bonuses'), row.get('extra_bonus_profile_id'), '2',
                                                           ('refinement_level', (str(profile), str(row['level']))))
            levels.append(row)
        self.replace('refinement_level', {'profile_id': profile}, levels)

    def apply_companion(self,document,child,key):
        for section,target in [('new_companion_form','form_id'),('new_riding_form','riding_form_id')]:
            form=document.get(section)
            if not form or target=='riding_form_id' and child['companion_kind']!='2':continue
            fields=('code','mob_template_id','model_asset')
            existing=next((r for r in self.tables['companion_form'] if r['code']==form['code']),None)
            if existing and any(str(form[k])!=existing[k] for k in fields):raise api.EditorError('Ez a társformakód már más tartalommal foglalt. Új kódot adj meg.')
            identity=existing['form_id'] if existing else self.allocate('companion_form','form_id')
            if not existing:self.replace('companion_form',{'form_id':identity},[dict(form,form_id=identity)])
            child[target]=identity
        if child['companion_kind']!='2':child['riding_form_id']=None
        elif document.get('riding_same_form'):child['riding_form_id']=child['form_id']
        if child['companion_kind']=='3':
            self.apply_growth(document,child,key);self.apply_training(document,child,key)
        else:child['growth_profile_id']=None;child['training_profile_id']=None

    @staticmethod
    def training_tables():
        return [('pet_age_tiers','pet_age_tier',('minimum_age_ms',)),('pet_slot_rules','pet_slot_rule',('slot',)),
                ('pet_skills','pet_skill_definition',('skill_id',)),('pet_skill_ranks','pet_skill_rank',('skill_id','skill_rank'))]

    @staticmethod
    def training_defaults():
        return dict(training=dict(hp_evolution_period='5',monster_evolution_period='7',pve_evolution_period='4',evolution_min='1',evolution_max='6'),
            pet_age_tiers=[dict(minimum_age_ms=str(seconds*1000),max_hp_flat=str(hp),monster_resistance_bp=str(resist),human_resistance_bp=str(resist)) for seconds,hp,resist in [(0,500,200),(950400,1200,400),(2246400,2100,700),(4147200,3000,1000)]],
            pet_slot_rules=[dict(slot=str(i),minimum_stage=str(i+1 if i<3 else 0),minimum_age_ms='0' if i<3 else '1296000000') for i in range(4)],
            pet_skills=[dict(skill_id='1',code='pet.skill_1',effect_kind='7')],
            pet_skill_ranks=[dict(skill_id='1',skill_rank=str(i),value_scaled=str(i*100)) for i in range(1,11)])

    @staticmethod
    def pet_supply_defaults():
        return dict(supply=dict(supply_kind='7'),pet_study=dict(skill_id=None,selected_allowed='1',known_allowed='1',known_success_bp='7100',known_cooldown_ms='10800000'),
            pet_extension=dict(added_ms='86400000',maximum_capacity_ms='31535940000'),
            pet_enchant=dict(success_bp='3000',step='5',hp_limit='150',monster_limit='100',pve_limit='100'),
            pet_skin=dict(form_id=None,clock_kind='1',duration_ms='0'))

    def apply_training(self,document,child,key):
        if document.get('training') is None:child['training_profile_id']=None;return
        old=child.get('training_profile_id');previous=self.row('pet_training_profile',old)
        head={k:text(document['training'].get(k)) for k in self.packet['columns']['pet_training_profile'] if k not in ('profile_id','code')}
        children={table:[{k:text(v) for k,v in r.items() if k!='profile_id'} for r in document[name]] for name,table,_ in self.training_tables()}
        equal=previous and all(previous[k]==head[k] for k in head)
        for _,table,keys in self.training_tables():
            current=[{k:v for k,v in r.items() if k!='profile_id'} for r in self.tables[table] if r['profile_id']==old]
            order=lambda row:tuple(int(row[k]) for k in keys)
            equal=equal and sorted(children[table],key=order)==sorted(current,key=order)
        if equal:return
        users=[(int(r['category_id']),int(r['local_id'])) for _,r in self.references('pet_training_profile',old)] if previous else []
        identity=old if previous and all(user==key for user in users) else self.allocate('pet_training_profile')
        self.replace('pet_training_profile',{'profile_id':identity},[dict(head,profile_id=identity,code=previous['code'] if identity==old else f'editor.pet_training.{identity}')])
        for table,values in children.items():self.replace(table,{'profile_id':identity},[dict(r,profile_id=identity) for r in values])
        child['training_profile_id']=identity

    def apply_pet_supply(self,document,child,key):
        if child.get('behavior_kind')!='1':raise api.EditorError('Ismeretlen különleges tárgyviselkedés.')
        old=child.get('profile_id');previous=self.row('pet_supply_profile',old);kind=text(document['supply']['supply_kind'])
        tables=[('pet_study','pet_study_rule','1'),('pet_extension','pet_extension_rule','4'),('pet_enchant','pet_enchant_rule','6'),('pet_skin','pet_skin_rule','8')]
        children={}
        for name,table,active in tables:
            values=[]
            if active==kind:
                data=document.get(name)
                if not isinstance(data,dict):raise api.EditorError('A petellátmány típusos beállításai hiányoznak.')
                values=[dict({k:text(data.get(k)) for k in self.packet['columns'][table] if k not in ('profile_id','supply_kind')},supply_kind=kind)]
            children[table]=values
        equal=previous and previous['supply_kind']==kind
        for table,values in children.items():equal=equal and values==[{k:v for k,v in r.items() if k!='profile_id'} for r in self.tables[table] if r['profile_id']==old]
        if equal:return
        users=[(int(r['category_id']),int(r['local_id'])) for _,r in self.references('pet_supply_profile',old)] if previous else []
        identity=old if previous and previous['supply_kind']==kind and all(user==key for user in users) else self.allocate('pet_supply_profile')
        self.replace('pet_supply_profile',{'profile_id':identity},[dict(profile_id=identity,code=previous['code'] if identity==old else f'editor.pet_supply.{identity}',supply_kind=kind)])
        for table,values in children.items():self.replace(table,{'profile_id':identity},[dict(r,profile_id=identity) for r in values])
        child['profile_id']=identity

    @staticmethod
    def hatch_defaults():
        return dict(target_category_id=None,target_local_id=None,gold_cost='100000',
                    hp_min='30',hp_max='40',hp_step='5',monster_min='10',monster_max='20',monster_step='5',
                    pve_normal_min='10',pve_normal_max='20',pve_normal_step='5')

    @staticmethod
    def growth_defaults():
        return dict(growth=dict(maximum_level='1',experience_policy='1'),
                    growth_levels=[dict(level='1',mob_experience='0',item_experience='0')],
                    growth_stages=[dict(stage='0',maximum_level='1',form_id=None)])

    def apply_growth(self,document,child,key):
        old=child.get('growth_profile_id');previous=self.row('growth_pet_profile',old)
        fields=('maximum_level','experience_policy');head={k:text(document['growth'][k]) for k in fields}
        levels=[{k:text(v) for k,v in r.items() if k!='profile_id'} for r in document['growth_levels']]
        stages=[dict(stage=text(r['stage']),maximum_level=text(r['maximum_level']),form_id=text(r.get('form_id') or child['form_id'])) for r in document['growth_stages']]
        equal=previous and all(previous[k]==head[k] for k in fields)
        for table,values,order in [('growth_pet_level',levels,'level'),('growth_pet_stage',stages,'stage')]:
            current=[{k:v for k,v in r.items() if k!='profile_id'} for r in self.book_rows(table,old,order)] if previous else []
            equal=equal and sorted(values,key=lambda r:int(r[order]))==current
        if equal:return
        users=[(int(r['category_id']),int(r['local_id'])) for _,r in self.references('growth_pet_profile',old)] if previous else []
        identity=old if previous and all(user==key for user in users) else self.allocate('growth_pet_profile')
        code=previous['code'] if identity==old else f'editor.growth_pet.{identity}'
        self.replace('growth_pet_profile',{'profile_id':identity},[dict(head,profile_id=identity,code=code)])
        self.replace('growth_pet_level',{'profile_id':identity},[dict(r,profile_id=identity) for r in levels])
        self.replace('growth_pet_stage',{'profile_id':identity},[dict(r,profile_id=identity) for r in stages]);child['growth_profile_id']=identity

    def apply_soul(self,document,child,key):
        if child['soul_kind']=='2':child['profile_id']=None;return
        old=child.get('profile_id');previous=self.row('soul_profile',old)
        levels=[{k:text(v) for k,v in r.items() if k!='profile_id'} for r in document['soul_levels']]
        current=[{k:v for k,v in r.items() if k!='profile_id'} for r in self.book_rows('soul_level',old,'level')] if previous else []
        if previous and levels==current:return
        users=[(int(r['category_id']),int(r['local_id'])) for _,r in self.references('soul_profile',old)] if previous else []
        identity=old if previous and all(user==key for user in users) else self.allocate('soul_profile')
        code=previous['code'] if identity==old else f'editor.soul.{identity}'
        self.replace('soul_profile',{'profile_id':identity},[dict(profile_id=identity,code=code)])
        self.replace('soul_level',{'profile_id':identity},[dict(r,profile_id=identity) for r in levels]);child['profile_id']=identity

    def apply_polymorph(self,document,child,key):
        form=document.get('new_poly_form')
        if form:
            fields=('code','mob_template_id','book_bonus')
            existing=next((r for r in self.tables['polymorph_form'] if r['code']==form['code']),None)
            if existing and any(str(form[k])!=existing[k] for k in fields):raise api.EditorError('Ez a formakód már más szabályokkal foglalt. Új kódot adj meg.')
            identity=existing['form_id'] if existing else self.allocate('polymorph_form','form_id')
            if not existing:self.replace('polymorph_form',{'form_id':identity},[dict(form,form_id=identity)])
            if document.get('poly_target_fixed'):child['fixed_form_id']=identity
        old=child.get('profile_id');previous=self.row('polymorph_profile',old)
        fields=[k for k in self.packet['columns']['polymorph_profile'] if k not in ('profile_id','code')]
        head={k:text(document['poly'][k]) for k in fields}
        children={t:[{k:text(v) for k,v in r.items() if k!='profile_id'} for r in document[d]] for t,d in
                  [('polymorph_skill_rule','poly_skills'),('polymorph_mastery_rule','poly_masteries')]}
        equal=previous and all(previous[k]==head[k] for k in fields)
        for table,order in [('polymorph_skill_rule','skill_level'),('polymorph_mastery_rule','mastery')]:
            current=[{k:v for k,v in r.items() if k!='profile_id'} for r in self.book_rows(table,old,order)] if previous else []
            equal=equal and current==children[table]
        if equal:return
        users=[(int(r['category_id']),int(r['local_id'])) for _,r in self.references('polymorph_profile',old)] if previous else []
        identity=old if previous and all(user==key for user in users) else self.allocate('polymorph_profile')
        code=previous['code'] if identity==old else f'editor.polymorph.{identity}'
        self.replace('polymorph_profile',{'profile_id':identity},[dict(head,profile_id=identity,code=code)])
        for table,rows in children.items():self.replace(table,{'profile_id':identity},[dict(r,profile_id=identity) for r in rows])
        child['profile_id']=identity

    def apply_tool(self,document,child,key):
        old=child.get('profile_id'); previous=self.row('tool_profile',old)
        levels=[{k:text(v) for k,v in r.items() if k!='profile_id'} for r in document['tool_levels']]
        current=[{k:v for k,v in r.items() if k!='profile_id'} for r in self.book_rows('tool_level',old,'level')] if previous else []
        if previous and levels==current:return
        users=[(int(r['category_id']),int(r['local_id'])) for _,r in self.references('tool_profile',old)] if previous else []
        identity=old if previous and all(user==key for user in users) else self.allocate('tool_profile')
        code=previous['code'] if identity==old else f'editor.tool.{identity}'
        self.replace('tool_profile',{'profile_id':identity},[dict(profile_id=identity,code=code)])
        self.replace('tool_level',{'profile_id':identity},[dict(r,profile_id=identity) for r in levels])
        child['profile_id']=identity

    def apply_recovery(self, document, child, key):
        recovery, old = document['recovery'], child.get('recovery_profile_id')
        previous = self.row('recovery_profile', old)
        users = [row for _, row in self.references('recovery_profile', old)] if old else []
        # The typed effect is content too, and a delivery change needs a new
        # head because the effect table is chosen by delivery kind.
        private = bool(previous) and all((int(r['category_id']), int(r['local_id'])) == key for r in users) and \
            previous['delivery_kind'] == recovery['delivery_kind']
        cooldown = document.get('cooldown')
        # A shared head keeps its cooldown; never edit that cooldown in place.
        recovery['cooldown_profile_id'] = self.profile('consumable_cooldown_profile', cooldown, recovery.get('cooldown_profile_id'),
                                                       reuse=private) if cooldown else None
        profile = self.profile('recovery_profile', recovery, old if private else None)
        if any(r.get('recovery_profile_id') == profile and (int(r['category_id']), int(r['local_id'])) != key for r in self.tables['item_consumable']):
            profile = self.allocate('recovery_profile')
            self.replace('recovery_profile', {'profile_id': profile}, [dict(recovery, profile_id=profile, code=f'editor.recovery.{profile}')])
        child.update(recovery_profile_id=profile, timed_effect_profile_id=None, toggle_effect_profile_id=None,socket_service_profile_id=None)
        table = EFFECT_TABLES[int(recovery['delivery_kind'])]
        for other in EFFECT_TABLES.values():
            if other != table: self.replace(other, {'profile_id': profile}, [])
        self.replace(table, {'profile_id': profile}, [dict(document['effect'], profile_id=profile, delivery_kind=recovery['delivery_kind'])])

    def apply_timed(self, document, child, key):
        timed = document['timed']
        group = document['timed_group']
        old_group = self.row('timed_effect_group', group.get('group_id'), 'group_id')
        fields = ('code', 'reapply_kind', 'clock_kind', 'remove_on_death')
        if old_group:
            if any(str(group.get(f)) != old_group[f] for f in fields):
                raise api.EditorError('Meglévő hatáscsoport szabályai nem módosíthatók tárgyszerkesztéssel. Válassz új csoportot.')
            group_id = old_group['group_id']
        else:
            matching = next((r for r in self.tables['timed_effect_group'] if r['code'] == group['code']), None)
            if matching:
                if any(str(group.get(f)) != matching[f] for f in fields):
                    raise api.EditorError('A hatáscsoport kódja már más szabályokkal foglalt.')
                group_id = matching['group_id']
            else:
                group_id = self.allocate('timed_effect_group', 'group_id')
                self.replace('timed_effect_group', {'group_id': group_id}, [dict(group, group_id=group_id)])
        old = child.get('timed_effect_profile_id')
        previous = self.row('timed_effect_profile', old)
        users = [row for _, row in self.references('timed_effect_profile', old)] if old else []
        private = bool(previous) and all((int(r['category_id']), int(r['local_id'])) == key for r in users)
        timed['group_id'] = group_id
        timed['bonus_profile_id'] = self.bonus(document.get('timed_bonuses'), previous['bonus_profile_id'] if private else None,
                                               '3', ('timed_effect_profile', str(old)))
        cooldown = document.get('cooldown')
        timed['cooldown_profile_id'] = self.profile('consumable_cooldown_profile', cooldown, timed.get('cooldown_profile_id'),
                                                   reuse=private) if cooldown else None
        child.update(recovery_profile_id=None, toggle_effect_profile_id=None,socket_service_profile_id=None, timed_effect_profile_id=self.profile('timed_effect_profile', timed, old if private else None))

    def apply_toggle(self, document, child, key):
        toggle = document['toggle']
        group = document['toggle_group']
        old_group = self.row('toggle_effect_group', group.get('group_id'), 'group_id')
        fields = ('code', 'remove_on_death')
        if old_group:
            if any(str(group.get(f)) != old_group[f] for f in fields):
                raise api.EditorError('Meglévő hatáscsoport szabályai nem módosíthatók tárgyszerkesztéssel. Válassz új csoportot.')
            group_id = old_group['group_id']
        else:
            matching = next((r for r in self.tables['toggle_effect_group'] if r['code'] == group['code']), None)
            if matching:
                if any(str(group.get(f)) != matching[f] for f in fields):
                    raise api.EditorError('A hatáscsoport kódja már más szabályokkal foglalt.')
                group_id = matching['group_id']
            else:
                group_id = self.allocate('toggle_effect_group', 'group_id')
                self.replace('toggle_effect_group', {'group_id': group_id}, [dict(group, group_id=group_id)])
        old = child.get('toggle_effect_profile_id')
        previous = self.row('toggle_effect_profile', old)
        users = [row for _, row in self.references('toggle_effect_profile', old)] if old else []
        private = bool(previous) and all((int(r['category_id']), int(r['local_id'])) == key for r in users)
        toggle['group_id'] = group_id
        toggle['bonus_profile_id'] = self.bonus(document.get('toggle_bonuses'), previous['bonus_profile_id'] if private else None,
                                               '3', ('toggle_effect_profile', str(old)))
        cooldown = document.get('cooldown')
        toggle['cooldown_profile_id'] = self.profile('consumable_cooldown_profile', cooldown, toggle.get('cooldown_profile_id'),
                                                   reuse=private) if cooldown else None
        child.update(recovery_profile_id=None, timed_effect_profile_id=None,socket_service_profile_id=None, toggle_effect_profile_id=self.profile('toggle_effect_profile', toggle, old if private else None))

    def apply_book(self, document, child, key):
        head, old = document['book'], child.get('profile_id')
        previous = self.row('skill_book_profile', old)
        users = [row for _, row in self.references('skill_book_profile', old)] if old else []
        private = bool(previous) and all((int(r['category_id']), int(r['local_id'])) == key for r in users)
        profile = self.profile('skill_book_profile', head, old if private else None)
        if any((int(r['category_id']), int(r['local_id'])) != key for _, r in self.references('skill_book_profile', profile)):
            profile = self.allocate('skill_book_profile')
            self.replace('skill_book_profile', {'profile_id': profile}, [dict(head, profile_id=profile, code=f'editor.skill_book.{profile}')])
        child['profile_id'] = profile
        self.replace('skill_book_target', {'profile_id': profile}, [dict(r, profile_id=profile) for r in document['book_targets']])
        self.replace('skill_book_step', {'profile_id': profile}, [dict(r, profile_id=profile) for r in document['book_steps']])

    def encode(self):
        return {name: [[row[column] for column in self.packet['columns'][name]] for row in rows] for name, rows in self.tables.items()}


# --- error text ------------------------------------------------------------
FIELDS = {
    'socket.service':'foglalattisztítás vagy kőkiemelés típusa és mennyisége',
    'pet.training':'kor, helynyitás és képességszintek', 'pet.skills':'petképesség azonosítója, hatása és szintjei', 'pet.supply':'petellátmány típusa és beállításai', 'pet.supply_state':'szállítódoboz/skin halmozása vagy bónusza', 'pet.study_skill':'tanítható petképesség', 'pet.skin_form':'pet megjelenési formája',
    'pet.hatch': 'keltetési költség és születési értékek', 'pet.hatch_target': 'keltetett fejlődő pet', 'pet.feed': 'petetetés forrása, mennyisége és hatása',
    'growth_pet.profile': 'fejlődő pet szintjei és szakaszai', 'growth_pet.form': 'evolúciós szakasz formája',
    'companion': 'pet/mount típusa és időtartama', 'companion.form': 'követő vagy lovaglási forma', 'companion.equipment_state': 'pet kötése vagy megjelenési profilja',
    'soul': 'lélek vagy lélektekercs típusa', 'soul.profile': 'lélek töltési és sebzési szabályai', 'soul.levels': 'lélek szintenkénti szabályai és halommérete', 'soul.scroll': 'lélektekercs fejlesztése',
    'polymorph': 'átváltoztató tárgy', 'polymorph.profile': 'átváltozási szabályok', 'polymorph.form': 'átváltozási forma',
    'polymorph.rules': 'átváltozási szintszabályok',
    'tool': 'eszköz típusa és halászati táblája', 'tool.profile': 'eszköz fejlődési szabályai',
    'tool.levels': 'eszköz gyakorlási és fejlesztési szintjei',
    'fishing': 'hal vagy csali szabályai', 'fishing.loot': 'élő hal feldolgozási eredménye',
    'fishing.grill_result': 'sütés eredménytárgya',
    'costume': 'kosztüm típusa, fegyverfeltétele és időkezelése',
    'costume.visual': 'kosztüm megjelenése (osztály és nem)',
    'socket_stone': 'foglalatkő szabályai',
    'quest_item': 'küldetéstárgy használati szabályai',
    'skill_book': 'képességkönyv szabályai', 'skill_book.children': 'könyv céljai és szintlépései',
    'item_base': 'alapadatok', 'content_code': 'tartalomkód', 'refinement': 'fejlesztés', 'category': 'kategóriaadatok',
    'material.kind': 'alapanyag típusa', 'weapon.type': 'fegyvertípus', 'weapon.ammo': 'lőszerszabály', 'weapon': 'fegyverprofilok',
    'armor.slot': 'páncél helye', 'armor.resistance': 'ellenállás', 'accessory.slot': 'ékszer helye',
    'accessory.resistance': 'ellenállás', 'accessory.socket': 'foglalatprofil', 'accessory_socket': 'foglalatprofil',
    'toggle_effect': 'kapcsolható harmat', 'toggle_effect_group': 'harmat hatáscsoportja',
    'toggle_effect.group': 'harmat hatáscsoportja', 'toggle_effect.bonus': 'harmat bónuszai',
    'toggle_effect.empty_bonus': 'harmat üres bónuszlistája', 'toggle_effect.cooldown': 'harmat várakozási ideje',
    'consumable.toggle_effect': 'kapcsolható harmat',
    'consumable.effect': 'fogyasztható hatásválasztása', 'consumable.timed_effect': 'időzített hatás',
    'timed_effect': 'időzített hatás', 'timed_effect.group': 'hatáscsoport', 'timed_effect.bonus': 'hatás bónuszai',
    'timed_effect.empty_bonus': 'üres hatás', 'timed_effect.cooldown': 'hatás várakozási ideje',
    'timed_effect_group': 'hatáscsoport', 'consumable.recovery': 'gyógyító hatás', 'recovery': 'gyógyító hatás', 'recovery.cooldown': 'várakozási idő',
    'consumable_cooldown': 'várakozási idő', 'item_base.bonus': 'bónuszok', 'bonus': 'bónuszok', 'bonus.entries': 'bónuszok',
    'base_plus_gain': 'alapérték + fejlesztés', 'use_policy': 'használat', 'trade_policy': 'kereskedelem',
    'appearance': 'megjelenés', 'attack': 'támadási profil', 'ammo': 'lőszerszabály', 'resistance': 'ellenállás',
    'container': 'tároló', 'container.kind': 'tároló', 'container.loot': 'tárolótartalom', 'container_key': 'kulcs',
    'container_key.lock': 'kulcs', 'currency_token': 'pénzjegy', 'loot': 'tárolótartalom', 'loot.item': 'tárolótartalom', 'loot.entries': 'tárolótartalom',
}
CATALOG_TEXT = {
    'DuplicateContentCode': 'Ez a tartalomkód már foglalt egy másik tárgynál.',
    'DuplicateProfile': 'Ismétlődő profilkód vagy azonosító.',
    'InvalidIdentity': 'Érvénytelen kód: kisbetűvel kezdődjön, és csak a–z, 0–9, _ és . (nem kettőzött) legyen benne.',
    'MissingProfile': 'A hivatkozott profil hiányzik.',
    'WrongRefinementKind': 'A fejlesztési profil típusa nem illik a kategóriához.',
    'InvalidRefinementRecipe': 'Hibás fejlesztési recept: a tárgyak, szintek, alapanyagok, pénzköltség vagy a sikertelen fejlesztés szabálya nem megfelelő.',
    'StatOutOfRange': 'Az alapérték és a legmagasabb szint növekménye együtt túl nagy.',
    'WrongBonusSource': 'A bónuszprofil forrása nem megfelelő (alap, fejlesztés vagy aktív hatás).',
    'UnsupportedCategory': 'Ez a kategória még nem tölthető be.',
    'LimitExceeded': 'Túl sok adat egy katalógusban.',
    'InvalidSocketProfile': 'Hibás foglalatprofil.', 'InvalidSocketMaterial': 'Hibás foglalatanyag.',
    'InvalidLootReward': 'A tárgyjutalom nem létező vagy kivezetett tárgyra mutat, vagy több, mint amennyi a tárgyból egy halomban lehet.',
    'InvalidContainer': 'Használatkor nyíló és kulcsos ládához tartalom kell, quest által nyitotthoz nem; '
                        'kulcsos ládánál és kulcsnál a zárazonosító legalább 1, máshol 0.',
}
LOOT_TEXT = {
    'InvalidIdentity': 'Érvénytelen tartalomkód.', 'InvalidMode': 'Ismeretlen sorsolási mód.',
    'EmptyProfile': 'A tartalomlista üres: adj meg legalább egy jutalmat.', 'TooManyEntries': 'Legfeljebb 256 jutalomsor lehet.',
    'InvalidEntryOrder': 'A jutalomsorok számozása hiányos.',
    'InvalidReward': 'Tárgyjutalomhoz tárgyat kell választani, más jutalomhoz nem.',
    'InvalidAmount': 'A mennyiség kívül esik a tartományon (lassítás és HP-szívás legfeljebb 100%, mérgezésé 0).',
    'InvalidWeight': 'A súly legalább 1; soronkénti esélynél legfeljebb 100%.',
    'InvalidRareChance': 'Ritka bónusz esélye csak tárgyjutalomnál adható, legfeljebb 100%.',
    'WeightOverflow': 'A súlyok összege túl nagy.',
}
BASE_TEXT = {
    'polymorph.book_stack': 'A gyakorlókönyv nem halmozható; a maximális halomméret 1 legyen.',
    'skill_book.stack_below_use_units': 'A halmozási korlát kisebb a könyv használatonkénti darabszámánál.',
    'consumable.toggle_stack': 'A kapcsolható harmat nem halmozható; a maximális halomméret 1 legyen.',
    'consumable.automatic_stack': 'Az automata palack nem halmozható; a maximális halomméret 1 legyen.',
    'item_base': 'Hibás alapadat. A kódok kisbetűvel kezdődjenek (a–z, 0–9, _ és .), az ikon útvonala ne legyen üres, '
                 'a méret és a halmozás legalább 1 legyen; felszerelés nem halmozható.',
    'non_equipment.equipment_state': 'Alapanyag és ital nem köthető felszereléskor, és nem kaphat megjelenést vagy bónuszt.',
    'consumable.stack_below_use_units': 'A halmozási korlát kisebb, mint egy használat egységigénye.',
}
REFINEMENT_TEXT = {
    'IncompleteLevels': 'A fejlesztési szinteknek +0-tól a maximumig hiánytalanul meg kell lenniük.',
    'InvalidLevelRange': 'Érvénytelen maximális szint (0–255; fejlesztés nélküli profilnál 0).',
    'NonzeroLevelZero': 'A +0 szint nem adhat növekményt vagy extra bónuszt.',
    'NonMonotonicGain': 'A halmozott növekmény szintről szintre nem csökkenhet.',
    'WrongStatKind': 'Fegyver csak támadást, páncél csak védekezést növelhet; ékszer csak bónuszt kaphat.',
    'InvalidGain': 'A növekmény nem lehet negatív vagy túl nagy.',
    'MissingBonusProfile': 'A szint extra bónuszprofilja hiányzik.',
    'WrongBonusSource': 'A szint bónuszai között alapbónusznak szánt érték van.',
    'InvalidKind': 'Érvénytelen fejlesztéstípus.',
}
BONUS_TEXT = {
    'EmptyProfile': 'Üres bónuszlista.', 'TooManyEntries': 'Legfeljebb 128 bónusz adható meg.',
    'UnknownBonus': 'Ismeretlen bónusz.', 'DuplicateBonus': 'Ugyanaz a bónusz kétszer szerepel.',
    'ValueOutOfRange': 'A bónusz értéke kívül esik a megengedett tartományon.',
    'SourceNotAllowed': 'Ez a bónusz itt nem használható.',
}
RECOVERY_TEXT = {
    'InvalidUnits': 'A használatonkénti egység legalább 1 legyen.',
    'InvalidContexts': 'Legalább egy használati helyszínt jelölj be.',
    'InvalidCooldown': 'Érvénytelen várakozási idő.', 'InvalidAmount': 'A gyógyítás mértéke kívül esik a tartományon.',
    'EmptyEffect': 'A hatás semmit sem tölt vissza: adj meg HP-t vagy SP-t.',
    'InvalidInterval': 'Érvénytelen tick-időköz.',
    'InvalidDuration': 'Az időtartam a tick-időköz egész számú többszöröse legyen.',
    'MultiplePools': 'Az időszakos hatás vagy HP-t, vagy SP-t töltsön, a kettőt egyszerre nem.',
    'InvalidAutomatic': 'Hibás automata palack: egy pool, véges pozitív vagy végtelen nulla kapacitás, egy darab és arénán kívüli használat szükséges.',
    'InvalidArenaQuota': 'Arénakvóta-költség csak akkor adható meg, ha a tárgy arénában is használható.',
}
READ_TEXT = {
    'InvalidNumber': 'Érvénytelen szám', 'NullValue': 'Kötelező érték hiányzik', 'InvalidText': 'Érvénytelen szöveg',
    'InvalidKey': 'Érvénytelen tárgykulcs', 'OrphanRow': 'Nem létező profilra hivatkozik', 'DuplicateRow': 'Ismétlődő sor',
    'InvalidChild': 'A gyógyítás típusa és a hatás táblája nem egyezik', 'ColumnCount': 'Hibás oszlopszám',
    'RegistryMismatch': 'A rendszerregiszter nem egyezik a programmal', 'FormatMismatch': 'Ismeretlen katalógusformátum',
    'RowLimit': 'Túl sok sor', 'ByteBudget': 'Túl nagy katalógus',
}


def describe(detail, columns=None):
    """Hungarian text for the native validator's structured rejection."""
    code, field = detail.get('catalog'), detail.get('field', '')
    if code:
        if code == 'InvalidRefinement': message = REFINEMENT_TEXT.get(detail.get('refinement'), 'Hibás fejlesztési profil.')
        elif code == 'InvalidBonusProfile': message = BONUS_TEXT.get(detail.get('bonus'), 'Hibás bónuszprofil.')
        elif code == 'InvalidRecoveryProfile': message = RECOVERY_TEXT.get(detail.get('recovery'), 'Hibás gyógyító hatás.')
        elif code == 'InvalidLootProfile': message = LOOT_TEXT.get(detail.get('loot'), 'Hibás tárolótartalom.')
        elif code == 'InvalidBase': message = BASE_TEXT.get(field, 'Hibás alapadat.')
        elif field=='loot.polymorph_binding': message='Példányhoz kötött formájú gömbhöz és gyakorlókönyvhöz külön példányadatok kellenek; általános ládajutalomként nem adhatók.'
        elif code in CATALOG_TEXT: message = CATALOG_TEXT[code]
        elif field == 'costume.visual' and code == 'MissingCategoryRow':
            message = 'A használati szabályokban engedélyezett minden osztályhoz és nemhez adj meg modellt.'
        elif code in ('InvalidProfile',): message = 'Érvénytelen érték.'
        else: message = 'A kategóriaadatok hiányosak vagy hibásak.'
        where = FIELDS.get(field)
        if where and code not in ('DuplicateContentCode', 'InvalidBase', 'InvalidContainer'): message = where[0].upper() + where[1:] + ': ' + message[0].lower() + message[1:]
        if detail.get('key'): message += ' (tárgy: %s:%s)' % tuple(detail['key'])
        return message
    read = detail.get('read')
    message = READ_TEXT.get(read, 'Adatellenőrzési hiba')
    table = detail.get('table')
    if table:
        column = detail.get('column', 0)
        names = (columns or {}).get(table, [])
        message += ' — %s, %d. sor%s' % (table, detail.get('row', 0) + 1, ', ' + names[column] if column < len(names) else '')
    return message + '.'


# --- native transport ----------------------------------------------------------
def helper():
    suffix = '.exe' if os.name == 'nt' else ''
    base = Path(getattr(sys, '_MEIPASS', Path(__file__).resolve().parent))
    choices = [base / 'validators' / ('CatalogEditorTool' + suffix),
               Path(__file__).resolve().parents[3] / 'build/SRC/ItemDomain/RelWithDebInfo' / ('CatalogEditorTool' + suffix)]
    path = next((path for path in choices if path.is_file()), None)
    if path is None: raise api.EditorError('Hiányzik a natív tárgyellenőrző. A szerverkezelő frissítése szükséges.')
    return path


def run_tool(payload, columns=None, executable=None):
    result = subprocess.run([str(executable or helper())], input=json.dumps(payload, ensure_ascii=True), capture_output=True,
                            text=True, encoding='utf-8', timeout=60, creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)
    lines = [line.removeprefix('AURIGA_CATALOG_RESULT ') for line in result.stdout.splitlines() if line.startswith('AURIGA_CATALOG_RESULT ')]
    if len(lines) != 1: raise api.EditorError('A tárgyszerkesztő nem kapott érvényes választ. Frissítsd a listát a mentés ismétlése előtt.')
    packet = api.strict_json(lines[0])
    if not packet.get('ok'):
        message = packet.get('error', 'A katalógusművelet sikertelen.')
        if isinstance(packet.get('detail'), dict): message = describe(packet['detail'], columns) + ' Semmi nem lett mentve.'
        raise api.EditorError(message)
    if result.returncode: raise api.EditorError('A katalógusművelet nem igazolt; frissítés szükséges.')
    return packet['result']


def blank(executable=None):
    """Registry-only packet, as a freshly installed protos database holds it."""
    return run_tool(dict(action='blank'), executable=executable)


def validate(packet, document, original_key=None, executable=None):
    """Offline check with the game's own catalog builder; writes nothing."""
    tables = Catalog(packet).apply(document, original_key)
    return run_tool(dict(action='validate', tables=tables), packet['columns'], executable)['hash']


def request(root, tables=None, expected_hash=None, columns=None):
    root = api.package_root(root); manager = api.load_manager(root)
    owner = manager.read_json(root / 'run/mariadb/owner.json')
    if not manager.owned(manager.read_json(root / 'run/mariadb/process.json')): raise api.EditorError('A helyi tesztadatbázis nem fut.')
    try:
        manager.verify_datadir()
    except RuntimeError as error:
        raise api.EditorError('A helyi adatbázis ellenőrzése sikertelen: ' + str(error)) from None
    config = manager.config()['common']['sql']['player']
    if config['host'] != '127.0.0.1' or config['port'] != owner['port']: raise api.EditorError('Csak a kiválasztott csomag saját helyi adatbázisa kezelhető.')
    auth = dict(port=owner['port'], user=config['user'], password=config['password'])
    mutation = tables is not None
    if mutation:
        private = configparser.ConfigParser(interpolation=None); private.read(root / 'run/mariadb/admin.cnf', encoding='utf-8')
        auth.update(user=private['client']['user'], password=private['client']['password'])
    payload = dict(action='commit' if mutation else 'read', connection=auth)
    if mutation: payload.update(tables=tables, expected_hash=expected_hash)
    return run_tool(payload, columns)


def save(root, before, document, original_key=None):
    model = Catalog(before); tables = model.apply(document, original_key)
    root = api.package_root(root)
    # Local process lock also prevents a concurrent server start during editing.
    manager = api.load_manager(root)
    with manager.supervisor_lock():
        if api.describe(root)['running']: raise api.EditorError('A katalógus mentéséhez állítsd le a helyi játékszervert.')
        stamp = datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S-') + secrets.token_hex(4)
        folder = root / 'run/catalog-editor' / stamp; folder.mkdir(parents=True, exist_ok=False)
        (folder / 'before.json').write_text(json.dumps(before, ensure_ascii=True), encoding='utf-8')
        (folder / 'requested.json').write_text(json.dumps(dict(expected_hash=before['hash'], tables=tables), ensure_ascii=True), encoding='utf-8')
        result = request(root, tables, before['hash'], before['columns'])
        (folder / 'result.json').write_text(json.dumps(dict(hash=result['hash'], key=[document['base']['category_id'], document['base']['local_id']])), encoding='utf-8')
        return result
