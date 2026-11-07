ALTER TABLE player.item_instance
 ADD COLUMN IF NOT EXISTS regalia_blob VARBINARY(93) NULL,
 ADD COLUMN IF NOT EXISTS bonus_tool_blob VARBINARY(18) NULL;
ALTER TABLE player.item_instance
 ADD CONSTRAINT IF NOT EXISTS ck_typed_regalia CHECK
 (regalia_blob IS NULL OR (category_id=19 AND OCTET_LENGTH(regalia_blob)=93)),
 ADD CONSTRAINT IF NOT EXISTS ck_typed_bonus_tool CHECK
 (bonus_tool_blob IS NULL OR (category_id=20 AND OCTET_LENGTH(bonus_tool_blob)=18));
UPDATE player.item_storage SET grid_columns=GREATEST(grid_columns,19) WHERE storage_kind=2;
