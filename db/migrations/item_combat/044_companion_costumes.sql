-- Worn skins grant their own bonuses even without an active companion.
-- Their form overrides the normal pet/mount only while the skin is worn.
ALTER TABLE protos.item_costume
 ADD COLUMN companion_form_id INT UNSIGNED NULL,
 DROP CONSTRAINT CONSTRAINT_3,
 DROP CONSTRAINT CONSTRAINT_4,
 ADD CONSTRAINT ck_costume_kind CHECK(costume_kind IN(1,2,3,4,5)),
 ADD CONSTRAINT ck_costume_weapon CHECK((costume_kind=3 AND weapon_subtype IS NOT NULL AND weapon_subtype BETWEEN 1 AND 7)
     OR (costume_kind<>3 AND weapon_subtype IS NULL)),
 ADD CONSTRAINT fk_costume_companion_form FOREIGN KEY(companion_form_id) REFERENCES protos.companion_form(form_id),
 ADD CONSTRAINT ck_costume_companion_form CHECK((costume_kind IN(4,5) AND companion_form_id IS NOT NULL AND companion_form_id>0)
     OR (costume_kind IN(1,2,3) AND companion_form_id IS NULL));
UPDATE protos.catalog_format SET schema_version=36,canonical_domain='auriga.item_catalog.v36' WHERE singleton=1;
