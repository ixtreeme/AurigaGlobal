-- Unreleased v8 draft: apply the whole fresh-schema migration chain.
-- Bonus registry foundation/seed also changed; this is NOT a v7 upgrade script.
CREATE TABLE protos.timed_effect_group (
    group_id INT UNSIGNED NOT NULL PRIMARY KEY,
    code VARCHAR(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL UNIQUE,
    reapply_kind TINYINT UNSIGNED NOT NULL, clock_kind TINYINT UNSIGNED NOT NULL,
    remove_on_death BOOLEAN NOT NULL,
    CHECK(group_id>0), CHECK(CHAR_LENGTH(code)>0),
    CHECK(reapply_kind IN (1,2)), CHECK(clock_kind IN (1,2)), CHECK(remove_on_death IN (0,1))
) ENGINE=InnoDB;
CREATE TABLE protos.timed_effect_profile (
    profile_id INT UNSIGNED NOT NULL PRIMARY KEY,
    code VARCHAR(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL UNIQUE,
    group_id INT UNSIGNED NOT NULL, bonus_profile_id INT UNSIGNED NOT NULL,
    duration_ms BIGINT UNSIGNED NOT NULL, units_per_use SMALLINT UNSIGNED NOT NULL,
    allowed_contexts TINYINT UNSIGNED NOT NULL, allow_during_pvp BOOLEAN NOT NULL,
    cooldown_profile_id INT UNSIGNED NULL,
    FOREIGN KEY(group_id) REFERENCES protos.timed_effect_group(group_id),
    FOREIGN KEY(bonus_profile_id) REFERENCES protos.bonus_profile(profile_id),
    FOREIGN KEY(cooldown_profile_id) REFERENCES protos.consumable_cooldown_profile(profile_id),
    CHECK(profile_id>0), CHECK(CHAR_LENGTH(code)>0), CHECK(duration_ms BETWEEN 1 AND 9223372036854775807),
    CHECK(units_per_use>0), CHECK(allowed_contexts BETWEEN 1 AND 15), CHECK(allow_during_pvp IN (0,1))
) ENGINE=InnoDB;
ALTER TABLE protos.item_consumable
    MODIFY recovery_profile_id INT UNSIGNED NULL,
    ADD timed_effect_profile_id INT UNSIGNED NULL,
    ADD FOREIGN KEY(timed_effect_profile_id) REFERENCES protos.timed_effect_profile(profile_id),
    ADD CONSTRAINT ck_consumable_one_effect CHECK (
        (recovery_profile_id IS NOT NULL AND timed_effect_profile_id IS NULL) OR
        (recovery_profile_id IS NULL AND timed_effect_profile_id IS NOT NULL));
UPDATE protos.catalog_format SET schema_version=8, canonical_domain='auriga.item_catalog.v8' WHERE singleton=1;
-- Bonus source compatibility and nonzero effects are checked by catalog publication.
