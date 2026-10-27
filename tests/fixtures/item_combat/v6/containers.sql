-- Interpretation fixture only, not production content. Loaded after
-- v5/recovery_consumables.sql; mirrors tests/server/ItemCatalogFixture.hpp.
INSERT INTO protos.loot_profile VALUES(1,'loot.gift_box',1),(2,'loot.iron_chest',2);
INSERT INTO protos.item_base VALUES
 (8,1,'container.gift_box','item.gift_box.name','item.gift_box.description','icons/gift_box.png',
  1,1,50,100,0,1,2,1,1,NULL,NULL,1,0,0,0),
 (8,2,'container.iron_chest','item.iron_chest.name','item.iron_chest.description','icons/iron_chest.png',
  1,1,10,100,0,1,2,1,1,NULL,NULL,1,0,0,0),
 (8,3,'container.quest_box','item.quest_box.name','item.quest_box.description','icons/quest_box.png',
  1,1,1,100,0,1,2,1,1,NULL,NULL,1,0,0,0),
 (18,1,'container_key.iron_key','item.iron_key.name','item.iron_key.description','icons/iron_key.png',
  1,1,20,100,0,1,2,1,1,NULL,NULL,1,0,0,0);
INSERT INTO protos.loot_entry VALUES
 (1,1,1,6,1,5,70,0),(1,2,2,NULL,NULL,1000,20,0),(1,3,1,1,1,1,10,500),
 (2,1,1,5,2,3,10000,0),(2,2,3,NULL,NULL,5000,2500,0),(2,3,5,NULL,NULL,101,500,0),(2,4,8,NULL,NULL,0,100,0);
INSERT INTO protos.item_container VALUES(8,1,1,1,0),(8,2,2,2,7),(8,3,3,NULL,0);
INSERT INTO protos.item_container_key VALUES(18,1,7);
