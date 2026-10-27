-- Fresh, unreleased catalog schema. Equipped-time socket grading is distinct
-- from normal refinement. No sentinel socket integers or vnum-family arithmetic.
CREATE TABLE protos.accessory_socket_profile (
    profile_id INT UNSIGNED NOT NULL PRIMARY KEY,
    code VARCHAR(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL UNIQUE,
    max_grade TINYINT UNSIGNED NOT NULL,
    unlock_units_per_attempt SMALLINT UNSIGNED NOT NULL,
    charge_units_per_attempt SMALLINT UNSIGNED NOT NULL,
    CHECK(profile_id>0), CHECK(CHAR_LENGTH(code)>0), CHECK(max_grade>0),
    CHECK(unlock_units_per_attempt>0), CHECK(charge_units_per_attempt>0)
) ENGINE=InnoDB;

CREATE TABLE protos.accessory_socket_grade (
    profile_id INT UNSIGNED NOT NULL, grade TINYINT UNSIGNED NOT NULL,
    duration_ms BIGINT UNSIGNED NOT NULL,
    bonus_increase_bp INT UNSIGNED NOT NULL,
    minimum_raw BIGINT NOT NULL, minimum_bp INT NOT NULL, minimum_integer INT NOT NULL,
    unlock_chance_bp SMALLINT UNSIGNED NOT NULL, charge_chance_bp SMALLINT UNSIGNED NOT NULL,
    PRIMARY KEY(profile_id,grade),
    FOREIGN KEY(profile_id) REFERENCES protos.accessory_socket_profile(profile_id),
    CHECK(duration_ms<=9223372036854775807), CHECK(grade=0 OR duration_ms>0),
    CHECK(bonus_increase_bp<=100000), CHECK(minimum_raw BETWEEN 0 AND 1000000000000000),
    CHECK(minimum_bp BETWEEN 0 AND 100000), CHECK(minimum_integer BETWEEN 0 AND 100000),
    CHECK(unlock_chance_bp<=10000), CHECK(charge_chance_bp<=10000),
    CHECK(grade<>0 OR (duration_ms=0 AND bonus_increase_bp=0 AND minimum_raw=0 AND minimum_bp=0 AND minimum_integer=0))
) ENGINE=InnoDB;

CREATE TABLE protos.accessory_socket_material (
    profile_id INT UNSIGNED NOT NULL, operation_kind TINYINT UNSIGNED NOT NULL,
    category_id SMALLINT UNSIGNED NOT NULL, local_id INT UNSIGNED NOT NULL,
    PRIMARY KEY(profile_id,operation_kind,category_id,local_id),
    FOREIGN KEY(profile_id) REFERENCES protos.accessory_socket_profile(profile_id),
    FOREIGN KEY(category_id,local_id) REFERENCES protos.item_material(category_id,local_id),
    CHECK(operation_kind IN (1,2,3)), CHECK(category_id=6)
) ENGINE=InnoDB;

ALTER TABLE protos.item_accessory ADD COLUMN socket_profile_id INT UNSIGNED NULL,
    ADD FOREIGN KEY(socket_profile_id) REFERENCES protos.accessory_socket_profile(profile_id);
-- Full grade coverage, terminal chances, monotone bonuses, compatibility count,
-- required nonempty lists, disjoint charge lifetimes and charge-material Jewel
-- kind are cross-row builder checks. Permanent-charge compatibility is optional.
-- operation_kind: 1=unlock, 2=equipped-time charge, 3=permanent charge.
