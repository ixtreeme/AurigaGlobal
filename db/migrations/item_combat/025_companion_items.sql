-- Fixed pet/mount seals. Growth pets, skins, eggs and feeding are separate.
CREATE TABLE protos.companion_form (
    form_id INT UNSIGNED NOT NULL PRIMARY KEY,
    code VARCHAR(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL UNIQUE,
    mob_template_id INT UNSIGNED NOT NULL,
    model_asset VARCHAR(192) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
    CHECK(form_id>0), CHECK(mob_template_id>0), CHECK(LENGTH(code)>0), CHECK(LENGTH(model_asset)>0)
) ENGINE=InnoDB;
CREATE TABLE protos.item_companion (
    category_id SMALLINT UNSIGNED NOT NULL,
    local_id INT UNSIGNED NOT NULL,
    companion_kind TINYINT UNSIGNED NOT NULL,
    form_id INT UNSIGNED NOT NULL,
    riding_form_id INT UNSIGNED NULL,
    clock_kind TINYINT UNSIGNED NOT NULL,
    duration_ms BIGINT UNSIGNED NOT NULL,
    dungeon_only_bonus TINYINT UNSIGNED NOT NULL,
    PRIMARY KEY(category_id,local_id),
    FOREIGN KEY(category_id,local_id) REFERENCES protos.item_base(category_id,local_id),
    FOREIGN KEY(form_id) REFERENCES protos.companion_form(form_id),
    FOREIGN KEY(riding_form_id) REFERENCES protos.companion_form(form_id),
    CHECK(category_id=15), CHECK(local_id>0), CHECK(dungeon_only_bonus IN(0,1)),
    CHECK((companion_kind=1 AND riding_form_id IS NULL) OR (companion_kind=2 AND riding_form_id IS NOT NULL)),
    CHECK(clock_kind IN(1,2,3,4)), CHECK(duration_ms<=9223372036854775807),
    CHECK((clock_kind=1 AND duration_ms=0) OR (clock_kind<>1 AND duration_ms>0))
) ENGINE=InnoDB;
ALTER TABLE protos.refinement_profile DROP CONSTRAINT ck_refinement_profile_kind,
    ADD CONSTRAINT ck_refinement_profile_kind CHECK(profile_kind IN(1,2,3,4,5,6,7,8,9));
ALTER TABLE protos.item_base DROP CONSTRAINT ck_item_base_category,
    ADD CONSTRAINT ck_item_base_category CHECK(category_id IN(1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,18)),
    ADD CONSTRAINT ck_item_base_companion CHECK(category_id<>15 OR appearance_profile_id IS NULL);
UPDATE protos.catalog_format SET schema_version=19,canonical_domain='auriga.item_catalog.v19' WHERE singleton=1;
