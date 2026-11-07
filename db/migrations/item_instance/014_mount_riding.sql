-- Existing seals keep the same canonical digest until explicitly mounted.
ALTER TABLE player.item_instance
 ADD COLUMN IF NOT EXISTS companion_riding_requested TINYINT UNSIGNED NOT NULL DEFAULT 0
 CHECK (companion_riding_requested IN (0,1));

ALTER TABLE player.item_instance
 ADD CONSTRAINT IF NOT EXISTS ck_item_mount_riding_intent
 CHECK (companion_riding_requested=0 OR
        (category_id=15 AND companion_recall_requested IS NOT NULL
         AND companion_recall_requested=1));
