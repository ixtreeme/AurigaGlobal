-- Interpretation fixture only, not approved balance or a production seed.
INSERT INTO protos.item_use_policy VALUES(1,'use.equipment',15,3,1);
INSERT INTO protos.item_trade_policy VALUES(1,'trade.unbound',1,1,1);
INSERT INTO protos.item_appearance_profile VALUES
    (1,'appearance.sword','models/iron_sword.model'),(2,'appearance.armor','models/iron_armor.model');
INSERT INTO protos.item_attack_profile VALUES(1,'attack.melee',25,10,3000,1);
INSERT INTO protos.item_ammo_policy VALUES(1,'ammo.none',1,0),(2,'ammo.arrow',2,1);
INSERT INTO protos.item_resistance_profile(profile_id,code) VALUES(1,'resistance.none');
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
