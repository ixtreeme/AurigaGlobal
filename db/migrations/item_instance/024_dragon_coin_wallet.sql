-- C45: Sárkányérme lives in the native account wallet (player.item_wallet,
-- owner_kind 2, currency 2). account.account.coins is only an inbox for legacy
-- and external top-ups: the game moves it into the wallet in one transaction
-- (SRC/Server/common/DragonCoinWallet.h) and writes one row here per move.
CREATE TABLE IF NOT EXISTS player.item_wallet_legacy_sweep (
 sweep_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
 account_id INT UNSIGNED NOT NULL CHECK(account_id>0),
 amount BIGINT UNSIGNED NOT NULL CHECK(amount>0 AND amount<=9223372036854775807),
 swept_at DATETIME NOT NULL,
 KEY ix_wallet_sweep_account(account_id,sweep_id)
) ENGINE=InnoDB;

-- One-time conversion of legacy item shop rows (player.ishop_data) to native
-- items, written by SRC/Tools/ItemCatalogBuilder/convert_item_shop.py. A shop
-- row without a line here, or whose itemVnum no longer equals legacy_vnum, is
-- not sold to characters on the native item system (no coins are taken).
CREATE TABLE IF NOT EXISTS player.ishop_native_item (
 shop_id INT UNSIGNED NOT NULL PRIMARY KEY,
 legacy_vnum INT UNSIGNED NOT NULL,
 category_id SMALLINT UNSIGNED NOT NULL,
 local_id INT UNSIGNED NOT NULL,
 refinement_level SMALLINT UNSIGNED NOT NULL DEFAULT 0 CHECK(refinement_level<=255),
 source VARCHAR(16) NOT NULL CHECK(source IN ('compiled','archive','review','mapping')),
 KEY ix_ishop_native_key(category_id,local_id)
) ENGINE=InnoDB;
