-- Currency tokens (16): items that carry wallet money (gold bars, coin vouchers).
-- The per-unit value is explicit; the old hidden shop_buy_price is not read.
-- Fresh, unreleased draft schema: no live gameplay DB upgrade.
CREATE TABLE protos.item_currency_token (
    category_id SMALLINT UNSIGNED NOT NULL DEFAULT 16, local_id INT UNSIGNED NOT NULL,
    wallet_currency TINYINT UNSIGNED NOT NULL,
    amount_per_unit BIGINT UNSIGNED NOT NULL,
    PRIMARY KEY(category_id, local_id),
    FOREIGN KEY(category_id, local_id) REFERENCES protos.item_base(category_id, local_id),
    CHECK(category_id=16), CHECK(wallet_currency IN (1,2)),
    CHECK(amount_per_unit BETWEEN 1 AND 9223372036854775807)
) ENGINE=InnoDB;

-- item_base admits the stackable, non-equipment token category (names from 012).
ALTER TABLE protos.item_base
    DROP CONSTRAINT ck_item_base_category, DROP CONSTRAINT ck_item_base_stack, DROP CONSTRAINT ck_item_base_non_equipment,
    ADD CONSTRAINT ck_item_base_category CHECK (category_id IN (1,2,3,5,6,8,16,18)),
    ADD CONSTRAINT ck_item_base_stack CHECK (category_id IN (5,6,8,16,18) OR max_stack=1),
    ADD CONSTRAINT ck_item_base_non_equipment CHECK (category_id NOT IN (5,6,8,16,18)
        OR (binding_policy<>3 AND appearance_profile_id IS NULL AND base_bonus_profile_id IS NULL));

UPDATE protos.catalog_format SET schema_version=7, canonical_domain='auriga.item_catalog.v7' WHERE singleton=1;
-- Wallet caps (gold maximum, coin maximum) are runtime configuration.
