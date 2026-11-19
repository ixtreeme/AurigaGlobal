-- Private and guild messages are immutable authenticated ciphertext, never client uploads.
CREATE TABLE IF NOT EXISTS player.social_chat_sequence (singleton TINYINT UNSIGNED PRIMARY KEY, value BIGINT UNSIGNED NOT NULL) ENGINE=InnoDB;
INSERT IGNORE INTO player.social_chat_sequence(singleton,value) VALUES(1,0);
CREATE TABLE IF NOT EXISTS player.social_chat_message (
 id BIGINT UNSIGNED PRIMARY KEY,
 kind TINYINT UNSIGNED NOT NULL,
 peer_a INT UNSIGNED NOT NULL,
 peer_b INT UNSIGNED NOT NULL,
 sender_id INT UNSIGNED NOT NULL,
 sender_name VARBINARY(24) NOT NULL,
 created_ms BIGINT UNSIGNED NOT NULL,
 empire TINYINT UNSIGNED NOT NULL,
 game_master TINYINT UNSIGNED NOT NULL,
 nonce BINARY(12) NOT NULL,
 ciphertext VARBINARY(1024) NOT NULL,
 auth_tag BINARY(16) NOT NULL,
 KEY thread_page(kind,peer_a,peer_b,id),
 KEY private_recipient(kind,peer_b,id),
 CONSTRAINT social_chat_scope CHECK((kind=1 AND peer_a>0 AND peer_a<peer_b AND sender_id IN(peer_a,peer_b)) OR (kind=2 AND peer_a>0 AND peer_b=0))
) ENGINE=InnoDB;
CREATE TABLE IF NOT EXISTS player.social_chat_read (
 player_id INT UNSIGNED NOT NULL,kind TINYINT UNSIGNED NOT NULL,peer_a INT UNSIGNED NOT NULL,peer_b INT UNSIGNED NOT NULL,last_id BIGINT UNSIGNED NOT NULL DEFAULT 0,
 PRIMARY KEY(player_id,kind,peer_a,peer_b)
) ENGINE=InnoDB;
CREATE TRIGGER IF NOT EXISTS player.social_chat_no_update BEFORE UPDATE ON player.social_chat_message FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Chat messages are append-only';
CREATE TRIGGER IF NOT EXISTS player.social_chat_no_delete BEFORE DELETE ON player.social_chat_message FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Chat messages are append-only';
