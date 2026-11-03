-- C22: install on the local player schema before deploying Game/Database together.
ALTER TABLE player.item_instance ADD COLUMN IF NOT EXISTS book_skill_id SMALLINT UNSIGNED NULL;
CREATE TABLE IF NOT EXISTS player.item_book_learning (
 owner_kind TINYINT UNSIGNED NOT NULL,
 owner_id BIGINT UNSIGNED NOT NULL,
 state_revision BIGINT UNSIGNED NOT NULL,
 state_blob VARBINARY(8192) NOT NULL,
 state_sha256 BINARY(32) NOT NULL,
 last_operation BINARY(16) NOT NULL,
 PRIMARY KEY(owner_kind,owner_id),
 CONSTRAINT item_book_owner_kind CHECK(owner_kind=1)
) ENGINE=InnoDB;
ALTER TABLE player.player ADD COLUMN IF NOT EXISTS typed_book_revision BIGINT UNSIGNED NOT NULL DEFAULT 0;
ALTER TABLE player.player_deleted ADD COLUMN IF NOT EXISTS typed_book_revision BIGINT UNSIGNED NOT NULL DEFAULT 0;
