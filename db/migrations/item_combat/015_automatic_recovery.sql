-- Unreleased v9 draft. Apply the full fresh-schema chain; not an existing
-- v7/v8 upgrade (the foundation's recovery discriminator was extended too).
ALTER TABLE protos.recovery_profile
    ADD arena_quota_cost SMALLINT UNSIGNED NOT NULL DEFAULT 0,
    ADD CONSTRAINT ck_recovery_arena_cost CHECK (arena_quota_cost=0 OR (allowed_contexts & 4)<>0);
CREATE TABLE protos.automatic_recovery_profile (
    profile_id INT UNSIGNED NOT NULL PRIMARY KEY,
    delivery_kind TINYINT UNSIGNED NOT NULL DEFAULT 4,
    pool_kind TINYINT UNSIGNED NOT NULL, supply_kind TINYINT UNSIGNED NOT NULL,
    capacity_raw BIGINT NOT NULL, tick_interval_ms BIGINT UNSIGNED NOT NULL,
    max_pool_per_tick_bp SMALLINT UNSIGNED NOT NULL,
    active_max_pool_bonus_bp SMALLINT UNSIGNED NOT NULL,
    FOREIGN KEY(profile_id,delivery_kind) REFERENCES protos.recovery_profile(profile_id,delivery_kind),
    CHECK(delivery_kind=4), CHECK(pool_kind IN (1,2)), CHECK(supply_kind IN (1,2)),
    CHECK((supply_kind=1 AND capacity_raw BETWEEN 1 AND 1000000000000000) OR (supply_kind=2 AND capacity_raw=0)),
    CHECK(tick_interval_ms BETWEEN 1 AND 9223372036854775807),
    CHECK(max_pool_per_tick_bp BETWEEN 1 AND 10000), CHECK(active_max_pool_bonus_bp<=10000)
) ENGINE=InnoDB;
-- Automatic head units/context/quota and item stack=1 are publication checks.
UPDATE protos.catalog_format SET schema_version=9, canonical_domain='auriga.item_catalog.v9' WHERE singleton=1;
