-- Online player shops reserve the original inventory cells. Every listing
-- lock is part of the concrete native instance and its optimistic state hash.
CREATE TABLE player.item_private_shop (
 owner_kind TINYINT UNSIGNED NOT NULL DEFAULT 1,
 owner_id BIGINT UNSIGNED NOT NULL PRIMARY KEY,
 session_generation BIGINT UNSIGNED NOT NULL,
 operation_id BINARY(16) NOT NULL UNIQUE,
 sign VARCHAR(32) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
 remember_prices BOOLEAN NOT NULL,
 created_at_ms BIGINT UNSIGNED NOT NULL,
 created_at DATETIME(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6),
 FOREIGN KEY(owner_kind,owner_id) REFERENCES player.item_owner_fence(owner_kind,owner_id) ON DELETE CASCADE,
 FOREIGN KEY(operation_id) REFERENCES player.item_operation_receipt(operation_id),
 CHECK(owner_kind=1 AND owner_id BETWEEN 1 AND 4294967295 AND session_generation>0),
 CHECK(LENGTH(sign) BETWEEN 1 AND 32 AND remember_prices IN(0,1)),
 CHECK(created_at_ms BETWEEN 1 AND 9223372036854775807)
) ENGINE=InnoDB;

CREATE TABLE player.item_private_shop_price (
 owner_kind TINYINT UNSIGNED NOT NULL DEFAULT 1,
 owner_id BIGINT UNSIGNED NOT NULL,
 category_id SMALLINT UNSIGNED NOT NULL,
 local_id INT UNSIGNED NOT NULL,
 refinement_level SMALLINT UNSIGNED NOT NULL,
 unit_price BIGINT UNSIGNED NOT NULL,
 updated_at DATETIME(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6),
 PRIMARY KEY(owner_id,category_id,local_id,refinement_level),
 FOREIGN KEY(owner_kind,owner_id) REFERENCES player.item_owner_fence(owner_kind,owner_id) ON DELETE CASCADE,
 FOREIGN KEY(category_id,local_id) REFERENCES protos.item_base(category_id,local_id),
 CHECK(owner_kind=1 AND owner_id BETWEEN 1 AND 4294967295 AND refinement_level<=255),
 CHECK(unit_price<=9223372036854775807)
) ENGINE=InnoDB;

ALTER TABLE player.item_instance
 ADD shop_permit_deadline_ms BIGINT UNSIGNED NULL,
 ADD private_shop_operation BINARY(16) NULL,
 ADD private_shop_price BIGINT UNSIGNED NULL,
 ADD private_shop_cell SMALLINT UNSIGNED NULL,
 ADD UNIQUE KEY ux_private_shop_cell(private_shop_operation,private_shop_cell),
 ADD CONSTRAINT fk_item_private_shop FOREIGN KEY(private_shop_operation) REFERENCES player.item_private_shop(operation_id),
 ADD CONSTRAINT ck_shop_permit_deadline CHECK
 (shop_permit_deadline_ms IS NULL OR (category_id=10 AND shop_permit_deadline_ms BETWEEN 1 AND 9223372036854775807)),
 ADD CONSTRAINT ck_private_shop_listing CHECK
 ((private_shop_operation IS NULL AND private_shop_price IS NULL AND private_shop_cell IS NULL) OR
  (private_shop_operation IS NOT NULL AND private_shop_price IS NOT NULL AND private_shop_cell IS NOT NULL AND private_shop_price BETWEEN 1 AND 9223372036854775807 AND
   private_shop_cell BETWEEN 0 AND 119 AND owner_kind=1 AND storage_kind=1 AND destroyed=0 AND bound_kind IS NULL));
