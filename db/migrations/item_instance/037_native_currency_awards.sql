ALTER TABLE player.item_instance
 ADD currency_amount BIGINT UNSIGNED NULL,
 ADD CONSTRAINT ck_item_currency_amount CHECK
 (currency_amount IS NULL OR (category_id=16 AND currency_amount BETWEEN 1 AND 9223372036854775807));

-- Written by the Database service from trusted guild settlement/refund facts.
-- The GameServer can claim a row only with the matching native pouch grant.
CREATE TABLE player.item_currency_award (
 id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
 login VARCHAR(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 amount BIGINT UNSIGNED NOT NULL,
 reason_kind TINYINT UNSIGNED NOT NULL,
 created_at DATETIME(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6),
 claimed_owner_id BIGINT UNSIGNED NULL,
 claimed_operation BINARY(16) NULL,
 claimed_at DATETIME(6) NULL,
 UNIQUE(claimed_operation),INDEX ix_currency_award_pending(login,claimed_at,id),
 FOREIGN KEY(claimed_operation) REFERENCES player.item_operation_receipt(operation_id),
 CHECK(LENGTH(login)>0 AND BINARY login=BINARY LOWER(TRIM(login))),
 CHECK(amount BETWEEN 1 AND 9223372036854775807 AND reason_kind BETWEEN 1 AND 4),
 CHECK((claimed_owner_id IS NULL AND claimed_operation IS NULL AND claimed_at IS NULL) OR
       (claimed_owner_id>0 AND claimed_operation IS NOT NULL AND claimed_at IS NOT NULL))
) ENGINE=InnoDB;

-- The debit and recoverable publication intent commit together. The current
-- core can publish after a lost callback/logout; a pickup claims this row in
-- the same transaction as the receiving wallet credit.
CREATE TABLE player.item_currency_ground (
 operation_id BINARY(16) NOT NULL PRIMARY KEY,
 core_key VARBINARY(64) NOT NULL,run_generation BIGINT UNSIGNED NOT NULL,
 owner_id BIGINT UNSIGNED NOT NULL,amount BIGINT UNSIGNED NOT NULL,
 map_index INT NOT NULL,x INT NOT NULL,y INT NOT NULL,z INT NOT NULL,
 created_at_ms BIGINT UNSIGNED NOT NULL,expires_at_ms BIGINT UNSIGNED NOT NULL,
 claimed_operation BINARY(16) NULL,claimed_at DATETIME(6) NULL,
 UNIQUE(claimed_operation),INDEX ix_currency_ground_core(core_key,run_generation,expires_at_ms),
 FOREIGN KEY(operation_id) REFERENCES player.item_operation_receipt(operation_id),
 FOREIGN KEY(claimed_operation) REFERENCES player.item_operation_receipt(operation_id),
 CHECK(run_generation>0 AND owner_id BETWEEN 1 AND 4294967295 AND amount BETWEEN 1 AND 9223372036854775807 AND map_index>0),
 CHECK(created_at_ms>0 AND expires_at_ms>created_at_ms AND expires_at_ms-created_at_ms<=300000 AND expires_at_ms<=9223372036854775807),
 CHECK((claimed_operation IS NULL AND claimed_at IS NULL) OR (claimed_operation IS NOT NULL AND claimed_at IS NOT NULL))
) ENGINE=InnoDB;
