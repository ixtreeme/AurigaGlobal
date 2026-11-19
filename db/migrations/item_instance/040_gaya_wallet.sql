-- Character Gaya uses the same authoritative, fenced wallet writer as Yang.
-- Never import a nonzero legacy balance implicitly.
ALTER TABLE player.player ADD COLUMN IF NOT EXISTS gaya INT UNSIGNED NOT NULL DEFAULT 0;
ALTER TABLE player.player_deleted ADD COLUMN IF NOT EXISTS gaya INT UNSIGNED NOT NULL DEFAULT 0;
SET @gaya_owner_check = (SELECT IF(COUNT(*)=1,MIN(CONSTRAINT_NAME),NULL)
 FROM information_schema.CHECK_CONSTRAINTS
 WHERE CONSTRAINT_SCHEMA='player' AND TABLE_NAME='item_wallet' AND LOWER(CHECK_CLAUSE) LIKE '%currency%');
SET @gaya_wallet_ddl = CONCAT('ALTER TABLE player.item_wallet DROP CONSTRAINT `',
 REPLACE(@gaya_owner_check,'`','``'),
 '`, ADD CONSTRAINT item_wallet_owner_currency CHECK (((currency IN (1,3) AND owner_kind=1) OR (currency=2 AND owner_kind=2)) AND (currency<>3 OR balance<=999998))');
PREPARE gaya_wallet_change FROM @gaya_wallet_ddl;
EXECUTE gaya_wallet_change;
DEALLOCATE PREPARE gaya_wallet_change;
ALTER TABLE player.item_persistence_format
 ADD COLUMN gaya_wallet_version TINYINT UNSIGNED NOT NULL DEFAULT 1 CHECK (gaya_wallet_version=1);
