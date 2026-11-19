-- One-time upgrade from 002. Stop game writers before applying.
-- Copy and atomically rename because message UPDATE/DELETE remain forbidden.
-- The old table is retained, protected by its original append-only triggers.
CREATE TABLE player.social_chat_message_datetime LIKE player.social_chat_message;
ALTER TABLE player.social_chat_message_datetime
 DROP COLUMN created_ms,
 ADD COLUMN created_at DATETIME(3) NOT NULL COMMENT 'UTC, millisecond precision' AFTER sender_name;
CREATE TRIGGER player.social_chat_datetime_no_update BEFORE UPDATE ON player.social_chat_message_datetime FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Chat messages are append-only';
CREATE TRIGGER player.social_chat_datetime_no_delete BEFORE DELETE ON player.social_chat_message_datetime FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Chat messages are append-only';
INSERT INTO player.social_chat_message_datetime(id,kind,peer_a,peer_b,sender_id,sender_name,created_at,empire,game_master,nonce,ciphertext,auth_tag)
 SELECT id,kind,peer_a,peer_b,sender_id,sender_name,
 TIMESTAMPADD(MICROSECOND,created_ms*1000,CAST('1970-01-01 00:00:00' AS DATETIME)),
 empire,game_master,nonce,ciphertext,auth_tag FROM player.social_chat_message;
RENAME TABLE player.social_chat_message TO player.social_chat_message_epoch_backup,
 player.social_chat_message_datetime TO player.social_chat_message;
