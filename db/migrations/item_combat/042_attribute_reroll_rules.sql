-- Preserve the source's separate ordinary/zodiac switcher admission.
ALTER TABLE protos.item_definition_rules
 ADD COLUMN attribute_reroll_group TINYINT UNSIGNED NOT NULL DEFAULT 1,
 ADD CONSTRAINT ck_definition_reroll_group CHECK(attribute_reroll_group IN (1,2));
ALTER TABLE protos.attribute_service_profile
 ADD COLUMN target_reroll_group TINYINT UNSIGNED NOT NULL DEFAULT 0,
 ADD CONSTRAINT ck_service_reroll_group CHECK(target_reroll_group<=2 AND (target_reroll_group=0 OR (operation_kind=2 AND pool_kind=1)));
UPDATE protos.catalog_format SET schema_version=34,canonical_domain='auriga.item_catalog.v34' WHERE singleton=1;
