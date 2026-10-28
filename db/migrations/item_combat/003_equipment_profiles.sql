-- Staged weapon/armor catalog profiles. Not connected to live init-db.
CREATE TABLE protos.item_use_policy (
    policy_id INT UNSIGNED NOT NULL PRIMARY KEY,
    code VARCHAR(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL UNIQUE,
    allowed_classes TINYINT UNSIGNED NOT NULL,
    allowed_sexes TINYINT UNSIGNED NOT NULL,
    use_during_combat BOOLEAN NOT NULL,
    CHECK (policy_id > 0), CHECK (CHAR_LENGTH(code) > 0),
    CHECK (allowed_classes BETWEEN 1 AND 15), CHECK (allowed_sexes BETWEEN 1 AND 3),
    CHECK (use_during_combat IN (0,1))
) ENGINE=InnoDB;

CREATE TABLE protos.item_trade_policy (
    policy_id INT UNSIGNED NOT NULL PRIMARY KEY,
    code VARCHAR(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL UNIQUE,
    allow_trade BOOLEAN NOT NULL, allow_drop BOOLEAN NOT NULL, allow_sell BOOLEAN NOT NULL,
    CHECK (policy_id > 0), CHECK (CHAR_LENGTH(code) > 0),
    CHECK (allow_trade IN (0,1)), CHECK (allow_drop IN (0,1)), CHECK (allow_sell IN (0,1))
) ENGINE=InnoDB;

CREATE TABLE protos.item_appearance_profile (
    profile_id INT UNSIGNED NOT NULL PRIMARY KEY,
    code VARCHAR(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL UNIQUE,
    model_asset VARCHAR(192) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
    CHECK (profile_id > 0), CHECK (CHAR_LENGTH(code) > 0), CHECK (CHAR_LENGTH(model_asset) > 0)
) ENGINE=InnoDB;

CREATE TABLE protos.item_attack_profile (
    profile_id INT UNSIGNED NOT NULL PRIMARY KEY,
    code VARCHAR(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL UNIQUE,
    interval_ticks INT UNSIGNED NOT NULL, windup_ticks INT UNSIGNED NOT NULL,
    reach_millimeters INT UNSIGNED NOT NULL, max_targets SMALLINT UNSIGNED NOT NULL,
    CHECK (profile_id > 0), CHECK (CHAR_LENGTH(code) > 0),
    CHECK (interval_ticks > 0), CHECK (windup_ticks <= interval_ticks),
    CHECK (reach_millimeters > 0), CHECK (max_targets BETWEEN 1 AND 1024)
) ENGINE=InnoDB;

CREATE TABLE protos.item_ammo_policy (
    policy_id INT UNSIGNED NOT NULL PRIMARY KEY,
    code VARCHAR(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL UNIQUE,
    ammo_kind TINYINT UNSIGNED NOT NULL,
    units_per_hit SMALLINT UNSIGNED NOT NULL,
    CHECK (policy_id > 0), CHECK (CHAR_LENGTH(code) > 0),
    CHECK ((ammo_kind=1 AND units_per_hit=0) OR (ammo_kind=2 AND units_per_hit>0))
) ENGINE=InnoDB;

CREATE TABLE protos.item_resistance_profile (
    profile_id INT UNSIGNED NOT NULL PRIMARY KEY,
    code VARCHAR(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL UNIQUE,
    physical_bp INT NOT NULL DEFAULT 0, magical_bp INT NOT NULL DEFAULT 0,
    sword_bp INT NOT NULL DEFAULT 0, dagger_bp INT NOT NULL DEFAULT 0, bow_bp INT NOT NULL DEFAULT 0,
    two_handed_bp INT NOT NULL DEFAULT 0, bell_bp INT NOT NULL DEFAULT 0, fan_bp INT NOT NULL DEFAULT 0,
    mount_spear_bp INT NOT NULL DEFAULT 0, fire_bp INT NOT NULL DEFAULT 0, electric_bp INT NOT NULL DEFAULT 0,
    ice_bp INT NOT NULL DEFAULT 0, wind_bp INT NOT NULL DEFAULT 0, earth_bp INT NOT NULL DEFAULT 0, dark_bp INT NOT NULL DEFAULT 0,
    CHECK (profile_id > 0), CHECK (CHAR_LENGTH(code) > 0),
    CHECK (physical_bp BETWEEN -10000 AND 10000), CHECK (magical_bp BETWEEN -10000 AND 10000),
    CHECK (sword_bp BETWEEN -10000 AND 10000), CHECK (dagger_bp BETWEEN -10000 AND 10000),
    CHECK (bow_bp BETWEEN -10000 AND 10000), CHECK (two_handed_bp BETWEEN -10000 AND 10000),
    CHECK (bell_bp BETWEEN -10000 AND 10000), CHECK (fan_bp BETWEEN -10000 AND 10000),
    CHECK (mount_spear_bp BETWEEN -10000 AND 10000), CHECK (fire_bp BETWEEN -10000 AND 10000),
    CHECK (electric_bp BETWEEN -10000 AND 10000), CHECK (ice_bp BETWEEN -10000 AND 10000),
    CHECK (wind_bp BETWEEN -10000 AND 10000), CHECK (earth_bp BETWEEN -10000 AND 10000), CHECK (dark_bp BETWEEN -10000 AND 10000)
) ENGINE=InnoDB;

-- Typed entries and compiled definitions follow in 006/007. Source kind is
-- explicit: item base and refinement cannot accidentally share a source scope.
CREATE TABLE protos.bonus_profile (
    profile_id INT UNSIGNED NOT NULL PRIMARY KEY,
    code VARCHAR(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL UNIQUE,
    source_kind TINYINT UNSIGNED NOT NULL,
    CHECK (profile_id > 0), CHECK (CHAR_LENGTH(code) > 0), CHECK (source_kind IN (1,2,3))
) ENGINE=InnoDB;
