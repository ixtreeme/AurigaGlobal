-- Receipt and immutable source data for the offline legacy collection import.
-- Retain the receipt if the native seal is moved, consumed or destroyed.
CREATE TABLE IF NOT EXISTS player.item_legacy_collection_import (
    source_store TINYINT UNSIGNED NOT NULL CHECK (source_store IN (1, 2)),
    legacy_instance_id BIGINT UNSIGNED NOT NULL,
    account_id BIGINT UNSIGNED NOT NULL,
    native_instance_id BIGINT UNSIGNED NOT NULL,
    source_checkpoint_sha256 BINARY(32) NOT NULL,
    source_row_sha256 BINARY(32) NOT NULL,
    source_row MEDIUMBLOB NOT NULL,
    PRIMARY KEY (source_store, legacy_instance_id),
    UNIQUE KEY ux_collection_native_instance (native_instance_id)
) ENGINE=InnoDB;
