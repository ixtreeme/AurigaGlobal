CREATE TABLE IF NOT EXISTS player.item_guild_master_publication (
    guild_id INT UNSIGNED NOT NULL,
    operation_id BINARY(16) NOT NULL,
    PRIMARY KEY (guild_id)
) ENGINE=InnoDB;
