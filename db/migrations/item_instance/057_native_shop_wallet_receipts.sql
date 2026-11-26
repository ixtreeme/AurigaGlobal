-- Fresh native installation: external-credit inbox ledger and atomic purchase receipts.
-- No old offer, prototype, package-conversion or item-ID mapping table is needed.
CREATE TABLE IF NOT EXISTS player.item_wallet_legacy_sweep (
 sweep_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
 account_id INT UNSIGNED NOT NULL CHECK(account_id>0),
 amount BIGINT UNSIGNED NOT NULL CHECK(amount>0 AND amount<=9223372036854775807),
 swept_at DATETIME NOT NULL,
 KEY ix_wallet_sweep_account(account_id,sweep_id)
) ENGINE=InnoDB;


CREATE TABLE IF NOT EXISTS player.ishop_native_purchase (
    operation_id BINARY(16) NOT NULL PRIMARY KEY,
    account_id INT UNSIGNED NOT NULL,
    shop_id INT UNSIGNED NOT NULL,
    units SMALLINT UNSIGNED NOT NULL CHECK(units BETWEEN 1 AND 20),
    paid BIGINT UNSIGNED NOT NULL,
    bought_at DATETIME NOT NULL,
    purchase_payload VARBINARY(1024) NOT NULL,
    KEY ix_ishop_native_history(account_id,bought_at,operation_id),
    KEY ix_ishop_native_limit(account_id,shop_id,bought_at),
    KEY ix_ishop_native_sales(shop_id),
    FOREIGN KEY(operation_id) REFERENCES player.item_operation_receipt(operation_id) ON DELETE CASCADE
) ENGINE=InnoDB;
