INSERT INTO protos.recovery_profile VALUES(4,'recovery.automatic_hp',4,1,9,0,1,0);
INSERT INTO protos.automatic_recovery_profile VALUES(4,4,1,1,5000000,1000,700,0);
INSERT INTO protos.item_base VALUES
    (5,5,'consumable.automatic_flask','item.automatic_flask.name','item.automatic_flask.description','icons/automatic_flask.png',
     1,1,1,100,10,1,3,1,1,NULL,NULL,1,0,0,0);
INSERT INTO protos.item_consumable(category_id,local_id,recovery_profile_id,timed_effect_profile_id) VALUES(5,5,4,NULL);
