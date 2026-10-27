-- Unreleased, isolated schema. Definition rows must equal the compiled registry
-- in the future SQL loader; mutable SQL cannot introduce a new runtime BonusId.
CREATE TABLE protos.bonus_definition (
    bonus_id INT UNSIGNED NOT NULL PRIMARY KEY,
    code VARCHAR(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL UNIQUE,
    display_key VARCHAR(128) CHARACTER SET ascii COLLATE ascii_bin NOT NULL UNIQUE,
    unit_kind TINYINT UNSIGNED NOT NULL,
    target_kind TINYINT UNSIGNED NOT NULL, stage_kind TINYINT UNSIGNED NOT NULL,
    channel_filter TINYINT UNSIGNED NOT NULL, action_filter TINYINT UNSIGNED NOT NULL,
    mode_filter TINYINT UNSIGNED NOT NULL,
    family_filter SMALLINT UNSIGNED NOT NULL, rank_filter SMALLINT UNSIGNED NOT NULL,
    element_filter TINYINT UNSIGNED NOT NULL, stack_policy TINYINT UNSIGNED NOT NULL,
    aggregate_cap TINYINT UNSIGNED NOT NULL, allowed_sources TINYINT UNSIGNED NOT NULL,
    minimum_value BIGINT NOT NULL, maximum_value BIGINT NOT NULL,
    CHECK (bonus_id > 0), CHECK (CHAR_LENGTH(code) > 0), CHECK (CHAR_LENGTH(display_key) > 0),
    CHECK (unit_kind BETWEEN 1 AND 3), CHECK (target_kind BETWEEN 1 AND 14),
    CHECK (stage_kind BETWEEN 1 AND 11), CHECK (channel_filter BETWEEN 1 AND 3),
    CHECK (action_filter BETWEEN 1 AND 3), CHECK (mode_filter BETWEEN 1 AND 3),
    CHECK (family_filter=0 AND rank_filter=0 AND element_filter=0),
    CHECK (stack_policy BETWEEN 1 AND 4), CHECK (aggregate_cap BETWEEN 1 AND 12),
    CHECK (allowed_sources BETWEEN 1 AND 3), CHECK (minimum_value <= maximum_value),
    CHECK ((unit_kind=1 AND minimum_value>=-1000000000000000 AND maximum_value<=1000000000000000)
        OR (unit_kind IN (2,3) AND minimum_value>=-100000 AND maximum_value<=100000))
) ENGINE=InnoDB;

CREATE TABLE protos.bonus_profile_entry (
    profile_id INT UNSIGNED NOT NULL,
    bonus_id INT UNSIGNED NOT NULL,
    value_scaled BIGINT NOT NULL,
    PRIMARY KEY(profile_id,bonus_id),
    FOREIGN KEY(profile_id) REFERENCES protos.bonus_profile(profile_id),
    FOREIGN KEY(bonus_id) REFERENCES protos.bonus_definition(bonus_id),
    CHECK (value_scaled BETWEEN -1000000000000000 AND 1000000000000000)
) ENGINE=InnoDB;
-- Per-definition bounds/source compatibility, nonempty and <=128 entries are
-- cross-row rules checked by ValidateBonusProfile before snapshot publication.
