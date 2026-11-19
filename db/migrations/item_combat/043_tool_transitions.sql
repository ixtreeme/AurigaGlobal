-- Native tool transitions preserve distinct source models/icons and self-loops.
ALTER TABLE protos.tool_level
 ADD COLUMN success_category_id SMALLINT UNSIGNED NULL,
 ADD COLUMN success_local_id INT UNSIGNED NULL,
 ADD COLUMN success_level SMALLINT UNSIGNED NOT NULL DEFAULT 0,
 ADD COLUMN failure_category_id SMALLINT UNSIGNED NULL,
 ADD COLUMN failure_local_id INT UNSIGNED NULL,
 ADD COLUMN display_grade SMALLINT UNSIGNED NOT NULL DEFAULT 65535,
 DROP CONSTRAINT CONSTRAINT_7,
 ADD CONSTRAINT ck_tool_failure_level CHECK((failure_kind=1 AND failure_level IS NULL) OR
     (failure_kind=2 AND failure_level IS NOT NULL AND failure_level<=255 AND (failure_local_id IS NOT NULL OR failure_level<=level) AND failure_loss_bp=0)),
 ADD CONSTRAINT fk_tool_success FOREIGN KEY(success_category_id,success_local_id) REFERENCES protos.item_base(category_id,local_id),
 ADD CONSTRAINT fk_tool_failure FOREIGN KEY(failure_category_id,failure_local_id) REFERENCES protos.item_base(category_id,local_id),
 ADD CONSTRAINT ck_tool_success CHECK((success_category_id IS NULL AND success_local_id IS NULL AND success_level=0) OR (success_category_id IS NOT NULL AND success_local_id IS NOT NULL AND success_category_id=11 AND success_local_id>0 AND success_level<=255)),
 ADD CONSTRAINT ck_tool_failure CHECK((failure_category_id IS NULL AND failure_local_id IS NULL) OR (failure_category_id IS NOT NULL AND failure_local_id IS NOT NULL AND failure_category_id=11 AND failure_local_id>0 AND failure_kind=2)),
 ADD CONSTRAINT ck_tool_display_grade CHECK(display_grade<=255 OR display_grade=65535);
ALTER TABLE protos.item_tool
 ADD COLUMN all_water_pools TINYINT UNSIGNED NOT NULL DEFAULT 0,
 ADD CONSTRAINT ck_tool_all_water_pools CHECK(all_water_pools IN(0,1) AND (all_water_pools=0 OR tool_kind=2));
UPDATE protos.catalog_format SET schema_version=35,canonical_domain='auriga.item_catalog.v35' WHERE singleton=1;
