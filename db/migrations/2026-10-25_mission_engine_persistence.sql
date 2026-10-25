-- Mission Engine persistence schema 1. Select the configured mission database
-- before running this additive migration. No USE/DROP/legacy table changes.
-- MariaDB/InnoDB required. DDL is not transactionally rolled back: take a backup
-- and validate the schema before a later production rollout.

CREATE TABLE IF NOT EXISTS mission_engine_schema (
    singleton TINYINT UNSIGNED NOT NULL PRIMARY KEY,
    schema_version INT UNSIGNED NOT NULL,
    CONSTRAINT me_schema_singleton CHECK (singleton = 1)
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS mission_engine_packages (
    package_digest BINARY(32) NOT NULL PRIMARY KEY,
    package_id VARBINARY(96) NOT NULL,
    content_version INT UNSIGNED NOT NULL,
    package_bytes LONGBLOB NOT NULL,
    created_at DATETIME(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6),
    UNIQUE KEY me_package_version (package_id, content_version),
    CONSTRAINT me_package_version_positive CHECK (content_version > 0),
    CONSTRAINT me_package_size CHECK (OCTET_LENGTH(package_bytes) BETWEEN 64 AND 16777216)
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS mission_engine_definitions (
    mission_id VARBINARY(96) NOT NULL,
    definition_version INT UNSIGNED NOT NULL,
    definition_digest BINARY(32) NOT NULL,
    package_digest BINARY(32) NOT NULL,
    PRIMARY KEY (mission_id, definition_version),
    UNIQUE KEY me_definition_content (mission_id, definition_version, definition_digest),
    CONSTRAINT me_definition_version_positive CHECK (definition_version > 0),
    CONSTRAINT me_definition_package FOREIGN KEY (package_digest)
        REFERENCES mission_engine_packages (package_digest) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS mission_engine_characters (
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

CREATE TABLE IF NOT EXISTS mission_engine_checkpoint_refs (
    character_id INT UNSIGNED NOT NULL,
    mission_id VARBINARY(96) NOT NULL,
    definition_version INT UNSIGNED NOT NULL,
    definition_digest BINARY(32) NOT NULL,
    PRIMARY KEY (character_id, mission_id, definition_version),
    CONSTRAINT me_ref_character FOREIGN KEY (character_id)
        REFERENCES mission_engine_characters (character_id) ON UPDATE RESTRICT ON DELETE RESTRICT,
    CONSTRAINT me_ref_definition FOREIGN KEY (mission_id, definition_version, definition_digest)
        REFERENCES mission_engine_definitions (mission_id, definition_version, definition_digest)
        ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS mission_engine_operations (
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
        REFERENCES mission_engine_characters (character_id) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB;

-- A future schema version is never silently reset by reapplying this file.
INSERT INTO mission_engine_schema (singleton, schema_version) VALUES (1, 1)
ON DUPLICATE KEY UPDATE singleton = singleton;
