-- Fresh, unreleased v22 chain. Never run this against the active v7 game DB.
CREATE TABLE protos.pet_training_profile (
 profile_id INT UNSIGNED NOT NULL PRIMARY KEY,code VARCHAR(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL UNIQUE,
 hp_evolution_period SMALLINT UNSIGNED NOT NULL,monster_evolution_period SMALLINT UNSIGNED NOT NULL,pve_evolution_period SMALLINT UNSIGNED NOT NULL,
 evolution_min SMALLINT UNSIGNED NOT NULL,evolution_max SMALLINT UNSIGNED NOT NULL,
 CHECK(profile_id>0 AND LENGTH(code)>0),CHECK(hp_evolution_period<=255 AND monster_evolution_period<=255 AND pve_evolution_period<=255),
 CHECK(evolution_min BETWEEN 1 AND 32767 AND evolution_max BETWEEN evolution_min AND 32767)
) ENGINE=InnoDB;
CREATE TABLE protos.pet_age_tier (
 profile_id INT UNSIGNED NOT NULL,minimum_age_ms BIGINT UNSIGNED NOT NULL,max_hp_flat INT UNSIGNED NOT NULL,
 monster_resistance_bp SMALLINT UNSIGNED NOT NULL,human_resistance_bp SMALLINT UNSIGNED NOT NULL,
 PRIMARY KEY(profile_id,minimum_age_ms),FOREIGN KEY(profile_id) REFERENCES protos.pet_training_profile(profile_id),
 CHECK(minimum_age_ms<=9223372036854775807),CHECK(monster_resistance_bp<=10000 AND human_resistance_bp<=10000)
) ENGINE=InnoDB;
CREATE TABLE protos.pet_slot_rule (
 profile_id INT UNSIGNED NOT NULL,slot TINYINT UNSIGNED NOT NULL,minimum_stage TINYINT UNSIGNED NOT NULL,minimum_age_ms BIGINT UNSIGNED NOT NULL,
 PRIMARY KEY(profile_id,slot),FOREIGN KEY(profile_id) REFERENCES protos.pet_training_profile(profile_id),
 CHECK(slot<=3 AND minimum_stage<=15),CHECK(minimum_age_ms<=9223372036854775807)
) ENGINE=InnoDB;
CREATE TABLE protos.pet_skill_definition (
 profile_id INT UNSIGNED NOT NULL,skill_id INT UNSIGNED NOT NULL,code VARCHAR(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 effect_kind TINYINT UNSIGNED NOT NULL,PRIMARY KEY(profile_id,skill_id),UNIQUE(profile_id,code),
 FOREIGN KEY(profile_id) REFERENCES protos.pet_training_profile(profile_id),CHECK(skill_id>0 AND LENGTH(code)>0),CHECK(effect_kind BETWEEN 1 AND 18)
) ENGINE=InnoDB;
CREATE TABLE protos.pet_skill_rank (
 profile_id INT UNSIGNED NOT NULL,skill_id INT UNSIGNED NOT NULL,skill_rank SMALLINT UNSIGNED NOT NULL,value_scaled BIGINT NOT NULL,
 PRIMARY KEY(profile_id,skill_id,skill_rank),FOREIGN KEY(profile_id,skill_id) REFERENCES protos.pet_skill_definition(profile_id,skill_id),
 CHECK(skill_rank BETWEEN 1 AND 255),CHECK(value_scaled BETWEEN 0 AND 1000000000000000)
) ENGINE=InnoDB;
CREATE TABLE protos.pet_supply_profile (
 profile_id INT UNSIGNED NOT NULL PRIMARY KEY,code VARCHAR(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL UNIQUE,
 supply_kind TINYINT UNSIGNED NOT NULL,UNIQUE(profile_id,supply_kind),CHECK(profile_id>0 AND LENGTH(code)>0),CHECK(supply_kind BETWEEN 1 AND 8)
) ENGINE=InnoDB;
CREATE TABLE protos.pet_study_rule (
 profile_id INT UNSIGNED NOT NULL PRIMARY KEY,supply_kind TINYINT UNSIGNED NOT NULL DEFAULT 1,skill_id INT UNSIGNED NOT NULL,
 selected_allowed TINYINT UNSIGNED NOT NULL,known_allowed TINYINT UNSIGNED NOT NULL,known_success_bp SMALLINT UNSIGNED NOT NULL,known_cooldown_ms BIGINT UNSIGNED NOT NULL,
 FOREIGN KEY(profile_id,supply_kind) REFERENCES protos.pet_supply_profile(profile_id,supply_kind),
 CHECK(supply_kind=1 AND skill_id>0),CHECK(selected_allowed IN(0,1) AND known_allowed IN(0,1) AND selected_allowed+known_allowed>0),
 CHECK(known_success_bp<=10000 AND known_cooldown_ms<=9223372036854775807),CHECK(known_allowed=1 OR (known_success_bp=0 AND known_cooldown_ms=0))
) ENGINE=InnoDB;
CREATE TABLE protos.pet_extension_rule (
 profile_id INT UNSIGNED NOT NULL PRIMARY KEY,supply_kind TINYINT UNSIGNED NOT NULL DEFAULT 4,added_ms BIGINT UNSIGNED NOT NULL,maximum_capacity_ms BIGINT UNSIGNED NOT NULL,
 FOREIGN KEY(profile_id,supply_kind) REFERENCES protos.pet_supply_profile(profile_id,supply_kind),
 CHECK(supply_kind=4),CHECK(added_ms>0 AND added_ms<=maximum_capacity_ms AND maximum_capacity_ms<=9223372036854775807)
) ENGINE=InnoDB;
CREATE TABLE protos.pet_enchant_rule (
 profile_id INT UNSIGNED NOT NULL PRIMARY KEY,supply_kind TINYINT UNSIGNED NOT NULL DEFAULT 6,success_bp SMALLINT UNSIGNED NOT NULL,step SMALLINT UNSIGNED NOT NULL,
 hp_limit SMALLINT UNSIGNED NOT NULL,monster_limit SMALLINT UNSIGNED NOT NULL,pve_limit SMALLINT UNSIGNED NOT NULL,
 FOREIGN KEY(profile_id,supply_kind) REFERENCES protos.pet_supply_profile(profile_id,supply_kind),
 CHECK(supply_kind=6 AND success_bp<=10000 AND step BETWEEN 1 AND 32767),
 CHECK(hp_limit BETWEEN 1 AND 32767 AND monster_limit BETWEEN 1 AND 32767 AND pve_limit BETWEEN 1 AND 32767)
) ENGINE=InnoDB;
CREATE TABLE protos.pet_skin_rule (
 profile_id INT UNSIGNED NOT NULL PRIMARY KEY,supply_kind TINYINT UNSIGNED NOT NULL DEFAULT 8,form_id INT UNSIGNED NOT NULL,clock_kind TINYINT UNSIGNED NOT NULL,duration_ms BIGINT UNSIGNED NOT NULL,
 FOREIGN KEY(profile_id,supply_kind) REFERENCES protos.pet_supply_profile(profile_id,supply_kind),FOREIGN KEY(form_id) REFERENCES protos.companion_form(form_id),
 CHECK(supply_kind=8),CHECK(clock_kind BETWEEN 1 AND 4 AND duration_ms<=9223372036854775807),CHECK((clock_kind=1)=(duration_ms=0))
) ENGINE=InnoDB;
CREATE TABLE protos.item_special (
 category_id SMALLINT UNSIGNED NOT NULL,local_id INT UNSIGNED NOT NULL,behavior_kind TINYINT UNSIGNED NOT NULL,profile_id INT UNSIGNED NOT NULL,
 PRIMARY KEY(category_id,local_id),FOREIGN KEY(category_id,local_id) REFERENCES protos.item_base(category_id,local_id),
 FOREIGN KEY(profile_id) REFERENCES protos.pet_supply_profile(profile_id),CHECK(category_id=17 AND behavior_kind=1)
) ENGINE=InnoDB;
ALTER TABLE protos.item_companion ADD COLUMN training_profile_id INT UNSIGNED NULL,
 ADD FOREIGN KEY(training_profile_id) REFERENCES protos.pet_training_profile(profile_id),
 ADD CHECK(training_profile_id IS NULL OR companion_kind=3);
ALTER TABLE protos.item_base DROP CONSTRAINT ck_item_base_category,DROP CONSTRAINT ck_item_base_stack,DROP CONSTRAINT ck_item_base_non_equipment,
 ADD CONSTRAINT ck_item_base_category CHECK(category_id BETWEEN 1 AND 18),
 ADD CONSTRAINT ck_item_base_stack CHECK(category_id IN(5,6,7,8,9,10,12,13,14,16,17,18) OR max_stack=1),
 ADD CONSTRAINT ck_item_base_non_equipment CHECK(category_id NOT IN(5,6,7,8,9,10,12,13,14,16,17,18)
  OR (binding_policy<>3 AND (appearance_profile_id IS NULL OR category_id IN(7,10)) AND (base_bonus_profile_id IS NULL OR category_id=7)));
UPDATE protos.catalog_format SET schema_version=22,canonical_domain='auriga.item_catalog.v22' WHERE singleton=1;
