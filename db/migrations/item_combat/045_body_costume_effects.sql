ALTER TABLE protos.item_costume
 ADD COLUMN body_effect_id SMALLINT UNSIGNED NOT NULL DEFAULT 0,
 DROP CONSTRAINT ck_costume_kind,
 DROP CONSTRAINT ck_costume_companion_form,
 ADD CONSTRAINT ck_costume_kind CHECK(costume_kind BETWEEN 1 AND 6),
 ADD CONSTRAINT ck_costume_companion_form CHECK((costume_kind IN(4,5) AND companion_form_id IS NOT NULL AND companion_form_id>0)
     OR (costume_kind IN(1,2,3,6) AND companion_form_id IS NULL)),
 ADD CONSTRAINT ck_costume_body_effect CHECK((costume_kind=6 AND body_effect_id>0) OR (costume_kind<>6 AND body_effect_id=0));
UPDATE protos.catalog_format SET schema_version=37,canonical_domain='auriga.item_catalog.v37' WHERE singleton=1;
