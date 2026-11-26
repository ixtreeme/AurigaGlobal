-- Explicit construction recipes. NULL is unconverted and fails startup.
ALTER TABLE player.object_proto ADD COLUMN IF NOT EXISTS native_materials JSON NULL
    CHECK(native_materials IS NULL OR JSON_TYPE(native_materials)='ARRAY');
CREATE TABLE IF NOT EXISTS player.guild_building_material_import (
 source VARCHAR(64) NOT NULL PRIMARY KEY,
 source_hash BINARY(32) NOT NULL,
 recipes INT UNSIGNED NOT NULL,
 completed_at DATETIME NOT NULL
) ENGINE=InnoDB;
