-- Character services remain consumables with an explicit, native service rule.
-- The native publisher verifies exactly one behavior across this binding and
-- the profile references in item_consumable. SQL also forbids mixed profiles.
ALTER TABLE protos.item_consumable DROP CONSTRAINT ck_consumable_one_effect,
 ADD character_service BOOLEAN NOT NULL DEFAULT 0,
 ADD CONSTRAINT ck_consumable_character_flag CHECK(character_service IN(0,1)),
 ADD CONSTRAINT ck_consumable_one_effect CHECK((recovery_profile_id IS NOT NULL)+(timed_effect_profile_id IS NOT NULL)+
  (toggle_effect_profile_id IS NOT NULL)+(socket_service_profile_id IS NOT NULL)+
  (attribute_service_profile_id IS NOT NULL)+(book_aid_profile_id IS NOT NULL)+character_service=1);
CREATE TABLE protos.item_character_service (
 category_id SMALLINT UNSIGNED NOT NULL DEFAULT 5,
 local_id INT UNSIGNED NOT NULL,
 service_kind TINYINT UNSIGNED NOT NULL,
 units_per_use SMALLINT UNSIGNED NOT NULL,
 amount INT UNSIGNED NOT NULL,
 minimum_value INT UNSIGNED NOT NULL,
 maximum_value INT UNSIGNED NOT NULL,
 PRIMARY KEY(category_id,local_id),
 FOREIGN KEY(category_id,local_id) REFERENCES protos.item_consumable(category_id,local_id),
 CHECK(category_id=5 AND service_kind=1 AND units_per_use>0 AND amount>0 AND amount<=50000000 AND
       minimum_value<maximum_value AND maximum_value<=50000000)
) ENGINE=InnoDB;
UPDATE protos.catalog_format SET schema_version=41,canonical_domain='auriga.item_catalog.v41' WHERE singleton=1;
