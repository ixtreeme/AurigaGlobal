-- Mission Engine schema 1 -> 2. Apply AFTER mission_engine_persistence.sql
-- to the selected player database. Additive: no legacy/seasonal tables touched.
-- DDL is not atomic. The version marker changes LAST; until then the new
-- executable refuses writes. Back up and validate before a production rollout.

CREATE TABLE IF NOT EXISTS mission_engine_action_outbox (
    action_id BINARY(32) NOT NULL PRIMARY KEY,
    character_id INT UNSIGNED NOT NULL,
    mission_id VARBINARY(96) NOT NULL,
    definition_version INT UNSIGNED NOT NULL,
    instance_id BIGINT UNSIGNED NOT NULL,
    node_id VARBINARY(96) NOT NULL,
    reward_id VARBINARY(96) NOT NULL,
    definition_digest BINARY(32) NOT NULL,
    first_storage_revision BIGINT UNSIGNED NOT NULL,
    action_state TINYINT UNSIGNED NOT NULL DEFAULT 1,
    created_at DATETIME(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6),
    UNIQUE KEY me_action_identity (character_id, mission_id, definition_version, instance_id, node_id, reward_id),
    KEY me_action_pending (character_id, action_state),
    CONSTRAINT me_action_state CHECK (action_state IN (1, 2, 3)),
    CONSTRAINT me_action_positive CHECK (instance_id > 0 AND first_storage_revision > 0),
    CONSTRAINT me_action_character FOREIGN KEY (character_id)
        REFERENCES mission_engine_characters (character_id) ON UPDATE RESTRICT ON DELETE RESTRICT,
    CONSTRAINT me_action_definition FOREIGN KEY (mission_id, definition_version, definition_digest)
        REFERENCES mission_engine_definitions (mission_id, definition_version, definition_digest)
        ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB;

-- A symbolic obligation to the character, pinned to the outbox's exact graph.
-- This is NOT proof that any item/currency/XP was delivered. A later verified
-- native delivery adapter needs its own payload binding and effect receipt.
CREATE TABLE IF NOT EXISTS mission_engine_reward_ledger (
    action_id BINARY(32) NOT NULL PRIMARY KEY,
    owner_epoch BIGINT UNSIGNED NOT NULL,
    storage_revision BIGINT UNSIGNED NOT NULL,
    created_at DATETIME(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6),
    CONSTRAINT me_entitlement_positive CHECK (owner_epoch > 0 AND storage_revision > 0),
    CONSTRAINT me_entitlement_action FOREIGN KEY (action_id)
        REFERENCES mission_engine_action_outbox (action_id) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB;

ALTER TABLE mission_engine_operations
    DROP CONSTRAINT IF EXISTS me_operation_kind,
    ADD CONSTRAINT me_operation_kind CHECK (operation_kind IN (1, 2, 3, 4));

-- Reapplying cannot reset a future schema or overwrite any entitlement.
UPDATE mission_engine_schema SET schema_version=2 WHERE singleton=1 AND schema_version=1;
