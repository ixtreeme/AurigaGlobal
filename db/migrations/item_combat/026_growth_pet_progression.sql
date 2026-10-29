-- Unreleased v20 fresh-schema chain, not an in-place live updater.
CREATE TABLE protos.growth_pet_profile (
    profile_id INT UNSIGNED NOT NULL PRIMARY KEY,
    code VARCHAR(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL UNIQUE,
    maximum_level SMALLINT UNSIGNED NOT NULL,
    experience_policy TINYINT UNSIGNED NOT NULL,
    CHECK(profile_id>0), CHECK(LENGTH(code)>0), CHECK(maximum_level BETWEEN 1 AND 255),
    CHECK(experience_policy IN(1,2))
) ENGINE=InnoDB;
CREATE TABLE protos.growth_pet_level (
    profile_id INT UNSIGNED NOT NULL,
    level SMALLINT UNSIGNED NOT NULL,
    mob_experience BIGINT UNSIGNED NOT NULL,
    item_experience BIGINT UNSIGNED NOT NULL,
    PRIMARY KEY(profile_id,level),
    FOREIGN KEY(profile_id) REFERENCES protos.growth_pet_profile(profile_id),
    CHECK(level BETWEEN 1 AND 255),
    CHECK(mob_experience<=9223372036854775807), CHECK(item_experience<=9223372036854775807)
) ENGINE=InnoDB;
CREATE TABLE protos.growth_pet_stage (
    profile_id INT UNSIGNED NOT NULL,
    stage TINYINT UNSIGNED NOT NULL,
    maximum_level SMALLINT UNSIGNED NOT NULL,
    form_id INT UNSIGNED NOT NULL,
    PRIMARY KEY(profile_id,stage),
    FOREIGN KEY(profile_id) REFERENCES protos.growth_pet_profile(profile_id),
    FOREIGN KEY(form_id) REFERENCES protos.companion_form(form_id),
    CHECK(stage<16), CHECK(maximum_level BETWEEN 1 AND 255)
) ENGINE=InnoDB;
ALTER TABLE protos.item_companion
    ADD COLUMN growth_profile_id INT UNSIGNED NULL,
    ADD FOREIGN KEY(growth_profile_id) REFERENCES protos.growth_pet_profile(profile_id),
    DROP CONSTRAINT ck_companion_kind,
    ADD CONSTRAINT ck_companion_kind CHECK(
        (companion_kind=1 AND riding_form_id IS NULL AND growth_profile_id IS NULL) OR
        (companion_kind=2 AND riding_form_id IS NOT NULL AND growth_profile_id IS NULL) OR
        (companion_kind=3 AND riding_form_id IS NULL AND growth_profile_id IS NOT NULL AND clock_kind IN(1,4)));
-- Full level/stage coverage, initial form and non-refinable growth seals are
-- also checked by the native catalog publication gate.
UPDATE protos.catalog_format SET schema_version=20,canonical_domain='auriga.item_catalog.v20' WHERE singleton=1;
