-- Optional typed gathering-tool instance state. Old item hashes are unchanged.
ALTER TABLE player.item_instance
 ADD COLUMN IF NOT EXISTS tool_training_points INT UNSIGNED NULL,
 ADD COLUMN IF NOT EXISTS tool_bait_power INT UNSIGNED NULL;
