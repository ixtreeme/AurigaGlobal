-- Apply before starting the C20 server. Existing item rows remain NULL.
ALTER TABLE player.item_instance
 ADD COLUMN IF NOT EXISTS automatic_remaining_raw BIGINT UNSIGNED NULL,
 ADD COLUMN IF NOT EXISTS automatic_active TINYINT UNSIGNED NULL,
 ADD COLUMN IF NOT EXISTS automatic_next_ms BIGINT UNSIGNED NULL;
