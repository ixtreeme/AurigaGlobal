-- Unreleased v10 draft, whole fresh-schema chain. Not an in-place v7/v9 upgrader.
CREATE TABLE protos.toggle_effect_group (
    group_id INT UNSIGNED NOT NULL PRIMARY KEY,
    code VARCHAR(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL UNIQUE,
    remove_on_death BOOLEAN NOT NULL,
    CHECK(group_id>0), CHECK(CHAR_LENGTH(code)>0), CHECK(remove_on_death IN (0,1))
) ENGINE=InnoDB;
CREATE TABLE protos.toggle_effect_profile (
    profile_id INT UNSIGNED NOT NULL PRIMARY KEY,
    code VARCHAR(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL UNIQUE,
    group_id INT UNSIGNED NOT NULL, bonus_profile_id INT UNSIGNED NOT NULL,
    variant_id SMALLINT UNSIGNED NOT NULL,
    duration_ms BIGINT UNSIGNED NOT NULL,
    requirements TINYINT UNSIGNED NOT NULL,
    allowed_contexts TINYINT UNSIGNED NOT NULL, allow_during_pvp BOOLEAN NOT NULL,
    cooldown_profile_id INT UNSIGNED NULL,
    FOREIGN KEY(group_id) REFERENCES protos.toggle_effect_group(group_id),
    FOREIGN KEY(bonus_profile_id) REFERENCES protos.bonus_profile(profile_id),
    FOREIGN KEY(cooldown_profile_id) REFERENCES protos.consumable_cooldown_profile(profile_id),
    CHECK(profile_id>0), CHECK(CHAR_LENGTH(code)>0), CHECK(variant_id>0),
    CHECK(duration_ms BETWEEN 1 AND 9223372036854775807), CHECK(requirements BETWEEN 0 AND 3),
    CHECK(allowed_contexts BETWEEN 1 AND 15), CHECK(allow_during_pvp IN (0,1))
) ENGINE=InnoDB;
ALTER TABLE protos.item_consumable
    DROP CONSTRAINT ck_consumable_one_effect,
    ADD toggle_effect_profile_id INT UNSIGNED NULL,
    ADD FOREIGN KEY(toggle_effect_profile_id) REFERENCES protos.toggle_effect_profile(profile_id),
    ADD CONSTRAINT ck_consumable_one_effect CHECK (
        (recovery_profile_id IS NOT NULL) + (timed_effect_profile_id IS NOT NULL) +
        (toggle_effect_profile_id IS NOT NULL) = 1);
UPDATE protos.catalog_format SET schema_version=10, canonical_domain='auriga.item_catalog.v10' WHERE singleton=1;
-- TimedEffect bonus source, nonzero bonus and unstacked item are publication invariants.
