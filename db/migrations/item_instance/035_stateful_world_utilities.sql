-- Independent typed components; no legacy socket, practice or bait reuse.
ALTER TABLE player.item_instance
 ADD COLUMN IF NOT EXISTS recall_blob VARBINARY(17) NULL,
 ADD COLUMN IF NOT EXISTS tool_remaining_uses SMALLINT UNSIGNED NULL,
 ADD CONSTRAINT IF NOT EXISTS ck_typed_recall CHECK(recall_blob IS NULL OR (category_id=10 AND OCTET_LENGTH(recall_blob) IN(5,17))),
 ADD CONSTRAINT IF NOT EXISTS ck_typed_detector_uses CHECK(tool_remaining_uses IS NULL OR category_id=11);
