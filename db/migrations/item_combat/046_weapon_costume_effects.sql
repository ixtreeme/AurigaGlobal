ALTER TABLE protos.item_costume
 ADD COLUMN weapon_sword_effect_id SMALLINT UNSIGNED NOT NULL DEFAULT 0,
 ADD COLUMN weapon_dagger_effect_id SMALLINT UNSIGNED NOT NULL DEFAULT 0,
 ADD COLUMN weapon_bow_effect_id SMALLINT UNSIGNED NOT NULL DEFAULT 0,
 ADD COLUMN weapon_two_handed_effect_id SMALLINT UNSIGNED NOT NULL DEFAULT 0,
 ADD COLUMN weapon_bell_effect_id SMALLINT UNSIGNED NOT NULL DEFAULT 0,
 ADD COLUMN weapon_fan_effect_id SMALLINT UNSIGNED NOT NULL DEFAULT 0,
 ADD COLUMN weapon_dagger_alternate_effect_id SMALLINT UNSIGNED NOT NULL DEFAULT 0,
 ADD COLUMN weapon_fan_alternate_effect_id SMALLINT UNSIGNED NOT NULL DEFAULT 0,
 DROP CONSTRAINT ck_costume_kind,
 DROP CONSTRAINT ck_costume_companion_form,
 ADD CONSTRAINT ck_costume_kind CHECK(costume_kind BETWEEN 1 AND 7),
 ADD CONSTRAINT ck_costume_companion_form CHECK((costume_kind IN(4,5) AND companion_form_id IS NOT NULL AND companion_form_id>0)
     OR (costume_kind IN(1,2,3,6,7) AND companion_form_id IS NULL)),
 ADD CONSTRAINT ck_costume_weapon_effects CHECK(
     (costume_kind=7 AND weapon_sword_effect_id>0 AND weapon_dagger_effect_id>0 AND weapon_bow_effect_id>0 AND weapon_two_handed_effect_id>0 AND weapon_bell_effect_id>0 AND weapon_fan_effect_id>0 AND weapon_dagger_alternate_effect_id>0 AND weapon_fan_alternate_effect_id>0)
     OR (costume_kind<>7 AND weapon_sword_effect_id=0 AND weapon_dagger_effect_id=0 AND weapon_bow_effect_id=0 AND weapon_two_handed_effect_id=0 AND weapon_bell_effect_id=0 AND weapon_fan_effect_id=0 AND weapon_dagger_alternate_effect_id=0 AND weapon_fan_alternate_effect_id=0));
ALTER TABLE protos.item_definition_rules ADD COLUMN weapon_effect_alternate BOOLEAN NOT NULL DEFAULT 0,
 ADD CONSTRAINT ck_definition_weapon_effect CHECK(weapon_effect_alternate IN(0,1) AND (weapon_effect_alternate=0 OR category_id=1));
UPDATE protos.catalog_format SET schema_version=38,canonical_domain='auriga.item_catalog.v38' WHERE singleton=1;
