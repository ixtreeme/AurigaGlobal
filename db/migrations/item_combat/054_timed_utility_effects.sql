-- Utilities are explicit, timed non-stat behavior. No placeholder bonus rows.
ALTER TABLE protos.timed_effect_profile
 MODIFY bonus_profile_id INT UNSIGNED NULL,
 ADD utility_kind TINYINT UNSIGNED NOT NULL DEFAULT 0,
 ADD CONSTRAINT ck_timed_utility_kind CHECK(utility_kind BETWEEN 0 AND 2),
 ADD CONSTRAINT ck_timed_utility_bonus CHECK
 ((utility_kind=0 AND bonus_profile_id IS NOT NULL) OR
  (utility_kind IN (1,2) AND bonus_profile_id IS NULL));
UPDATE protos.catalog_format SET schema_version=46,canonical_domain='auriga.item_catalog.v46' WHERE singleton=1;
