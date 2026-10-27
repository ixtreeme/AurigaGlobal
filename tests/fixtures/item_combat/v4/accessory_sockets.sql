-- Interpretation fixture only, not approved balance or a production seed.
INSERT INTO protos.item_use_policy VALUES(1,'use.equipment',15,3,1);
INSERT INTO protos.item_trade_policy VALUES(1,'trade.unbound',1,1,1);
INSERT INTO protos.item_appearance_profile VALUES
    (1,'appearance.sword','models/iron_sword.model'),(2,'appearance.armor','models/iron_armor.model');
INSERT INTO protos.item_attack_profile VALUES(1,'attack.melee',25,10,3000,1);
INSERT INTO protos.item_ammo_policy VALUES(1,'ammo.none',1,0),(2,'ammo.arrow',2,1);
INSERT INTO protos.item_resistance_profile(profile_id,code) VALUES(1,'resistance.none');
INSERT INTO protos.bonus_profile VALUES
    (1,'bonus.sword_base',1),(2,'bonus.refinement_three',2),(3,'bonus.refinement_nine',2);
INSERT INTO protos.bonus_profile_entry VALUES(1,1,2500),(1,7,500),(2,1,5000),(2,21,100),(3,21,200);
INSERT INTO protos.refinement_profile VALUES(1,'refine.none',1,0),(2,'refine.weapon_linear',2,9),(3,'refine.armor',3,2);
INSERT INTO protos.refinement_level(profile_id,level,physical_attack_gain_raw,magical_attack_gain_raw) VALUES
    (1,0,0,0),(2,0,0,0),(2,1,10000,4000),(2,2,20000,8000),(2,3,30000,12000),
    (2,4,40000,16000),(2,5,50000,20000),(2,6,60000,24000),(2,7,70000,28000),(2,8,80000,32000),(2,9,90000,36000);
INSERT INTO protos.refinement_level(profile_id,level,physical_defense_gain_raw,magical_defense_gain_raw) VALUES
    (3,0,0,0),(3,1,5000,2000),(3,2,10000,4000);
INSERT INTO protos.item_base(category_id,local_id,content_code,name_key,description_key,icon_asset,
    inventory_width,inventory_height,max_stack,weight_grams,min_level,binding_policy,
    use_policy_id,trade_policy_id,refinement_profile_id,appearance_profile_id,currency_kind,buy_amount,sell_amount)
VALUES
    (1,1,'weapon.iron_sword','item.iron_sword.name','item.iron_sword.description','icons/iron_sword.png',1,3,1,1200,1,1,1,1,2,1,1,100,50),
    (2,1,'armor.iron_armor','item.iron_armor.name','item.iron_armor.description','icons/iron_armor.png',1,2,1,5000,5,1,1,1,3,2,1,200,100);
INSERT INTO protos.item_weapon VALUES(1,1,1,1,120000,30000,1,1);
INSERT INTO protos.item_armor VALUES(2,1,2,40000,20000,1);
UPDATE protos.item_base SET base_bonus_profile_id=1 WHERE category_id=1 AND local_id=1;
UPDATE protos.refinement_level SET extra_bonus_profile_id=2 WHERE profile_id=2 AND level=3;
UPDATE protos.refinement_level SET extra_bonus_profile_id=3 WHERE profile_id=2 AND level=9;

-- Plain accessory and material interpretation fixtures, not production balance.
INSERT INTO protos.item_use_policy VALUES(2,'use.material',15,3,0);
INSERT INTO protos.bonus_profile VALUES
    (4,'bonus.accessory_low',2),(5,'bonus.accessory_high',2),(6,'bonus.accessory_base',1);
INSERT INTO protos.bonus_profile_entry VALUES(4,3,2000),(5,3,6000),(6,3,3000);
INSERT INTO protos.refinement_profile VALUES(4,'refine.accessory',4,15);
INSERT INTO protos.refinement_level(profile_id,level) VALUES(4,0);
INSERT INTO protos.refinement_level(profile_id,level,extra_bonus_profile_id) VALUES(4,1,4);
INSERT INTO protos.refinement_level(profile_id,level,extra_bonus_profile_id) VALUES(4,2,4);
INSERT INTO protos.refinement_level(profile_id,level,extra_bonus_profile_id) VALUES(4,3,4);
INSERT INTO protos.refinement_level(profile_id,level,extra_bonus_profile_id) VALUES(4,4,4);
INSERT INTO protos.refinement_level(profile_id,level,extra_bonus_profile_id) VALUES(4,5,4);
INSERT INTO protos.refinement_level(profile_id,level,extra_bonus_profile_id) VALUES(4,6,4);
INSERT INTO protos.refinement_level(profile_id,level,extra_bonus_profile_id) VALUES(4,7,4);
INSERT INTO protos.refinement_level(profile_id,level,extra_bonus_profile_id) VALUES(4,8,4);
INSERT INTO protos.refinement_level(profile_id,level,extra_bonus_profile_id) VALUES(4,9,4);
INSERT INTO protos.refinement_level(profile_id,level,extra_bonus_profile_id) VALUES(4,10,4);
INSERT INTO protos.refinement_level(profile_id,level,extra_bonus_profile_id) VALUES(4,11,4);
INSERT INTO protos.refinement_level(profile_id,level,extra_bonus_profile_id) VALUES(4,12,4);
INSERT INTO protos.refinement_level(profile_id,level,extra_bonus_profile_id) VALUES(4,13,4);
INSERT INTO protos.refinement_level(profile_id,level,extra_bonus_profile_id) VALUES(4,14,4);
INSERT INTO protos.refinement_level(profile_id,level,extra_bonus_profile_id) VALUES(4,15,5);
INSERT INTO protos.item_base(category_id,local_id,content_code,name_key,description_key,icon_asset,
    inventory_width,inventory_height,max_stack,weight_grams,min_level,binding_policy,
    use_policy_id,trade_policy_id,refinement_profile_id,base_bonus_profile_id,currency_kind)
VALUES
    (3,1,'accessory.copper_bracelet','item.copper_bracelet.name','item.copper_bracelet.description','icons/copper_bracelet.png',1,1,1,80,0,1,1,1,4,6,1),
    (6,1,'material.leather','item.leather.name','item.leather.description','icons/leather.png',1,1,200,10,0,1,2,1,1,NULL,1);
INSERT INTO protos.item_accessory(category_id,local_id,equip_slot,resistance_profile_id) VALUES(3,1,6,1);
INSERT INTO protos.item_material VALUES(6,1,1);

-- Equipped-time socket interpretation fixture; amounts/chances are not approved balance.
INSERT INTO protos.item_base(category_id,local_id,content_code,name_key,description_key,icon_asset,
    inventory_width,inventory_height,max_stack,weight_grams,min_level,binding_policy,
    use_policy_id,trade_policy_id,refinement_profile_id,currency_kind)
VALUES
    (6,2,'material.amber','item.amber.name','item.amber.description','icons/amber.png',1,1,200,5,0,1,2,1,1,1),
    (6,3,'material.socket_drill','item.socket_drill.name','item.socket_drill.description','icons/socket_drill.png',1,1,20,30,0,1,2,1,1,1);
INSERT INTO protos.item_material VALUES(6,2,5),(6,3,4);
INSERT INTO protos.accessory_socket_profile VALUES(1,'socket.copper',3,1,1);
INSERT INTO protos.accessory_socket_grade VALUES
    (1,0,0,0,0,0,0,5000,10000),
    (1,1,86400000,1000,1000,100,1,5000,10000),
    (1,2,43200000,2000,2000,200,2,5000,10000),
    (1,3,21600000,4000,3000,300,3,0,0);
INSERT INTO protos.accessory_socket_material VALUES(1,1,6,3),(1,2,6,2);
UPDATE protos.item_accessory SET socket_profile_id=1 WHERE category_id=3 AND local_id=1;
