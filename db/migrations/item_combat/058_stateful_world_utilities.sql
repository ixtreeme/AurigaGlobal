-- Explicit detector capacity is independent of gathering practice and bait.
ALTER TABLE protos.item_tool
 ADD maximum_uses SMALLINT UNSIGNED NOT NULL DEFAULT 0,
 DROP CONSTRAINT CONSTRAINT_3,
 DROP CONSTRAINT CONSTRAINT_4,
 ADD CONSTRAINT ck_tool_kind CHECK(tool_kind IN(1,2,3,4,5)),
 ADD CONSTRAINT ck_tool_pool CHECK((tool_kind=2 AND fishing_pool IN(2,3)) OR (tool_kind IN(1,3,4,5) AND fishing_pool=1)),
 ADD CONSTRAINT ck_detector_uses CHECK((tool_kind=5 AND maximum_uses>0) OR (tool_kind<>5 AND maximum_uses=0));
UPDATE protos.catalog_format SET schema_version=50,canonical_domain='auriga.item_catalog.v50' WHERE singleton=1;
