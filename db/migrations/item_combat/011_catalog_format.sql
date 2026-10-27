-- Exact format marker for the staged SQL reader. This does not publish content
-- or allocate a release revision. Publishers/runtime readers need separate roles.
CREATE TABLE protos.catalog_format (
    singleton TINYINT UNSIGNED NOT NULL PRIMARY KEY,
    schema_version INT UNSIGNED NOT NULL,
    canonical_domain VARCHAR(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
    CHECK(singleton=1), CHECK(schema_version>0), CHECK(CHAR_LENGTH(canonical_domain)>0)
) ENGINE=InnoDB;
INSERT INTO protos.catalog_format VALUES (1,5,'auriga.item_catalog.v5');
