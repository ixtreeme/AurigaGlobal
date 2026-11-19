-- The existing 22-byte wearable clock also belongs to finite six-bonus stoles.
-- No instance state is rebuilt or refilled. The native validator distinguishes
-- C19 stoles from runes and requires the exact authored clock policy.
ALTER TABLE player.item_instance
 DROP CONSTRAINT IF EXISTS ck_typed_costume_lifetime;
ALTER TABLE player.item_instance
 ADD CONSTRAINT ck_typed_costume_lifetime CHECK
 (costume_lifetime_blob IS NULL OR (category_id IN (4,19) AND OCTET_LENGTH(costume_lifetime_blob)=22));
