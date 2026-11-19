-- Native full-catalog rules. No source prototype identity or legacy APPLY data.
SET @full_attr_check=(SELECT CONSTRAINT_NAME FROM information_schema.CHECK_CONSTRAINTS WHERE CONSTRAINT_SCHEMA='protos' AND TABLE_NAME='attribute_service_profile' AND CHECK_CLAUSE LIKE '%target_mask%' LIMIT 1);
SET @full_attr_ddl=CONCAT('ALTER TABLE protos.attribute_service_profile DROP CONSTRAINT `',REPLACE(@full_attr_check,'`','``'),'`');
PREPARE full_attr_upgrade FROM @full_attr_ddl; EXECUTE full_attr_upgrade; DEALLOCATE PREPARE full_attr_upgrade;
ALTER TABLE protos.attribute_service_profile ADD CONSTRAINT ck_full_attribute_target CHECK(units_per_use>0 AND success_bp BETWEEN 1 AND 10000 AND target_mask BETWEEN 1 AND 31);
ALTER TABLE protos.refinement_profile DROP CONSTRAINT ck_refinement_profile_kind,
 ADD CONSTRAINT ck_refinement_profile_kind CHECK(profile_kind BETWEEN 1 AND 10);
CREATE TABLE protos.item_definition_rules (
 category_id SMALLINT UNSIGNED NOT NULL, local_id INT UNSIGNED NOT NULL,
 refinement_level SMALLINT UNSIGNED NOT NULL DEFAULT 0 CHECK(refinement_level<=255),
 display_refinement_offset SMALLINT UNSIGNED NOT NULL DEFAULT 0 CHECK(display_refinement_offset<=255),
 rarity TINYINT UNSIGNED NOT NULL DEFAULT 0,
 clock_kind TINYINT UNSIGNED NOT NULL DEFAULT 1 CHECK(clock_kind BETWEEN 1 AND 4),
 duration_ms BIGINT UNSIGNED NOT NULL DEFAULT 0,
 allow_account_storage BOOLEAN NOT NULL DEFAULT 1 CHECK(allow_account_storage IN(0,1)),
 allow_acquire BOOLEAN NOT NULL DEFAULT 1 CHECK(allow_acquire IN(0,1)),
 allow_unequip BOOLEAN NOT NULL DEFAULT 1 CHECK(allow_unequip IN(0,1)),
 allowed_empires TINYINT UNSIGNED NOT NULL DEFAULT 7 CHECK(allowed_empires BETWEEN 1 AND 7),
 minimum_grant_count INT UNSIGNED NOT NULL DEFAULT 1 CHECK(minimum_grant_count>0),
 birth_magic_chance_bp SMALLINT UNSIGNED NOT NULL DEFAULT 0 CHECK(birth_magic_chance_bp<=10000),
 birth_socket_chance_bp SMALLINT UNSIGNED NOT NULL DEFAULT 0 CHECK(birth_socket_chance_bp<=10000),
 birth_addon TINYINT UNSIGNED NOT NULL DEFAULT 0 CHECK(birth_addon IN(0,1)),
 unique_group VARCHAR(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL DEFAULT '',
 available BOOLEAN NOT NULL DEFAULT 1 CHECK(available IN(0,1)),
 PRIMARY KEY(category_id,local_id,refinement_level),
 FOREIGN KEY(category_id,local_id) REFERENCES protos.item_base(category_id,local_id),
 CHECK((clock_kind=1 AND duration_ms=0) OR (clock_kind<>1 AND duration_ms>0)),
 CHECK(duration_ms<=9223372036854775807)
) ENGINE=InnoDB;
UPDATE protos.catalog_format SET schema_version=32,canonical_domain='auriga.item_catalog.v32' WHERE singleton=1;
