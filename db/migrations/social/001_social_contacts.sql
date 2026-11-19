-- Companion metadata stays in player.player. These tables store new Social actions only.
CREATE TABLE IF NOT EXISTS player.social_friend_request (
    requester_id INT UNSIGNED NOT NULL,
    recipient_id INT UNSIGNED NOT NULL,
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (requester_id,recipient_id),
    KEY recipient_created (recipient_id,created_at),
    CHECK (requester_id<>recipient_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
CREATE TABLE IF NOT EXISTS player.social_block (
    owner_id INT UNSIGNED NOT NULL,
    blocked_id INT UNSIGNED NOT NULL,
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (owner_id,blocked_id),
    KEY blocked_owner (blocked_id,owner_id),
    CHECK (owner_id<>blocked_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
