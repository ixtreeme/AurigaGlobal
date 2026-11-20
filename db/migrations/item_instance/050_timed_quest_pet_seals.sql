-- Preserve the clock on an unredeemed quest seal using the existing instance lifetime blob.
ALTER TABLE player.item_instance
 DROP CONSTRAINT IF EXISTS ck_typed_costume_lifetime,
 ADD CONSTRAINT IF NOT EXISTS ck_typed_item_lifetime CHECK
 (costume_lifetime_blob IS NULL OR (category_id IN (3,4,10,19) AND OCTET_LENGTH(costume_lifetime_blob)=22));
