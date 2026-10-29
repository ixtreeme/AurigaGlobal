-- Fresh unreleased v23 chain; never apply to the installed v7 database.
CREATE TABLE protos.socket_service_profile (
    profile_id INT UNSIGNED NOT NULL PRIMARY KEY,
    code VARCHAR(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL UNIQUE,
    service_kind TINYINT UNSIGNED NOT NULL,
    units_per_use SMALLINT UNSIGNED NOT NULL,
    CHECK(profile_id>0 AND LENGTH(code)>0),
    CHECK(service_kind BETWEEN 1 AND 3), CHECK(units_per_use>0)
) ENGINE=InnoDB;
ALTER TABLE protos.item_consumable
    DROP CONSTRAINT ck_consumable_one_effect,
    ADD socket_service_profile_id INT UNSIGNED NULL,
    ADD FOREIGN KEY(socket_service_profile_id) REFERENCES protos.socket_service_profile(profile_id),
    ADD CONSTRAINT ck_consumable_one_effect CHECK (
        (recovery_profile_id IS NOT NULL) + (timed_effect_profile_id IS NOT NULL) +
        (toggle_effect_profile_id IS NOT NULL) + (socket_service_profile_id IS NOT NULL) = 1);
UPDATE protos.catalog_format SET schema_version=23,canonical_domain='auriga.item_catalog.v23' WHERE singleton=1;
