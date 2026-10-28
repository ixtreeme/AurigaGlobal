-- Unreleased v14 draft; existing v7 content is upgraded through offline export.
CREATE TABLE protos.item_socket_stone (
    category_id SMALLINT UNSIGNED NOT NULL DEFAULT 7, local_id INT UNSIGNED NOT NULL,
    stone_kind TINYINT UNSIGNED NOT NULL, allowed_targets TINYINT UNSIGNED NOT NULL,
    minimum_socket_grade TINYINT UNSIGNED NOT NULL,
    primary_group VARCHAR(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL DEFAULT '',
    secondary_group VARCHAR(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL DEFAULT '',
    PRIMARY KEY(category_id,local_id),
    FOREIGN KEY(category_id,local_id) REFERENCES protos.item_base(category_id,local_id),
    CHECK(category_id=7), CHECK(local_id>0), CHECK(stone_kind IN(1,2)),
    CHECK(allowed_targets BETWEEN 0 AND 3), CHECK(minimum_socket_grade BETWEEN 0 AND 2),
    CHECK(secondary_group='' OR secondary_group<>primary_group),
    CHECK((stone_kind=1 AND allowed_targets>0 AND minimum_socket_grade>0)
       OR (stone_kind=2 AND allowed_targets=0 AND minimum_socket_grade=0 AND primary_group='' AND secondary_group=''))
) ENGINE=InnoDB;
ALTER TABLE protos.refinement_profile DROP CONSTRAINT ck_refinement_profile_kind,
    ADD CONSTRAINT ck_refinement_profile_kind CHECK(profile_kind IN(1,2,3,4,5));
ALTER TABLE protos.item_base
    DROP CONSTRAINT ck_item_base_category, DROP CONSTRAINT ck_item_base_stack, DROP CONSTRAINT ck_item_base_non_equipment,
    ADD CONSTRAINT ck_item_base_category CHECK(category_id IN(1,2,3,5,6,7,8,9,10,16,18)),
    ADD CONSTRAINT ck_item_base_stack CHECK(category_id IN(5,6,7,8,9,10,16,18) OR max_stack=1),
    ADD CONSTRAINT ck_item_base_non_equipment CHECK(category_id NOT IN(5,6,7,8,9,10,16,18)
        OR (binding_policy<>3 AND (appearance_profile_id IS NULL OR category_id IN(7,10))
            AND (base_bonus_profile_id IS NULL OR category_id=7)));
UPDATE protos.catalog_format SET schema_version=14,canonical_domain='auriga.item_catalog.v14' WHERE singleton=1;
