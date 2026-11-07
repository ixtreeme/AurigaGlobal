-- Full carrier/cosmetic instance state. Existing NULL payloads and digests stay unchanged.
-- Runtime accounts additionally need INSERT,DELETE on item_storage. The
-- native writer only creates 1x1 PetCarrier storage and deletes it on unpack;
-- ordinary bag/equipment dimensions still cannot be updated by that account.
-- Dedicated runtime writers retain SELECT,UPDATE (not INSERT,DELETE) on
-- item_owner_fence. Owner provisioning remains an administrative operation.
-- Apply this player-schema migration with player as the active database.
USE player;
ALTER TABLE player.item_instance
 ADD COLUMN IF NOT EXISTS pet_supply_blob VARBINARY(128) NULL
 CHECK (pet_supply_blob IS NULL OR (category_id=17 AND OCTET_LENGTH(pet_supply_blob)>0));

-- The inline column check changes with MODIFY COLUMN, not DROP CONSTRAINT.
-- Select only the exact old/new ownership clauses and an optional named kind
-- check, preserving every unrelated dimension or ownership guard.
SET @pet_storage_checks = (
 SELECT GROUP_CONCAT(CONCAT('DROP CONSTRAINT `',REPLACE(CONSTRAINT_NAME,'`','``'),'`') SEPARATOR ',')
 FROM information_schema.CHECK_CONSTRAINTS
 WHERE CONSTRAINT_SCHEMA='player' AND TABLE_NAME='item_storage' AND CONSTRAINT_NAME<>'storage_kind'
 AND LOWER(REPLACE(REPLACE(REPLACE(REPLACE(REPLACE(CHECK_CLAUSE,'`',''),' ',''),'(',''),')',''),'item_storage.','')) IN
 ('storage_kindbetween1and9','storage_kindbetween1and10',
  'storage_kindin1,2,6andowner_kind=1orstorage_kindin3,4andowner_kind=2orstorage_kind=5andowner_kind=3orstorage_kindin7,8,9',
  'storage_kindin1,2,6,10andowner_kind=1orstorage_kindin3,4andowner_kind=2orstorage_kind=5andowner_kind=3orstorage_kindin7,8,9')
);
SET @pet_storage_ddl = CONCAT('ALTER TABLE player.item_storage MODIFY COLUMN storage_kind TINYINT UNSIGNED NOT NULL CHECK (storage_kind BETWEEN 1 AND 10),',
 IF(@pet_storage_checks IS NULL,'',CONCAT(@pet_storage_checks,',')),
 'ADD CONSTRAINT ck_item_storage_ownership CHECK ((storage_kind IN (1,2,6,10) AND owner_kind=1) OR (storage_kind IN (3,4) AND owner_kind=2) OR (storage_kind=5 AND owner_kind=3) OR storage_kind IN (7,8,9))');
PREPARE pet_storage_upgrade FROM @pet_storage_ddl;
EXECUTE pet_storage_upgrade;
DEALLOCATE PREPARE pet_storage_upgrade;
