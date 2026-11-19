-- Item rewards carry an explicit native refinement state throughout opening.
ALTER TABLE protos.loot_entry
 ADD COLUMN refinement_level SMALLINT UNSIGNED NOT NULL DEFAULT 0,
 ADD CONSTRAINT ck_loot_refinement_level CHECK(refinement_level<=255 AND (reward_kind=1 OR refinement_level=0));
UPDATE protos.catalog_format SET schema_version=33,canonical_domain='auriga.item_catalog.v33' WHERE singleton=1;
