INSERT IGNORE INTO protos.item_category(category_id,code,prototype_table) VALUES(22,'forge_aid','item_forge_aid');
INSERT IGNORE INTO protos.item_local_id_sequence(category_id,next_local_id) VALUES(22,1);
CREATE TABLE IF NOT EXISTS protos.item_forge_aid (
 category_id SMALLINT UNSIGNED NOT NULL DEFAULT 22,local_id INT UNSIGNED NOT NULL,
 aid_kind TINYINT UNSIGNED NOT NULL,units SMALLINT UNSIGNED NOT NULL,target_mask TINYINT UNSIGNED NOT NULL,
 minimum_level SMALLINT UNSIGNED NOT NULL,maximum_level SMALLINT UNSIGNED NOT NULL,
 chance_mode TINYINT UNSIGNED NOT NULL,chance_bp SMALLINT UNSIGNED NOT NULL,failure_kind TINYINT UNSIGNED NOT NULL,booster_group TINYINT UNSIGNED NOT NULL,
 PRIMARY KEY(category_id,local_id),CHECK(category_id=22 AND units>0 AND target_mask BETWEEN 1 AND 15 AND minimum_level<=maximum_level AND maximum_level<=255 AND chance_bp<=10000 AND failure_kind<=3),
 CHECK((aid_kind=1 AND booster_group=0 AND chance_mode IN(1,2,4) AND (chance_mode=2 OR chance_bp=0)) OR (aid_kind=2 AND booster_group BETWEEN 1 AND 3 AND chance_mode=3 AND chance_bp>0 AND failure_kind=0)),
 FOREIGN KEY(category_id,local_id) REFERENCES protos.item_base(category_id,local_id)
) ENGINE=InnoDB;
CREATE TABLE IF NOT EXISTS protos.forge_aid_level (
 category_id SMALLINT UNSIGNED NOT NULL DEFAULT 22,local_id INT UNSIGNED NOT NULL,source_level SMALLINT UNSIGNED NOT NULL,chance_bp SMALLINT UNSIGNED NOT NULL,
 PRIMARY KEY(category_id,local_id,source_level),CHECK(source_level<=255 AND chance_bp<=10000),
 FOREIGN KEY(category_id,local_id) REFERENCES protos.item_forge_aid(category_id,local_id)
) ENGINE=InnoDB;
ALTER TABLE protos.item_base DROP CONSTRAINT IF EXISTS ck_item_base_category,DROP CONSTRAINT IF EXISTS ck_item_base_stack,DROP CONSTRAINT IF EXISTS ck_item_base_non_equipment;
ALTER TABLE protos.item_base
 ADD CONSTRAINT ck_item_base_category CHECK(category_id BETWEEN 1 AND 22),
 ADD CONSTRAINT ck_item_base_stack CHECK(category_id IN(5,6,7,8,9,10,12,13,14,16,17,18,20,21,22) OR max_stack=1),
 ADD CONSTRAINT ck_item_base_non_equipment CHECK(category_id NOT IN(5,6,7,8,9,10,12,13,14,16,17,18,20,21,22) OR (binding_policy<>3 AND (appearance_profile_id IS NULL OR category_id IN(7,10)) AND (base_bonus_profile_id IS NULL OR category_id=7)));
UPDATE protos.catalog_format SET schema_version=30,canonical_domain='auriga.item_catalog.v30' WHERE singleton=1;
