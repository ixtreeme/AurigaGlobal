-- Canonical native offers and immutable pre-cutover history.
-- Fresh installations require no legacy shop/proto tables.
CREATE TABLE IF NOT EXISTS player.ishop_offer (
 offer_id INT UNSIGNED NOT NULL PRIMARY KEY,
 display_category TINYINT UNSIGNED NOT NULL,
 display_subcategory TINYINT UNSIGNED NOT NULL,
 unit_price INT UNSIGNED NOT NULL,
 discount_percent TINYINT UNSIGNED NOT NULL CHECK(discount_percent<=100),
 offer_until DATETIME NULL,
 added_at DATETIME NULL,
 baseline_sales BIGINT UNSIGNED NOT NULL DEFAULT 0,
 weekly_limit INT UNSIGNED NOT NULL DEFAULT 0,
 monthly_limit INT UNSIGNED NOT NULL DEFAULT 0,
 history_scope_id INT UNSIGNED NOT NULL DEFAULT 0,
 package_title VARCHAR(128) NULL CHECK(package_title IS NULL OR CHAR_LENGTH(package_title)>0),
 enabled BOOLEAN NOT NULL DEFAULT TRUE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
CREATE TABLE IF NOT EXISTS player.ishop_offer_member (
 offer_id INT UNSIGNED NOT NULL,
 slot TINYINT UNSIGNED NOT NULL CHECK(slot BETWEEN 1 AND 6),
 category_id SMALLINT UNSIGNED NOT NULL CHECK(category_id>0),
 local_id INT UNSIGNED NOT NULL CHECK(local_id>0),
 refinement_level SMALLINT UNSIGNED NOT NULL CHECK(refinement_level<=255),
 quantity SMALLINT UNSIGNED NOT NULL CHECK(quantity BETWEEN 1 AND 128),
 PRIMARY KEY(offer_id,slot),
 UNIQUE KEY ix_native_offer_key(offer_id,category_id,local_id,refinement_level),
 FOREIGN KEY(offer_id) REFERENCES player.ishop_offer(offer_id) ON DELETE CASCADE
) ENGINE=InnoDB;
CREATE TABLE IF NOT EXISTS player.ishop_purchase_archive (
 archive_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
 account_id INT UNSIGNED NOT NULL,
 bought_at DATETIME NOT NULL,
 units INT NOT NULL,
 paid BIGINT UNSIGNED NOT NULL,
 history_scope_id INT UNSIGNED NOT NULL DEFAULT 0,
 category_id SMALLINT UNSIGNED NOT NULL DEFAULT 0,
 local_id INT UNSIGNED NOT NULL DEFAULT 0,
 refinement_level SMALLINT UNSIGNED NOT NULL DEFAULT 0,
 KEY ix_native_archive_history(account_id,bought_at,archive_id),
 KEY ix_native_archive_limit(account_id,history_scope_id,bought_at)
) ENGINE=InnoDB;
-- Import receipt is operational metadata, never a runtime item-ID map.
CREATE TABLE IF NOT EXISTS player.ishop_offer_import (
 source VARCHAR(64) NOT NULL PRIMARY KEY,
 completed_at DATETIME NOT NULL,
 source_hash BINARY(32) NOT NULL,
 offers INT UNSIGNED NOT NULL,
 members INT UNSIGNED NOT NULL,
 history_rows BIGINT UNSIGNED NOT NULL
) ENGINE=InnoDB;
