-- Unreleased fresh-chain v21. No update of an existing game database.
CREATE TABLE protos.pet_hatch_profile (
    profile_id INT UNSIGNED NOT NULL PRIMARY KEY,
    code VARCHAR(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL UNIQUE,
    target_category_id SMALLINT UNSIGNED NOT NULL,
    target_local_id INT UNSIGNED NOT NULL,
    gold_cost BIGINT UNSIGNED NOT NULL,
    hp_min SMALLINT UNSIGNED NOT NULL, hp_max SMALLINT UNSIGNED NOT NULL, hp_step SMALLINT UNSIGNED NOT NULL,
    monster_min SMALLINT UNSIGNED NOT NULL, monster_max SMALLINT UNSIGNED NOT NULL, monster_step SMALLINT UNSIGNED NOT NULL,
    pve_normal_min SMALLINT UNSIGNED NOT NULL, pve_normal_max SMALLINT UNSIGNED NOT NULL, pve_normal_step SMALLINT UNSIGNED NOT NULL,
    FOREIGN KEY(target_category_id,target_local_id) REFERENCES protos.item_base(category_id,local_id),
    CHECK(profile_id>0), CHECK(LENGTH(code)>0), CHECK(target_category_id=15), CHECK(target_local_id>0),
    CHECK(gold_cost<=9223372036854775807),
    CHECK(hp_min<=hp_max AND hp_max<=32767 AND hp_step BETWEEN 1 AND 32767 AND MOD(hp_max-hp_min,hp_step)=0),
    CHECK(monster_min<=monster_max AND monster_max<=32767 AND monster_step BETWEEN 1 AND 32767 AND MOD(monster_max-monster_min,monster_step)=0),
    CHECK(pve_normal_min<=pve_normal_max AND pve_normal_max<=32767 AND pve_normal_step BETWEEN 1 AND 32767 AND MOD(pve_normal_max-pve_normal_min,pve_normal_step)=0)
) ENGINE=InnoDB;
CREATE TABLE protos.pet_feed_rule (
    category_id SMALLINT UNSIGNED NOT NULL, local_id INT UNSIGNED NOT NULL,
    service_kind TINYINT UNSIGNED NOT NULL,
    food_kind TINYINT UNSIGNED NOT NULL,
    consumption_kind TINYINT UNSIGNED NOT NULL,
    units INT UNSIGNED NOT NULL,
    lifetime_fraction_bp SMALLINT UNSIGNED NOT NULL,
    dead_recovery_bp SMALLINT UNSIGNED NOT NULL,
    experience_price_divisor INT UNSIGNED NOT NULL,
    PRIMARY KEY(category_id,local_id,service_kind),
    FOREIGN KEY(category_id,local_id) REFERENCES protos.item_base(category_id,local_id),
    CHECK((consumption_kind=1 AND units>0) OR (consumption_kind=2 AND units=0)),
    CHECK((food_kind=1 AND service_kind=1 AND lifetime_fraction_bp BETWEEN 1 AND 10000 AND dead_recovery_bp=0 AND experience_price_divisor=0) OR
          (food_kind=2 AND service_kind=2 AND lifetime_fraction_bp=0 AND dead_recovery_bp BETWEEN 1 AND 10000 AND experience_price_divisor=0) OR
          (food_kind=3 AND service_kind=1 AND lifetime_fraction_bp=0 AND dead_recovery_bp=0 AND experience_price_divisor>0))
) ENGINE=InnoDB;
ALTER TABLE protos.item_container
    ADD COLUMN hatch_profile_id INT UNSIGNED NULL,
    ADD FOREIGN KEY(hatch_profile_id) REFERENCES protos.pet_hatch_profile(profile_id),
    DROP CONSTRAINT ck_container_kind, DROP CONSTRAINT ck_container_loot,
    ADD CONSTRAINT ck_container_kind CHECK(container_kind IN(1,2,3,4)),
    ADD CONSTRAINT ck_container_loot CHECK((container_kind IN(3,4)) = (loot_profile_id IS NULL)),
    ADD CONSTRAINT ck_container_hatch CHECK((container_kind=4) = (hatch_profile_id IS NOT NULL));
UPDATE protos.catalog_format SET schema_version=21,canonical_domain='auriga.item_catalog.v21' WHERE singleton=1;
