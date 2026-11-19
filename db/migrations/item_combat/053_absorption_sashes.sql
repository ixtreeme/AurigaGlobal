-- Separate absorption equipment. Existing six-bonus stoles and rune clocks
-- retain their original rows and instance codecs.
ALTER TABLE protos.item_regalia DROP CONSTRAINT CONSTRAINT_2,
 ADD CONSTRAINT ck_regalia_kind CHECK
 ((regalia_kind IN(1,3) AND rune_slot=0 AND stole_grade BETWEEN 1 AND 4 AND capacity_ms=0 AND initial_ms=0)
 OR (regalia_kind=2 AND rune_slot BETWEEN 1 AND 7 AND stole_grade=0 AND
 ((rune_slot=7 AND capacity_ms=0 AND initial_ms=0) OR
 (rune_slot<7 AND capacity_ms BETWEEN 100 AND 9223372036854775807 AND initial_ms<=capacity_ms))));
ALTER TABLE protos.item_bonus_tool DROP CONSTRAINT CONSTRAINT_3,
 ADD CONSTRAINT ck_bonus_tool_operation CHECK(operation_kind BETWEEN 1 AND 7 AND units>0 AND
 ((operation_kind=2 AND parameter BETWEEN 1 AND 10000 AND units=1) OR (operation_kind<>2 AND parameter=0)));
CREATE TABLE protos.absorption_sash_profile (
 category_id SMALLINT UNSIGNED NOT NULL DEFAULT 19,local_id INT UNSIGNED NOT NULL,
 grade TINYINT UNSIGNED NOT NULL,
 next_category_id SMALLINT UNSIGNED NOT NULL DEFAULT 19,next_local_id INT UNSIGNED NOT NULL,
 initial_min TINYINT UNSIGNED NOT NULL,initial_max TINYINT UNSIGNED NOT NULL,
 combine_min TINYINT UNSIGNED NOT NULL,combine_max TINYINT UNSIGNED NOT NULL,
 max_percent TINYINT UNSIGNED NOT NULL,success_bp SMALLINT UNSIGNED NOT NULL,gold_cost BIGINT UNSIGNED NOT NULL,
 PRIMARY KEY(category_id,local_id),
 FOREIGN KEY(category_id,local_id) REFERENCES protos.item_regalia(category_id,local_id),
 FOREIGN KEY(next_category_id,next_local_id) REFERENCES protos.item_regalia(category_id,local_id),
 CHECK(category_id=19 AND next_category_id=19 AND max_percent=25 AND success_bp BETWEEN 1 AND 10000 AND gold_cost BETWEEN 1 AND 9223372036854775807),
 CHECK((grade=1 AND initial_min=1 AND initial_max=1 AND combine_min=5 AND combine_max=5) OR
 (grade=2 AND initial_min=5 AND initial_max=5 AND combine_min=10 AND combine_max=10) OR
 (grade=3 AND initial_min=10 AND initial_max=10 AND combine_min=12 AND combine_max=19) OR
 (grade=4 AND initial_min=11 AND initial_max=19 AND combine_min=1 AND combine_max=5 AND next_local_id=local_id))
) ENGINE=InnoDB;
CREATE TABLE protos.sash_donor_profile (
 category_id SMALLINT UNSIGNED NOT NULL,local_id INT UNSIGNED NOT NULL,
 refinement_level SMALLINT UNSIGNED NOT NULL,
 physical_max_raw BIGINT NOT NULL,magical_max_raw BIGINT NOT NULL,
 physical_base_positive BOOLEAN NOT NULL,magical_base_positive BOOLEAN NOT NULL,
 PRIMARY KEY(category_id,local_id,refinement_level),
 FOREIGN KEY(category_id,local_id) REFERENCES protos.item_base(category_id,local_id),
 CHECK(category_id IN(1,2) AND refinement_level<=255 AND
 physical_max_raw BETWEEN 0 AND 1000000000000000 AND magical_max_raw BETWEEN 0 AND 1000000000000000 AND
 physical_base_positive IN(0,1) AND magical_base_positive IN(0,1) AND
 (physical_base_positive=0 OR physical_max_raw>0) AND (magical_base_positive=0 OR magical_max_raw>0) AND
 (category_id=1 OR (physical_max_raw=0 AND magical_max_raw=0 AND physical_base_positive=0 AND magical_base_positive=0)))
) ENGINE=InnoDB;
CREATE TABLE protos.sash_donor_component (
 category_id SMALLINT UNSIGNED NOT NULL,local_id INT UNSIGNED NOT NULL,
 refinement_level SMALLINT UNSIGNED NOT NULL,component_no TINYINT UNSIGNED NOT NULL,
 bonus_id INT UNSIGNED NOT NULL,value_scaled BIGINT NOT NULL,
 PRIMARY KEY(category_id,local_id,refinement_level,component_no),
 FOREIGN KEY(category_id,local_id,refinement_level) REFERENCES protos.sash_donor_profile(category_id,local_id,refinement_level),
 FOREIGN KEY(bonus_id) REFERENCES protos.bonus_definition(bonus_id),
 CHECK(component_no<5 AND value_scaled>0)
) ENGINE=InnoDB;
UPDATE protos.catalog_format SET schema_version=45,canonical_domain='auriga.item_catalog.v45' WHERE singleton=1;
