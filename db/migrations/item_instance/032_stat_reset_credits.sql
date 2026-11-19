-- Existing installations already have this original signed player column.
-- Minimal typed-only schemas acquire the same column for atomic character writes.
ALTER TABLE player.player ADD COLUMN IF NOT EXISTS stat_reset_count SMALLINT NOT NULL DEFAULT 0;
ALTER TABLE player.player_deleted ADD COLUMN IF NOT EXISTS stat_reset_count SMALLINT NOT NULL DEFAULT 0;
