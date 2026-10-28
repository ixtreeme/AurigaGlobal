-- Unreleased v16: typed gathering tools and fishing items. No saved sockets.
CREATE TABLE protos.tool_profile (
    profile_id INT UNSIGNED NOT NULL PRIMARY KEY,
    code VARCHAR(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL UNIQUE,
    CHECK(profile_id>0), CHECK(code<>'')
) ENGINE=InnoDB;
CREATE TABLE protos.tool_level (
    profile_id INT UNSIGNED NOT NULL, level SMALLINT UNSIGNED NOT NULL,
    power INT UNSIGNED NOT NULL, gather_bonus_bp SMALLINT UNSIGNED NOT NULL,
    required_points INT UNSIGNED NOT NULL, practice_numerator INT UNSIGNED NOT NULL,
    practice_denominator INT UNSIGNED NOT NULL, refine_chance_bp SMALLINT UNSIGNED NOT NULL,
    failure_kind TINYINT UNSIGNED NOT NULL, failure_level SMALLINT UNSIGNED NULL,
    failure_loss_bp SMALLINT UNSIGNED NOT NULL,
    PRIMARY KEY(profile_id,level), FOREIGN KEY(profile_id) REFERENCES protos.tool_profile(profile_id),
    CHECK(level<=255), CHECK(power<=2147483647), CHECK(gather_bonus_bp<=10000),
    CHECK(practice_denominator>0 AND practice_numerator<=practice_denominator),
    CHECK(refine_chance_bp<=10000), CHECK(failure_loss_bp<=10000),
    CHECK((failure_kind=1 AND failure_level IS NULL) OR
          (failure_kind=2 AND failure_level IS NOT NULL AND failure_level<=level AND failure_loss_bp=0)),
    CHECK(required_points>0 OR (practice_numerator=0 AND practice_denominator=1 AND refine_chance_bp=0
          AND failure_kind=1 AND failure_loss_bp=0))
) ENGINE=InnoDB;
CREATE TABLE protos.item_tool (
    category_id SMALLINT UNSIGNED NOT NULL DEFAULT 11, local_id INT UNSIGNED NOT NULL,
    tool_kind TINYINT UNSIGNED NOT NULL, profile_id INT UNSIGNED NOT NULL, fishing_pool TINYINT UNSIGNED NOT NULL,
    PRIMARY KEY(category_id,local_id), FOREIGN KEY(category_id,local_id) REFERENCES protos.item_base(category_id,local_id),
    FOREIGN KEY(profile_id) REFERENCES protos.tool_profile(profile_id),
    CHECK(category_id=11), CHECK(local_id>0), CHECK(tool_kind IN(1,2,3,4)),
    CHECK((tool_kind=2 AND fishing_pool IN(2,3)) OR (tool_kind IN(1,3,4) AND fishing_pool=1))
) ENGINE=InnoDB;
CREATE TABLE protos.item_fishing (
    category_id SMALLINT UNSIGNED NOT NULL DEFAULT 12, local_id INT UNSIGNED NOT NULL,
    fishing_kind TINYINT UNSIGNED NOT NULL, use_loot_profile_id INT UNSIGNED NULL,
    grill_category_id SMALLINT UNSIGNED NULL, grill_local_id INT UNSIGNED NULL,
    bait_power INT UNSIGNED NOT NULL,
    PRIMARY KEY(category_id,local_id), FOREIGN KEY(category_id,local_id) REFERENCES protos.item_base(category_id,local_id),
    FOREIGN KEY(use_loot_profile_id) REFERENCES protos.loot_profile(profile_id),
    FOREIGN KEY(grill_category_id,grill_local_id) REFERENCES protos.item_base(category_id,local_id),
    CHECK(category_id=12), CHECK(local_id>0), CHECK(fishing_kind IN(1,2,3)), CHECK(bait_power<=2147483647),
    CHECK((grill_category_id IS NULL AND grill_local_id IS NULL) OR
          (grill_category_id IS NOT NULL AND grill_category_id=5 AND grill_local_id IS NOT NULL AND grill_local_id>0)),
    CHECK((fishing_kind=1 AND use_loot_profile_id IS NOT NULL AND grill_category_id IS NULL AND bait_power=0) OR
          (fishing_kind=2 AND use_loot_profile_id IS NULL AND bait_power=0) OR
          (fishing_kind=3 AND use_loot_profile_id IS NULL AND grill_category_id IS NULL AND bait_power>0))
) ENGINE=InnoDB;
ALTER TABLE protos.refinement_profile DROP CONSTRAINT ck_refinement_profile_kind,
    ADD CONSTRAINT ck_refinement_profile_kind CHECK(profile_kind IN(1,2,3,4,5,6,7));
ALTER TABLE protos.item_base DROP CONSTRAINT ck_item_base_category, DROP CONSTRAINT ck_item_base_stack,
    DROP CONSTRAINT ck_item_base_non_equipment,
    ADD CONSTRAINT ck_item_base_category CHECK(category_id IN(1,2,3,4,5,6,7,8,9,10,11,12,16,18)),
    ADD CONSTRAINT ck_item_base_stack CHECK(category_id IN(5,6,7,8,9,10,12,16,18) OR max_stack=1),
    ADD CONSTRAINT ck_item_base_non_equipment CHECK(category_id NOT IN(5,6,7,8,9,10,12,16,18)
        OR (binding_policy<>3 AND (appearance_profile_id IS NULL OR category_id IN(7,10))
            AND (base_bonus_profile_id IS NULL OR category_id=7)));
UPDATE protos.catalog_format SET schema_version=16,canonical_domain='auriga.item_catalog.v16' WHERE singleton=1;
