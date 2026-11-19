-- A separate owned component; existing regalia blobs remain byte-identical.
ALTER TABLE player.item_instance ADD COLUMN IF NOT EXISTS sash_blob VARBINARY(110) NULL,
 ADD CONSTRAINT IF NOT EXISTS ck_typed_absorption_sash CHECK
 (sash_blob IS NULL OR (category_id=19 AND regalia_blob IS NULL AND OCTET_LENGTH(sash_blob)=110));
UPDATE player.item_storage SET grid_columns=GREATEST(grid_columns,29) WHERE storage_kind=2;
