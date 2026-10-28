-- Unreleased v13 draft: passive tokens and typed quest-use dispatch.
CREATE TABLE protos.item_quest (
    category_id SMALLINT UNSIGNED NOT NULL DEFAULT 10, local_id INT UNSIGNED NOT NULL,
    dispatch_kind TINYINT UNSIGNED NOT NULL,
    group_code VARCHAR(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL DEFAULT '',
    allowed_contexts TINYINT UNSIGNED NOT NULL, allow_observer BOOLEAN NOT NULL,
    PRIMARY KEY(category_id,local_id),
    FOREIGN KEY(category_id,local_id) REFERENCES protos.item_base(category_id,local_id),
    CHECK(category_id=10), CHECK(local_id>0), CHECK(dispatch_kind IN(1,2,3)),
    CHECK(allow_observer IN(0,1)), CHECK(allowed_contexts BETWEEN 0 AND 15),
    CHECK((dispatch_kind=1 AND group_code='' AND allowed_contexts=0 AND allow_observer=0)
       OR (dispatch_kind IN(2,3) AND allowed_contexts>0))
) ENGINE=InnoDB;
ALTER TABLE protos.item_base
    DROP CONSTRAINT ck_item_base_category, DROP CONSTRAINT ck_item_base_stack, DROP CONSTRAINT ck_item_base_non_equipment,
    ADD CONSTRAINT ck_item_base_category CHECK(category_id IN(1,2,3,5,6,8,9,10,16,18)),
    ADD CONSTRAINT ck_item_base_stack CHECK(category_id IN(5,6,8,9,10,16,18) OR max_stack=1),
    ADD CONSTRAINT ck_item_base_non_equipment CHECK(category_id NOT IN(5,6,8,9,10,16,18)
        OR (binding_policy<>3 AND (appearance_profile_id IS NULL OR category_id=10) AND base_bonus_profile_id IS NULL));
UPDATE protos.catalog_format SET schema_version=13,canonical_domain='auriga.item_catalog.v13' WHERE singleton=1;
