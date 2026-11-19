-- Explicit native targets; original exclusions are compiled offline.
CREATE TABLE protos.costume_transfer_target (
 category_id SMALLINT UNSIGNED NOT NULL,local_id INT UNSIGNED NOT NULL,
 PRIMARY KEY(category_id,local_id),
 CONSTRAINT fk_costume_transfer_target FOREIGN KEY(category_id,local_id)
 REFERENCES protos.item_base(category_id,local_id) ON DELETE CASCADE,
 CONSTRAINT ck_costume_transfer_category CHECK(category_id IN(4,19))
) ENGINE=InnoDB;
ALTER TABLE protos.item_bonus_tool DROP CONSTRAINT ck_bonus_tool_operation,
 ADD CONSTRAINT ck_bonus_tool_operation CHECK(operation_kind BETWEEN 1 AND 8 AND units>0 AND
 ((operation_kind=2 AND parameter BETWEEN 1 AND 10000 AND units=1) OR (operation_kind<>2 AND parameter=0)));
UPDATE protos.catalog_format SET schema_version=52,canonical_domain='auriga.item_catalog.v52' WHERE singleton=1;
