-- Explicit initial magic attribute probabilities and three independent tier draws.
-- Existing issued item attributes and all existing content cells are preserved.
CREATE TABLE protos.attribute_birth_profile (
 profile_id INT UNSIGNED NOT NULL PRIMARY KEY,
 code VARCHAR(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL UNIQUE,
 second_chance_bp SMALLINT UNSIGNED NOT NULL CHECK(second_chance_bp<=10000),
 third_chance_bp SMALLINT UNSIGNED NOT NULL CHECK(third_chance_bp<=10000)
) ENGINE=InnoDB;
CREATE TABLE protos.attribute_birth_tier (
 profile_id INT UNSIGNED NOT NULL,
 draw_no TINYINT UNSIGNED NOT NULL CHECK(draw_no<=2),
 tier TINYINT UNSIGNED NOT NULL CHECK(tier BETWEEN 1 AND 16),
 chance_bp SMALLINT UNSIGNED NOT NULL CHECK(chance_bp BETWEEN 1 AND 10000),
 PRIMARY KEY(profile_id,draw_no,tier),
 FOREIGN KEY(profile_id) REFERENCES protos.attribute_birth_profile(profile_id)
) ENGINE=InnoDB;
ALTER TABLE protos.item_definition_rules
 ADD COLUMN birth_attribute_profile_id INT UNSIGNED NULL,
 ADD CONSTRAINT fk_definition_birth_attributes FOREIGN KEY(birth_attribute_profile_id) REFERENCES protos.attribute_birth_profile(profile_id);
UPDATE protos.catalog_format SET schema_version=43,canonical_domain='auriga.item_catalog.v43' WHERE singleton=1;
