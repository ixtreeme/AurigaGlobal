-- Fresh, unreleased typed recovery catalog. No existing gameplay DB upgrade.
CREATE TABLE protos.consumable_cooldown_profile (
    profile_id INT UNSIGNED NOT NULL PRIMARY KEY,
    code VARCHAR(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL UNIQUE,
    duration_ms BIGINT UNSIGNED NOT NULL,
    CHECK(profile_id>0), CHECK(CHAR_LENGTH(code)>0),
    CHECK(duration_ms BETWEEN 1 AND 9223372036854775807)
) ENGINE=InnoDB;
CREATE TABLE protos.recovery_profile (
    profile_id INT UNSIGNED NOT NULL PRIMARY KEY,
    code VARCHAR(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL UNIQUE,
    delivery_kind TINYINT UNSIGNED NOT NULL,
    units_per_use SMALLINT UNSIGNED NOT NULL,
    allowed_contexts TINYINT UNSIGNED NOT NULL, allow_during_pvp BOOLEAN NOT NULL,
    cooldown_profile_id INT UNSIGNED NULL,
    UNIQUE KEY uq_recovery_delivery(profile_id,delivery_kind),
    FOREIGN KEY(cooldown_profile_id) REFERENCES protos.consumable_cooldown_profile(profile_id),
    CHECK(profile_id>0), CHECK(CHAR_LENGTH(code)>0), CHECK(delivery_kind IN (1,2,3,4)),
    CHECK(units_per_use>0), CHECK(allowed_contexts BETWEEN 1 AND 15), CHECK(allow_during_pvp IN (0,1))
) ENGINE=InnoDB;
CREATE TABLE protos.instant_recovery_profile (
    profile_id INT UNSIGNED NOT NULL PRIMARY KEY, delivery_kind TINYINT UNSIGNED NOT NULL DEFAULT 1,
    hp_raw BIGINT NOT NULL, sp_raw BIGINT NOT NULL,
    max_hp_bp SMALLINT UNSIGNED NOT NULL, max_sp_bp SMALLINT UNSIGNED NOT NULL,
    flat_bonus_cap_bp INT UNSIGNED NOT NULL,
    FOREIGN KEY(profile_id,delivery_kind) REFERENCES protos.recovery_profile(profile_id,delivery_kind),
    CHECK(delivery_kind=1), CHECK(hp_raw BETWEEN 0 AND 1000000000000000), CHECK(sp_raw BETWEEN 0 AND 1000000000000000),
    CHECK(max_hp_bp<=10000), CHECK(max_sp_bp<=10000), CHECK(flat_bonus_cap_bp<=100000),
    CHECK(hp_raw>0 OR sp_raw>0 OR max_hp_bp>0 OR max_sp_bp>0)
) ENGINE=InnoDB;
CREATE TABLE protos.reserve_recovery_profile (
    profile_id INT UNSIGNED NOT NULL PRIMARY KEY, delivery_kind TINYINT UNSIGNED NOT NULL DEFAULT 2,
    hp_raw BIGINT NOT NULL, sp_raw BIGINT NOT NULL, flat_bonus_cap_bp INT UNSIGNED NOT NULL,
    tick_interval_ms BIGINT UNSIGNED NOT NULL, max_pool_per_tick_bp SMALLINT UNSIGNED NOT NULL,
    FOREIGN KEY(profile_id,delivery_kind) REFERENCES protos.recovery_profile(profile_id,delivery_kind),
    CHECK(delivery_kind=2), CHECK(hp_raw BETWEEN 0 AND 1000000000000000), CHECK(sp_raw BETWEEN 0 AND 1000000000000000),
    CHECK(hp_raw>0 OR sp_raw>0), CHECK(flat_bonus_cap_bp<=100000),
    CHECK(tick_interval_ms BETWEEN 1 AND 9223372036854775807), CHECK(max_pool_per_tick_bp BETWEEN 1 AND 10000)
) ENGINE=InnoDB;
CREATE TABLE protos.periodic_recovery_profile (
    profile_id INT UNSIGNED NOT NULL PRIMARY KEY, delivery_kind TINYINT UNSIGNED NOT NULL DEFAULT 3,
    hp_per_tick_raw BIGINT NOT NULL, sp_per_tick_raw BIGINT NOT NULL,
    tick_interval_ms BIGINT UNSIGNED NOT NULL, duration_ms BIGINT UNSIGNED NOT NULL,
    FOREIGN KEY(profile_id,delivery_kind) REFERENCES protos.recovery_profile(profile_id,delivery_kind),
    CHECK(delivery_kind=3), CHECK(hp_per_tick_raw BETWEEN 0 AND 1000000000000000), CHECK(sp_per_tick_raw BETWEEN 0 AND 1000000000000000),
    CHECK((hp_per_tick_raw>0 AND sp_per_tick_raw=0) OR (hp_per_tick_raw=0 AND sp_per_tick_raw>0)),
    CHECK(tick_interval_ms BETWEEN 1 AND 9223372036854775807), CHECK(duration_ms<=9223372036854775807),
    CHECK(duration_ms>=tick_interval_ms), CHECK(duration_ms % tick_interval_ms=0)
) ENGINE=InnoDB;
CREATE TABLE protos.item_consumable (
    category_id SMALLINT UNSIGNED NOT NULL DEFAULT 5, local_id INT UNSIGNED NOT NULL,
    recovery_profile_id INT UNSIGNED NOT NULL,
    PRIMARY KEY(category_id,local_id),
    FOREIGN KEY(category_id,local_id) REFERENCES protos.item_base(category_id,local_id),
    FOREIGN KEY(recovery_profile_id) REFERENCES protos.recovery_profile(profile_id),
    CHECK(category_id=5)
) ENGINE=InnoDB;
-- Exactly one matching child per recovery root and item_base child coverage are
-- publication checks. SQL->catalog loading and state/resource commits are pending.
