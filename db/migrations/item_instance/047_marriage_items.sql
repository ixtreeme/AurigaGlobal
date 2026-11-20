ALTER TABLE player.marriage
    ADD COLUMN IF NOT EXISTS typed_items TINYINT UNSIGNED NOT NULL DEFAULT 0,
    ADD INDEX IF NOT EXISTS marriage_partner (pid2);

CREATE TABLE IF NOT EXISTS player.item_marriage_publication (
    id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    operation_id BINARY(16) NOT NULL,
    pid1 INT UNSIGNED NOT NULL,
    pid2 INT UNSIGNED NOT NULL,
    engage TINYINT UNSIGNED NOT NULL,
    PRIMARY KEY (id),
    UNIQUE KEY item_marriage_operation (operation_id)
) ENGINE=InnoDB;
