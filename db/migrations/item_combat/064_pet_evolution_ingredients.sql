CREATE TABLE protos.growth_pet_evolution_ingredient (
    profile_id INT UNSIGNED NOT NULL,
    stage TINYINT UNSIGNED NOT NULL,
    category_id SMALLINT UNSIGNED NOT NULL,
    local_id INT UNSIGNED NOT NULL,
    amount INT UNSIGNED NOT NULL,
    PRIMARY KEY(profile_id,stage,category_id,local_id),
    FOREIGN KEY(profile_id,stage) REFERENCES protos.growth_pet_stage(profile_id,stage),
    FOREIGN KEY(category_id,local_id) REFERENCES protos.item_base(category_id,local_id),
    CHECK(category_id=6), CHECK(amount BETWEEN 1 AND 64)
) ENGINE=InnoDB;
UPDATE protos.catalog_format SET schema_version=56,canonical_domain='auriga.item_catalog.v56' WHERE singleton=1;
