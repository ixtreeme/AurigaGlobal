-- Unreleased v12 fresh-schema draft. Existing catalogs need a staged upgrade.
-- A recipe is an operation on an instance; it is not another +N prototype.
CREATE TABLE protos.refinement_recipe (
    recipe_id INT UNSIGNED NOT NULL PRIMARY KEY,
    code VARCHAR(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL UNIQUE,
    source_category_id SMALLINT UNSIGNED NOT NULL, source_local_id INT UNSIGNED NOT NULL,
    source_level SMALLINT UNSIGNED NOT NULL,
    target_category_id SMALLINT UNSIGNED NOT NULL, target_local_id INT UNSIGNED NOT NULL,
    target_level SMALLINT UNSIGNED NOT NULL,
    gold_cost BIGINT UNSIGNED NOT NULL,
    success_chance_bp SMALLINT UNSIGNED NOT NULL,
    failure_kind TINYINT UNSIGNED NOT NULL, failure_level SMALLINT UNSIGNED NULL,
    FOREIGN KEY(source_category_id,source_local_id) REFERENCES protos.item_base(category_id,local_id),
    FOREIGN KEY(target_category_id,target_local_id) REFERENCES protos.item_base(category_id,local_id),
    CHECK(recipe_id>0), CHECK(CHAR_LENGTH(code)>0),
    CHECK(source_level<=255), CHECK(target_level<=255),
    CHECK(gold_cost<=9223372036854775807), CHECK(success_chance_bp<=10000),
    CHECK(failure_kind IN(1,2,3)),
    CHECK((failure_kind=3 AND failure_level IS NOT NULL AND failure_level<source_level)
       OR (failure_kind IN(1,2) AND failure_level IS NULL)),
    CHECK(source_category_id<>target_category_id OR source_local_id<>target_local_id OR target_level=source_level+1)
) ENGINE=InnoDB;
CREATE TABLE protos.refinement_ingredient (
    recipe_id INT UNSIGNED NOT NULL,
    category_id SMALLINT UNSIGNED NOT NULL, local_id INT UNSIGNED NOT NULL,
    refinement_level SMALLINT UNSIGNED NOT NULL, amount INT UNSIGNED NOT NULL,
    PRIMARY KEY(recipe_id,category_id,local_id,refinement_level),
    FOREIGN KEY(recipe_id) REFERENCES protos.refinement_recipe(recipe_id),
    FOREIGN KEY(category_id,local_id) REFERENCES protos.item_base(category_id,local_id),
    CHECK(refinement_level<=255), CHECK(amount>0)
) ENGINE=InnoDB;
-- Publication also checks retirement, stack rules, actual level coverage and
-- the 32-ingredient bound. A missing recipe never means free refinement.
UPDATE protos.catalog_format SET schema_version=12, canonical_domain='auriga.item_catalog.v12' WHERE singleton=1;
