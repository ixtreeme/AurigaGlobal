-- Additive schema only. NOT applied automatically and does not enable a season.
-- Mission Pass premium is unrelated to account premium expiration columns.
-- Use the existing player database; account_id references account.account logically
-- (cross-database foreign keys are intentionally avoided).
USE player;

CREATE TABLE IF NOT EXISTS account_mission_pass (
    account_id INT UNSIGNED NOT NULL,
    season_id INT UNSIGNED NOT NULL,
    xp BIGINT UNSIGNED NOT NULL DEFAULT 0,
    last_event_sequence BIGINT UNSIGNED NOT NULL DEFAULT 0,
    premium_activated_at DATETIME NULL,
    revision BIGINT UNSIGNED NOT NULL DEFAULT 0,
    PRIMARY KEY (account_id, season_id)
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS mission_pass_progress (
    account_id INT UNSIGNED NOT NULL,
    season_id INT UNSIGNED NOT NULL,
    mission_id INT UNSIGNED NOT NULL,
    period_start BIGINT NOT NULL COMMENT 'UTC Unix seconds; daily/weekly/season boundary',
    progress INT UNSIGNED NOT NULL DEFAULT 0,
    xp_awarded TINYINT UNSIGNED NOT NULL DEFAULT 0,
    PRIMARY KEY (account_id, season_id, mission_id, period_start),
    CONSTRAINT fk_mp_progress_account FOREIGN KEY (account_id,season_id)
        REFERENCES account_mission_pass(account_id,season_id)
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS mission_pass_subject (
    account_id INT UNSIGNED NOT NULL,
    season_id INT UNSIGNED NOT NULL,
    mission_id INT UNSIGNED NOT NULL,
    period_start BIGINT NOT NULL,
    subject_id INT UNSIGNED NOT NULL,
    PRIMARY KEY (account_id, season_id, mission_id, period_start, subject_id),
    CONSTRAINT fk_mp_subject_progress FOREIGN KEY (account_id,season_id,mission_id,period_start)
        REFERENCES mission_pass_progress(account_id,season_id,mission_id,period_start)
) ENGINE=InnoDB;

-- Reservation and delivery must be different states. An inventory-full failure
-- never creates a reservation; a disconnect after reservation leaves it pending.
-- The delivery adapter MUST use this primary key as an idempotency key when
-- persisting an inventory/item-award record, not a fire-and-forget GiveItem call.
CREATE TABLE IF NOT EXISTS mission_pass_reward_outbox (
    account_id INT UNSIGNED NOT NULL,
    season_id INT UNSIGNED NOT NULL,
    reward_level SMALLINT UNSIGNED NOT NULL,
    track TINYINT UNSIGNED NOT NULL COMMENT '0 free, 1 premium',
    item_vnum INT UNSIGNED NOT NULL,
    item_count INT UNSIGNED NOT NULL,
    delivery_state TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '0 pending, 1 delivered',
    reserved_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    delivered_at DATETIME NULL,
    PRIMARY KEY (account_id,season_id,reward_level,track),
    KEY pending_delivery (delivery_state, account_id),
    CONSTRAINT fk_mp_reward_account FOREIGN KEY (account_id,season_id)
        REFERENCES account_mission_pass(account_id,season_id)
) ENGINE=InnoDB;
