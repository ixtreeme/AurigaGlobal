"""Independent Python encoder for the v15 typed item fixture (not a runtime reader)."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess


def fixture_digest(with_refinement_recipe=False, with_quest_items=False, with_socket_stones=False, with_costumes=False):
    data = bytearray()
    def numbers(format, *values):
        data.extend(struct.pack("<" + format, *values))
    def text(value):
        raw = value.encode("ascii")
        numbers("I", len(raw)); data.extend(raw)
    def identity(id, code):
        numbers("I", id); text(code)
    def table(tag, count):
        numbers("BI", tag, count)
    def optional(value):
        numbers('B', value is not None)
        if value is not None:
            numbers('I', value)
    text("auriga.item_catalog.v15"); numbers("I", 15)
    table(1, 3); identity(1, "use.equipment"); numbers("BBB", 15, 3, 1)
    identity(2, "use.material"); numbers("BBB", 15, 3, 0)
    identity(3, "use.recovery"); numbers("BBB", 15, 3, 1)
    table(2, 1); identity(1, "trade.unbound"); numbers("BBB", 1, 1, 1)
    table(3, 2)
    for id, code, model in [(1, "appearance.sword", "models/iron_sword.model"), (2, "appearance.armor", "models/iron_armor.model")]:
        identity(id, code); text(model)
    table(4, 1); identity(1, "attack.melee"); numbers("IIIH", 25, 10, 3000, 1)
    table(5, 2)
    identity(1, "ammo.none"); numbers("BH", 1, 0)
    identity(2, "ammo.arrow"); numbers("BH", 2, 1)
    table(6, 1); identity(1, "resistance.none"); numbers("15i", *([0] * 15))
    profiles=[(1, "refine.none", 1, 0), (2, "refine.weapon_linear", 2, 9), (3, "refine.armor", 3, 2), (4, "refine.accessory", 4, 15)]
    if with_socket_stones:profiles.append((5,'refine.stone',5,30))
    if with_costumes:profiles.append((5,'refine.costume',6,30))
    table(7, len(profiles))
    for id, code, kind, maximum in profiles:
        if with_refinement_recipe and kind == 2:
            maximum = 30
        identity(id, code); numbers("BHI", kind, maximum, maximum + 1)
        for level in range(maximum + 1):
            gain = (10000 * level, 4000 * level, 0, 0) if kind == 2 else (0, 0, 5000 * level, 2000 * level) if kind == 3 else (0, 0, 0, 0)
            numbers("H4q", level, *gain)
            bonus = 2 if kind == 2 and level == 3 else 3 if kind == 2 and level == 9 else None
            if kind == 4 and level:
                bonus = 5 if level == 15 else 4
            if kind in (5,6) and level==10:bonus=2
            optional(bonus)
    base_items = [
        (1, 1, "weapon", "iron_sword", 3, 1, 1200, 1, 1, 2, 1, 1, 100, 50),
        (2, 1, "armor", "iron_armor", 2, 1, 5000, 5, 1, 3, 2, None, 200, 100),
        (3, 1, "accessory", "copper_bracelet", 1, 1, 80, 0, 1, 4, None, 6, 0, 0),
        (5, 1, "consumable", "instant_flask", 1, 200, 50, 10, 3, 1, None, None, 0, 0),
        (5, 2, "consumable", "reserve_flask", 1, 200, 50, 10, 3, 1, None, None, 0, 0),
        (5, 3, "consumable", "regeneration_flask", 1, 200, 50, 10, 3, 1, None, None, 0, 0),
        (5, 4, "consumable", "ability_flask", 1, 200, 50, 10, 3, 1, None, None, 0, 0),
        (5, 5, "consumable", "automatic_flask", 1, 1, 100, 10, 3, 1, None, None, 0, 0),
        (6, 1, "material", "leather", 1, 200, 10, 0, 2, 1, None, None, 0, 0),
        (6, 2, "material", "amber", 1, 200, 5, 0, 2, 1, None, None, 0, 0),
        (6, 3, "material", "socket_drill", 1, 20, 30, 0, 2, 1, None, None, 0, 0),
        (8, 1, "container", "gift_box", 1, 50, 100, 0, 2, 1, None, None, 0, 0),
        (8, 2, "container", "iron_chest", 1, 10, 100, 0, 2, 1, None, None, 0, 0),
        (8, 3, "container", "quest_box", 1, 1, 100, 0, 2, 1, None, None, 0, 0),
        (16, 1, "currency_token", "gold_bar", 1, 200, 100, 0, 2, 1, None, None, 0, 0),
        (16, 2, "currency_token", "coin_voucher", 1, 200, 100, 0, 2, 1, None, None, 0, 0),
        (18, 1, "container_key", "iron_key", 1, 20, 100, 0, 2, 1, None, None, 0, 0),
    ]
    if with_quest_items:
        base_items.extend([
            (10,1,'quest','soul_orb',1,200,10,0,2,1,None,None,0,0),
            (10,2,'quest','horse_scroll',1,200,10,50,2,1,None,None,0,0)])
    if with_socket_stones:
        base_items.extend([(7,1,'socket_stone','precision',1,200,10,0,2,5,None,1,0,0),
                           (7,2,'socket_stone','shard',1,200,10,0,2,1,None,None,0,0)])
    if with_costumes:base_items.append((4,1,'costume','sword_mask',1,1,10,0,1,5,None,None,0,0))
    table(8, len(base_items))
    for category, local, prefix, code, height, stack, grams, minimum, use, refine, appearance, bonus, buy, sell in sorted(base_items):
        numbers("HI", category, local)
        text(prefix + "." + code)
        text("item." + code + ".name"); text("item." + code + ".description"); text("icons/" + ('leather' if category in (4,7,10) else code) + ".png")
        numbers("BBIIH", 1, height, stack, grams, minimum)
        numbers("BIII", 1, use, 1, refine)
        optional(appearance); optional(bonus)
        numbers("BQQB", 1, buy, sell, 0)
    table(9, 1); numbers("HIBBqqII", 1, 1, 1, 1, 120000, 30000, 1, 1)
    table(10, 1); numbers("HIBqqI", 2, 1, 2, 40000, 20000, 1)
    # Independent wire enumeration, not imported from the C++/SQL generator.
    fields = [
        ('unit', ['Raw','BasisPoints','Integer']),
        ('target', ['PhysicalAttack','MagicalAttack','PhysicalDefense','MagicalDefense','OutgoingDamage','IncomingDamage',
                    'FlatPenetration','PercentPenetration','Resistance','CritChance','CritResistance','CritMultiplierExtra','BlockReduction','Evasion','MovementSpeed','AttackSpeed','CastSpeed','Strength','Dexterity','Constitution','Intelligence','Experience']),
        ('stage', ['StatFlat','StatPercent','OutgoingAction','OutgoingMode','Penetration','Resistance','Critical','IncomingAction','IncomingMode','Block','Accuracy','OutgoingGlobal','Reward']),
        ('channel', ['Any','Physical','Magical']), ('action', ['Any','Normal','Skill']), ('mode', ['Any','Pve','Pvp']),
        ('family', ['Any']), ('rank', ['Any']), ('element', ['Any']),
        ('stacking', ['Add','Max','ReplaceByPriority','UniqueSource']),
        ('aggregate_cap', ['RawStat','StatPercent','Outgoing','Incoming','FlatPenetration','PercentPenetration','Resistance','CritChance','CritResistance','CritMultiplierExtra','Block','Accuracy','Speed','Attribute','Experience']),
    ]
    registry = json.loads((Path(__file__).resolve().parents[2] / 'config/item_combat/bonus_registry.json').read_text(encoding='utf-8'))
    numbers('I', registry['schema_version']); text(registry['status'])
    table(11, len(registry['bonuses']))
    for row in sorted(registry['bonuses'], key=lambda r: r['id']):
        identity(row['id'], row['code']); text(row['display_key'])
        codes = [names.index(row[field]) + (0 if field in ('family','rank','element') else 1) for field, names in fields]
        mask = sum({'ItemBase': 1, 'Refinement': 2, 'TimedEffect': 4}[name] for name in row['allowed_sources'])
        numbers('6BHH4Bqq', *codes, mask, row['minimum'], row['maximum'])
    table(12, 7)
    for id, code, source, entries in [
        (1, 'bonus.sword_base', 1, [(1, 2500), (7, 500)]),
        (2, 'bonus.refinement_three', 2, [(1, 5000), (21, 100)]),
        (3, 'bonus.refinement_nine', 2, [(21, 200)]),
        (4, 'bonus.accessory_low', 2, [(3, 2000)]),
        (5, 'bonus.accessory_high', 2, [(3, 6000)]),
        (6, 'bonus.accessory_base', 1, [(3, 3000)]),
        (7, 'bonus.timed_ability', 3, [(26, 1500), (29, 5), (33, 2000)]),
    ]:
        identity(id, code); numbers('BI', source, len(entries))
        for bonus, value in entries:
            numbers('Iq', bonus, value)
    table(13, 1); numbers('HIBI', 3, 1, 6, 1); optional(1)
    table(14, 3)
    for local, kind in [(1,1),(2,5),(3,4)]:
        numbers('HIB', 6, local, kind)
    table(15, 1); identity(1, 'socket.copper'); numbers('BHHI', 3, 1, 1, 4)
    for grade, duration, increase, minimum_raw, minimum_bp, minimum_integer, unlock, charge in [
        (0,0,0,0,0,0,5000,10000), (1,86400000,1000,1000,100,1,5000,10000),
        (2,43200000,2000,2000,200,2,5000,10000), (3,21600000,4000,3000,300,3,0,0)
    ]:
        numbers('BQIqiiHH', grade, duration, increase, minimum_raw, minimum_bp, minimum_integer, unlock, charge)
    numbers('IHI', 1, 6, 3); numbers('IHI', 1, 6, 2)
    numbers('I', 0)  # Optional permanent-charge compatibility list.
    table(16, 1); identity(1, 'cooldown.recovery'); numbers('Q', 1000)
    table(17, 4)
    identity(1, 'recovery.instant'); numbers('HBB', 1, 15, 1); optional(1); numbers('HB', 0, 1)
    numbers('qqHHI', 300000, 100000, 500, 1000, 10000)
    identity(2, 'recovery.reserve'); numbers('HBB', 1, 9, 0); optional(1); numbers('HB', 0, 2)
    numbers('qqIQH', 500000, 200000, 10000, 1000, 700)
    identity(3, 'recovery.periodic'); numbers('HBB', 1, 15, 0); optional(None); numbers('HB', 0, 3)
    numbers('qqQQ', 25000, 0, 1000, 60000)
    identity(4, 'recovery.automatic_hp'); numbers('HBB', 1, 9, 0); optional(1); numbers('HB', 0, 4)
    numbers('BBqQHH', 1, 1, 5000000, 1000, 700, 0)
    table(18, 5)
    for local, profile, timed in [(1,1,None),(2,2,None),(3,3,None),(4,None,1),(5,4,None)]:
        numbers('HI',5,local); optional(profile); optional(timed); optional(None)
    # Loot: mode 1 weighted / 2 independent bp; reward 1 item, 2 gold, 3 exp, 5 group, 8 poison.
    table(19, 2)
    for id, code, mode, entries in [
        (1, 'loot.gift_box', 1, [(1, 1, (6, 1), 5, 70, 0), (2, 2, None, 1000, 20, 0), (3, 1, (1, 1), 1, 10, 500)]),
        (2, 'loot.iron_chest', 2, [(1, 1, (5, 2), 3, 10000, 0), (2, 3, None, 5000, 2500, 0), (3, 5, None, 101, 500, 0),
                                   (4, 8, None, 0, 100, 0)]),
    ]:
        identity(id, code); numbers('BI', mode, len(entries))
        for entry, kind, item, amount, weight, rare in entries:
            numbers('HBB', entry, kind, item is not None)
            if item is not None:
                numbers('HI', *item)
            numbers('QIH', amount, weight, rare)
    table(20, 3)
    for local, kind, loot, lock in [(1, 1, 1, 0), (2, 2, 2, 7), (3, 3, None, 0)]:
        numbers('HIB', 8, local, kind); optional(loot); numbers('H', lock)
    table(21, 1); numbers('HIH', 18, 1, 7)
    # Currency tokens: wallet 1 gold / 2 dragon coin, value per unit.
    table(22, 2); numbers('HIBQ', 16, 1, 1, 5000000); numbers('HIBQ', 16, 2, 2, 100)
    table(23, 1); identity(1, 'effect.ability'); numbers('BBB',2,2,0)
    table(24, 1); identity(1, 'timed.ability'); numbers('IIQHBB',1,7,60000,1,9,0); optional(1)
    table(25, 0); table(26, 0); table(27, 0); table(28, 0)
    table(29, 1 if with_refinement_recipe else 0)
    if with_refinement_recipe:
        identity(1,'refine.iron_sword_9_10')
        numbers('HIH',1,1,9);numbers('HIH',1,1,10)
        numbers('QHBBI',5000,7500,1,0,2)  # Keep; no optional failure level; 2 materials.
        numbers('HIHI',6,1,0,7);numbers('HIHI',6,2,0,2)
    table(30,2 if with_quest_items else 0)
    if with_quest_items:
        numbers('HIB',10,1,1);text('');numbers('BB',0,0)
        numbers('HIB',10,2,2);text('quest.horse_training');numbers('BB',9,0)
    table(31,2 if with_socket_stones else 0)
    if with_socket_stones:
        numbers('HIBBB',7,1,1,3,1);text('stone.precision');text('stone.penetration')
        numbers('HIBBB',7,2,2,0,0);text('');text('')
    table(32,1 if with_costumes else 0)
    if with_costumes:numbers('HIBBBBQ',4,1,3,1,1,3,60000)
    table(33,8 if with_costumes else 0)
    if with_costumes:
        for c in range(1,5):
            for sex in range(1,3):
                numbers('HIBB',4,1,c,sex);text(f'models/costume_{c}_{sex}.model')
    return hashlib.sha256(data).hexdigest()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--catalog-executable", type=Path, required=True)
    parser.add_argument("--refinement-executable", type=Path)
    parser.add_argument("--quest-executable", type=Path)
    parser.add_argument("--stone-executable", type=Path)
    parser.add_argument("--costume-executable", type=Path)
    args = parser.parse_args()
    expected = fixture_digest()
    if expected != "23e4d38d621f063682ef777f9c559e249076f1fa1db36d4690dfc5d615ba805b":
        raise SystemExit("Python canonical fixture changed without a schema/golden review")
    result = subprocess.run([str(args.catalog_executable.resolve(strict=True)), "--hash-fixture"],
                            capture_output=True, check=True, timeout=30)
    actual = result.stdout.decode("ascii").strip()
    if actual != expected:
        raise SystemExit("C++ catalog digest disagrees with independent Python encoding")
    print("Independent canonical SHA-256 fixture passed: " + expected)
    if args.refinement_executable:
        expected=fixture_digest(True)
        if expected!='0c7e8184521af79ee5a9aadb24b65cfbb80a5fde64af39055a577db7bb6bba91':
            raise SystemExit('Refinement fixture changed without a schema/golden review')
        result=subprocess.run([str(args.refinement_executable.resolve(strict=True)), '--hash-fixture'],capture_output=True,check=True,timeout=30)
        if result.stdout.decode('ascii').strip()!=expected:
            raise SystemExit('Refinement recipe digest disagrees with independent Python encoding')
        print('Independent refinement SHA-256 fixture passed: '+expected)
    if args.quest_executable:
        expected=fixture_digest(with_quest_items=True)
        if expected!='c8ad1ae9711402411264c4560dbf5b284ec5b224aabebda6ea8648ca1765c934':
            raise SystemExit('Quest fixture changed without a schema/golden review: '+expected)
        result=subprocess.run([str(args.quest_executable.resolve(strict=True)), '--hash-fixture'],capture_output=True,check=True,timeout=30)
        if result.stdout.decode('ascii').strip()!=expected:
            raise SystemExit('Quest item digest disagrees with independent Python encoding')
        print('Independent quest SHA-256 fixture passed: '+expected)
    if args.stone_executable:
        expected=fixture_digest(with_socket_stones=True)
        if expected!='d6783d6ea8169a98cc1b46a3506f5b4573ca5bf5e161008757d8fef0551827c9':raise SystemExit('Stone fixture changed without a schema/golden review: '+expected)
        result=subprocess.run([str(args.stone_executable.resolve(strict=True)), '--hash-fixture'],capture_output=True,check=True,timeout=30)
        if result.stdout.decode('ascii').strip()!=expected:raise SystemExit('Stone digest disagrees with independent Python encoding')
        print('Independent stone SHA-256 fixture passed: '+expected)

    if args.costume_executable:
        expected=fixture_digest(with_costumes=True)
        if expected!='c165a3254110d886c490d1203c1b5a30e2167eee8cdae82e40f6c7bec51c3471':raise SystemExit('Costume fixture changed without a schema/golden review: '+expected)
        result=subprocess.run([str(args.costume_executable.resolve(strict=True)), '--hash-fixture'],capture_output=True,check=True,timeout=30)
        if result.stdout.decode('ascii').strip()!=expected:raise SystemExit('Costume digest disagrees with independent Python encoding')
        print('Independent costume SHA-256 fixture passed: '+expected)
