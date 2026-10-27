-- Fresh unreleased schema: weapon/armor, plain accessories and ordinary material
-- bases and recovery consumables. Child tables follow in 008/010. No live upgrade.
CREATE TABLE protos.item_base (
    category_id SMALLINT UNSIGNED NOT NULL, local_id INT UNSIGNED NOT NULL,
    content_code VARCHAR(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
    name_key VARCHAR(128) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
    description_key VARCHAR(128) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
    icon_asset VARCHAR(192) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
    inventory_width TINYINT UNSIGNED NOT NULL DEFAULT 1, inventory_height TINYINT UNSIGNED NOT NULL DEFAULT 1,
    max_stack INT UNSIGNED NOT NULL DEFAULT 1, weight_grams INT UNSIGNED NOT NULL DEFAULT 0,
    min_level SMALLINT UNSIGNED NOT NULL DEFAULT 0, binding_policy TINYINT UNSIGNED NOT NULL,
    use_policy_id INT UNSIGNED NOT NULL, trade_policy_id INT UNSIGNED NOT NULL,
    refinement_profile_id INT UNSIGNED NOT NULL,
    appearance_profile_id INT UNSIGNED NULL, base_bonus_profile_id INT UNSIGNED NULL,
    currency_kind TINYINT UNSIGNED NOT NULL,
    buy_amount BIGINT UNSIGNED NOT NULL DEFAULT 0, sell_amount BIGINT UNSIGNED NOT NULL DEFAULT 0,
    retired BOOLEAN NOT NULL DEFAULT 0,
    PRIMARY KEY (category_id,local_id), UNIQUE KEY uq_item_content_code (content_code),
    FOREIGN KEY (category_id) REFERENCES protos.item_category(category_id),
    FOREIGN KEY (use_policy_id) REFERENCES protos.item_use_policy(policy_id),
    FOREIGN KEY (trade_policy_id) REFERENCES protos.item_trade_policy(policy_id),
    FOREIGN KEY (refinement_profile_id) REFERENCES protos.refinement_profile(profile_id),
    FOREIGN KEY (appearance_profile_id) REFERENCES protos.item_appearance_profile(profile_id),
    FOREIGN KEY (base_bonus_profile_id) REFERENCES protos.bonus_profile(profile_id),
    CHECK (category_id IN (1,2,3,5,6)), CHECK (local_id>0), CHECK (max_stack>0),
    CHECK (category_id IN (5,6) OR max_stack=1),
    CHECK (category_id NOT IN (5,6) OR (binding_policy<>3 AND appearance_profile_id IS NULL AND base_bonus_profile_id IS NULL)),
    CHECK (inventory_width>0), CHECK (inventory_height>0), CHECK (binding_policy IN (1,2,3)),
    CHECK (currency_kind=1), CHECK (buy_amount<=9223372036854775807), CHECK (sell_amount<=9223372036854775807),
    CHECK (CHAR_LENGTH(content_code)>0), CHECK (CHAR_LENGTH(name_key)>0), CHECK (CHAR_LENGTH(description_key)>0),
    CHECK (CHAR_LENGTH(icon_asset)>0), CHECK (retired IN (0,1))
) ENGINE=InnoDB;

CREATE TABLE protos.item_weapon (
    category_id SMALLINT UNSIGNED NOT NULL DEFAULT 1, local_id INT UNSIGNED NOT NULL,
    weapon_subtype TINYINT UNSIGNED NOT NULL, equip_slot TINYINT UNSIGNED NOT NULL DEFAULT 1,
    physical_attack_base_raw BIGINT NOT NULL, magical_attack_base_raw BIGINT NOT NULL,
    attack_profile_id INT UNSIGNED NOT NULL, ammo_policy_id INT UNSIGNED NOT NULL,
    PRIMARY KEY (category_id,local_id),
    FOREIGN KEY (category_id,local_id) REFERENCES protos.item_base(category_id,local_id),
    FOREIGN KEY (attack_profile_id) REFERENCES protos.item_attack_profile(profile_id),
    FOREIGN KEY (ammo_policy_id) REFERENCES protos.item_ammo_policy(policy_id),
    CHECK (category_id=1), CHECK (weapon_subtype BETWEEN 1 AND 7), CHECK (equip_slot=1),
    CHECK (physical_attack_base_raw BETWEEN 0 AND 1000000000000000),
    CHECK (magical_attack_base_raw BETWEEN 0 AND 1000000000000000)
) ENGINE=InnoDB;

CREATE TABLE protos.item_armor (
    category_id SMALLINT UNSIGNED NOT NULL DEFAULT 2, local_id INT UNSIGNED NOT NULL,
    equip_slot TINYINT UNSIGNED NOT NULL,
    physical_defense_base_raw BIGINT NOT NULL, magical_defense_base_raw BIGINT NOT NULL,
    resistance_profile_id INT UNSIGNED NOT NULL,
    PRIMARY KEY (category_id,local_id),
    FOREIGN KEY (category_id,local_id) REFERENCES protos.item_base(category_id,local_id),
    FOREIGN KEY (resistance_profile_id) REFERENCES protos.item_resistance_profile(profile_id),
    CHECK (category_id=2), CHECK (equip_slot IN (2,3,4,5)),
    CHECK (physical_defense_base_raw BETWEEN 0 AND 1000000000000000),
    CHECK (magical_defense_base_raw BETWEEN 0 AND 1000000000000000)
) ENGINE=InnoDB;
