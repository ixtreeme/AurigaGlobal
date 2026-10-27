CREATE TABLE protos.refinement_profile (
    profile_id INT UNSIGNED NOT NULL PRIMARY KEY,
    code VARCHAR(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL UNIQUE,
    profile_kind TINYINT UNSIGNED NOT NULL,
    max_level SMALLINT UNSIGNED NOT NULL,
    CHECK (profile_id > 0), CHECK (CHAR_LENGTH(code) > 0),
    CHECK (profile_kind IN (1,2,3,4)), CHECK (max_level <= 255),
    CHECK (profile_kind <> 1 OR max_level=0)
) ENGINE=InnoDB;

-- Gains are cumulative relative to zero. Coverage, profile-kind stat matching,
-- monotonicity, max_level and base+gain bounds are also checked by the builder.
CREATE TABLE protos.refinement_level (
    profile_id INT UNSIGNED NOT NULL,
    level SMALLINT UNSIGNED NOT NULL,
    physical_attack_gain_raw BIGINT NOT NULL DEFAULT 0,
    magical_attack_gain_raw BIGINT NOT NULL DEFAULT 0,
    physical_defense_gain_raw BIGINT NOT NULL DEFAULT 0,
    magical_defense_gain_raw BIGINT NOT NULL DEFAULT 0,
    extra_bonus_profile_id INT UNSIGNED NULL,
    PRIMARY KEY (profile_id,level),
    FOREIGN KEY (profile_id) REFERENCES protos.refinement_profile(profile_id),
    FOREIGN KEY (extra_bonus_profile_id) REFERENCES protos.bonus_profile(profile_id),
    CHECK (level <= 255),
    CHECK (physical_attack_gain_raw BETWEEN 0 AND 1000000000000000),
    CHECK (magical_attack_gain_raw BETWEEN 0 AND 1000000000000000),
    CHECK (physical_defense_gain_raw BETWEEN 0 AND 1000000000000000),
    CHECK (magical_defense_gain_raw BETWEEN 0 AND 1000000000000000),
    CHECK (level <> 0 OR (physical_attack_gain_raw=0 AND magical_attack_gain_raw=0
        AND physical_defense_gain_raw=0 AND magical_defense_gain_raw=0 AND extra_bonus_profile_id IS NULL))
) ENGINE=InnoDB;
