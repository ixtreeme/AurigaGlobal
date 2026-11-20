-- Existing names must be unique under the player table's collation.
ALTER TABLE player.player
    ADD UNIQUE INDEX IF NOT EXISTS item_identity_character_name (name);
