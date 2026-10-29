"""Independent Python encoder for the v21 typed item fixture (not a runtime reader)."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess


def fixture_digest(with_refinement_recipe=False, with_quest_items=False, with_socket_stones=False, with_costumes=False, with_gathering=False, with_polymorph=False, with_souls=False, with_companions=False, with_growth=False, with_pet_care=False):
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
    text("auriga.item_catalog.v21"); numbers("I", 21)
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
    if with_gathering:profiles.append((5,'refine.tool',7,30))
    if with_souls:profiles.append((5,'refine.soul',8,30))
    if with_companions:profiles.append((5,'refine.companion',9,30))
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
    if with_gathering:
        base_items.extend([(11,1,'tool','rod',1,1,10,0,1,5,None,None,0,0),
                           (12,1,'fishing','live',1,200,10,0,2,1,None,None,0,0),
                           (12,2,'fishing','dead',1,200,10,0,2,1,None,None,0,0),
                           (12,3,'fishing','bait',1,200,10,0,2,1,None,None,0,0)])
    if with_polymorph:base_items.extend([(13,1,'polymorph','orb',1,200,10,0,1,1,None,None,0,0),(13,2,'polymorph','book',1,1,10,0,1,1,None,None,0,0)])
    if with_souls:base_items.extend([(14,i,'soul','item_'+str(i),1,200 if i==3 else 1,10,0,1,1 if i==3 else 5,None,None,0,0) for i in range(1,4)])
    if with_companions:base_items.extend([(15,i,'companion','item_'+str(i),1,1,10,0,1,5,None,None,0,0) for i in range(1,3)])
    if with_growth or with_pet_care:base_items.append((15,1,'companion','item_1',1,1,10,0,1,1,None,None,0,0))
    if with_pet_care:base_items.append((8,4,'container','pet_egg',1,20,10,0,1,1,None,None,0,0))
    table(8, len(base_items))
    for category, local, prefix, code, height, stack, grams, minimum, use, refine, appearance, bonus, buy, sell in sorted(base_items):
        numbers("HI", category, local)
        text(prefix + "." + code)
        text("item." + ("poly_"+code if category==13 else "soul_"+str(local) if category==14 else "companion_"+str(local) if category==15 else code) + ".name"); text("item." + ("poly_"+code if category==13 else "soul_"+str(local) if category==14 else "companion_"+str(local) if category==15 else code) + ".description"); text("icons/" + ('leather' if category in (4,7,10,11,12,13,14,15) or with_pet_care and (category,local)==(8,4) else code) + ".png")
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
    table(19, 3 if with_gathering else 2)
    for id, code, mode, entries in [
        (1, 'loot.gift_box', 1, [(1, 1, (6, 1), 5, 70, 0), (2, 2, None, 1000, 20, 0), (3, 1, (1, 1), 1, 10, 500)]),
        (2, 'loot.iron_chest', 2, [(1, 1, (5, 2), 3, 10000, 0), (2, 3, None, 5000, 2500, 0), (3, 5, None, 101, 500, 0),
                                   (4, 8, None, 0, 100, 0)]),
    ] + ([(3,'loot.fish',1,[(1,1,(12,2),1,31,0),(2,1,(6,1),1,69,0)])] if with_gathering else []):
        identity(id, code); numbers('BI', mode, len(entries))
        for entry, kind, item, amount, weight, rare in entries:
            numbers('HBB', entry, kind, item is not None)
            if item is not None:
                numbers('HI', *item)
            numbers('QIH', amount, weight, rare)
    table(20, 4 if with_pet_care else 3)
    for local, kind, loot, lock in [(1, 1, 1, 0), (2, 2, 2, 7), (3, 3, None, 0)]+([(4,4,None,0)] if with_pet_care else []):
        numbers('HIB', 8, local, kind); optional(loot); numbers('H', lock);optional(1 if local==4 else None)
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
    table(34,1 if with_gathering else 0)
    if with_gathering:
        identity(1,'training.rod');numbers('I',31)
        for level in range(31):
            final=level==30
            numbers('HIHIIIHBBH',level,10+level,level*100,0 if final else 100,0 if final else 1,1 if final else 5,0 if final else 8000,1,0,0 if final else 1000)
    table(35,1 if with_gathering else 0)
    if with_gathering:numbers('HIBIB',11,1,2,1,3)
    table(36,3 if with_gathering else 0)
    if with_gathering:
        numbers('HIB',12,1,1);optional(3);numbers('BI',0,0)
        numbers('HIB',12,2,2);optional(None);numbers('BHI',1,5,1);numbers('I',0)
        numbers('HIB',12,3,3);optional(None);numbers('BI',0,20)
    table(37,2 if with_polymorph else 0)
    if with_polymorph:
        identity(1,'form.wolf');numbers('IB',101,4);identity(2,'form.boar');numbers('IB',102,1)
    table(38,2 if with_polymorph else 0)
    if with_polymorph:
        for profile in (1,2):
            identity(profile,'poly.orb' if profile==1 else 'poly.book')
            numbers('BHHHHIh',profile,20 if profile==1 else 0,3 if profile==1 else 0,10 if profile==1 else 1,0 if profile==1 else 30,0 if profile==1 else 50,0 if profile==1 else 29)
            numbers('I',41)
            for skill in range(41):
                numbers('BQIH',skill,(300000 if skill==0 else 2100000 if skill==40 else 600000) if profile==1 else 0,(30000 if skill==40 else 20000) if profile==1 else 0,1000)
            numbers('I',0 if profile==1 else 4)
            if profile==2:
                for mastery in range(4):numbers('BQQ',mastery,10000+mastery*5000,9000+mastery*5000)
    table(39,2 if with_polymorph else 0)
    if with_polymorph:
        numbers('HII',13,1,1);optional(1);numbers('HII',13,2,2);optional(None)
    table(40,1 if with_souls else 0)
    if with_souls:
        identity(1,'soul.standard');numbers('I',31)
        for level in range(31):numbers('H5IQ3I',level,180,60,500,0,500,60000,15000,18000,25000)
    table(41,3 if with_souls else 0)
    if with_souls:
        for local,kind,channel in [(1,1,1),(2,1,2),(3,2,None)]:
            numbers('HIBB',14,local,kind,channel is not None)
            if channel is not None:numbers('B',channel)
            optional(1 if kind==1 else None);numbers('H',7500 if kind==2 else 0)
    table(42,2 if with_companions or with_growth else 1 if with_pet_care else 0)
    if with_companions:
        identity(1,'companion.pet');numbers('I',34001);text('models/pet.model')
        identity(2,'companion.mount');numbers('I',20110);text('models/mount.model')
    if with_growth:
        identity(1,'growth.young');numbers('I',34041);text('models/growth_young.model')
        identity(2,'growth.adult');numbers('I',34042);text('models/growth_adult.model')
    if with_pet_care:identity(1,'care.pet');numbers('I',34041);text('models/care_pet.model')
    table(43,2 if with_companions else 1 if with_growth or with_pet_care else 0)
    if with_companions:
        numbers('HIBI',15,1,1,1);optional(None);numbers('BQB',3,3000,1);optional(None)
        numbers('HIBI',15,2,2,2);optional(2);numbers('BQB',4,5000,0);optional(None)
    if with_growth:
        numbers('HIBI',15,1,3,1);optional(None);numbers('BQB',4,259200000,0);optional(1)
    if with_pet_care:numbers('HIBI',15,1,3,1);optional(None);numbers('BQB',4,300000,0);optional(1)
    table(44,1 if with_growth or with_pet_care else 0)
    if with_growth:
        identity(1,'growth.standard');numbers('HB',120,1);numbers('I',120)
        for level in range(1,121):numbers('HQQ',level,0 if level==120 else level*1000,0 if level==120 else level*100)
        numbers('I',4)
        for stage,maximum,form in [(0,40,1),(1,60,1),(2,80,1),(3,120,2)]:numbers('BHI',stage,maximum,form)
    if with_pet_care:
        identity(1,'growth.care');numbers('HB',3,2);numbers('I',3)
        for level,mob,item in [(1,100,10),(2,200,20),(3,0,0)]:numbers('HQQ',level,mob,item)
        numbers('I',1);numbers('BHI',0,3,1)
    table(45,1 if with_pet_care else 0)
    if with_pet_care:
        identity(1,'hatch.care');numbers('HIQ',15,1,100000);numbers('9H',30,40,5,10,20,5,10,20,5)
    table(46,4 if with_pet_care else 0)
    if with_pet_care:
        for row in [(1,1,1,3,1,1,0,0,2),(6,1,1,1,2,0,300,0,0),(6,2,2,2,1,1,0,5000,0),(8,4,1,1,1,1,500,0,0)]:numbers('HIBBBIHHI',*row)
    return hashlib.sha256(data).hexdigest()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--catalog-executable", type=Path, required=True)
    parser.add_argument("--refinement-executable", type=Path)
    parser.add_argument("--quest-executable", type=Path)
    parser.add_argument("--stone-executable", type=Path)
    parser.add_argument("--costume-executable", type=Path)
    parser.add_argument("--gathering-executable", type=Path)
    parser.add_argument("--polymorph-executable", type=Path)
    parser.add_argument("--soul-executable", type=Path)
    parser.add_argument("--companion-executable", type=Path)
    parser.add_argument('--growth-executable',type=Path)
    parser.add_argument('--pet-care-executable',type=Path)
    args = parser.parse_args()
    expected = fixture_digest()
    if expected != "885acaa3f10f968857fa77b6a0f76839c2c4c7279d62adaa33c5050daa0c5ddf":
        raise SystemExit("Python canonical fixture changed without a schema/golden review")
    result = subprocess.run([str(args.catalog_executable.resolve(strict=True)), "--hash-fixture"],
                            capture_output=True, check=True, timeout=30)
    actual = result.stdout.decode("ascii").strip()
    if actual != expected:
        raise SystemExit("C++ catalog digest disagrees with independent Python encoding")
    print("Independent canonical SHA-256 fixture passed: " + expected)
    if args.refinement_executable:
        expected=fixture_digest(True)
        if expected!='c29e8ce40a13d0b52bee659a78c48dc96bce03778dbc84a267e1bd4aab3e5def':
            raise SystemExit('Refinement fixture changed without a schema/golden review')
        result=subprocess.run([str(args.refinement_executable.resolve(strict=True)), '--hash-fixture'],capture_output=True,check=True,timeout=30)
        if result.stdout.decode('ascii').strip()!=expected:
            raise SystemExit('Refinement recipe digest disagrees with independent Python encoding')
        print('Independent refinement SHA-256 fixture passed: '+expected)
    if args.quest_executable:
        expected=fixture_digest(with_quest_items=True)
        if expected!='99f7a77aeabef3fc632740a41805f1a75fd8c9c9878450836b7c27667c9c2339':
            raise SystemExit('Quest fixture changed without a schema/golden review: '+expected)
        result=subprocess.run([str(args.quest_executable.resolve(strict=True)), '--hash-fixture'],capture_output=True,check=True,timeout=30)
        if result.stdout.decode('ascii').strip()!=expected:
            raise SystemExit('Quest item digest disagrees with independent Python encoding')
        print('Independent quest SHA-256 fixture passed: '+expected)
    if args.stone_executable:
        expected=fixture_digest(with_socket_stones=True)
        if expected!='42ffa205ec6f262767e49ef3cdbe7e69c13d3d3ed0ed73faa1ac0b61d41ec1f9':raise SystemExit('Stone fixture changed without a schema/golden review: '+expected)
        result=subprocess.run([str(args.stone_executable.resolve(strict=True)), '--hash-fixture'],capture_output=True,check=True,timeout=30)
        if result.stdout.decode('ascii').strip()!=expected:raise SystemExit('Stone digest disagrees with independent Python encoding')
        print('Independent stone SHA-256 fixture passed: '+expected)

    if args.costume_executable:
        expected=fixture_digest(with_costumes=True)
        if expected!='a479bf0313f968c03fff660c42d21b3c8aacfd6c815fc9da26875872a6941fd7':raise SystemExit('Costume fixture changed without a schema/golden review: '+expected)
        result=subprocess.run([str(args.costume_executable.resolve(strict=True)), '--hash-fixture'],capture_output=True,check=True,timeout=30)
        if result.stdout.decode('ascii').strip()!=expected:raise SystemExit('Costume digest disagrees with independent Python encoding')
        print('Independent costume SHA-256 fixture passed: '+expected)

    if args.gathering_executable:
        expected=fixture_digest(with_gathering=True)
        if expected!='6ec8b6ac84b20b3197a474b315c2e166b5cccf38cebe21bc188333684d4ff37a':raise SystemExit('Gathering fixture changed without a schema/golden review: '+expected)
        result=subprocess.run([str(args.gathering_executable.resolve(strict=True)), '--hash-fixture'],capture_output=True,check=True,timeout=30)
        if result.stdout.decode('ascii').strip()!=expected:raise SystemExit('Gathering digest disagrees with independent Python encoding')
        print('Independent gathering SHA-256 fixture passed: '+expected)

    if args.polymorph_executable:
        expected=fixture_digest(with_polymorph=True)
        if expected!='15d4492c26fdb5ff434f3cfc7edfb4722d724d372f81e2692165c3a0a816abbf':raise SystemExit('Polymorph fixture changed without a schema/golden review: '+expected)
        result=subprocess.run([str(args.polymorph_executable.resolve(strict=True)), '--hash-fixture'],capture_output=True,check=True,timeout=30)
        if result.stdout.decode('ascii').strip()!=expected:raise SystemExit('Polymorph digest disagrees with independent Python encoding')
        print('Independent polymorph SHA-256 fixture passed: '+expected)

    if args.soul_executable:
        expected=fixture_digest(with_souls=True)
        if expected!='253ed523e861823ba76bf51aa31c52cbb463e8219e101d7633f3f6b8d41e5e01':raise SystemExit('Soul fixture changed without a schema/golden review: '+expected)
        result=subprocess.run([str(args.soul_executable.resolve(strict=True)), '--hash-fixture'],capture_output=True,check=True,timeout=30)
        if result.stdout.decode('ascii').strip()!=expected:raise SystemExit('Soul digest disagrees with independent Python encoding')
        print('Independent Soul SHA-256 fixture passed: '+expected)

    if args.companion_executable:
        expected=fixture_digest(with_companions=True)
        if expected!='b95db06a2816f097ce42247af30268d79fe18b985f2a08fe66bede294304a6f8':raise SystemExit('Companion fixture changed without a schema/golden review: '+expected)
        result=subprocess.run([str(args.companion_executable.resolve(strict=True)), '--hash-fixture'],capture_output=True,check=True,timeout=30)
        if result.stdout.decode('ascii').strip()!=expected:raise SystemExit('Companion digest disagrees with independent Python encoding')
        print('Independent Companion SHA-256 fixture passed: '+expected)

    if args.growth_executable:
        expected=fixture_digest(with_growth=True)
        if expected!='9a17396c27100b2ff580b008901d386e829a8ba84fbd781fa0cafedf2d594145':raise SystemExit('Growth fixture changed without a schema/golden review: '+expected)
        result=subprocess.run([str(args.growth_executable.resolve(strict=True)), '--hash-fixture'],capture_output=True,check=True,timeout=30)
        if result.stdout.decode('ascii').strip()!=expected:raise SystemExit('Growth pet digest disagrees with independent Python encoding')
        print('Independent growth pet SHA-256 fixture passed: '+expected)

    if args.pet_care_executable:
        expected=fixture_digest(with_pet_care=True)
        if expected!='f18dcb8ca36d2f564cd20d1d87815eace21e24ce6211ea4f92dacad392eb83d1':raise SystemExit('Pet care fixture changed without a schema/golden review: '+expected)
        result=subprocess.run([str(args.pet_care_executable.resolve(strict=True)), '--hash-fixture'],capture_output=True,check=True,timeout=30)
        if result.stdout.decode('ascii').strip()!=expected:raise SystemExit('Pet care digest disagrees with independent Python encoding')
        print('Independent pet care SHA-256 fixture passed: '+expected)
