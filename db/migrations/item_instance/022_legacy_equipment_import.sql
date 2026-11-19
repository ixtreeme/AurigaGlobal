-- Offline cutover only. A committed receipt means the whole owner import
-- (fence, wallet, all item batches, archive, source deletion) committed together.
CREATE TABLE IF NOT EXISTS player.item_legacy_import (
 import_id BINARY(16) NOT NULL PRIMARY KEY,
 owner_kind TINYINT UNSIGNED NOT NULL CHECK(owner_kind=1),
 owner_id BIGINT UNSIGNED NOT NULL,
 source_checkpoint_sha256 BINARY(32) NOT NULL,
 request_sha256 BINARY(32) NOT NULL,
 catalog_sha256 BINARY(32) NOT NULL,
 item_count INT UNSIGNED NOT NULL,
 total_units BIGINT UNSIGNED NOT NULL,
 UNIQUE KEY ux_legacy_import_owner(owner_kind,owner_id)
) ENGINE=InnoDB;
CREATE TABLE IF NOT EXISTS player.item_legacy_archive (
 import_id BINARY(16) NOT NULL,
 legacy_instance_id INT UNSIGNED NOT NULL,
 native_instance_id BIGINT UNSIGNED NOT NULL,
 source_row MEDIUMBLOB NOT NULL,
 PRIMARY KEY(import_id,legacy_instance_id),
 UNIQUE KEY ux_legacy_native_instance(native_instance_id),
 FOREIGN KEY(import_id) REFERENCES player.item_legacy_import(import_id)
) ENGINE=InnoDB;
-- Audit identities deliberately outlive item destruction/character deletion.
