-- Resource NPC prototypes for the typed woodcutting and herbalism systems.
-- Run against the player database while the game cores are stopped.
DROP PROCEDURE IF EXISTS prepare_field_gathering;
DELIMITER //
CREATE PROCEDURE prepare_field_gathering()
BEGIN
    DECLARE species INT DEFAULT 0;
    DECLARE EXIT HANDLER FOR SQLEXCEPTION
    BEGIN
        ROLLBACK;
        DROP TEMPORARY TABLE IF EXISTS field_gathering_template;
        RESIGNAL;
    END;
    IF NOT EXISTS (SELECT 1 FROM mob_proto WHERE vnum = 20047 AND type = 1) THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Missing resource NPC template';
    END IF;
    IF EXISTS (
        SELECT 1 FROM mob_proto
        WHERE (vnum = 34090 AND name <> 'Woodcutting Pine')
           OR (vnum BETWEEN 34800 AND 34811
               AND NOT (type = 1 AND max_hp = 1 AND on_click = 0
                   AND (name = CONCAT('Herbalism Plant ', vnum - 34799)
                        OR LEFT(name, 10) = 'Herbalism_')))
    ) THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Field gathering NPC ID already in use';
    END IF;

    CREATE TEMPORARY TABLE field_gathering_template LIKE mob_proto;
    INSERT INTO field_gathering_template SELECT * FROM mob_proto WHERE vnum = 20047;
    UPDATE field_gathering_template SET vnum = 34090, name = 'Woodcutting Pine', locale_name = 'Pine',
        locale_name_en = 'Pine', locale_name_hu = 'Fenyő',
        type = 1, ai_flag = 2, on_click = 0, max_hp = 1, exp = 0,
        gold_min = 0, gold_max = 0, drop_item = 0, resurrection_vnum = 0, summon = 0;

    START TRANSACTION;
    INSERT INTO mob_proto SELECT * FROM field_gathering_template
        WHERE NOT EXISTS (SELECT 1 FROM mob_proto WHERE vnum = 34090);
    WHILE species < 12 DO
        UPDATE field_gathering_template SET vnum = 34800 + species,
            name = CONCAT('Herbalism Plant ', 1 + species),
            locale_name = CONCAT('Herbalism Plant ', 1 + species),
            locale_name_en = CONCAT('Herbalism Plant ', 1 + species),
            locale_name_hu = CONCAT('Gyógynövény ', 1 + species);
        INSERT INTO mob_proto SELECT * FROM field_gathering_template
            WHERE NOT EXISTS (SELECT 1 FROM mob_proto WHERE vnum = 34800 + species);
        SET species = species + 1;
    END WHILE;
    DROP TEMPORARY TABLE field_gathering_template;
    COMMIT;
END//
DELIMITER ;
CALL prepare_field_gathering();
DROP PROCEDURE prepare_field_gathering;
