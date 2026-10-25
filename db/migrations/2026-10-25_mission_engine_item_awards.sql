-- Mission schema 2 -> 3. Item-only delivery receipt; no legacy table changes.
-- Apply after the persistence and actions migrations. Version changes LAST.
-- Award IDs remain receipts even after the award/item has been consumed/deleted.
CREATE TABLE IF NOT EXISTS mission_engine_item_awards (
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
        REFERENCES mission_engine_reward_ledger (action_id) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB;

UPDATE mission_engine_schema SET schema_version=3 WHERE singleton=1 AND schema_version=2;
