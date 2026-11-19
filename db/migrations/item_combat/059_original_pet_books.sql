-- Appended explicit character-point pet effects. Existing enum identities are unchanged.
ALTER TABLE protos.pet_skill_definition DROP CONSTRAINT CONSTRAINT_2,
 ADD CONSTRAINT ck_pet_skill_effect CHECK(effect_kind BETWEEN 1 AND 30);
UPDATE protos.catalog_format SET schema_version=51,canonical_domain='auriga.item_catalog.v51' WHERE singleton=1;
