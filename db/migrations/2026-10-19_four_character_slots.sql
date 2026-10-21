-- Run with all Database/GameServer/LoginServer processes stopped, after a backup.
-- Select the player database here. For TABLE_POSTFIX use the matching table in CALL.
USE player;
DELIMITER $$
DROP PROCEDURE IF EXISTS migrate_four_character_slots$$
CREATE PROCEDURE migrate_four_character_slots(IN index_table VARCHAR(64))
migration: BEGIN
    DECLARE column_count INT DEFAULT 0;
    DECLARE EXIT HANDLER FOR SQLEXCEPTION
    BEGIN
        ROLLBACK;
        RESIGNAL;
    END;
    IF index_table IS NULL OR index_table NOT REGEXP '^player_index(_[A-Za-z0-9_]+)?$' THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Expected player_index or player_index_<postfix>';
    END IF;
    SELECT COUNT(*) INTO column_count FROM information_schema.COLUMNS
      WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = index_table AND COLUMN_NAME IN ('id','pid1','pid2','pid3','pid4');
    IF column_count <> 5 THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'The target player_index table is missing its four required slots';
    END IF;
    SELECT COUNT(*) INTO column_count FROM information_schema.COLUMNS
      WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = index_table AND COLUMN_NAME = 'pid5';
    IF column_count = 0 THEN LEAVE migration; END IF;

    SET @four_slot_sql = CONCAT('SELECT COUNT(*) INTO @four_slot_blocked FROM `', index_table,
      '` WHERE pid5 <> 0 AND pid1 <> 0 AND pid2 <> 0 AND pid3 <> 0 AND pid4 <> 0 AND pid5 NOT IN (pid1,pid2,pid3,pid4)');
    PREPARE four_slot_statement FROM @four_slot_sql;
    EXECUTE four_slot_statement;
    DEALLOCATE PREPARE four_slot_statement;
    IF @four_slot_blocked <> 0 THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Accounts with five characters require reassignment; no character mapping was changed';
    END IF;

    START TRANSACTION;
    -- Collapse an already duplicated pointer, then move each fifth character into
    -- the first free slot. No player, item, quest or other character data is deleted.
    SET @four_slot_sql = CONCAT('UPDATE `', index_table, '` SET pid5=0 WHERE pid5 IN (pid1,pid2,pid3,pid4)');
    PREPARE four_slot_statement FROM @four_slot_sql; EXECUTE four_slot_statement; DEALLOCATE PREPARE four_slot_statement;
    SET @four_slot_sql = CONCAT('UPDATE `', index_table, '` SET pid1=pid5,pid5=0 WHERE pid5<>0 AND pid1=0');
    PREPARE four_slot_statement FROM @four_slot_sql; EXECUTE four_slot_statement; DEALLOCATE PREPARE four_slot_statement;
    SET @four_slot_sql = CONCAT('UPDATE `', index_table, '` SET pid2=pid5,pid5=0 WHERE pid5<>0 AND pid2=0');
    PREPARE four_slot_statement FROM @four_slot_sql; EXECUTE four_slot_statement; DEALLOCATE PREPARE four_slot_statement;
    SET @four_slot_sql = CONCAT('UPDATE `', index_table, '` SET pid3=pid5,pid5=0 WHERE pid5<>0 AND pid3=0');
    PREPARE four_slot_statement FROM @four_slot_sql; EXECUTE four_slot_statement; DEALLOCATE PREPARE four_slot_statement;
    SET @four_slot_sql = CONCAT('UPDATE `', index_table, '` SET pid4=pid5,pid5=0 WHERE pid5<>0 AND pid4=0');
    PREPARE four_slot_statement FROM @four_slot_sql; EXECUTE four_slot_statement; DEALLOCATE PREPARE four_slot_statement;
    COMMIT;
    -- MariaDB DDL implicitly commits. With writers stopped, pid5 is now empty.
    SET @four_slot_sql = CONCAT('ALTER TABLE `', index_table, '` DROP COLUMN pid5');
    PREPARE four_slot_statement FROM @four_slot_sql; EXECUTE four_slot_statement; DEALLOCATE PREPARE four_slot_statement;
END$$
DELIMITER ;
CALL migrate_four_character_slots('player_index');
DROP PROCEDURE migrate_four_character_slots;
