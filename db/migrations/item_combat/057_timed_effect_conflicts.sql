ALTER TABLE protos.timed_effect_group
 ADD family_kind TINYINT UNSIGNED NOT NULL DEFAULT 0,
 ADD CONSTRAINT ck_timed_effect_family CHECK(family_kind BETWEEN 0 AND 3),
 ADD CONSTRAINT ck_fish_effect_group CHECK(family_kind=0 OR (reapply_kind=1 AND clock_kind=1 AND remove_on_death=1));
ALTER TABLE protos.timed_effect_profile
 ADD conflict_bonus_id INT UNSIGNED NULL,
 ADD FOREIGN KEY(conflict_bonus_id) REFERENCES protos.bonus_definition(bonus_id);
UPDATE protos.catalog_format SET schema_version=49,canonical_domain='auriga.item_catalog.v49' WHERE singleton=1;
