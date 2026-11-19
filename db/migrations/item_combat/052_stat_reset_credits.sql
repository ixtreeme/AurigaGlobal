-- One original blood pill grants one reusable stat-minus credit. The signed
-- SMALLINT in the existing player table is the authoritative storage bound.
ALTER TABLE protos.item_character_service DROP CONSTRAINT CONSTRAINT_1,
 ADD CONSTRAINT ck_character_service CHECK(category_id=5 AND units_per_use>0 AND amount>0 AND amount<=50000000 AND
 minimum_value<maximum_value AND maximum_value<=50000000 AND
 (service_kind=1 OR (service_kind=2 AND amount=1 AND minimum_value=0 AND maximum_value=32767)));
UPDATE protos.catalog_format SET schema_version=44,canonical_domain='auriga.item_catalog.v44' WHERE singleton=1;
