-- The cooldown lives in BOK5's optional typed hair state. Keep the original
-- player column in the same item transaction and legacy-cache revision fence.
ALTER TABLE player.player ADD COLUMN IF NOT EXISTS part_hair INT UNSIGNED NOT NULL DEFAULT 0;
ALTER TABLE player.player_deleted ADD COLUMN IF NOT EXISTS part_hair INT UNSIGNED NOT NULL DEFAULT 0;
