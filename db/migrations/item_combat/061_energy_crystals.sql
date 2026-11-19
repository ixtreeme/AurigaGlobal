-- Birth rolls are authored content, not client-supplied item sockets.
ALTER TABLE protos.timed_effect_profile DROP CONSTRAINT ck_timed_utility_kind,
 DROP CONSTRAINT ck_timed_utility_bonus,
 ADD CONSTRAINT ck_timed_utility_kind CHECK(utility_kind BETWEEN 0 AND 3),
 ADD CONSTRAINT ck_timed_utility_bonus CHECK
 ((utility_kind=0 AND bonus_profile_id IS NOT NULL) OR
  (utility_kind IN(1,2,3) AND bonus_profile_id IS NULL));
CREATE TABLE protos.energy_crystal_tier (
 profile_id INT UNSIGNED NOT NULL,percent TINYINT UNSIGNED NOT NULL,
 chance_bp SMALLINT UNSIGNED NOT NULL,
 PRIMARY KEY(profile_id,percent),
 FOREIGN KEY(profile_id) REFERENCES protos.timed_effect_profile(profile_id),
 CHECK(percent BETWEEN 1 AND 100 AND chance_bp BETWEEN 1 AND 10000)
) ENGINE=InnoDB;
UPDATE protos.catalog_format SET schema_version=53,canonical_domain='auriga.item_catalog.v53' WHERE singleton=1;
