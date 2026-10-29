-- Typed containers (8), their keys (18) and loot profiles. Replaces box lookup
-- in special_item_group.json by vnum and the GOLD/EXP/MOB sentinel "vnums".
-- Fresh, unreleased draft schema: no live gameplay DB upgrade.
CREATE TABLE protos.loot_profile (
    profile_id INT UNSIGNED NOT NULL PRIMARY KEY,
    code VARCHAR(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL UNIQUE,
    pick_mode TINYINT UNSIGNED NOT NULL,
    CHECK(profile_id>0), CHECK(CHAR_LENGTH(code)>0), CHECK(pick_mode IN (1,2))
) ENGINE=InnoDB;

-- reward_kind: 1 item, 2 gold, 3 experience, 4 spawn monster, 5 spawn monster
-- group, 6 slow, 7 drain HP (bp of max), 8 poison. pick_mode 1 uses weight as a
-- relative weight, pick_mode 2 as an independent chance in bp (checked by the builder).
CREATE TABLE protos.loot_entry (
    profile_id INT UNSIGNED NOT NULL,
    entry_no SMALLINT UNSIGNED NOT NULL,
    reward_kind TINYINT UNSIGNED NOT NULL,
    item_category_id SMALLINT UNSIGNED NULL, item_local_id INT UNSIGNED NULL,
    amount BIGINT UNSIGNED NOT NULL,
    weight INT UNSIGNED NOT NULL,
    rare_attribute_chance_bp SMALLINT UNSIGNED NOT NULL DEFAULT 0,
    PRIMARY KEY(profile_id, entry_no),
    FOREIGN KEY(profile_id) REFERENCES protos.loot_profile(profile_id),
    FOREIGN KEY(item_category_id, item_local_id) REFERENCES protos.item_base(category_id, local_id),
    CHECK(entry_no>0), CHECK(reward_kind BETWEEN 1 AND 8), CHECK(weight>0),
    CHECK((reward_kind=1) = (item_category_id IS NOT NULL AND item_local_id IS NOT NULL)),
    CHECK(reward_kind=1 OR (item_category_id IS NULL AND item_local_id IS NULL)),
    CHECK(rare_attribute_chance_bp<=10000), CHECK(reward_kind=1 OR rare_attribute_chance_bp=0),
    CHECK(reward_kind NOT IN (1,4,5) OR amount BETWEEN 1 AND 4294967295),
    CHECK(reward_kind NOT IN (2,3) OR amount BETWEEN 1 AND 9223372036854775807),
    CHECK(reward_kind NOT IN (6,7) OR amount BETWEEN 1 AND 10000),
    CHECK(reward_kind<>8 OR amount=0)
) ENGINE=InnoDB;

CREATE TABLE protos.item_container (
    category_id SMALLINT UNSIGNED NOT NULL DEFAULT 8, local_id INT UNSIGNED NOT NULL,
    container_kind TINYINT UNSIGNED NOT NULL,
    loot_profile_id INT UNSIGNED NULL,
    lock_id SMALLINT UNSIGNED NOT NULL DEFAULT 0,
    PRIMARY KEY(category_id, local_id),
    FOREIGN KEY(category_id, local_id) REFERENCES protos.item_base(category_id, local_id),
    FOREIGN KEY(loot_profile_id) REFERENCES protos.loot_profile(profile_id),
    CHECK(category_id=8), CONSTRAINT ck_container_kind CHECK(container_kind IN (1,2,3)),
    CONSTRAINT ck_container_loot CHECK((container_kind=3) = (loot_profile_id IS NULL)),
    CHECK((container_kind=2) = (lock_id>0))
) ENGINE=InnoDB;

CREATE TABLE protos.item_container_key (
    category_id SMALLINT UNSIGNED NOT NULL DEFAULT 18, local_id INT UNSIGNED NOT NULL,
    lock_id SMALLINT UNSIGNED NOT NULL,
    PRIMARY KEY(category_id, local_id),
    FOREIGN KEY(category_id, local_id) REFERENCES protos.item_base(category_id, local_id),
    CHECK(category_id=18), CHECK(lock_id>0)
) ENGINE=InnoDB;

-- item_base admits the two new stackable, non-equipment categories. 005 left
-- these CHECKs unnamed; MariaDB numbers them in declaration order, so the
-- names below are fixed for this schema (verified by ItemCombatSchemaTests).
ALTER TABLE protos.item_base
    DROP CONSTRAINT CONSTRAINT_1, DROP CONSTRAINT CONSTRAINT_4, DROP CONSTRAINT CONSTRAINT_5,
    ADD CONSTRAINT ck_item_base_category CHECK (category_id IN (1,2,3,5,6,8,18)),
    ADD CONSTRAINT ck_item_base_stack CHECK (category_id IN (5,6,8,18) OR max_stack=1),
    ADD CONSTRAINT ck_item_base_non_equipment CHECK (category_id NOT IN (5,6,8,18)
        OR (binding_policy<>3 AND appearance_profile_id IS NULL AND base_bonus_profile_id IS NULL));

UPDATE protos.catalog_format SET schema_version=6, canonical_domain='auriga.item_catalog.v6' WHERE singleton=1;
-- Contents of a Locked container, the key/lock match, stack limits of item
-- rewards and contiguous entry numbers are cross-row builder checks.
