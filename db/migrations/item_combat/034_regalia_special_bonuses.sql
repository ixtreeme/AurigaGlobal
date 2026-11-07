-- New typed identities; no legacy vnum, socket or APPLY dispatch.
INSERT IGNORE INTO protos.item_category(category_id,code,prototype_table) VALUES
 (19,'regalia','item_regalia'),(20,'bonus_tool','item_bonus_tool');
INSERT IGNORE INTO protos.item_local_id_sequence(category_id,next_local_id) VALUES(19,1),(20,1);
CREATE TABLE IF NOT EXISTS protos.item_regalia (
 category_id SMALLINT UNSIGNED NOT NULL DEFAULT 19,local_id INT UNSIGNED NOT NULL,
 regalia_kind TINYINT UNSIGNED NOT NULL,rune_slot TINYINT UNSIGNED NOT NULL,
 stole_grade TINYINT UNSIGNED NOT NULL,capacity_ms BIGINT UNSIGNED NOT NULL,initial_ms BIGINT UNSIGNED NOT NULL,
 PRIMARY KEY(category_id,local_id),CHECK(category_id=19),
 CHECK((regalia_kind=1 AND rune_slot=0 AND stole_grade BETWEEN 1 AND 4 AND capacity_ms=0 AND initial_ms=0)
 OR (regalia_kind=2 AND rune_slot BETWEEN 1 AND 7 AND stole_grade=0 AND
 ((rune_slot=7 AND capacity_ms=0 AND initial_ms=0) OR (rune_slot<7 AND capacity_ms BETWEEN 100 AND 9223372036854775807 AND initial_ms<=capacity_ms)))),
 FOREIGN KEY(category_id,local_id) REFERENCES protos.item_base(category_id,local_id)
) ENGINE=InnoDB;
CREATE TABLE IF NOT EXISTS protos.regalia_bonus_rule (
 category_id SMALLINT UNSIGNED NOT NULL DEFAULT 19,local_id INT UNSIGNED NOT NULL,
 bonus_index TINYINT UNSIGNED NOT NULL,variant_no TINYINT UNSIGNED NOT NULL,minimum_charge_bp SMALLINT UNSIGNED NOT NULL,
 bonus_id INT UNSIGNED NOT NULL,value_scaled BIGINT NOT NULL,
 PRIMARY KEY(category_id,local_id,bonus_index,minimum_charge_bp,variant_no),
 CHECK(bonus_index<6 AND variant_no<4 AND minimum_charge_bp<=10000 AND value_scaled<>0),
 FOREIGN KEY(category_id,local_id) REFERENCES protos.item_regalia(category_id,local_id),
 FOREIGN KEY(bonus_id) REFERENCES protos.bonus_definition(bonus_id)
) ENGINE=InnoDB;
CREATE TABLE IF NOT EXISTS protos.item_bonus_tool (
 category_id SMALLINT UNSIGNED NOT NULL DEFAULT 20,local_id INT UNSIGNED NOT NULL,
 operation_kind TINYINT UNSIGNED NOT NULL,units SMALLINT UNSIGNED NOT NULL,parameter SMALLINT UNSIGNED NOT NULL,
 bonus_id INT UNSIGNED NULL,value_scaled BIGINT NULL,
 PRIMARY KEY(category_id,local_id),CHECK(category_id=20),CHECK((bonus_id IS NULL)=(value_scaled IS NULL)),
 CHECK(operation_kind BETWEEN 1 AND 6 AND units>0 AND ((operation_kind=2 AND parameter BETWEEN 1 AND 10000 AND units=1) OR (operation_kind<>2 AND parameter=0))),
 CHECK((operation_kind=4 AND bonus_id IS NOT NULL AND value_scaled<>0) OR (operation_kind<>4 AND bonus_id IS NULL)),
 FOREIGN KEY(category_id,local_id) REFERENCES protos.item_base(category_id,local_id),
 FOREIGN KEY(bonus_id) REFERENCES protos.bonus_definition(bonus_id)
) ENGINE=InnoDB;
CREATE TABLE IF NOT EXISTS protos.special_bonus_tier (
 category_id SMALLINT UNSIGNED NOT NULL DEFAULT 20,local_id INT UNSIGNED NOT NULL,
 draw_no TINYINT UNSIGNED NOT NULL,tier TINYINT UNSIGNED NOT NULL,chance_bp SMALLINT UNSIGNED NOT NULL,
 PRIMARY KEY(category_id,local_id,draw_no,tier),CHECK(draw_no<3 AND tier BETWEEN 1 AND 16 AND chance_bp BETWEEN 1 AND 10000),
 FOREIGN KEY(category_id,local_id) REFERENCES protos.item_bonus_tool(category_id,local_id)
) ENGINE=InnoDB;
ALTER TABLE protos.item_base DROP CONSTRAINT IF EXISTS ck_item_base_category,
 DROP CONSTRAINT IF EXISTS ck_item_base_stack,DROP CONSTRAINT IF EXISTS ck_item_base_non_equipment;
ALTER TABLE protos.item_base
 ADD CONSTRAINT ck_item_base_category CHECK(category_id BETWEEN 1 AND 20),
 ADD CONSTRAINT ck_item_base_stack CHECK(category_id IN(5,6,7,8,9,10,12,13,14,16,17,18,20) OR max_stack=1),
 ADD CONSTRAINT ck_item_base_non_equipment CHECK(category_id NOT IN(5,6,7,8,9,10,12,13,14,16,17,18,20)
 OR (binding_policy<>3 AND (appearance_profile_id IS NULL OR category_id IN(7,10)) AND (base_bonus_profile_id IS NULL OR category_id=7)));
UPDATE protos.catalog_format SET schema_version=28,canonical_domain='auriga.item_catalog.v28' WHERE singleton=1;
