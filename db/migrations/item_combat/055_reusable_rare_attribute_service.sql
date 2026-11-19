-- A reusable original rare reroller changes the target, never the source stack.
-- Keep the zero-debit exception constrained to the audited operation family.
ALTER TABLE protos.attribute_service_profile DROP CONSTRAINT ck_full_attribute_target;
ALTER TABLE protos.attribute_service_profile
 ADD CONSTRAINT ck_full_attribute_target CHECK(
  success_bp BETWEEN 1 AND 10000 AND target_mask BETWEEN 1 AND 31 AND
  (units_per_use>0 OR (operation_kind=2 AND pool_kind=2 AND target_mask=7 AND minimum_count>=1 AND success_bp=10000 AND consume_on_failure=0)));
UPDATE protos.catalog_format SET schema_version=47,canonical_domain='auriga.item_catalog.v47' WHERE singleton=1;
