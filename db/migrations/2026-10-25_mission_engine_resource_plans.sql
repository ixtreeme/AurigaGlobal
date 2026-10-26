-- Mission schema 3 -> 4. Immutable bounded resource PROPOSALS, not effects.
-- No player/item update, entitlement, acknowledgement or resource-writer fence.
CREATE TABLE IF NOT EXISTS mission_engine_resource_plans (
    action_id BINARY(32) NOT NULL,
    decision_digest BINARY(32) NOT NULL,
    definition_digest BINARY(32) NOT NULL,
    payload_digest BINARY(32) NOT NULL,
    source_revision BIGINT UNSIGNED NOT NULL,
    plan_bytes MEDIUMBLOB NOT NULL,
    created_at DATETIME(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6),
    PRIMARY KEY (action_id, decision_digest),
    CONSTRAINT me_resource_bounds CHECK (source_revision BETWEEN 1 AND 18446744073709551613
        AND OCTET_LENGTH(plan_bytes) BETWEEN 64 AND 2097152),
    CONSTRAINT me_resource_action FOREIGN KEY (action_id)
        REFERENCES mission_engine_action_outbox (action_id) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB;

SET @me_resource_upgrade = IF(
    (SELECT schema_version FROM mission_engine_schema WHERE singleton=1) IN (3, 4),
    'ALTER TABLE mission_engine_operations DROP CONSTRAINT IF EXISTS me_operation_kind, ADD CONSTRAINT me_operation_kind CHECK (operation_kind IN (1,2,3,4,5))',
    'DO 0');
PREPARE me_resource_upgrade_statement FROM @me_resource_upgrade;
EXECUTE me_resource_upgrade_statement;
DEALLOCATE PREPARE me_resource_upgrade_statement;

UPDATE mission_engine_schema SET schema_version=4 WHERE singleton=1 AND schema_version=3;
