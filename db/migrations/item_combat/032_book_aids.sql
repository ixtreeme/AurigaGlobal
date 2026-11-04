-- Typed consumables that credit persistent book-use charges. No legacy affect IDs.
CREATE TABLE protos.book_aid_profile (
 profile_id INT UNSIGNED NOT NULL PRIMARY KEY,
 code VARCHAR(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL UNIQUE,
 aid_kind TINYINT UNSIGNED NOT NULL, units_per_use SMALLINT UNSIGNED NOT NULL,
 charges_per_use INT UNSIGNED NOT NULL, maximum_charges INT UNSIGNED NOT NULL,
 CHECK(profile_id>0 AND LENGTH(code)>0), CHECK(aid_kind BETWEEN 1 AND 2),
 CHECK(units_per_use>0 AND charges_per_use>0 AND charges_per_use<=maximum_charges)
) ENGINE=InnoDB;
ALTER TABLE protos.item_consumable DROP CONSTRAINT ck_consumable_one_effect,
 ADD book_aid_profile_id INT UNSIGNED NULL,
 ADD FOREIGN KEY(book_aid_profile_id) REFERENCES protos.book_aid_profile(profile_id),
 ADD CONSTRAINT ck_consumable_one_effect CHECK((recovery_profile_id IS NOT NULL)+(timed_effect_profile_id IS NOT NULL)+
  (toggle_effect_profile_id IS NOT NULL)+(socket_service_profile_id IS NOT NULL)+
  (attribute_service_profile_id IS NOT NULL)+(book_aid_profile_id IS NOT NULL)=1);
UPDATE protos.catalog_format SET schema_version=26,canonical_domain='auriga.item_catalog.v26' WHERE singleton=1;
