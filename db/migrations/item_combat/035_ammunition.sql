-- Typed ammunition has its own stable identity; no legacy arrow vnum mapping.
INSERT IGNORE INTO protos.item_category(category_id,code,prototype_table) VALUES(21,'ammo','item_ammo');
INSERT IGNORE INTO protos.item_local_id_sequence(category_id,next_local_id) VALUES(21,1);
CREATE TABLE IF NOT EXISTS protos.item_ammo (
 category_id SMALLINT UNSIGNED NOT NULL DEFAULT 21,local_id INT UNSIGNED NOT NULL,
 ammo_policy_id INT UNSIGNED NOT NULL,
 PRIMARY KEY(category_id,local_id),CHECK(category_id=21),
 FOREIGN KEY(category_id,local_id) REFERENCES protos.item_base(category_id,local_id),
 FOREIGN KEY(ammo_policy_id) REFERENCES protos.item_ammo_policy(policy_id)
) ENGINE=InnoDB;
ALTER TABLE protos.item_base DROP CONSTRAINT IF EXISTS ck_item_base_category,
 DROP CONSTRAINT IF EXISTS ck_item_base_stack,DROP CONSTRAINT IF EXISTS ck_item_base_non_equipment;
ALTER TABLE protos.item_base
 ADD CONSTRAINT ck_item_base_category CHECK(category_id BETWEEN 1 AND 21),
 ADD CONSTRAINT ck_item_base_stack CHECK(category_id IN(5,6,7,8,9,10,12,13,14,16,17,18,20,21) OR max_stack=1),
 ADD CONSTRAINT ck_item_base_non_equipment CHECK(category_id NOT IN(5,6,7,8,9,10,12,13,14,16,17,18,20,21)
 OR (binding_policy<>3 AND (appearance_profile_id IS NULL OR category_id IN(7,10)) AND (base_bonus_profile_id IS NULL OR category_id=7)));
UPDATE protos.catalog_format SET schema_version=29,canonical_domain='auriga.item_catalog.v29' WHERE singleton=1;
