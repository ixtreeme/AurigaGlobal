-- Finite unique accessories share the existing instance-owned 22-byte clock.
-- Native catalog validation restricts C3 clocks to the two unique wear slots.
-- Existing issued clocks and permanent accessories are preserved unchanged.
ALTER TABLE player.item_instance
 DROP CONSTRAINT IF EXISTS ck_typed_costume_lifetime;
ALTER TABLE player.item_instance
 ADD CONSTRAINT ck_typed_costume_lifetime CHECK
 (costume_lifetime_blob IS NULL OR (category_id IN (3,4,19) AND OCTET_LENGTH(costume_lifetime_blob)=22));
