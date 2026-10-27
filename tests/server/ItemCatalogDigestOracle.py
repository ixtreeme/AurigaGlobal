"""Independent Python encoder for the v5 recovery fixture (not a runtime reader)."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess


def fixture_digest():
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
    text("auriga.item_catalog.v5"); numbers("I", 5)
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
    table(7, 4)
    for id, code, kind, maximum in [(1, "refine.none", 1, 0), (2, "refine.weapon_linear", 2, 9), (3, "refine.armor", 3, 2), (4, "refine.accessory", 4, 15)]:
        identity(id, code); numbers("BHI", kind, maximum, maximum + 1)
        for level in range(maximum + 1):
            gain = (10000 * level, 4000 * level, 0, 0) if kind == 2 else (0, 0, 5000 * level, 2000 * level) if kind == 3 else (0, 0, 0, 0)
            numbers("H4q", level, *gain)
            bonus = 2 if kind == 2 and level == 3 else 3 if kind == 2 and level == 9 else None
            if kind == 4 and level:
                bonus = 5 if level == 15 else 4
            optional(bonus)
    table(8, 9)
    for category, local, prefix, code, height, stack, grams, minimum, use, refine, appearance, bonus, buy, sell in [
        (1, 1, "weapon", "iron_sword", 3, 1, 1200, 1, 1, 2, 1, 1, 100, 50),
        (2, 1, "armor", "iron_armor", 2, 1, 5000, 5, 1, 3, 2, None, 200, 100),
        (3, 1, "accessory", "copper_bracelet", 1, 1, 80, 0, 1, 4, None, 6, 0, 0),
        (5, 1, "consumable", "instant_flask", 1, 200, 50, 10, 3, 1, None, None, 0, 0),
        (5, 2, "consumable", "reserve_flask", 1, 200, 50, 10, 3, 1, None, None, 0, 0),
        (5, 3, "consumable", "regeneration_flask", 1, 200, 50, 10, 3, 1, None, None, 0, 0),
        (6, 1, "material", "leather", 1, 200, 10, 0, 2, 1, None, None, 0, 0),
        (6, 2, "material", "amber", 1, 200, 5, 0, 2, 1, None, None, 0, 0),
        (6, 3, "material", "socket_drill", 1, 20, 30, 0, 2, 1, None, None, 0, 0),
    ]:
        numbers("HI", category, local)
        text(prefix + "." + code)
        text("item." + code + ".name"); text("item." + code + ".description"); text("icons/" + code + ".png")
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
                    'FlatPenetration','PercentPenetration','Resistance','CritChance','CritResistance','CritMultiplierExtra','BlockReduction','Evasion']),
        ('stage', ['StatFlat','StatPercent','OutgoingAction','OutgoingMode','Penetration','Resistance','Critical','IncomingAction','IncomingMode','Block','Accuracy']),
        ('channel', ['Any','Physical','Magical']), ('action', ['Any','Normal','Skill']), ('mode', ['Any','Pve','Pvp']),
        ('family', ['Any']), ('rank', ['Any']), ('element', ['Any']),
        ('stacking', ['Add','Max','ReplaceByPriority','UniqueSource']),
        ('aggregate_cap', ['RawStat','StatPercent','Outgoing','Incoming','FlatPenetration','PercentPenetration','Resistance','CritChance','CritResistance','CritMultiplierExtra','Block','Accuracy']),
    ]
    registry = json.loads((Path(__file__).resolve().parents[2] / 'config/item_combat/bonus_registry.json').read_text(encoding='utf-8'))
    numbers('I', registry['schema_version']); text(registry['status'])
    table(11, len(registry['bonuses']))
    for row in sorted(registry['bonuses'], key=lambda r: r['id']):
        identity(row['id'], row['code']); text(row['display_key'])
        codes = [names.index(row[field]) + (0 if field in ('family','rank','element') else 1) for field, names in fields]
        mask = sum({'ItemBase': 1, 'Refinement': 2}[name] for name in row['allowed_sources'])
        numbers('6BHH4Bqq', *codes, mask, row['minimum'], row['maximum'])
    table(12, 6)
    for id, code, source, entries in [
        (1, 'bonus.sword_base', 1, [(1, 2500), (7, 500)]),
        (2, 'bonus.refinement_three', 2, [(1, 5000), (21, 100)]),
        (3, 'bonus.refinement_nine', 2, [(21, 200)]),
        (4, 'bonus.accessory_low', 2, [(3, 2000)]),
        (5, 'bonus.accessory_high', 2, [(3, 6000)]),
        (6, 'bonus.accessory_base', 1, [(3, 3000)]),
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
    table(17, 3)
    identity(1, 'recovery.instant'); numbers('HBB', 1, 15, 1); optional(1); numbers('B', 1)
    numbers('qqHHI', 300000, 100000, 500, 1000, 10000)
    identity(2, 'recovery.reserve'); numbers('HBB', 1, 9, 0); optional(1); numbers('B', 2)
    numbers('qqIQH', 500000, 200000, 10000, 1000, 700)
    identity(3, 'recovery.periodic'); numbers('HBB', 1, 15, 0); optional(None); numbers('B', 3)
    numbers('qqQQ', 25000, 0, 1000, 60000)
    table(18, 3)
    for local, profile in [(1,1),(2,2),(3,3)]: numbers('HII', 5, local, profile)
    return hashlib.sha256(data).hexdigest()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--catalog-executable", type=Path, required=True)
    args = parser.parse_args()
    expected = fixture_digest()
    if expected != "ecffcba2e2774c3f67f54c4e95b58075bf2755663f2f8e905836298f29fb0c59":
        raise SystemExit("Python canonical fixture changed without a schema/golden review")
    result = subprocess.run([str(args.catalog_executable.resolve(strict=True)), "--hash-fixture"],
                            capture_output=True, check=True, timeout=30)
    actual = result.stdout.decode("ascii").strip()
    if actual != expected:
        raise SystemExit("C++ catalog digest disagrees with independent Python encoding")
    print("Independent canonical SHA-256 fixture passed: " + expected)
