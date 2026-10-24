-- Apply after the base migration. Transactional procedures execute off-thread.
USE player;
ALTER TABLE account_mission_pass ADD COLUMN IF NOT EXISTS state_json LONGTEXT NULL;
CREATE TABLE IF NOT EXISTS mission_pass_receipt (
 account_id INT UNSIGNED NOT NULL,season_id INT UNSIGNED NOT NULL,
 token VARCHAR(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
 PRIMARY KEY(account_id,season_id,token)
) ENGINE=InnoDB;
CREATE TABLE IF NOT EXISTS mission_pass_season (
 season_id INT UNSIGNED NOT NULL PRIMARY KEY,starts_at BIGINT NOT NULL,ends_at BIGINT NOT NULL,
 xp_per_level INT UNSIGNED NOT NULL,levels INT UNSIGNED NOT NULL,enabled TINYINT UNSIGNED NOT NULL
) ENGINE=InnoDB;
CREATE TABLE IF NOT EXISTS mission_pass_reward (
 season_id INT UNSIGNED NOT NULL,reward_level SMALLINT UNSIGNED NOT NULL,
 track TINYINT UNSIGNED NOT NULL,item_vnum INT UNSIGNED NOT NULL,item_count INT UNSIGNED NOT NULL,
 PRIMARY KEY(season_id,reward_level,track)
) ENGINE=InnoDB;
DELIMITER $$
DROP PROCEDURE IF EXISTS mission_pass_snapshot$$
CREATE PROCEDURE mission_pass_snapshot(IN a INT UNSIGNED,IN s INT UNSIGNED)
BEGIN
 IF a=0 OR NOT EXISTS(SELECT 1 FROM account.account WHERE id=a) THEN
   SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Invalid Mission Pass account';
 END IF;
 INSERT IGNORE INTO account_mission_pass(account_id,season_id) VALUES(a,s);
 SELECT 'ok',COALESCE(state_json,''),revision,COALESCE(UNIX_TIMESTAMP(premium_activated_at),0),xp,
   COALESCE((SELECT JSON_ARRAYAGG(JSON_ARRAY(reward_level,track))
       FROM mission_pass_reward_outbox o WHERE o.account_id=a AND o.season_id=s),'[]'),
   COALESCE((SELECT JSON_ARRAYAGG(JSON_ARRAY(reward_level,track,item_vnum,item_count))
       FROM mission_pass_reward r WHERE r.season_id=s),'[]')
 FROM account_mission_pass WHERE account_id=a AND season_id=s;
END$$
DROP PROCEDURE IF EXISTS mission_pass_commit$$
CREATE PROCEDURE mission_pass_commit(IN a INT UNSIGNED,IN s INT UNSIGNED,
 IN expected_revision BIGINT UNSIGNED,IN token_value VARCHAR(64),IN body LONGTEXT)
commit_work: BEGIN
 DECLARE current_revision BIGINT UNSIGNED;
 DECLARE accepted VARCHAR(16) DEFAULT 'conflict';
 DECLARE EXIT HANDLER FOR SQLEXCEPTION BEGIN ROLLBACK; RESIGNAL; END;
 START TRANSACTION;
 INSERT IGNORE INTO account_mission_pass(account_id,season_id) VALUES(a,s);
 SELECT revision INTO current_revision FROM account_mission_pass WHERE account_id=a AND season_id=s FOR UPDATE;
 IF EXISTS(SELECT 1 FROM mission_pass_receipt WHERE account_id=a AND season_id=s AND token=token_value) THEN
   SET accepted='ok';
 ELSEIF current_revision=expected_revision THEN
   IF NOT JSON_VALID(body) OR COALESCE(JSON_VALUE(body,'$.account'),-1)<>a OR COALESCE(JSON_VALUE(body,'$.season'),-1)<>s THEN
     SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Invalid Mission Pass state';
   END IF;
   UPDATE account_mission_pass SET state_json=body,xp=JSON_VALUE(body,'$.xp'),
       last_event_sequence=JSON_VALUE(body,'$.sequence'),revision=revision+1
     WHERE account_id=a AND season_id=s;
   INSERT INTO mission_pass_receipt(account_id,season_id,token) VALUES(a,s,token_value);
   SET accepted='ok';
 END IF;
 COMMIT;
 SELECT accepted,COALESCE(state_json,''),revision,COALESCE(UNIX_TIMESTAMP(premium_activated_at),0),xp,
   COALESCE((SELECT JSON_ARRAYAGG(JSON_ARRAY(reward_level,track))
       FROM mission_pass_reward_outbox o WHERE o.account_id=a AND o.season_id=s),'[]'),
   COALESCE((SELECT JSON_ARRAYAGG(JSON_ARRAY(reward_level,track,item_vnum,item_count))
       FROM mission_pass_reward r WHERE r.season_id=s),'[]')
 FROM account_mission_pass WHERE account_id=a AND season_id=s;
END$$
DROP PROCEDURE IF EXISTS mission_pass_claim$$
CREATE PROCEDURE mission_pass_claim(IN a INT UNSIGNED,IN s INT UNSIGNED,
 IN wanted_level INT UNSIGNED,IN wanted_track INT UNSIGNED)
claim_work: BEGIN
 DECLARE points BIGINT UNSIGNED;
 DECLARE premium DATETIME;
 DECLARE character_login VARCHAR(31);
 DECLARE start_time BIGINT; DECLARE end_time BIGINT; DECLARE step_xp INT;
 DECLARE reward_item INT UNSIGNED; DECLARE reward_count INT UNSIGNED;
 DECLARE EXIT HANDLER FOR SQLEXCEPTION BEGIN ROLLBACK; RESIGNAL; END;
 START TRANSACTION;
 SELECT xp,premium_activated_at INTO points,premium FROM account_mission_pass
   WHERE account_id=a AND season_id=s FOR UPDATE;
 SELECT starts_at,ends_at,xp_per_level INTO start_time,end_time,step_xp FROM mission_pass_season
   WHERE season_id=s AND enabled=1;
 IF start_time IS NULL OR UNIX_TIMESTAMP()<start_time OR UNIX_TIMESTAMP()>=end_time THEN
   ROLLBACK; SELECT 'MP_OFFLINE'; LEAVE claim_work;
 END IF;
 IF wanted_level=0 OR points IS NULL OR points<CAST(wanted_level AS UNSIGNED)*CAST(step_xp AS UNSIGNED) THEN
   ROLLBACK; SELECT 'MP_LOCKED'; LEAVE claim_work;
 END IF;
 IF wanted_track>1 OR (wanted_track=1 AND (premium IS NULL OR UNIX_TIMESTAMP(premium)<start_time OR premium>NOW())) THEN
   ROLLBACK; SELECT 'MP_PREMIUM_UNAVAILABLE'; LEAVE claim_work;
 END IF;
 SELECT item_vnum,item_count INTO reward_item,reward_count FROM mission_pass_reward
   WHERE season_id=s AND reward_level=wanted_level AND track=wanted_track;
 IF reward_item IS NULL THEN ROLLBACK; SELECT 'MP_LOCKED'; LEAVE claim_work; END IF;
 IF EXISTS(SELECT 1 FROM mission_pass_reward_outbox WHERE account_id=a AND season_id=s
    AND reward_level=wanted_level AND track=wanted_track) THEN
   ROLLBACK; SELECT 'MP_CLAIMED'; LEAVE claim_work;
 END IF;
 SELECT login INTO character_login FROM account.account WHERE id=a;
 IF character_login IS NULL THEN SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Missing Mission Pass account'; END IF;
 INSERT INTO mission_pass_reward_outbox(account_id,season_id,reward_level,track,item_vnum,item_count,delivery_state,delivered_at)
   VALUES(a,s,wanted_level,wanted_track,reward_item,reward_count,1,NOW());
 -- Both writes commit together. The existing durable item-award system delivers
 -- to Item Mall storage; no GiveItem/crash window, no inventory-full item loss.
 INSERT INTO item_award(login,vnum,count,mall,why,given_time)
   VALUES(character_login,reward_item,reward_count,1,CONCAT('Auriga MP ',s,'/',wanted_level,'/',wanted_track),NOW());
 COMMIT;
 SELECT 'MP_REWARD_SENT';
END$$
DELIMITER ;
