-- Independent SQL representation of the typed timed-stat fixture.
INSERT INTO protos.bonus_profile VALUES(7,'bonus.timed_ability',3);
INSERT INTO protos.bonus_profile_entry VALUES(7,26,1500),(7,29,5),(7,33,2000);
INSERT INTO protos.timed_effect_group VALUES(1,'effect.ability',2,2,0);
INSERT INTO protos.timed_effect_profile VALUES(1,'timed.ability',1,7,60000,1,9,0,1);
INSERT INTO protos.item_base VALUES
    (5,4,'consumable.ability_flask','item.ability_flask.name','item.ability_flask.description','icons/ability_flask.png',
     1,1,200,50,10,1,3,1,1,NULL,NULL,1,0,0,0);
INSERT INTO protos.item_consumable(category_id,local_id,recovery_profile_id,timed_effect_profile_id) VALUES(5,4,NULL,1);
