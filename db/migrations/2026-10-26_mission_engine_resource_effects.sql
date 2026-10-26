-- Mission schema 4 -> 5. Resource writer head and immutable actual effect receipt.
-- Backend only: live producers must implement stop/drain/guard/rebase before activation.
CREATE TABLE IF NOT EXISTS mission_engine_resource_heads (
    character_id INT UNSIGNED NOT NULL PRIMARY KEY,
    owner_epoch BIGINT UNSIGNED NOT NULL,
    owner_token BINARY(16) NOT NULL,
    writer_generation BINARY(16) NOT NULL,
    core_generation BIGINT UNSIGNED NOT NULL,
    session_generation BIGINT UNSIGNED NOT NULL,
    resource_revision BIGINT UNSIGNED NOT NULL,
    publication_pending BOOLEAN NOT NULL,
    last_action BINARY(32) NULL,
    last_decision BINARY(32) NULL,
    CONSTRAINT me_resource_head_bounds CHECK (owner_epoch>0 AND core_generation>0 AND session_generation>0 AND resource_revision BETWEEN 1 AND 18446744073709551614
        AND publication_pending IN (0,1) AND ((last_action IS NULL AND last_decision IS NULL AND publication_pending=0)
        OR (last_action IS NOT NULL AND last_decision IS NOT NULL))),
    CONSTRAINT me_resource_head_character FOREIGN KEY (character_id)
        REFERENCES mission_engine_characters (character_id) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS mission_engine_resource_effects (
    action_id BINARY(32) NOT NULL PRIMARY KEY,
    decision_digest BINARY(32) NOT NULL,
    binding_digest BINARY(32) NOT NULL,
    source_revision BIGINT UNSIGNED NOT NULL,
    applied_revision BIGINT UNSIGNED NOT NULL,
    writer_generation BINARY(16) NOT NULL,
    created_at DATETIME(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6),
    CONSTRAINT me_effect_revision CHECK (source_revision BETWEEN 1 AND 18446744073709551613
        AND applied_revision=source_revision+1),
    CONSTRAINT me_effect_plan FOREIGN KEY (action_id,decision_digest)
        REFERENCES mission_engine_resource_plans (action_id,decision_digest) ON UPDATE RESTRICT ON DELETE RESTRICT,
    CONSTRAINT me_effect_entitlement FOREIGN KEY (action_id)
        REFERENCES mission_engine_reward_ledger (action_id) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB;

SET @me_effect_upgrade = IF(
    (SELECT schema_version FROM mission_engine_schema WHERE singleton=1) IN (4,5),
    'ALTER TABLE mission_engine_operations DROP CONSTRAINT IF EXISTS me_operation_kind, ADD CONSTRAINT me_operation_kind CHECK (operation_kind IN (1,2,3,4,5,6,7,8))',
    'DO 0');
PREPARE me_effect_upgrade_statement FROM @me_effect_upgrade;
EXECUTE me_effect_upgrade_statement;
DEALLOCATE PREPARE me_effect_upgrade_statement;

UPDATE mission_engine_schema SET schema_version=5 WHERE singleton=1 AND schema_version=4;
