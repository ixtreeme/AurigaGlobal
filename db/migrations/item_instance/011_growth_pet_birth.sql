-- Full versioned growth state is part of the seal and its state digest.
-- Existing item rows keep NULL and the same canonical digest.
ALTER TABLE player.item_instance
 ADD COLUMN IF NOT EXISTS growth_pet_blob VARBINARY(512) NULL
 CHECK (growth_pet_blob IS NULL OR
        (category_id=15 AND companion_recall_requested IS NULL AND OCTET_LENGTH(growth_pet_blob)>0));
