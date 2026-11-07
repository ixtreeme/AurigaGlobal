-- Explicit instance clock, including permanent costume state. Existing rows stay NULL.
ALTER TABLE player.item_instance
 ADD COLUMN IF NOT EXISTS costume_lifetime_blob VARBINARY(22) NULL;
ALTER TABLE player.item_instance
 ADD CONSTRAINT IF NOT EXISTS ck_typed_costume_lifetime CHECK
 (costume_lifetime_blob IS NULL OR (category_id=4 AND OCTET_LENGTH(costume_lifetime_blob)=22));
-- Keep all occupied cells; extend old eight-slot owner layouts for costume slots 9..11.
UPDATE player.item_storage SET grid_columns=GREATEST(grid_columns,11)
 WHERE storage_kind=2;
