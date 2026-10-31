-- Unreleased v25 fresh-schema draft (F4 C7b, IC-035). Existing catalogs need a
-- staged upgrade. Monster drop tables replace mob_drop_item.json,
-- drop_item_group.json and common_drop_item.json; only the offline converter
-- reads those. Mobs are not catalog content yet: target_value is a legacy mob
-- vnum (target 1) or a mob rank 0..5 for the common drop (target 2).
-- Chances are parts per billion per kill at neutral modifiers.
CREATE TABLE protos.mob_drop_group (
    group_id INT UNSIGNED NOT NULL PRIMARY KEY,
    code VARCHAR(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL UNIQUE,
    target_kind TINYINT UNSIGNED NOT NULL, target_value INT UNSIGNED NOT NULL,
    roll_kind TINYINT UNSIGNED NOT NULL,
    kills_per_drop INT UNSIGNED NOT NULL, minimum_killer_level SMALLINT UNSIGNED NOT NULL,
    KEY ix_mob_drop_target(target_kind,target_value),
    CHECK(group_id>0), CHECK(CHAR_LENGTH(code)>0),
    CHECK(roll_kind BETWEEN 1 AND 4),
    CHECK((target_kind=1 AND target_value>0) OR (target_kind=2 AND target_value<=5 AND roll_kind=1)),
    CHECK((roll_kind=2)=(kills_per_drop>0)), CHECK((roll_kind=3)=(minimum_killer_level>0))
) ENGINE=InnoDB;
CREATE TABLE protos.mob_drop_entry (
    group_id INT UNSIGNED NOT NULL, entry_no SMALLINT UNSIGNED NOT NULL,
    category_id SMALLINT UNSIGNED NOT NULL, local_id INT UNSIGNED NOT NULL,
    refinement_level SMALLINT UNSIGNED NOT NULL, amount INT UNSIGNED NOT NULL,
    chance_ppb INT UNSIGNED NOT NULL, weight INT UNSIGNED NOT NULL,
    rare_attribute_chance_bp SMALLINT UNSIGNED NOT NULL,
    killer_level_min SMALLINT UNSIGNED NOT NULL, killer_level_max SMALLINT UNSIGNED NOT NULL,
    PRIMARY KEY(group_id,entry_no),
    FOREIGN KEY(group_id) REFERENCES protos.mob_drop_group(group_id),
    FOREIGN KEY(category_id,local_id) REFERENCES protos.item_base(category_id,local_id),
    CHECK(entry_no BETWEEN 1 AND 256), CHECK(refinement_level<=255), CHECK(amount>0),
    CHECK(chance_ppb<=1000000000), CHECK((chance_ppb>0)<>(weight>0)),
    CHECK(rare_attribute_chance_bp<=10000),
    CHECK(killer_level_min>0 AND killer_level_min<=killer_level_max)
) ENGINE=InnoDB;
UPDATE protos.catalog_format SET schema_version=25, canonical_domain='auriga.item_catalog.v25' WHERE singleton=1;
