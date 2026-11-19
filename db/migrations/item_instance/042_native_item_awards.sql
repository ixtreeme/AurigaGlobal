-- Native reward definitions waiting for account Mall space. A claimed award and
-- its concrete typed instances are committed together by the item writer.
CREATE TABLE player.item_native_award (
    id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
    request_id BINARY(16) NOT NULL,
    recipient_pid INT UNSIGNED NOT NULL CHECK (recipient_pid>0),
    account_id INT UNSIGNED NOT NULL CHECK (account_id>0),
    category_id SMALLINT UNSIGNED NOT NULL CHECK (category_id>0),
    local_id INT UNSIGNED NOT NULL CHECK (local_id>0),
    refinement_level SMALLINT UNSIGNED NOT NULL,
    stack_count INT UNSIGNED NOT NULL CHECK (stack_count>0),
    created_at TIMESTAMP(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6),
    claimed_operation BINARY(16) NULL,
    claimed_at TIMESTAMP(6) NULL,
    UNIQUE KEY native_award_request_member(request_id,recipient_pid),
    KEY native_award_pending(account_id,claimed_operation,id),
    CHECK ((claimed_operation IS NULL)=(claimed_at IS NULL))
) ENGINE=InnoDB;
