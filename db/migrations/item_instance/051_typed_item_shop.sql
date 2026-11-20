-- Offer IDs still identify merchandising rows. Package contents are native grants.
CREATE TABLE IF NOT EXISTS player.ishop_native_package (
    shop_id INT UNSIGNED NOT NULL PRIMARY KEY,
    title VARCHAR(128) NOT NULL CHECK(CHAR_LENGTH(title)>0)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS player.ishop_native_package_member (
    shop_id INT UNSIGNED NOT NULL,
    slot TINYINT UNSIGNED NOT NULL CHECK(slot BETWEEN 1 AND 6),
    category_id SMALLINT UNSIGNED NOT NULL CHECK(category_id>0),
    local_id INT UNSIGNED NOT NULL CHECK(local_id>0),
    refinement_level SMALLINT UNSIGNED NOT NULL DEFAULT 0 CHECK(refinement_level<=255),
    quantity SMALLINT UNSIGNED NOT NULL CHECK(quantity BETWEEN 1 AND 128),
    PRIMARY KEY(shop_id,slot),
    UNIQUE KEY ix_ishop_package_key(shop_id,category_id,local_id,refinement_level),
    FOREIGN KEY(shop_id) REFERENCES player.ishop_native_package(shop_id) ON DELETE CASCADE
) ENGINE=InnoDB;

-- An indexed projection of the purchase receipt, written with the wallet and inventory.
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
