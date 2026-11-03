-- Additive typed recovery state. Apply only to a verified, stopped native target.
-- The owner fence and receipt make item debit + schedule one transaction.
CREATE TABLE player.item_recovery_schedule (
    owner_kind TINYINT UNSIGNED NOT NULL CHECK (owner_kind=1),
    owner_id BIGINT UNSIGNED NOT NULL CHECK (owner_id>0),
    state_revision BIGINT UNSIGNED NOT NULL CHECK (state_revision>0),
    state_blob VARBINARY(4096) NOT NULL,
    state_sha256 BINARY(32) NOT NULL,
    last_operation BINARY(16) NOT NULL,
    PRIMARY KEY (owner_kind,owner_id),
    FOREIGN KEY (owner_kind,owner_id) REFERENCES player.item_owner_fence(owner_kind,owner_id),
    FOREIGN KEY (last_operation) REFERENCES player.item_operation_receipt(operation_id)
) ENGINE=InnoDB;
