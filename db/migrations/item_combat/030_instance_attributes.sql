-- Fresh unreleased v24 chain only; generated bonus registry/006 seed foundation
-- also changed. Never execute as an in-place upgrade of the installed v7.
CREATE TABLE protos.attribute_profile (
 profile_id INT UNSIGNED NOT NULL PRIMARY KEY,code VARCHAR(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL UNIQUE,
 allow_lock BOOLEAN NOT NULL,CHECK(profile_id>0 AND LENGTH(code)>0 AND allow_lock IN(0,1))
) ENGINE=InnoDB;
CREATE TABLE protos.attribute_group (
 profile_id INT UNSIGNED NOT NULL,pool_kind TINYINT UNSIGNED NOT NULL,capacity TINYINT UNSIGNED NOT NULL,exclude_static BOOLEAN NOT NULL,
 PRIMARY KEY(profile_id,pool_kind),FOREIGN KEY(profile_id) REFERENCES protos.attribute_profile(profile_id),
 CHECK(pool_kind BETWEEN 1 AND 3),CHECK(capacity BETWEEN 1 AND CASE pool_kind WHEN 1 THEN 5 WHEN 2 THEN 2 ELSE 1 END),CHECK(exclude_static IN(0,1))
) ENGINE=InnoDB;
CREATE TABLE protos.attribute_candidate (
 profile_id INT UNSIGNED NOT NULL,pool_kind TINYINT UNSIGNED NOT NULL,bonus_id INT UNSIGNED NOT NULL,weight INT UNSIGNED NOT NULL,
 PRIMARY KEY(profile_id,pool_kind,bonus_id),FOREIGN KEY(profile_id,pool_kind) REFERENCES protos.attribute_group(profile_id,pool_kind),
 FOREIGN KEY(bonus_id) REFERENCES protos.bonus_definition(bonus_id),CHECK(weight BETWEEN 1 AND 1000000)
) ENGINE=InnoDB;
CREATE TABLE protos.attribute_value (
 profile_id INT UNSIGNED NOT NULL,pool_kind TINYINT UNSIGNED NOT NULL,bonus_id INT UNSIGNED NOT NULL,tier TINYINT UNSIGNED NOT NULL,value_scaled BIGINT NOT NULL,
 PRIMARY KEY(profile_id,pool_kind,bonus_id,tier),FOREIGN KEY(profile_id,pool_kind,bonus_id) REFERENCES protos.attribute_candidate(profile_id,pool_kind,bonus_id),
 CHECK(tier BETWEEN 1 AND 16),CHECK(value_scaled BETWEEN -1000000000000000 AND 1000000000000000 AND value_scaled<>0)
) ENGINE=InnoDB;
CREATE TABLE protos.item_attribute_binding (
 category_id SMALLINT UNSIGNED NOT NULL,local_id INT UNSIGNED NOT NULL,profile_id INT UNSIGNED NOT NULL,
 PRIMARY KEY(category_id,local_id),FOREIGN KEY(category_id,local_id) REFERENCES protos.item_base(category_id,local_id),
 FOREIGN KEY(profile_id) REFERENCES protos.attribute_profile(profile_id),CHECK(category_id BETWEEN 1 AND 4)
) ENGINE=InnoDB;
CREATE TABLE protos.attribute_service_profile (
 profile_id INT UNSIGNED NOT NULL PRIMARY KEY,code VARCHAR(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL UNIQUE,
 operation_kind TINYINT UNSIGNED NOT NULL,pool_kind TINYINT UNSIGNED NOT NULL,units_per_use SMALLINT UNSIGNED NOT NULL,success_bp SMALLINT UNSIGNED NOT NULL,
 target_mask TINYINT UNSIGNED NOT NULL,minimum_count TINYINT UNSIGNED NOT NULL,maximum_count TINYINT UNSIGNED NOT NULL,add_count TINYINT UNSIGNED NOT NULL,
 maximum_target_level SMALLINT UNSIGNED NOT NULL,consume_on_failure BOOLEAN NOT NULL,
 CHECK(profile_id>0 AND LENGTH(code)>0),CHECK(operation_kind BETWEEN 1 AND 5 AND pool_kind BETWEEN 1 AND 3),
 CHECK(units_per_use>0 AND success_bp BETWEEN 1 AND 10000 AND target_mask BETWEEN 1 AND 15),
 CHECK(minimum_count<=maximum_count AND maximum_count<=CASE pool_kind WHEN 1 THEN 5 WHEN 2 THEN 2 ELSE 1 END),
 CHECK((operation_kind=1 AND add_count BETWEEN 1 AND CASE pool_kind WHEN 1 THEN 5 WHEN 2 THEN 2 ELSE 1 END) OR (operation_kind<>1 AND add_count=0)),
 CHECK(consume_on_failure IN(0,1)),CHECK(operation_kind=1 OR (success_bp=10000 AND consume_on_failure=0)),
 CHECK(operation_kind<=2 OR (pool_kind=1 AND (target_mask & 8)=0))
) ENGINE=InnoDB;
CREATE TABLE protos.attribute_tier_chance (
 profile_id INT UNSIGNED NOT NULL,tier TINYINT UNSIGNED NOT NULL,chance_bp SMALLINT UNSIGNED NOT NULL,
 PRIMARY KEY(profile_id,tier),FOREIGN KEY(profile_id) REFERENCES protos.attribute_service_profile(profile_id),
 CHECK(tier BETWEEN 1 AND 16 AND chance_bp BETWEEN 1 AND 10000)
) ENGINE=InnoDB;
ALTER TABLE protos.item_consumable DROP CONSTRAINT ck_consumable_one_effect,
 ADD attribute_service_profile_id INT UNSIGNED NULL,
 ADD FOREIGN KEY(attribute_service_profile_id) REFERENCES protos.attribute_service_profile(profile_id),
 ADD CONSTRAINT ck_consumable_one_effect CHECK((recovery_profile_id IS NOT NULL)+(timed_effect_profile_id IS NOT NULL)+
  (toggle_effect_profile_id IS NOT NULL)+(socket_service_profile_id IS NOT NULL)+(attribute_service_profile_id IS NOT NULL)=1);
UPDATE protos.catalog_format SET schema_version=24,canonical_domain='auriga.item_catalog.v24' WHERE singleton=1;
