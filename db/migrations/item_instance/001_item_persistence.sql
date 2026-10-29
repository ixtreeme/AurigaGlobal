-- Fresh, stopped target only. Not an ALTER/converter for player.item.
-- Apply after the native protos catalog. DDL is not transactionally reversible.
CREATE TABLE player.item_persistence_format (
    singleton TINYINT UNSIGNED NOT NULL PRIMARY KEY CHECK (singleton=1),
    schema_version INT UNSIGNED NOT NULL CHECK (schema_version=1),
    catalog_revision BIGINT UNSIGNED NOT NULL CHECK (catalog_revision>0),
    catalog_sha256 BINARY(32) NOT NULL
) ENGINE=InnoDB;

-- Sessions and balances are explicitly provisioned by the stopped importer /
-- future login owner. The item writer never invents owners or grants balances.
CREATE TABLE player.item_owner_fence (
    owner_kind TINYINT UNSIGNED NOT NULL CHECK (owner_kind BETWEEN 1 AND 3),
    owner_id BIGINT UNSIGNED NOT NULL CHECK (owner_id>0),
    session_generation BIGINT UNSIGNED NOT NULL CHECK (session_generation>0),
    state_revision BIGINT UNSIGNED NOT NULL CHECK (state_revision>0),
    PRIMARY KEY (owner_kind,owner_id)
) ENGINE=InnoDB;

CREATE TABLE player.item_storage (
    owner_kind TINYINT UNSIGNED NOT NULL,
    owner_id BIGINT UNSIGNED NOT NULL,
    storage_kind TINYINT UNSIGNED NOT NULL CHECK (storage_kind BETWEEN 1 AND 9),
    storage_id BIGINT UNSIGNED NOT NULL CHECK (storage_id>0),
    grid_columns SMALLINT UNSIGNED NOT NULL CHECK (grid_columns>0),
    grid_rows SMALLINT UNSIGNED NOT NULL CHECK (grid_rows>0),
    grid_pages SMALLINT UNSIGNED NOT NULL CHECK (grid_pages>0),
    PRIMARY KEY (owner_kind,owner_id,storage_kind,storage_id),
    FOREIGN KEY (owner_kind,owner_id) REFERENCES player.item_owner_fence(owner_kind,owner_id),
    CHECK (CAST(grid_columns AS UNSIGNED)*grid_rows*grid_pages<=65536),
    CHECK (storage_kind<>2 OR (grid_rows=1 AND grid_pages=1)),
    CHECK ((storage_kind IN (1,2,6) AND owner_kind=1) OR
           (storage_kind IN (3,4) AND owner_kind=2) OR
           (storage_kind=5 AND owner_kind=3) OR storage_kind IN (7,8,9))
) ENGINE=InnoDB;

CREATE TABLE player.item_wallet (
    owner_kind TINYINT UNSIGNED NOT NULL,
    owner_id BIGINT UNSIGNED NOT NULL,
    currency TINYINT UNSIGNED NOT NULL,
    state_revision BIGINT UNSIGNED NOT NULL CHECK (state_revision>0),
    balance BIGINT UNSIGNED NOT NULL CHECK (balance<=9223372036854775807),
    PRIMARY KEY (owner_kind,owner_id,currency),
    FOREIGN KEY (owner_kind,owner_id) REFERENCES player.item_owner_fence(owner_kind,owner_id),
    CHECK ((currency=1 AND owner_kind=1) OR (currency=2 AND owner_kind=2))
) ENGINE=InnoDB;

CREATE TABLE player.item_operation_receipt (
    operation_id BINARY(16) NOT NULL PRIMARY KEY,
    request_sha256 BINARY(32) NOT NULL,
    catalog_revision BIGINT UNSIGNED NOT NULL CHECK (catalog_revision>0),
    catalog_sha256 BINARY(32) NOT NULL,
    operation_kind TINYINT UNSIGNED NOT NULL CHECK (operation_kind BETWEEN 1 AND 6),
    outcome_code SMALLINT UNSIGNED NOT NULL CHECK (outcome_code>0),
    result_payload VARBINARY(4096) NOT NULL
) ENGINE=InnoDB;

CREATE TABLE player.item_instance (
    instance_id BIGINT UNSIGNED NOT NULL PRIMARY KEY CHECK (instance_id>0),
    category_id SMALLINT UNSIGNED NOT NULL,
    local_id INT UNSIGNED NOT NULL,
    owner_kind TINYINT UNSIGNED NOT NULL,
    owner_id BIGINT UNSIGNED NOT NULL,
    state_revision BIGINT UNSIGNED NOT NULL CHECK (state_revision>0),
    refinement_level SMALLINT UNSIGNED NOT NULL CHECK (refinement_level<=255),
    stack_count INT UNSIGNED NOT NULL,
    destroyed TINYINT UNSIGNED NOT NULL CHECK (destroyed IN (0,1)),
    bound_kind TINYINT UNSIGNED NULL,
    bound_id BIGINT UNSIGNED NULL,
    storage_kind TINYINT UNSIGNED NULL,
    storage_id BIGINT UNSIGNED NULL,
    anchor_cell INT UNSIGNED NULL,
    locked_attribute TINYINT UNSIGNED NULL CHECK (locked_attribute<5),
    accessory_capacity TINYINT UNSIGNED NULL,
    accessory_grade TINYINT UNSIGNED NULL,
    accessory_remaining_ms BIGINT UNSIGNED NULL,
    accessory_permanent TINYINT UNSIGNED NULL,
    last_operation BINARY(16) NOT NULL,
    state_sha256 BINARY(32) NOT NULL,
    KEY ix_item_owner (owner_kind,owner_id,destroyed,instance_id),
    FOREIGN KEY (category_id,local_id) REFERENCES protos.item_base(category_id,local_id),
    FOREIGN KEY (owner_kind,owner_id) REFERENCES player.item_owner_fence(owner_kind,owner_id),
    FOREIGN KEY (bound_kind,bound_id) REFERENCES player.item_owner_fence(owner_kind,owner_id),
    FOREIGN KEY (owner_kind,owner_id,storage_kind,storage_id)
      REFERENCES player.item_storage(owner_kind,owner_id,storage_kind,storage_id),
    FOREIGN KEY (last_operation) REFERENCES player.item_operation_receipt(operation_id),
    CHECK ((bound_kind IS NULL AND bound_id IS NULL) OR (bound_kind IS NOT NULL AND bound_id IS NOT NULL AND bound_kind BETWEEN 1 AND 3 AND bound_id>0)),
    CHECK ((destroyed=1 AND stack_count=0 AND storage_kind IS NULL AND storage_id IS NULL AND anchor_cell IS NULL) OR
           (destroyed=0 AND stack_count>0 AND storage_kind IS NOT NULL AND storage_id IS NOT NULL AND storage_id>0 AND anchor_cell IS NOT NULL)),
    CHECK ((accessory_capacity IS NULL AND accessory_grade IS NULL AND accessory_remaining_ms IS NULL AND accessory_permanent IS NULL) OR
           (category_id=3 AND accessory_capacity IS NOT NULL AND accessory_grade IS NOT NULL AND accessory_grade<=accessory_capacity AND
            accessory_remaining_ms IS NOT NULL AND accessory_remaining_ms<=9223372036854775807 AND accessory_permanent IS NOT NULL AND accessory_permanent IN (0,1)))
) ENGINE=InnoDB;

CREATE TABLE player.item_instance_attribute (
    instance_id BIGINT UNSIGNED NOT NULL,
    slot_index TINYINT UNSIGNED NOT NULL CHECK (slot_index<8),
    bonus_id INT UNSIGNED NOT NULL,
    value_scaled BIGINT NOT NULL,
    PRIMARY KEY (instance_id,slot_index),
    FOREIGN KEY (instance_id) REFERENCES player.item_instance(instance_id),
    FOREIGN KEY (bonus_id) REFERENCES protos.bonus_definition(bonus_id)
) ENGINE=InnoDB;

CREATE TABLE player.item_instance_socket (
    instance_id BIGINT UNSIGNED NOT NULL,
    slot_index TINYINT UNSIGNED NOT NULL CHECK (slot_index<3),
    socket_state TINYINT UNSIGNED NOT NULL CHECK (socket_state BETWEEN 1 AND 4),
    socket_grade TINYINT UNSIGNED NOT NULL CHECK (socket_grade<=2),
    stone_category SMALLINT UNSIGNED NULL,
    stone_local INT UNSIGNED NULL,
    stone_level SMALLINT UNSIGNED NOT NULL CHECK (stone_level<=255),
    PRIMARY KEY (instance_id,slot_index),
    FOREIGN KEY (instance_id) REFERENCES player.item_instance(instance_id),
    FOREIGN KEY (stone_category,stone_local) REFERENCES protos.item_base(category_id,local_id),
    CHECK ((socket_state=3 AND socket_grade>0 AND stone_category IS NOT NULL AND stone_local IS NOT NULL AND stone_category=7 AND stone_local>0) OR
           (socket_state<>3 AND stone_category IS NULL AND stone_local IS NULL AND stone_level=0)),
    CHECK ((socket_state=1 AND socket_grade=0) OR (socket_state<>1 AND socket_grade>0))
) ENGINE=InnoDB;

-- All footprint cells are reserved, not just the anchor. No FK cascade hiding
-- partial cleanup; the bounded transaction explicitly replaces every row.
CREATE TABLE player.item_occupied_cell (
    owner_kind TINYINT UNSIGNED NOT NULL,
    owner_id BIGINT UNSIGNED NOT NULL,
    storage_kind TINYINT UNSIGNED NOT NULL,
    storage_id BIGINT UNSIGNED NOT NULL,
    cell_index INT UNSIGNED NOT NULL CHECK (cell_index<65536),
    instance_id BIGINT UNSIGNED NOT NULL,
    PRIMARY KEY (owner_kind,owner_id,storage_kind,storage_id,cell_index),
    UNIQUE KEY uq_item_cell (instance_id,cell_index),
    FOREIGN KEY (owner_kind,owner_id,storage_kind,storage_id)
      REFERENCES player.item_storage(owner_kind,owner_id,storage_kind,storage_id),
    FOREIGN KEY (instance_id) REFERENCES player.item_instance(instance_id)
) ENGINE=InnoDB;
