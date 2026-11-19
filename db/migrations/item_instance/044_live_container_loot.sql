-- Chest contents share the immutable release/head used by live monster drops.
-- Overrides bind to a chest key, so editing a shared loot profile never changes
-- an unselected chest or fishing reward. Item definitions/persistence stay pinned.
ALTER TABLE protos.mob_drop_live_release
    ADD COLUMN reward_schema_version TINYINT UNSIGNED NOT NULL DEFAULT 0,
    ADD COLUMN container_group_count INT UNSIGNED NOT NULL DEFAULT 0,
    ADD COLUMN container_entry_count INT UNSIGNED NOT NULL DEFAULT 0;

CREATE TABLE protos.container_loot_live_group (
    release_id CHAR(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
    category_id SMALLINT UNSIGNED NOT NULL,
    local_id INT UNSIGNED NOT NULL,
    pick_mode TINYINT UNSIGNED NOT NULL,
    PRIMARY KEY(release_id,category_id,local_id),
    FOREIGN KEY(release_id) REFERENCES protos.mob_drop_live_release(release_id),
    CHECK(category_id=8 AND local_id>0), CHECK(pick_mode IN (1,2))
) ENGINE=InnoDB;
CREATE TABLE protos.container_loot_live_entry (
    release_id CHAR(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
    category_id SMALLINT UNSIGNED NOT NULL, local_id INT UNSIGNED NOT NULL,
    entry_no SMALLINT UNSIGNED NOT NULL, reward_kind TINYINT UNSIGNED NOT NULL,
    item_category_id SMALLINT UNSIGNED NULL, item_local_id INT UNSIGNED NULL,
    amount BIGINT UNSIGNED NOT NULL, weight INT UNSIGNED NOT NULL,
    rare_attribute_chance_bp SMALLINT UNSIGNED NOT NULL,
    refinement_level SMALLINT UNSIGNED NOT NULL,
    PRIMARY KEY(release_id,category_id,local_id,entry_no),
    FOREIGN KEY(release_id,category_id,local_id) REFERENCES protos.container_loot_live_group(release_id,category_id,local_id),
    CHECK(entry_no BETWEEN 1 AND 256), CHECK(reward_kind BETWEEN 1 AND 9),
    CHECK((reward_kind=1 AND item_category_id IS NOT NULL AND item_category_id>0 AND item_local_id IS NOT NULL AND item_local_id>0)
       OR (reward_kind<>1 AND item_category_id IS NULL AND item_local_id IS NULL)),
    CHECK(weight>0), CHECK(rare_attribute_chance_bp<=10000), CHECK(refinement_level<=255)
) ENGINE=InnoDB;
