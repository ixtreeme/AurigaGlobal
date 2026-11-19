-- Exact state receipt survives live item deletion and replay after gameplay.
CREATE TABLE IF NOT EXISTS player.item_legacy_owner_state (
 import_id BINARY(16) NOT NULL PRIMARY KEY,
 source_state MEDIUMBLOB NOT NULL,
 FOREIGN KEY(import_id) REFERENCES player.item_legacy_import(import_id)
) ENGINE=InnoDB;
