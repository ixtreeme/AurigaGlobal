-- Unreleased v11 fresh-schema draft. No existing user catalog is upgraded here.
CREATE TABLE protos.skill_book_profile (
    profile_id INT UNSIGNED NOT NULL PRIMARY KEY,
    code VARCHAR(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL UNIQUE,
    operation_kind TINYINT UNSIGNED NOT NULL, target_mode TINYINT UNSIGNED NOT NULL,
    units_per_use SMALLINT UNSIGNED NOT NULL, allowed_masteries TINYINT UNSIGNED NOT NULL,
    allow_delay_bypass BOOLEAN NOT NULL, bonus_policy TINYINT UNSIGNED NOT NULL,
    CHECK(profile_id>0), CHECK(CHAR_LENGTH(code)>0), CHECK(operation_kind IN(1,2,3)),
    CHECK(target_mode IN(1,2,3)), CHECK(units_per_use>0), CHECK(allowed_masteries BETWEEN 0 AND 7),
    CHECK(allow_delay_bypass IN(0,1)), CHECK(bonus_policy IN(1,2)),
    CHECK((operation_kind=3 AND target_mode=3 AND allowed_masteries=0 AND bonus_policy=1) OR
          (operation_kind=2 AND target_mode IN(1,2) AND allowed_masteries=1 AND allow_delay_bypass=0 AND bonus_policy=1) OR
          (operation_kind=1 AND target_mode IN(1,2) AND allowed_masteries>0))
) ENGINE=InnoDB;
CREATE TABLE protos.skill_book_target (
    profile_id INT UNSIGNED NOT NULL, skill_id SMALLINT UNSIGNED NOT NULL,
    refund_pool TINYINT UNSIGNED NOT NULL, forbidden_empire TINYINT UNSIGNED NOT NULL,
    PRIMARY KEY(profile_id,skill_id),
    FOREIGN KEY(profile_id) REFERENCES protos.skill_book_profile(profile_id),
    CHECK(skill_id BETWEEN 1 AND 254), CHECK(refund_pool BETWEEN 0 AND 3), CHECK(forbidden_empire BETWEEN 0 AND 3)
) ENGINE=InnoDB;
CREATE TABLE protos.skill_book_step (
    profile_id INT UNSIGNED NOT NULL, current_level TINYINT UNSIGNED NOT NULL,
    next_level TINYINT UNSIGNED NOT NULL, min_character_level SMALLINT UNSIGNED NOT NULL,
    required_successes SMALLINT UNSIGNED NOT NULL,
    success_chance_bp SMALLINT UNSIGNED NOT NULL, boosted_chance_bp SMALLINT UNSIGNED NOT NULL,
    experience_cost BIGINT UNSIGNED NOT NULL,
    delay_minimum_ms BIGINT UNSIGNED NOT NULL, delay_maximum_ms BIGINT UNSIGNED NOT NULL,
    delay_on_failure BOOLEAN NOT NULL,
    PRIMARY KEY(profile_id,current_level),
    FOREIGN KEY(profile_id) REFERENCES protos.skill_book_profile(profile_id),
    CHECK(current_level BETWEEN 0 AND 39), CHECK(next_level BETWEEN 0 AND 40), CHECK(required_successes>0),
    CHECK(success_chance_bp BETWEEN 0 AND 10000), CHECK(boosted_chance_bp BETWEEN success_chance_bp AND 10000),
    CHECK(experience_cost<=9223372036854775807), CHECK(delay_minimum_ms<=delay_maximum_ms),
    CHECK(delay_maximum_ms<=9223372036854775807), CHECK(delay_minimum_ms>0 OR delay_maximum_ms=0),
    CHECK(delay_on_failure IN(0,1))
) ENGINE=InnoDB;
CREATE TABLE protos.item_skill_book (
    category_id SMALLINT UNSIGNED NOT NULL DEFAULT 9, local_id INT UNSIGNED NOT NULL,
    profile_id INT UNSIGNED NOT NULL,
    PRIMARY KEY(category_id,local_id),
    FOREIGN KEY(category_id,local_id) REFERENCES protos.item_base(category_id,local_id),
    FOREIGN KEY(profile_id) REFERENCES protos.skill_book_profile(profile_id),
    CHECK(category_id=9), CHECK(local_id>0)
) ENGINE=InnoDB;
ALTER TABLE protos.item_base
    DROP CONSTRAINT ck_item_base_category, DROP CONSTRAINT ck_item_base_stack, DROP CONSTRAINT ck_item_base_non_equipment,
    ADD CONSTRAINT ck_item_base_category CHECK (category_id IN (1,2,3,5,6,8,9,16,18)),
    ADD CONSTRAINT ck_item_base_stack CHECK (category_id IN (5,6,8,9,16,18) OR max_stack=1),
    ADD CONSTRAINT ck_item_base_non_equipment CHECK (category_id NOT IN (5,6,8,9,16,18)
        OR (binding_policy<>3 AND appearance_profile_id IS NULL AND base_bonus_profile_id IS NULL));
UPDATE protos.catalog_format SET schema_version=11, canonical_domain='auriga.item_catalog.v11' WHERE singleton=1;
-- Cross-row operation/target/step semantics are enforced by catalog publication.
