"""Independent Python encoder for the v24 typed item fixture (not a runtime reader)."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess


def fixture_digest(with_refinement_recipe=False, with_quest_items=False, with_socket_stones=False, with_costumes=False, with_gathering=False, with_polymorph=False, with_souls=False, with_companions=False, with_growth=False, with_pet_care=False, with_lifecycle=False, with_services=False, with_attributes=False):
    with_pet_care=with_pet_care or with_lifecycle
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
    text("auriga.item_catalog.v24"); numbers("I", 24)
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
    if with_lifecycle:
        for kind in range(1,9):base_items.append((17,kind,'special','pet_supply_'+str(kind),1,1 if kind in (7,8) else 20,10,0,1,1,None,None,0,0))
    table(8, len(base_items))
    for category, local, prefix, code, height, stack, grams, minimum, use, refine, appearance, bonus, buy, sell in sorted(base_items):
        numbers("HI", category, local)
        text(prefix + "." + code)
        text("item." + ("poly_"+code if category==13 else "soul_"+str(local) if category==14 else "companion_"+str(local) if category==15 else code) + ".name"); text("item." + ("poly_"+code if category==13 else "soul_"+str(local) if category==14 else "companion_"+str(local) if category==15 else code) + ".description"); text("icons/" + ('leather' if category in (4,7,10,11,12,13,14,15,17) or with_pet_care and (category,local)==(8,4) else code) + ".png")
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
        mask = sum({'ItemBase': 1, 'Refinement': 2, 'TimedEffect': 4,'Instance':8}[name] for name in row['allowed_sources'])
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
        numbers('HI',5,local); optional(None if with_services and local<=3 else None if with_attributes and local==1 else profile); optional(timed); optional(None); optional(local if with_services and local<=3 else None);optional(1 if with_attributes and local==1 else None)
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
    table(42,2 if with_lifecycle or with_companions or with_growth else 1 if with_pet_care else 0)
    if with_companions:
        identity(1,'companion.pet');numbers('I',34001);text('models/pet.model')
        identity(2,'companion.mount');numbers('I',20110);text('models/mount.model')
    if with_growth:
        identity(1,'growth.young');numbers('I',34041);text('models/growth_young.model')
        identity(2,'growth.adult');numbers('I',34042);text('models/growth_adult.model')
    if with_pet_care:identity(1,'care.pet');numbers('I',34041);text('models/care_pet.model')
    if with_lifecycle:identity(2,'lifecycle.skin');numbers('I',34042);text('models/pet_skin.model')
    table(43,2 if with_companions else 1 if with_growth or with_pet_care else 0)
    if with_companions:
        numbers('HIBI',15,1,1,1);optional(None);numbers('BQB',3,3000,1);optional(None);optional(None)
        numbers('HIBI',15,2,2,2);optional(2);numbers('BQB',4,5000,0);optional(None);optional(None)
    if with_growth:
        numbers('HIBI',15,1,3,1);optional(None);numbers('BQB',4,259200000,0);optional(1);optional(None)
    if with_pet_care:numbers('HIBI',15,1,3,1);optional(None);numbers('BQB',4,300000,0);optional(1);optional(1 if with_lifecycle else None)
    table(44,1 if with_growth or with_pet_care else 0)
    if with_growth:
        identity(1,'growth.standard');numbers('HB',120,1);numbers('I',120)
        for level in range(1,121):numbers('HQQ',level,0 if level==120 else level*1000,0 if level==120 else level*100)
        numbers('I',4)
        for stage,maximum,form in [(0,40,1),(1,60,1),(2,80,1),(3,120,2)]:numbers('BHI',stage,maximum,form)
    if with_pet_care:
        identity(1,'growth.care');numbers('HB',3,2);numbers('I',3)
        for level,mob,item in [(1,100,10),(2,200,20),(3,0,0)]:numbers('HQQ',level,mob,item)
        stages=[(0,1,1),(1,2,1),(2,3,1)] if with_lifecycle else [(0,3,1)]
        numbers('I',len(stages))
        for row in stages:numbers('BHI',*row)
    table(45,1 if with_pet_care else 0)
    if with_pet_care:
        identity(1,'hatch.care');numbers('HIQ',15,1,100000);numbers('9H',30,40,5,10,20,5,10,20,5)
    table(46,4 if with_pet_care else 0)
    if with_pet_care:
        for row in [(1,1,1,3,1,1,0,0,2),(6,1,1,1,2,0,300,0,0),(6,2,2,2,1,1,0,5000,0),(8,4,1,1,1,1,500,0,0)]:numbers('HIBBBIHHI',*row)
    table(47,1 if with_lifecycle else 0)
    if with_lifecycle:
        identity(1,'training.fixture');numbers('5H',2,3,0,1,6)
        numbers('I',4)
        for row in [(0,500,200,200),(1000,1200,400,400),(2000,2100,700,700),(3000,3000,1000,1000)]:numbers('QIHH',*row)
        numbers('I',4)
        for row in [(0,1,0),(1,2,0),(2,3,0),(3,0,1500)]:numbers('BBQ',*row)
        numbers('I',2)
        for skill,code,effect,values in [(1,'pet.warrior_resist',7,[100,200]),(2,'pet.boss_damage',11,[200,400])]:
            identity(skill,code);numbers('BI',effect,len(values))
            for rank,value in enumerate(values,1):numbers('Hq',rank,value)
    table(48,8 if with_lifecycle else 0)
    if with_lifecycle:
        for kind in range(1,9):
            identity(kind,'supply.fixture_'+str(kind));numbers('B',kind)
            if kind==1:numbers('IBBHQ',1,1,1,7100,10800000)
            if kind==4:numbers('QQ',86400000,31535940000)
            if kind==6:numbers('5H',3000,5,150,100,100)
            if kind==8:numbers('IBQ',2,4,10000)
    table(49,8 if with_lifecycle else 0)
    if with_lifecycle:
        for kind in range(1,9):numbers('HII',17,kind,kind)
    table(50,3 if with_services else 0)
    if with_services:
        for kind in range(1,4):identity(kind,'socket.service_'+str(kind));numbers('BH',kind,1)
    table(51,1 if with_attributes else 0)
    if with_attributes:
        identity(1,'attribute.fixture');numbers('BI',1,3)
        for pool,capacity,exclude in [(1,5,1),(2,2,0),(3,1,0)]:numbers('BBB',pool,capacity,exclude)
        numbers('I',14)
        for pool,ids in [(1,range(1,9)),(2,range(21,24)),(3,range(21,24))]:
            for bonus in ids:
                numbers('BIII',pool,bonus,bonus*10 if pool==1 else 1,2)
                for tier in (1,2):numbers('Bq',tier,tier*100)
    table(52,1 if with_attributes else 0)
    if with_attributes:
        identity(1,'attribute.service_1');numbers('BBHHBBBBHBI',1,1,1,10000,7,0,5,1,0,0,2)
        numbers('BHBH',1,4000,2,6000)
    table(53,1 if with_attributes else 0)
    if with_attributes:numbers('HII',1,1,1)
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
    parser.add_argument('--pet-lifecycle-executable',type=Path)
    parser.add_argument('--socket-service-executable',type=Path)
    parser.add_argument('--attribute-executable',type=Path)
    args = parser.parse_args()
    expected = fixture_digest()
    if expected != "d06b2c17c2d31e239fea2aeb169c5918690c33cd4a7c7021e02271a3765fdf47":
        raise SystemExit("Python canonical fixture changed without a schema/golden review")
    result = subprocess.run([str(args.catalog_executable.resolve(strict=True)), "--hash-fixture"],
                            capture_output=True, check=True, timeout=30)
    actual = result.stdout.decode("ascii").strip()
    if actual != expected:
        raise SystemExit("C++ catalog digest disagrees with independent Python encoding")
    print("Independent canonical SHA-256 fixture passed: " + expected)
    if args.refinement_executable:
        expected=fixture_digest(True)
        if expected!='663061c1418e4191f76abaabae3bd45034c486849d0590d9e440ce53638540fe':
            raise SystemExit('Refinement fixture changed without a schema/golden review')
        result=subprocess.run([str(args.refinement_executable.resolve(strict=True)), '--hash-fixture'],capture_output=True,check=True,timeout=30)
        if result.stdout.decode('ascii').strip()!=expected:
            raise SystemExit('Refinement recipe digest disagrees with independent Python encoding')
        print('Independent refinement SHA-256 fixture passed: '+expected)
    if args.quest_executable:
        expected=fixture_digest(with_quest_items=True)
        if expected!='503ff5d163efd7267525e856722c69d0fcd992ba18a1f27c7854775deae2d005':
            raise SystemExit('Quest fixture changed without a schema/golden review: '+expected)
        result=subprocess.run([str(args.quest_executable.resolve(strict=True)), '--hash-fixture'],capture_output=True,check=True,timeout=30)
        if result.stdout.decode('ascii').strip()!=expected:
            raise SystemExit('Quest item digest disagrees with independent Python encoding')
        print('Independent quest SHA-256 fixture passed: '+expected)
    if args.stone_executable:
        expected=fixture_digest(with_socket_stones=True)
        if expected!='c9a02a8148477c952b1503ec2b67d4e97d3c351be97fcc15b01a872821e9cd07':raise SystemExit('Stone fixture changed without a schema/golden review: '+expected)
        result=subprocess.run([str(args.stone_executable.resolve(strict=True)), '--hash-fixture'],capture_output=True,check=True,timeout=30)
        if result.stdout.decode('ascii').strip()!=expected:raise SystemExit('Stone digest disagrees with independent Python encoding')
        print('Independent stone SHA-256 fixture passed: '+expected)

    if args.costume_executable:
        expected=fixture_digest(with_costumes=True)
        if expected!='5d3dc8ebf3c24bcdb7c4251b8cab03afa5c7b6b0eb19354a42dc647c7368f70f':raise SystemExit('Costume fixture changed without a schema/golden review: '+expected)
        result=subprocess.run([str(args.costume_executable.resolve(strict=True)), '--hash-fixture'],capture_output=True,check=True,timeout=30)
        if result.stdout.decode('ascii').strip()!=expected:raise SystemExit('Costume digest disagrees with independent Python encoding')
        print('Independent costume SHA-256 fixture passed: '+expected)

    if args.gathering_executable:
        expected=fixture_digest(with_gathering=True)
        if expected!='e160ab1b7e7b515d8db7b051e750ae51648a26b1f4e2963ec671c415ec4c6ac2':raise SystemExit('Gathering fixture changed without a schema/golden review: '+expected)
        result=subprocess.run([str(args.gathering_executable.resolve(strict=True)), '--hash-fixture'],capture_output=True,check=True,timeout=30)
        if result.stdout.decode('ascii').strip()!=expected:raise SystemExit('Gathering digest disagrees with independent Python encoding')
        print('Independent gathering SHA-256 fixture passed: '+expected)

    if args.polymorph_executable:
        expected=fixture_digest(with_polymorph=True)
        if expected!='0a355b93a62c01da240c46aca823f9fd19e6cf8c11fe9bbf9bb9c22aa6d394ed':raise SystemExit('Polymorph fixture changed without a schema/golden review: '+expected)
        result=subprocess.run([str(args.polymorph_executable.resolve(strict=True)), '--hash-fixture'],capture_output=True,check=True,timeout=30)
        if result.stdout.decode('ascii').strip()!=expected:raise SystemExit('Polymorph digest disagrees with independent Python encoding')
        print('Independent polymorph SHA-256 fixture passed: '+expected)

    if args.soul_executable:
        expected=fixture_digest(with_souls=True)
        if expected!='a41ca5bd3248a22129fcf6e63709264df794753c92a3f63517dd58e405403d38':raise SystemExit('Soul fixture changed without a schema/golden review: '+expected)
        result=subprocess.run([str(args.soul_executable.resolve(strict=True)), '--hash-fixture'],capture_output=True,check=True,timeout=30)
        if result.stdout.decode('ascii').strip()!=expected:raise SystemExit('Soul digest disagrees with independent Python encoding')
        print('Independent Soul SHA-256 fixture passed: '+expected)

    if args.companion_executable:
        expected=fixture_digest(with_companions=True)
        if expected!='13bbad225c60635be11250e208b18dceecbc80025ee83673943151f646015ace':raise SystemExit('Companion fixture changed without a schema/golden review: '+expected)
        result=subprocess.run([str(args.companion_executable.resolve(strict=True)), '--hash-fixture'],capture_output=True,check=True,timeout=30)
        if result.stdout.decode('ascii').strip()!=expected:raise SystemExit('Companion digest disagrees with independent Python encoding')
        print('Independent Companion SHA-256 fixture passed: '+expected)

    if args.growth_executable:
        expected=fixture_digest(with_growth=True)
        if expected!='0b65c3d9f8b67ea6383b4dfab41a6638d30a3a76a178eda43a16a54059919cca':raise SystemExit('Growth fixture changed without a schema/golden review: '+expected)
        result=subprocess.run([str(args.growth_executable.resolve(strict=True)), '--hash-fixture'],capture_output=True,check=True,timeout=30)
        if result.stdout.decode('ascii').strip()!=expected:raise SystemExit('Growth pet digest disagrees with independent Python encoding')
        print('Independent growth pet SHA-256 fixture passed: '+expected)

    if args.pet_care_executable:
        expected=fixture_digest(with_pet_care=True)
        if expected!='d20155144678372244e88e868107eee9db08e99b44884c53e254f0aa4ed87ad0':raise SystemExit('Pet care fixture changed without a schema/golden review: '+expected)
        result=subprocess.run([str(args.pet_care_executable.resolve(strict=True)), '--hash-fixture'],capture_output=True,check=True,timeout=30)
        if result.stdout.decode('ascii').strip()!=expected:raise SystemExit('Pet care digest disagrees with independent Python encoding')
        print('Independent pet care SHA-256 fixture passed: '+expected)

    if args.pet_lifecycle_executable:
        expected=fixture_digest(with_lifecycle=True)
        if expected!='65be2e28eaa85cf1709c59aeb9e8e74f5759ea6f205ec4cbb8222814949d19c2':raise SystemExit('Lifecycle fixture changed without schema/golden review: '+expected)
        result=subprocess.run([str(args.pet_lifecycle_executable.resolve(strict=True)), '--hash-fixture'],capture_output=True,check=True,timeout=30)
        if result.stdout.decode('ascii').strip()!=expected:raise SystemExit('Lifecycle digest disagrees with independent Python encoding')
        print('Independent pet lifecycle SHA-256 fixture passed: '+expected)
        state=bytearray()
        def put(fmt,*values):state.extend(struct.pack('<'+fmt,*values))
        name=b'Fixture';put('IHHIB',0x54504741,1,15,1,len(name));state.extend(name)
        put('QQHBQQBQQB3H',100000,123456789,1,0,0,0,4,300000,1234,0,35,15,20)
        for row in [(1,1,2),(0,0,0),(0,0,0),(1,2,1)]:put('BIH',*row)
        put('QBB',300,0,0)
        result=subprocess.run([str(args.pet_lifecycle_executable.resolve(strict=True)), '--state-fixture'],capture_output=True,check=True,timeout=30)
        if bytes.fromhex(result.stdout.decode('ascii').strip())!=state:raise SystemExit('Pet state codec disagrees with independent byte encoding')
        print('Independent complete pet-state bytes passed: '+hashlib.sha256(state).hexdigest())

    if args.socket_service_executable:
        expected=fixture_digest(with_services=True)
        if expected!='71aba59d525993260d645c34ce59b00ee5748b5187e2a0501d325b8c0804530e':raise SystemExit('Socket service fixture changed without schema/golden review: '+expected)
        result=subprocess.run([str(args.socket_service_executable.resolve(strict=True)), '--hash-fixture'],capture_output=True,check=True,timeout=30)
        if result.stdout.decode('ascii').strip()!=expected:raise SystemExit('Socket service digest disagrees with independent encoding')
        print('Independent socket service SHA-256 fixture passed: '+expected)

    if args.attribute_executable:
        expected=fixture_digest(with_attributes=True)
        if expected!='23e2fbfcd54f352facafe8ffa1d02933bf62e8756992aa58029563703f386766':raise SystemExit('Attribute fixture changed without schema/golden review: '+expected)
        result=subprocess.run([str(args.attribute_executable.resolve(strict=True)), '--hash-fixture'],capture_output=True,check=True,timeout=30)
        if result.stdout.decode('ascii').strip()!=expected:raise SystemExit('Instance attribute digest disagrees with independent encoding')
        state=bytearray(struct.pack('<IHHIB',0x54414741,1,1,1,3))
        for slot in [(2,-100),None,(3,200),None,None,(21,100),None,None]:
            state.extend(struct.pack('<B',slot is not None))
            if slot:state.extend(struct.pack('<Iq',*slot))
        result=subprocess.run([str(args.attribute_executable.resolve(strict=True)), '--state-fixture'],capture_output=True,check=True,timeout=30)
        if bytes.fromhex(result.stdout.decode('ascii').strip())!=state:raise SystemExit('Instance attribute payload differs from independent byte encoding')
        print('Independent instance attribute digest passed: '+expected)
        print('Independent instance attribute payload passed: '+hashlib.sha256(state).hexdigest())
