-- Versioned clock only; existing permanent seals keep NULL and their digest.
ALTER TABLE player.item_instance
 ADD COLUMN IF NOT EXISTS companion_lifetime_blob VARBINARY(22) NULL;
ALTER TABLE player.item_instance
 ADD CONSTRAINT IF NOT EXISTS ck_typed_companion_lifetime CHECK
 (companion_lifetime_blob IS NULL OR
  (category_id=15 AND companion_recall_requested IS NOT NULL AND growth_pet_blob IS NULL
   AND OCTET_LENGTH(companion_lifetime_blob)=22));
