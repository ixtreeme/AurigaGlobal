ALTER TABLE protos.item_character_service DROP CONSTRAINT ck_character_service,
 ADD CONSTRAINT ck_character_service CHECK(category_id=5 AND units_per_use>0 AND amount>0 AND amount<=50000000 AND
 minimum_value<maximum_value AND maximum_value<=50000000 AND
 (service_kind=1 OR (service_kind=2 AND amount=1 AND minimum_value=0 AND maximum_value=32767) OR
  (service_kind=3 AND amount BETWEEN 1 AND 6 AND minimum_value=0 AND maximum_value=1001 AND units_per_use=1)));
UPDATE protos.catalog_format SET schema_version=48,canonical_domain='auriga.item_catalog.v48' WHERE singleton=1;
