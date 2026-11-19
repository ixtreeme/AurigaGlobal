-- Independent, immutable drop releases. Item definitions and the persistence
-- catalog pin do not change when the active drop list changes.
CREATE TABLE protos.mob_drop_live_release (
    release_id CHAR(32) CHARACTER SET ascii COLLATE ascii_bin PRIMARY KEY,
    base_sha256 CHAR(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
    drop_sha256 CHAR(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
    group_count INT UNSIGNED NOT NULL,
    entry_count INT UNSIGNED NOT NULL,
    created_at TIMESTAMP(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6)
) ENGINE=InnoDB;
CREATE TABLE protos.mob_drop_live_group (
    release_id CHAR(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
    group_id INT UNSIGNED NOT NULL,
    code VARCHAR(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
    target_kind TINYINT UNSIGNED NOT NULL, target_value INT UNSIGNED NOT NULL,
    roll_kind TINYINT UNSIGNED NOT NULL, kills_per_drop INT UNSIGNED NOT NULL,
    minimum_killer_level SMALLINT UNSIGNED NOT NULL,
    PRIMARY KEY(release_id,group_id), UNIQUE KEY live_drop_code(release_id,code),
    FOREIGN KEY(release_id) REFERENCES protos.mob_drop_live_release(release_id),
    CHECK(group_id>0), CHECK(roll_kind BETWEEN 1 AND 4),
    CHECK((target_kind=1 AND target_value>0) OR (target_kind=2 AND target_value<=5 AND roll_kind=1)),
    CHECK((roll_kind=2)=(kills_per_drop>0)), CHECK((roll_kind=3)=(minimum_killer_level>0))
) ENGINE=InnoDB;
CREATE TABLE protos.mob_drop_live_entry (
    release_id CHAR(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
    group_id INT UNSIGNED NOT NULL, entry_no SMALLINT UNSIGNED NOT NULL,
    category_id SMALLINT UNSIGNED NOT NULL, local_id INT UNSIGNED NOT NULL,
    refinement_level SMALLINT UNSIGNED NOT NULL, amount INT UNSIGNED NOT NULL,
    chance_ppb INT UNSIGNED NOT NULL, weight INT UNSIGNED NOT NULL,
    rare_attribute_chance_bp SMALLINT UNSIGNED NOT NULL,
    killer_level_min SMALLINT UNSIGNED NOT NULL, killer_level_max SMALLINT UNSIGNED NOT NULL,
    PRIMARY KEY(release_id,group_id,entry_no),
    FOREIGN KEY(release_id,group_id) REFERENCES protos.mob_drop_live_group(release_id,group_id),
    CHECK(entry_no BETWEEN 1 AND 256), CHECK(refinement_level<=255), CHECK(amount>0),
    CHECK(chance_ppb<=1000000000), CHECK((chance_ppb>0)<>(weight>0)),
    CHECK(rare_attribute_chance_bp<=10000), CHECK(killer_level_min>0 AND killer_level_min<=killer_level_max)
) ENGINE=InnoDB;
CREATE TABLE protos.mob_drop_live_head (
    singleton TINYINT UNSIGNED PRIMARY KEY CHECK(singleton=1),
    release_id CHAR(32) CHARACTER SET ascii COLLATE ascii_bin NULL,
    FOREIGN KEY(release_id) REFERENCES protos.mob_drop_live_release(release_id)
) ENGINE=InnoDB;
INSERT INTO protos.mob_drop_live_head VALUES(1,NULL);
