-- The original bGainSocketPct loop opens this many normal stone sockets.
-- It is a count, not a random chance. Keep the old captured metadata intact.
ALTER TABLE protos.item_definition_rules
 ADD birth_open_stone_sockets TINYINT UNSIGNED NOT NULL DEFAULT 0,
 ADD CONSTRAINT ck_birth_open_stone_sockets CHECK(birth_open_stone_sockets<=3);
UPDATE protos.catalog_format SET schema_version=42,canonical_domain='auriga.item_catalog.v42' WHERE singleton=1;
