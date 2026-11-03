-- C21: existing rows and hashes stay intact; apply before server startup.
ALTER TABLE player.item_instance
 ADD COLUMN IF NOT EXISTS toggle_remaining_ms BIGINT UNSIGNED NULL,
 ADD COLUMN IF NOT EXISTS toggle_active TINYINT UNSIGNED NULL;
