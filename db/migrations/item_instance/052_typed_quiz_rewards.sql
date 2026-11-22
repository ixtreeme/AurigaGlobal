-- One open quiz round per shared player database. A claim and its native
-- inventory write or pending account award use the same InnoDB transaction.
CREATE TABLE IF NOT EXISTS player.item_quiz_round (
    id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
    active_slot TINYINT UNSIGNED NULL CHECK (active_slot IS NULL OR active_slot = 1),
    answer BIGINT NOT NULL,
    category_id SMALLINT UNSIGNED NOT NULL CHECK (category_id > 0),
    local_id INT UNSIGNED NOT NULL CHECK (local_id > 0),
    refinement_level SMALLINT UNSIGNED NOT NULL,
    stack_count INT UNSIGNED NOT NULL CHECK (stack_count > 0),
    winner_pid INT UNSIGNED NULL CHECK (winner_pid IS NULL OR winner_pid > 0),
    claim_operation BINARY(16) NULL,
    UNIQUE KEY item_quiz_one_open_round (active_slot),
    UNIQUE KEY item_quiz_claim_operation (claim_operation),
    CHECK ((winner_pid IS NULL) = (claim_operation IS NULL)),
    CHECK (active_slot IS NULL OR winner_pid IS NULL)
) ENGINE=InnoDB;
