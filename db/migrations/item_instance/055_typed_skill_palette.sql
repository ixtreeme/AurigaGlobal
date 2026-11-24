-- Unedited slots continue to use the original character palette. Explicit all-zero rows are free resets.
CREATE TABLE IF NOT EXISTS player.item_skill_palette (
    player_id INT UNSIGNED NOT NULL,
    slot TINYINT UNSIGNED NOT NULL CHECK (slot < 6),
    c1 INT UNSIGNED NOT NULL DEFAULT 0,
    c2 INT UNSIGNED NOT NULL DEFAULT 0,
    c3 INT UNSIGNED NOT NULL DEFAULT 0,
    c4 INT UNSIGNED NOT NULL DEFAULT 0,
    c5 INT UNSIGNED NOT NULL DEFAULT 0,
    PRIMARY KEY (player_id, slot)
) ENGINE=InnoDB;
