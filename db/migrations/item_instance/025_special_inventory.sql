-- C47: character special inventory (storage kind 11, one 9x4 storage with eight
-- pages per special type), eight-page bags and 9x5 account storage pages.
-- Apply with the game stopped. Idempotent; never moves or rewrites an item.
USE player;

-- Storage kind 11 belongs to a character (owner_kind 1).
SET @c47_storage_checks = (
 SELECT GROUP_CONCAT(CONCAT('DROP CONSTRAINT `',REPLACE(CONSTRAINT_NAME,'`','``'),'`') SEPARATOR ',')
 FROM information_schema.CHECK_CONSTRAINTS
 WHERE CONSTRAINT_SCHEMA='player' AND TABLE_NAME='item_storage' AND CONSTRAINT_NAME<>'storage_kind'
 AND LOWER(REPLACE(REPLACE(REPLACE(REPLACE(REPLACE(CHECK_CLAUSE,'`',''),' ',''),'(',''),')',''),'item_storage.','')) IN
 ('storage_kindbetween1and10','storage_kindbetween1and11',
  'storage_kindin1,2,6,10andowner_kind=1orstorage_kindin3,4andowner_kind=2orstorage_kind=5andowner_kind=3orstorage_kindin7,8,9',
  'storage_kindin1,2,6,10,11andowner_kind=1orstorage_kindin3,4andowner_kind=2orstorage_kind=5andowner_kind=3orstorage_kindin7,8,9')
);
SET @c47_storage_ddl = CONCAT('ALTER TABLE player.item_storage MODIFY COLUMN storage_kind TINYINT UNSIGNED NOT NULL CHECK (storage_kind BETWEEN 1 AND 11),',
 IF(@c47_storage_checks IS NULL,'',CONCAT(@c47_storage_checks,',')),
 'ADD CONSTRAINT ck_item_storage_ownership CHECK ((storage_kind IN (1,2,6,10,11) AND owner_kind=1) OR (storage_kind IN (3,4) AND owner_kind=2) OR (storage_kind=5 AND owner_kind=3) OR storage_kind IN (7,8,9))');
PREPARE c47_storage_upgrade FROM @c47_storage_ddl;
EXECUTE c47_storage_upgrade;
DEALLOCATE PREPARE c47_storage_upgrade;

START TRANSACTION;
CREATE TEMPORARY TABLE c47_touched (owner_kind TINYINT UNSIGNED NOT NULL, owner_id BIGINT UNSIGNED NOT NULL, PRIMARY KEY(owner_kind,owner_id));

-- Bags grow to eight 5x9 pages: existing anchors keep their meaning.
INSERT IGNORE INTO c47_touched SELECT owner_kind,owner_id FROM player.item_storage
 WHERE storage_kind=1 AND grid_columns=5 AND grid_rows=9 AND grid_pages<8;
UPDATE player.item_storage SET grid_pages=8 WHERE storage_kind=1 AND grid_columns=5 AND grid_rows=9 AND grid_pages<8;

-- Every native character receives the special inventory.
INSERT IGNORE INTO c47_touched SELECT DISTINCT s.owner_kind,s.owner_id FROM player.item_storage s
 WHERE s.storage_kind=1 AND s.owner_kind=1 AND NOT EXISTS (SELECT 1 FROM player.item_storage x
 WHERE x.owner_kind=s.owner_kind AND x.owner_id=s.owner_id AND x.storage_kind=11);
INSERT IGNORE INTO player.item_storage(owner_kind,owner_id,storage_kind,storage_id,grid_columns,grid_rows,grid_pages)
 SELECT DISTINCT owner_kind,owner_id,11,1,9,4,48 FROM player.item_storage WHERE storage_kind=1 AND owner_kind=1;

-- Bank and item-shop storage show 9x5 pages. A page keeps 45 cells, so a 1x1
-- item keeps its anchor and its single occupied cell; a storage that holds a
-- larger item keeps 5x9 (the client renders any layout).
INSERT IGNORE INTO c47_touched SELECT s.owner_kind,s.owner_id FROM player.item_storage s
 WHERE s.storage_kind IN (3,4) AND s.grid_columns=5 AND s.grid_rows=9 AND NOT EXISTS (
  SELECT 1 FROM player.item_instance i JOIN protos.item_base b ON b.category_id=i.category_id AND b.local_id=i.local_id
  WHERE i.owner_kind=s.owner_kind AND i.owner_id=s.owner_id AND i.storage_kind=s.storage_kind AND i.storage_id=s.storage_id
  AND i.destroyed=0 AND (b.inventory_width<>1 OR b.inventory_height<>1));
UPDATE player.item_storage s SET s.grid_columns=9, s.grid_rows=5
 WHERE s.storage_kind IN (3,4) AND s.grid_columns=5 AND s.grid_rows=9 AND NOT EXISTS (
  SELECT 1 FROM player.item_instance i JOIN protos.item_base b ON b.category_id=i.category_id AND b.local_id=i.local_id
  WHERE i.owner_kind=s.owner_kind AND i.owner_id=s.owner_id AND i.storage_kind=s.storage_kind AND i.storage_id=s.storage_id
  AND i.destroyed=0 AND (b.inventory_width<>1 OR b.inventory_height<>1));

-- Any loaded snapshot of a touched owner is now stale and must reload (CAS).
UPDATE player.item_owner_fence f JOIN c47_touched t ON t.owner_kind=f.owner_kind AND t.owner_id=f.owner_id
 SET f.state_revision=f.state_revision+1;
DROP TEMPORARY TABLE c47_touched;
COMMIT;
