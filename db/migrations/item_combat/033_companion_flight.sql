-- Explicit capability of a typed mount seal, independent of legacy items.
ALTER TABLE protos.item_companion
 ADD COLUMN IF NOT EXISTS flying_mount TINYINT UNSIGNED NOT NULL DEFAULT 0,
 ADD CONSTRAINT IF NOT EXISTS ck_typed_companion_flying CHECK
 (flying_mount IN (0,1) AND (flying_mount=0 OR companion_kind=2));
UPDATE protos.catalog_format SET schema_version=27,canonical_domain='auriga.item_catalog.v27' WHERE singleton=1;
