-- A shared guild gate serializes typed deposits against guild disband.
-- Closing never resets itself; an existing guild owner is never recreated.
CREATE TABLE IF NOT EXISTS player.item_shared_storage_gate (
 guild_id INT UNSIGNED NOT NULL PRIMARY KEY CHECK(guild_id>0),
 closing TINYINT UNSIGNED NOT NULL DEFAULT 0 CHECK(closing IN(0,1)),
 closed_by_pid INT UNSIGNED NULL
) ENGINE=InnoDB;
