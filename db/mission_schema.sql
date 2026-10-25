-- Generated from Mission migrations by mission_database.install_sql. MariaDB/InnoDB.
CREATE DATABASE IF NOT EXISTS `mission` DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_bin;


CREATE TABLE IF NOT EXISTS `mission`.`mission_engine_schema` (
    singleton TINYINT UNSIGNED NOT NULL PRIMARY KEY,
    schema_version INT UNSIGNED NOT NULL,
    CONSTRAINT me_schema_singleton CHECK (singleton = 1)
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS `mission`.`mission_engine_packages` (
    package_digest BINARY(32) NOT NULL PRIMARY KEY,
    package_id VARBINARY(96) NOT NULL,
    content_version INT UNSIGNED NOT NULL,
    package_bytes LONGBLOB NOT NULL,
    created_at DATETIME(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6),
    UNIQUE KEY me_package_version (package_id, content_version),
    CONSTRAINT me_package_version_positive CHECK (content_version > 0),
    CONSTRAINT me_package_size CHECK (OCTET_LENGTH(package_bytes) BETWEEN 64 AND 16777216)
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS `mission`.`mission_engine_definitions` (
    mission_id VARBINARY(96) NOT NULL,
    definition_version INT UNSIGNED NOT NULL,
    definition_digest BINARY(32) NOT NULL,
    package_digest BINARY(32) NOT NULL,
    PRIMARY KEY (mission_id, definition_version),
    UNIQUE KEY me_definition_content (mission_id, definition_version, definition_digest),
    CONSTRAINT me_definition_version_positive CHECK (definition_version > 0),
    CONSTRAINT me_definition_package FOREIGN KEY (package_digest)
        REFERENCES `mission`.`mission_engine_packages` (package_digest) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS `mission`.`mission_engine_characters` (
    character_id INT UNSIGNED NOT NULL PRIMARY KEY,
    owner_epoch BIGINT UNSIGNED NOT NULL DEFAULT 0,
    owner_token BINARY(16) NULL,
    storage_revision BIGINT UNSIGNED NOT NULL DEFAULT 0,
    runtime_revision BIGINT UNSIGNED NOT NULL DEFAULT 0,
    checkpoint_time BIGINT NOT NULL DEFAULT 0,
    checkpoint_digest BINARY(32) NULL,
    checkpoint_bytes LONGBLOB NULL,
    CONSTRAINT me_character_positive CHECK (character_id > 0),
    CONSTRAINT me_checkpoint_time CHECK (checkpoint_time >= 0),
    CONSTRAINT me_checkpoint_state CHECK (
        (checkpoint_bytes IS NULL AND checkpoint_digest IS NULL AND storage_revision = 0
         AND runtime_revision = 0 AND checkpoint_time = 0)
        OR (checkpoint_bytes IS NOT NULL AND checkpoint_digest IS NOT NULL AND storage_revision > 0
            AND OCTET_LENGTH(checkpoint_bytes) BETWEEN 64 AND 16777216)
    )
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS `mission`.`mission_engine_checkpoint_refs` (
    character_id INT UNSIGNED NOT NULL,
    mission_id VARBINARY(96) NOT NULL,
    definition_version INT UNSIGNED NOT NULL,
    definition_digest BINARY(32) NOT NULL,
    PRIMARY KEY (character_id, mission_id, definition_version),
    CONSTRAINT me_ref_character FOREIGN KEY (character_id)
        REFERENCES `mission`.`mission_engine_characters` (character_id) ON UPDATE RESTRICT ON DELETE RESTRICT,
    CONSTRAINT me_ref_definition FOREIGN KEY (mission_id, definition_version, definition_digest)
        REFERENCES `mission`.`mission_engine_definitions` (mission_id, definition_version, definition_digest)
        ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS `mission`.`mission_engine_operations` (
    operation_id BINARY(16) NOT NULL PRIMARY KEY,
    character_id INT UNSIGNED NOT NULL,
    operation_kind TINYINT UNSIGNED NOT NULL,
    request_digest BINARY(32) NOT NULL,
    owner_epoch BIGINT UNSIGNED NOT NULL,
    storage_revision BIGINT UNSIGNED NOT NULL,
    created_at DATETIME(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6),
    KEY me_operation_character (character_id, created_at),
    CONSTRAINT me_operation_kind CHECK (operation_kind IN (1, 2, 3)),
    CONSTRAINT me_operation_character_fk FOREIGN KEY (character_id)
        REFERENCES `mission`.`mission_engine_characters` (character_id) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB;

INSERT INTO `mission`.`mission_engine_schema` (singleton, schema_version) VALUES (1, 1)
ON DUPLICATE KEY UPDATE singleton = singleton;


CREATE TABLE IF NOT EXISTS `mission`.`mission_engine_action_outbox` (
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
        REFERENCES `mission`.`mission_engine_characters` (character_id) ON UPDATE RESTRICT ON DELETE RESTRICT,
    CONSTRAINT me_action_definition FOREIGN KEY (mission_id, definition_version, definition_digest)
        REFERENCES `mission`.`mission_engine_definitions` (mission_id, definition_version, definition_digest)
        ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS `mission`.`mission_engine_reward_ledger` (
    action_id BINARY(32) NOT NULL PRIMARY KEY,
    owner_epoch BIGINT UNSIGNED NOT NULL,
    storage_revision BIGINT UNSIGNED NOT NULL,
    created_at DATETIME(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6),
    CONSTRAINT me_entitlement_positive CHECK (owner_epoch > 0 AND storage_revision > 0),
    CONSTRAINT me_entitlement_action FOREIGN KEY (action_id)
        REFERENCES `mission`.`mission_engine_action_outbox` (action_id) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB;

ALTER TABLE `mission`.`mission_engine_operations`
    DROP CONSTRAINT IF EXISTS me_operation_kind,
    ADD CONSTRAINT me_operation_kind CHECK (operation_kind IN (1, 2, 3, 4));

UPDATE `mission`.`mission_engine_schema` SET schema_version=2 WHERE singleton=1 AND schema_version=1;

CREATE TABLE IF NOT EXISTS `mission`.`mission_engine_item_awards` (
    action_id BINARY(32) NOT NULL,
    ordinal TINYINT UNSIGNED NOT NULL,
    receipt_format TINYINT UNSIGNED NOT NULL DEFAULT 1,
    payload_digest BINARY(32) NOT NULL,
    player_schema VARBINARY(64) NOT NULL,
    account_schema VARBINARY(64) NOT NULL,
    player_postfix VARBINARY(32) NOT NULL,
    account_id INT UNSIGNED NOT NULL,
    login VARBINARY(30) NOT NULL,
    award_id INT UNSIGNED NOT NULL,
    vnum INT UNSIGNED NOT NULL,
    item_count INT UNSIGNED NOT NULL,
    PRIMARY KEY (action_id, ordinal),
    UNIQUE KEY me_item_award_identity (player_schema, award_id),
    CONSTRAINT me_item_award_bounds CHECK (ordinal < 32 AND receipt_format=1 AND
        account_id > 0 AND award_id > 0 AND vnum > 0 AND item_count BETWEEN 1 AND 50000),
    CONSTRAINT me_item_award_action FOREIGN KEY (action_id)
        REFERENCES `mission`.`mission_engine_reward_ledger` (action_id) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB;

UPDATE `mission`.`mission_engine_schema` SET schema_version=3 WHERE singleton=1 AND schema_version=2;
