CREATE TABLE IF NOT EXISTS player.item_pet_study_cooldown (
    owner_kind TINYINT UNSIGNED NOT NULL CHECK (owner_kind=1),
    owner_id BIGINT UNSIGNED NOT NULL,
    skill_id INT UNSIGNED NOT NULL CHECK (skill_id>0),
    ready_at_ms BIGINT UNSIGNED NOT NULL CHECK (ready_at_ms BETWEEN 1 AND 9223372036854775807),
    PRIMARY KEY (owner_kind,owner_id,skill_id),
    FOREIGN KEY (owner_kind,owner_id) REFERENCES player.item_owner_fence(owner_kind,owner_id)
) ENGINE=InnoDB;
