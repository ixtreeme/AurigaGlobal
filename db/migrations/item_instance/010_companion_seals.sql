-- Permanent typed seals store recall intent. NULL preserves every older item state.
ALTER TABLE player.item_instance
 ADD COLUMN IF NOT EXISTS companion_recall_requested TINYINT UNSIGNED NULL
 CHECK (companion_recall_requested IN (0,1));
