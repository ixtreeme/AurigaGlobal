CREATE TABLE player.daily_reward_native_offer (
    reward TINYINT UNSIGNED NOT NULL,
    slot TINYINT UNSIGNED NOT NULL,
    category_id SMALLINT UNSIGNED NOT NULL,
    local_id INT UNSIGNED NOT NULL,
    refinement_level SMALLINT UNSIGNED NOT NULL DEFAULT 0,
    stack_count INT UNSIGNED NOT NULL,
    PRIMARY KEY(reward,slot),
    CHECK(reward<7), CHECK(slot<30),
    CHECK(category_id>0), CHECK(local_id>0), CHECK(stack_count>0)
) ENGINE=InnoDB;
