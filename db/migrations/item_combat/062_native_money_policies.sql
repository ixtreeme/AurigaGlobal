-- Fixed tokens retain their old explicit unit value. Zero is admitted only
-- with a native dynamic-pouch policy by the complete catalog validator.
ALTER TABLE protos.item_currency_token DROP CONSTRAINT CONSTRAINT_3,
 ADD CONSTRAINT ck_currency_token_amount CHECK(amount_per_unit BETWEEN 0 AND 9223372036854775807);
CREATE TABLE protos.currency_token_policy (
 category_id SMALLINT UNSIGNED NOT NULL,local_id INT UNSIGNED NOT NULL,
 mode_kind TINYINT UNSIGNED NOT NULL,
 PRIMARY KEY(category_id,local_id),UNIQUE(mode_kind),
 FOREIGN KEY(category_id,local_id) REFERENCES protos.item_currency_token(category_id,local_id),
 CHECK(category_id=16 AND mode_kind IN(2,3))
) ENGINE=InnoDB;
UPDATE protos.catalog_format SET schema_version=54,canonical_domain='auriga.item_catalog.v54' WHERE singleton=1;
