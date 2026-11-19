-- duration_ms=0 explicitly represents a permanent toggle, with zero stored time.
SET @toggle_duration_constraint=(SELECT CONSTRAINT_NAME FROM information_schema.CHECK_CONSTRAINTS
 WHERE CONSTRAINT_SCHEMA='protos' AND TABLE_NAME='toggle_effect_profile' AND CHECK_CLAUSE LIKE '%duration_ms%');
SET @toggle_duration_ddl=CONCAT('ALTER TABLE protos.toggle_effect_profile DROP CONSTRAINT `',
 REPLACE(@toggle_duration_constraint,'`','``'),'`, ADD CONSTRAINT ck_toggle_duration CHECK(duration_ms BETWEEN 0 AND 9223372036854775807)');
PREPARE toggle_duration_statement FROM @toggle_duration_ddl;
EXECUTE toggle_duration_statement;
DEALLOCATE PREPARE toggle_duration_statement;
UPDATE protos.catalog_format SET schema_version=39,canonical_domain='auriga.item_catalog.v39' WHERE singleton=1;
