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

CATEGORIES = {1: 'Fegyver', 2: 'Páncél', 3: 'Ékszer', 5: 'Gyógyító ital', 6: 'Alapanyag'}
CHILD_TABLES = {1: 'item_weapon', 2: 'item_armor', 3: 'item_accessory', 5: 'item_consumable', 6: 'item_material'}
CODE_PREFIX = {1: 'weapon', 2: 'armor', 3: 'accessory', 5: 'consumable', 6: 'material'}
REFINEMENT_KIND = {1: 2, 2: 3, 3: 4}
EFFECT_TABLES = {1: 'instant_recovery_profile', 2: 'reserve_recovery_profile', 3: 'periodic_recovery_profile'}
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
    'bonus_profile': ('profile_id', [('item_base', 'base_bonus_profile_id'), ('refinement_level', 'extra_bonus_profile_id')], ['bonus_profile_entry']),
    'refinement_profile': ('profile_id', [('item_base', 'refinement_profile_id')], ['refinement_level']),
    'consumable_cooldown_profile': ('profile_id', [('recovery_profile', 'cooldown_profile_id')], []),
    'recovery_profile': ('profile_id', [('item_consumable', 'recovery_profile_id')], list(EFFECT_TABLES.values())),
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
        if category == 1:
            result.update(attack=self.linked('item_attack_profile', child, 'attack_profile_id'),
                          ammo=self.linked('item_ammo_policy', child, 'ammo_policy_id', 'policy_id'))
        if category in (2, 3): result['resistance'] = self.linked('item_resistance_profile', child, 'resistance_profile_id')
        if category == 5:
            recovery = self.linked('recovery_profile', child, 'recovery_profile_id')
            result.update(recovery=recovery, effect=copy.deepcopy(self.row(EFFECT_TABLES[int(recovery['delivery_kind'])], recovery['profile_id'])),
                          cooldown=self.linked('consumable_cooldown_profile', recovery, 'cooldown_profile_id'))
        return result

    def new(self, category):
        if category not in CATEGORIES: raise api.EditorError('Ez a kategória még nem szerkeszthető.')
        local = int(self.packet['sequences'][str(category)])
        if not 1 <= local <= 4294967295: raise api.EditorError('A kategória azonosítótartománya elfogyott.')
        code = f'{CODE_PREFIX[category]}.new_item_{local}'
        base = dict(category_id=str(category), local_id=str(local), content_code=code, name_key=f'item.{code}.name',
                    description_key=f'item.{code}.description', icon_asset='icons/item.png', inventory_width='1', inventory_height='1',
                    max_stack='200' if category in (5, 6) else '1', weight_grams='0', min_level='0', binding_policy='1',
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
        if category == 6: item['child']['material_kind'] = '1'
        if category == 5:
            item.update(recovery=dict(delivery_kind='1', units_per_use='1', allowed_contexts='15', allow_during_pvp='1', cooldown_profile_id=None),
                        effect=dict(delivery_kind='1', hp_raw='1000', sp_raw='0', max_hp_bp='0', max_sp_bp='0', flat_bonus_cap_bp='10000'),
                        cooldown=None)
            item['child']['recovery_profile_id'] = None
        return item

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

    @staticmethod
    def owner(table, row):
        return (row['category_id'], row['local_id']) if table == 'item_base' else (row['profile_id'], row['level'])

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
        if category == 5: self.apply_recovery(document, child, key)
        self.replace('item_base', {'category_id': key[0], 'local_id': key[1]}, [base])
        self.replace(CHILD_TABLES[category], {'category_id': key[0], 'local_id': key[1]}, [child])
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
        child['recovery_profile_id'] = profile
        table = EFFECT_TABLES[int(recovery['delivery_kind'])]
        for other in EFFECT_TABLES.values():
            if other != table: self.replace(other, {'profile_id': profile}, [])
        self.replace(table, {'profile_id': profile}, [dict(document['effect'], profile_id=profile, delivery_kind=recovery['delivery_kind'])])

    def encode(self):
        return {name: [[row[column] for column in self.packet['columns'][name]] for row in rows] for name, rows in self.tables.items()}


# --- error text ------------------------------------------------------------
FIELDS = {
    'item_base': 'alapadatok', 'content_code': 'tartalomkód', 'refinement': 'fejlesztés', 'category': 'kategóriaadatok',
    'material.kind': 'alapanyag típusa', 'weapon.type': 'fegyvertípus', 'weapon.ammo': 'lőszerszabály', 'weapon': 'fegyverprofilok',
    'armor.slot': 'páncél helye', 'armor.resistance': 'ellenállás', 'accessory.slot': 'ékszer helye',
    'accessory.resistance': 'ellenállás', 'accessory.socket': 'foglalatprofil', 'accessory_socket': 'foglalatprofil',
    'consumable.recovery': 'gyógyító hatás', 'recovery': 'gyógyító hatás', 'recovery.cooldown': 'várakozási idő',
    'consumable_cooldown': 'várakozási idő', 'item_base.bonus': 'bónuszok', 'bonus': 'bónuszok', 'bonus.entries': 'bónuszok',
    'base_plus_gain': 'alapérték + fejlesztés', 'use_policy': 'használat', 'trade_policy': 'kereskedelem',
    'appearance': 'megjelenés', 'attack': 'támadási profil', 'ammo': 'lőszerszabály', 'resistance': 'ellenállás',
}
CATALOG_TEXT = {
    'DuplicateContentCode': 'Ez a tartalomkód már foglalt egy másik tárgynál.',
    'DuplicateProfile': 'Ismétlődő profilkód vagy azonosító.',
    'InvalidIdentity': 'Érvénytelen kód: kisbetűvel kezdődjön, és csak a–z, 0–9, _ és . (nem kettőzött) legyen benne.',
    'MissingProfile': 'A hivatkozott profil hiányzik.',
    'WrongRefinementKind': 'A fejlesztési profil típusa nem illik a kategóriához.',
    'StatOutOfRange': 'Az alapérték és a legmagasabb szint növekménye együtt túl nagy.',
    'WrongBonusSource': 'A bónuszprofil forrása nem megfelelő (alap vagy fejlesztési bónusz).',
    'UnsupportedCategory': 'Ez a kategória még nem tölthető be.',
    'LimitExceeded': 'Túl sok adat egy katalógusban.',
    'InvalidSocketProfile': 'Hibás foglalatprofil.', 'InvalidSocketMaterial': 'Hibás foglalatanyag.',
}
BASE_TEXT = {
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
        elif code == 'InvalidBase': message = BASE_TEXT.get(field, 'Hibás alapadat.')
        elif code in CATALOG_TEXT: message = CATALOG_TEXT[code]
        elif code in ('InvalidProfile',): message = 'Érvénytelen érték.'
        else: message = 'A kategóriaadatok hiányosak vagy hibásak.'
        where = FIELDS.get(field)
        if where and code not in ('DuplicateContentCode', 'InvalidBase'): message = where[0].upper() + where[1:] + ': ' + message[0].lower() + message[1:]
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
