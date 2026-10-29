-- Unreleased v18: independent red/blue Soul cores and their refinement scrolls.
CREATE TABLE protos.soul_profile (
    profile_id INT UNSIGNED NOT NULL PRIMARY KEY,
    code VARCHAR(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL UNIQUE,
    CHECK(profile_id>0), CHECK(code<>'')
) ENGINE=InnoDB;
CREATE TABLE protos.soul_level (
    profile_id INT UNSIGNED NOT NULL, level SMALLINT UNSIGNED NOT NULL,
    charge_capacity INT UNSIGNED NOT NULL, cycle_charge_cost INT UNSIGNED NOT NULL,
    strikes_per_cycle INT UNSIGNED NOT NULL, initial_charge INT UNSIGNED NOT NULL,
    initial_strikes INT UNSIGNED NOT NULL, recharge_interval_ms BIGINT UNSIGNED NOT NULL,
    first_multiplier_bp INT UNSIGNED NOT NULL, second_multiplier_bp INT UNSIGNED NOT NULL,
    third_multiplier_bp INT UNSIGNED NOT NULL,
    PRIMARY KEY(profile_id,level), FOREIGN KEY(profile_id) REFERENCES protos.soul_profile(profile_id),
    CHECK(level<=255), CHECK(cycle_charge_cost>0),
    CHECK(charge_capacity>=cycle_charge_cost AND CAST(charge_capacity AS UNSIGNED)<=CAST(cycle_charge_cost AS UNSIGNED)*3),
    CHECK(MOD(charge_capacity,cycle_charge_cost)=0), CHECK(strikes_per_cycle>0),
    CHECK(initial_charge<=charge_capacity), CHECK(initial_strikes BETWEEN 1 AND strikes_per_cycle),
    CHECK(recharge_interval_ms BETWEEN 1 AND 9223372036854775807),
    CHECK(first_multiplier_bp BETWEEN 10000 AND 1000000),
    CHECK(second_multiplier_bp BETWEEN 10000 AND 1000000),
    CHECK(third_multiplier_bp BETWEEN 10000 AND 1000000)
) ENGINE=InnoDB;
CREATE TABLE protos.item_soul (
    category_id SMALLINT UNSIGNED NOT NULL DEFAULT 14, local_id INT UNSIGNED NOT NULL,
    soul_kind TINYINT UNSIGNED NOT NULL, damage_channel TINYINT UNSIGNED NULL,
    profile_id INT UNSIGNED NULL, refine_chance_bp SMALLINT UNSIGNED NOT NULL,
    PRIMARY KEY(category_id,local_id), FOREIGN KEY(category_id,local_id) REFERENCES protos.item_base(category_id,local_id),
    FOREIGN KEY(profile_id) REFERENCES protos.soul_profile(profile_id),
    CHECK(category_id=14), CHECK(local_id>0), CHECK(refine_chance_bp<=10000),
    CHECK((soul_kind=1 AND damage_channel IS NOT NULL AND damage_channel IN(1,2) AND profile_id IS NOT NULL AND refine_chance_bp=0)
       OR (soul_kind=2 AND damage_channel IS NULL AND profile_id IS NULL))
) ENGINE=InnoDB;
ALTER TABLE protos.refinement_profile DROP CONSTRAINT ck_refinement_profile_kind,
    ADD CONSTRAINT ck_refinement_profile_kind CHECK(profile_kind IN(1,2,3,4,5,6,7,8));
ALTER TABLE protos.item_base DROP CONSTRAINT ck_item_base_category, DROP CONSTRAINT ck_item_base_stack,
    DROP CONSTRAINT ck_item_base_non_equipment,
    ADD CONSTRAINT ck_item_base_category CHECK(category_id IN(1,2,3,4,5,6,7,8,9,10,11,12,13,14,16,18)),
    ADD CONSTRAINT ck_item_base_stack CHECK(category_id IN(5,6,7,8,9,10,12,13,14,16,18) OR max_stack=1),
    ADD CONSTRAINT ck_item_base_non_equipment CHECK(category_id NOT IN(5,6,7,8,9,10,12,13,14,16,18)
        OR (binding_policy<>3 AND (appearance_profile_id IS NULL OR category_id IN(7,10))
            AND (base_bonus_profile_id IS NULL OR category_id=7)));
UPDATE protos.catalog_format SET schema_version=18,canonical_domain='auriga.item_catalog.v18' WHERE singleton=1;
