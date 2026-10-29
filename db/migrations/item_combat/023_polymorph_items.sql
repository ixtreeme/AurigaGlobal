-- Unreleased v17. Form identities are append-only in the administrative writer.
CREATE TABLE protos.polymorph_form (
    form_id INT UNSIGNED NOT NULL PRIMARY KEY, code VARCHAR(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL UNIQUE,
    mob_template_id INT UNSIGNED NOT NULL, book_bonus TINYINT UNSIGNED NOT NULL,
    CHECK(form_id>0), CHECK(code<>''), CHECK(mob_template_id>=8), CHECK(book_bonus IN(1,2,3,4))
) ENGINE=InnoDB;
CREATE TABLE protos.polymorph_profile (
    profile_id INT UNSIGNED NOT NULL PRIMARY KEY, code VARCHAR(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL UNIQUE,
    polymorph_kind TINYINT UNSIGNED NOT NULL, mob_level_grace SMALLINT UNSIGNED NOT NULL,
    grace_loss_numerator SMALLINT UNSIGNED NOT NULL, grace_loss_denominator SMALLINT UNSIGNED NOT NULL,
    max_book_grade SMALLINT UNSIGNED NOT NULL, practice_per_grade INT UNSIGNED NOT NULL,
    book_chance_base_percent SMALLINT NOT NULL,
    CHECK(profile_id>0), CHECK(code<>''), CHECK(polymorph_kind IN(1,2)), CHECK(grace_loss_denominator>0),
    CHECK(max_book_grade<=255), CHECK(CAST(max_book_grade AS UNSIGNED)*practice_per_grade<=4294967295),
    CHECK(book_chance_base_percent BETWEEN -10000 AND 10000),
    CHECK((polymorph_kind=1 AND max_book_grade=0 AND practice_per_grade=0 AND book_chance_base_percent=0)
       OR (polymorph_kind=2 AND mob_level_grace=0 AND grace_loss_numerator=0 AND grace_loss_denominator=1))
) ENGINE=InnoDB;
CREATE TABLE protos.polymorph_skill_rule (
    profile_id INT UNSIGNED NOT NULL, skill_level TINYINT UNSIGNED NOT NULL,
    duration_ms BIGINT UNSIGNED NOT NULL, attack_bonus_bp INT UNSIGNED NOT NULL, power_bp SMALLINT UNSIGNED NOT NULL,
    PRIMARY KEY(profile_id,skill_level), FOREIGN KEY(profile_id) REFERENCES protos.polymorph_profile(profile_id),
    CHECK(skill_level<=40), CHECK(duration_ms<=9223372036854775807), CHECK(attack_bonus_bp<=1000000), CHECK(power_bp<=10000)
) ENGINE=InnoDB;
CREATE TABLE protos.polymorph_mastery_rule (
    profile_id INT UNSIGNED NOT NULL, mastery TINYINT UNSIGNED NOT NULL,
    duration_ms BIGINT UNSIGNED NOT NULL, bonus_duration_ms BIGINT UNSIGNED NOT NULL,
    PRIMARY KEY(profile_id,mastery), FOREIGN KEY(profile_id) REFERENCES protos.polymorph_profile(profile_id),
    CHECK(mastery<=3), CHECK(duration_ms BETWEEN 1 AND 9223372036854775807), CHECK(bonus_duration_ms<=duration_ms)
) ENGINE=InnoDB;
CREATE TABLE protos.item_polymorph (
    category_id SMALLINT UNSIGNED NOT NULL DEFAULT 13, local_id INT UNSIGNED NOT NULL,
    profile_id INT UNSIGNED NOT NULL, fixed_form_id INT UNSIGNED NULL,
    PRIMARY KEY(category_id,local_id), FOREIGN KEY(category_id,local_id) REFERENCES protos.item_base(category_id,local_id),
    FOREIGN KEY(profile_id) REFERENCES protos.polymorph_profile(profile_id),
    FOREIGN KEY(fixed_form_id) REFERENCES protos.polymorph_form(form_id), CHECK(category_id=13), CHECK(local_id>0)
) ENGINE=InnoDB;
ALTER TABLE protos.item_base DROP CONSTRAINT ck_item_base_category, DROP CONSTRAINT ck_item_base_stack,
    DROP CONSTRAINT ck_item_base_non_equipment,
    ADD CONSTRAINT ck_item_base_category CHECK(category_id IN(1,2,3,4,5,6,7,8,9,10,11,12,13,16,18)),
    ADD CONSTRAINT ck_item_base_stack CHECK(category_id IN(5,6,7,8,9,10,12,13,16,18) OR max_stack=1),
    ADD CONSTRAINT ck_item_base_non_equipment CHECK(category_id NOT IN(5,6,7,8,9,10,12,13,16,18)
        OR (binding_policy<>3 AND (appearance_profile_id IS NULL OR category_id IN(7,10))
            AND (base_bonus_profile_id IS NULL OR category_id=7)));
UPDATE protos.catalog_format SET schema_version=17,canonical_domain='auriga.item_catalog.v17' WHERE singleton=1;
