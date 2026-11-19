-- Server-owned run results. No difficulty levels or spendable point system.
CREATE TABLE IF NOT EXISTS player.dungeon_ui_run (
 id BINARY(16) NOT NULL PRIMARY KEY,
 dungeon INT UNSIGNED NOT NULL,
 started_at DATETIME(3) NOT NULL COMMENT 'UTC',
 completed_at DATETIME(3) NOT NULL COMMENT 'UTC',
 elapsed INT UNSIGNED NOT NULL,
 monsters INT UNSIGNED NOT NULL,
 deaths INT UNSIGNED NOT NULL,
 grade TINYINT UNSIGNED NOT NULL,
 KEY weekly (dungeon,completed_at)
) ENGINE=InnoDB;
CREATE TABLE IF NOT EXISTS player.dungeon_ui_member (
 run_id BINARY(16) NOT NULL,
 pid INT UNSIGNED NOT NULL,
 name VARBINARY(64) NOT NULL,
 level INT UNSIGNED NOT NULL,
 race INT UNSIGNED NOT NULL,
 damage BIGINT UNSIGNED NOT NULL,
 healing BIGINT UNSIGNED NOT NULL,
 taken BIGINT UNSIGNED NOT NULL,
 highest BIGINT UNSIGNED NOT NULL,
 deaths INT UNSIGNED NOT NULL,
 experience BIGINT UNSIGNED NOT NULL,
 yang_delta BIGINT NOT NULL,
 is_record TINYINT UNSIGNED NOT NULL,
 PRIMARY KEY (run_id,pid), KEY player_results (pid,run_id),
 CONSTRAINT dungeon_ui_member_run FOREIGN KEY(run_id) REFERENCES player.dungeon_ui_run(id)
) ENGINE=InnoDB;
CREATE TABLE IF NOT EXISTS player.dungeon_ui_loot (
 run_id BINARY(16) NOT NULL,
 pid INT UNSIGNED NOT NULL,
 item_key BIGINT UNSIGNED NOT NULL,
 refinement INT UNSIGNED NOT NULL,
 count BIGINT UNSIGNED NOT NULL,
 PRIMARY KEY (run_id,pid,item_key,refinement),
 CONSTRAINT dungeon_ui_loot_member FOREIGN KEY(run_id,pid) REFERENCES player.dungeon_ui_member(run_id,pid)
) ENGINE=InnoDB;
