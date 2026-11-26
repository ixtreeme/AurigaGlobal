-- Native Mission Pass. No legacy item ID, mail or prototype table is required.
CREATE TABLE IF NOT EXISTS player.native_mission_pass_account (
 account_id INT UNSIGNED NOT NULL,season_id INT UNSIGNED NOT NULL,
 xp BIGINT UNSIGNED NOT NULL DEFAULT 0,last_event_sequence BIGINT UNSIGNED NOT NULL DEFAULT 0,
 revision BIGINT UNSIGNED NOT NULL DEFAULT 0,premium_activated_at DATETIME NULL,state_json LONGTEXT NULL,
 PRIMARY KEY(account_id,season_id)
) ENGINE=InnoDB;
CREATE TABLE IF NOT EXISTS player.native_mission_pass_receipt (
 account_id INT UNSIGNED NOT NULL,season_id INT UNSIGNED NOT NULL,
 token VARCHAR(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
 PRIMARY KEY(account_id,season_id,token)
) ENGINE=InnoDB;
CREATE TABLE IF NOT EXISTS player.native_mission_pass_season (
 season_id INT UNSIGNED NOT NULL PRIMARY KEY,starts_at BIGINT NOT NULL,ends_at BIGINT NOT NULL,
 xp_per_level INT UNSIGNED NOT NULL CHECK(xp_per_level>0),levels INT UNSIGNED NOT NULL CHECK(levels BETWEEN 1 AND 100),
 enabled BOOLEAN NOT NULL,contract_sha256 BINARY(32) NOT NULL
) ENGINE=InnoDB;
CREATE TABLE IF NOT EXISTS player.native_mission_pass_reward (
 season_id INT UNSIGNED NOT NULL,reward_level SMALLINT UNSIGNED NOT NULL,
 track TINYINT UNSIGNED NOT NULL CHECK(track<=1),
 category_id SMALLINT UNSIGNED NOT NULL CHECK(category_id>0),local_id INT UNSIGNED NOT NULL CHECK(local_id>0),
 refinement_level SMALLINT UNSIGNED NOT NULL CHECK(refinement_level<=255),item_count INT UNSIGNED NOT NULL CHECK(item_count BETWEEN 1 AND 200),
 PRIMARY KEY(season_id,reward_level,track)
) ENGINE=InnoDB;
CREATE TABLE IF NOT EXISTS player.native_mission_pass_claim (
 account_id INT UNSIGNED NOT NULL,season_id INT UNSIGNED NOT NULL,reward_level SMALLINT UNSIGNED NOT NULL,
 track TINYINT UNSIGNED NOT NULL,request_id BINARY(16) NOT NULL,recipient_pid INT UNSIGNED NOT NULL,
 category_id SMALLINT UNSIGNED NOT NULL,local_id INT UNSIGNED NOT NULL,refinement_level SMALLINT UNSIGNED NOT NULL,
 item_count INT UNSIGNED NOT NULL,created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
 PRIMARY KEY(account_id,season_id,reward_level,track),UNIQUE KEY native_pass_award(request_id)
) ENGINE=InnoDB;
DELIMITER $$
DROP PROCEDURE IF EXISTS player.native_mission_pass_snapshot$$
CREATE PROCEDURE player.native_mission_pass_snapshot(IN a INT UNSIGNED,IN s INT UNSIGNED)
BEGIN
 IF a=0 OR NOT EXISTS(SELECT 1 FROM account.account WHERE id=a) THEN
   SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Invalid Mission Pass account';
 END IF;
 INSERT IGNORE INTO player.native_mission_pass_account(account_id,season_id) VALUES(a,s);
 SELECT 'ok',COALESCE(state_json,''),revision,COALESCE(UNIX_TIMESTAMP(premium_activated_at),0),xp,
   COALESCE((SELECT JSON_ARRAYAGG(JSON_ARRAY(reward_level,track))
       FROM player.native_mission_pass_claim o WHERE o.account_id=a AND o.season_id=s),'[]'),
   COALESCE((SELECT JSON_ARRAYAGG(JSON_ARRAY(reward_level,track,category_id,local_id,refinement_level,item_count))
       FROM player.native_mission_pass_reward r WHERE r.season_id=s),'[]'),
   (SELECT starts_at FROM player.native_mission_pass_season WHERE season_id=s),
   (SELECT ends_at FROM player.native_mission_pass_season WHERE season_id=s),
   (SELECT xp_per_level FROM player.native_mission_pass_season WHERE season_id=s),
   (SELECT levels FROM player.native_mission_pass_season WHERE season_id=s),
   (SELECT LOWER(HEX(contract_sha256)) FROM player.native_mission_pass_season WHERE season_id=s)

 FROM player.native_mission_pass_account WHERE account_id=a AND season_id=s;
END$$
DROP PROCEDURE IF EXISTS player.native_mission_pass_commit$$
CREATE PROCEDURE player.native_mission_pass_commit(IN a INT UNSIGNED,IN s INT UNSIGNED,
 IN expected_revision BIGINT UNSIGNED,IN token_value VARCHAR(64),IN body LONGTEXT,IN expected_contract CHAR(64))
commit_work: BEGIN
 DECLARE current_revision BIGINT UNSIGNED;
 DECLARE accepted VARCHAR(16) DEFAULT 'conflict';
 DECLARE pinned BINARY(32);
 DECLARE EXIT HANDLER FOR SQLEXCEPTION BEGIN ROLLBACK; RESIGNAL; END;
 START TRANSACTION;
 SELECT contract_sha256 INTO pinned FROM player.native_mission_pass_season WHERE season_id=s LOCK IN SHARE MODE;
 IF pinned IS NULL OR pinned<>UNHEX(expected_contract) THEN
   SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Native Mission Pass contract changed';
 END IF;
 INSERT IGNORE INTO player.native_mission_pass_account(account_id,season_id) VALUES(a,s);
 SELECT revision INTO current_revision FROM player.native_mission_pass_account WHERE account_id=a AND season_id=s FOR UPDATE;
 IF EXISTS(SELECT 1 FROM player.native_mission_pass_receipt WHERE account_id=a AND season_id=s AND token=token_value) THEN
   SET accepted='ok';
 ELSEIF current_revision=expected_revision THEN
   IF NOT JSON_VALID(body) OR COALESCE(JSON_VALUE(body,'$.account'),-1)<>a OR COALESCE(JSON_VALUE(body,'$.season'),-1)<>s THEN
     SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Invalid Mission Pass state';
   END IF;
   UPDATE player.native_mission_pass_account SET state_json=body,xp=JSON_VALUE(body,'$.xp'),
       last_event_sequence=JSON_VALUE(body,'$.sequence'),revision=revision+1
     WHERE account_id=a AND season_id=s;
   INSERT INTO player.native_mission_pass_receipt(account_id,season_id,token) VALUES(a,s,token_value);
   SET accepted='ok';
 END IF;
 COMMIT;
 SELECT accepted,COALESCE(state_json,''),revision,COALESCE(UNIX_TIMESTAMP(premium_activated_at),0),xp,
   COALESCE((SELECT JSON_ARRAYAGG(JSON_ARRAY(reward_level,track))
       FROM player.native_mission_pass_claim o WHERE o.account_id=a AND o.season_id=s),'[]'),
   COALESCE((SELECT JSON_ARRAYAGG(JSON_ARRAY(reward_level,track,category_id,local_id,refinement_level,item_count))
       FROM player.native_mission_pass_reward r WHERE r.season_id=s),'[]'),
   (SELECT starts_at FROM player.native_mission_pass_season WHERE season_id=s),
   (SELECT ends_at FROM player.native_mission_pass_season WHERE season_id=s),
   (SELECT xp_per_level FROM player.native_mission_pass_season WHERE season_id=s),
   (SELECT levels FROM player.native_mission_pass_season WHERE season_id=s),
   (SELECT LOWER(HEX(contract_sha256)) FROM player.native_mission_pass_season WHERE season_id=s)

 FROM player.native_mission_pass_account WHERE account_id=a AND season_id=s;
END$$
DROP PROCEDURE IF EXISTS player.native_mission_pass_claim$$
CREATE PROCEDURE player.native_mission_pass_claim(IN a INT UNSIGNED,IN s INT UNSIGNED,IN pid INT UNSIGNED,
 IN wanted_level INT UNSIGNED,IN wanted_track INT UNSIGNED,
 IN expected_category INT UNSIGNED,IN expected_local INT UNSIGNED,IN expected_refinement INT UNSIGNED,IN expected_count INT UNSIGNED,IN expected_contract CHAR(64))
claim_work: BEGIN
 DECLARE points BIGINT UNSIGNED; DECLARE premium DATETIME;
 DECLARE start_time BIGINT; DECLARE end_time BIGINT; DECLARE step_xp INT;
 DECLARE native_request BINARY(16);
 DECLARE EXIT HANDLER FOR SQLEXCEPTION BEGIN ROLLBACK; RESIGNAL; END;
 START TRANSACTION;
 SELECT xp,premium_activated_at INTO points,premium FROM player.native_mission_pass_account
   WHERE account_id=a AND season_id=s FOR UPDATE;
 SELECT starts_at,ends_at,xp_per_level INTO start_time,end_time,step_xp FROM player.native_mission_pass_season WHERE season_id=s AND enabled=1 AND contract_sha256=UNHEX(expected_contract) FOR UPDATE;
 IF start_time IS NULL OR UNIX_TIMESTAMP()<start_time OR UNIX_TIMESTAMP()>=end_time THEN
   ROLLBACK; SELECT 'MP_OFFLINE'; LEAVE claim_work;
 END IF;
 IF wanted_level=0 OR points IS NULL OR points<CAST(wanted_level AS UNSIGNED)*CAST(step_xp AS UNSIGNED) THEN
   ROLLBACK; SELECT 'MP_LOCKED'; LEAVE claim_work;
 END IF;
 IF wanted_track>1 OR (wanted_track=1 AND (premium IS NULL OR UNIX_TIMESTAMP(premium)<start_time OR premium>NOW())) THEN
   ROLLBACK; SELECT 'MP_PREMIUM_UNAVAILABLE'; LEAVE claim_work;
 END IF;
 IF EXISTS(SELECT 1 FROM player.native_mission_pass_claim WHERE account_id=a AND season_id=s AND reward_level=wanted_level AND track=wanted_track) THEN
   ROLLBACK; SELECT 'MP_CLAIMED'; LEAVE claim_work;
 END IF;
 IF NOT EXISTS(SELECT 1 FROM player.player WHERE id=pid AND account_id=a) OR
    NOT EXISTS(SELECT 1 FROM player.native_mission_pass_reward WHERE season_id=s AND reward_level=wanted_level AND track=wanted_track
      AND category_id=expected_category AND local_id=expected_local AND refinement_level=expected_refinement AND item_count=expected_count) THEN
   ROLLBACK; SELECT 'MP_DATABASE_ERROR'; LEAVE claim_work;
 END IF;
 SET native_request=UNHEX(SUBSTR(SHA2(CONCAT('auriga-native-pass-v2/',a,'/',s,'/',wanted_level,'/',wanted_track),256),1,32));
 INSERT INTO player.native_mission_pass_claim(account_id,season_id,reward_level,track,request_id,recipient_pid,category_id,local_id,refinement_level,item_count)
   VALUES(a,s,wanted_level,wanted_track,native_request,pid,expected_category,expected_local,expected_refinement,expected_count);
 INSERT INTO player.item_native_award(request_id,recipient_pid,account_id,category_id,local_id,refinement_level,stack_count)
   VALUES(native_request,pid,a,expected_category,expected_local,expected_refinement,expected_count);
 COMMIT;
 SELECT 'MP_REWARD_SENT';
END$$
DELIMITER ;
