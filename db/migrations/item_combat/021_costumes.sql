-- Unreleased v15 draft. Ordinary body/hair/weapon costumes only.
-- Timers below are definitions; saved instance lifetime is a separate owner.
CREATE TABLE protos.item_costume (
    category_id SMALLINT UNSIGNED NOT NULL DEFAULT 4, local_id INT UNSIGNED NOT NULL,
    costume_kind TINYINT UNSIGNED NOT NULL, weapon_subtype TINYINT UNSIGNED NULL,
    clock_kind TINYINT UNSIGNED NOT NULL, duration_ms BIGINT UNSIGNED NOT NULL,
    PRIMARY KEY(category_id,local_id),
    FOREIGN KEY(category_id,local_id) REFERENCES protos.item_base(category_id,local_id),
    CHECK(category_id=4), CHECK(local_id>0), CHECK(costume_kind IN(1,2,3)),
    CHECK((costume_kind=3 AND weapon_subtype IS NOT NULL AND weapon_subtype BETWEEN 1 AND 7)
       OR (costume_kind IN(1,2) AND weapon_subtype IS NULL)),
    CHECK(clock_kind IN(1,2,3,4)), CHECK(duration_ms<=9223372036854775807),
    CHECK((clock_kind=1 AND duration_ms=0) OR (clock_kind<>1 AND duration_ms>0))
) ENGINE=InnoDB;
CREATE TABLE protos.item_costume_visual (
    category_id SMALLINT UNSIGNED NOT NULL DEFAULT 4, local_id INT UNSIGNED NOT NULL,
    character_class TINYINT UNSIGNED NOT NULL, sex TINYINT UNSIGNED NOT NULL,
    model_asset VARCHAR(192) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
    PRIMARY KEY(category_id,local_id,character_class,sex),
    FOREIGN KEY(category_id,local_id) REFERENCES protos.item_costume(category_id,local_id),
    CHECK(category_id=4), CHECK(local_id>0), CHECK(character_class BETWEEN 1 AND 4),
    CHECK(sex IN(1,2)), CHECK(model_asset<>'')
) ENGINE=InnoDB;
ALTER TABLE protos.item_base DROP CONSTRAINT ck_item_base_category,
    ADD CONSTRAINT ck_item_base_category CHECK(category_id IN(1,2,3,4,5,6,7,8,9,10,16,18));
ALTER TABLE protos.refinement_profile DROP CONSTRAINT ck_refinement_profile_kind,
    ADD CONSTRAINT ck_refinement_profile_kind CHECK(profile_kind IN(1,2,3,4,5,6));
UPDATE protos.catalog_format SET schema_version=15,canonical_domain='auriga.item_catalog.v15' WHERE singleton=1;
