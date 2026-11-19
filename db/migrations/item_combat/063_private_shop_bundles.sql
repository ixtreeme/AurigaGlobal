-- Private-shop bundle roles use native Quest FirstHandler groups and explicit definition clocks.
UPDATE protos.catalog_format SET schema_version=55,canonical_domain='auriga.item_catalog.v55' WHERE singleton=1;
