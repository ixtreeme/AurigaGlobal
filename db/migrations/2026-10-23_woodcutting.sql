-- Run ONLY after a DB backup, with Database/Login/Game processes stopped.
-- Source and Unity clients must be deployed together. This migration intentionally
-- repurposes sword vnums 210..219. Inspect existing player items/shops/drops first.
-- Table postfixes are not inferred: adapt these names before running if needed.
USE player;
DELIMITER $$
DROP PROCEDURE IF EXISTS migrate_woodcutting$$
CREATE PROCEDURE migrate_woodcutting()
migration: BEGIN
    DECLARE EXIT HANDLER FOR SQLEXCEPTION
    BEGIN
        ROLLBACK;
        RESIGNAL;
    END;
    IF (SELECT COUNT(*) FROM item_proto WHERE vnum BETWEEN 210 AND 219) <> 10 THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Expected all ten original axe-range items';
    END IF;
    IF NOT EXISTS(SELECT 1 FROM item_proto WHERE vnum=90011) THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Missing wood log item 90011';
    END IF;
    IF NOT EXISTS(SELECT 1 FROM mob_proto WHERE vnum=20047 AND type=1) THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Missing neutral resource prototype 20047';
    END IF;
    IF EXISTS(SELECT 1 FROM mob_proto WHERE vnum=34090 AND name <> 'Woodcutting Pine') THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Tree vnum 34090 already belongs to another actor';
    END IF;

    -- DDL is outside the data transaction. Immutable first-run backup preserves
    -- original sword rows even when this migration is run a second time.
    CREATE TABLE IF NOT EXISTS woodcutting_item_proto_backup_20261023 LIKE item_proto;
    INSERT IGNORE INTO woodcutting_item_proto_backup_20261023
        SELECT * FROM item_proto WHERE vnum BETWEEN 210 AND 219;
    DROP TEMPORARY TABLE IF EXISTS woodcutting_tree_template;
    CREATE TEMPORARY TABLE woodcutting_tree_template LIKE mob_proto;
    INSERT INTO woodcutting_tree_template SELECT * FROM mob_proto WHERE vnum=20047;
    UPDATE woodcutting_tree_template SET vnum=34090, name='Woodcutting Pine', locale_name='Pine',
        locale_name_cz='Pine', locale_name_de='Kiefer', locale_name_en='Pine',
        locale_name_es='Pino', locale_name_hu='Fenyő', locale_name_it='Pino',
        locale_name_pl='Sosna', locale_name_pt='Pinheiro', locale_name_ro='Pin', locale_name_tr='Çam',
        type=1, ai_flag=2, on_click=0, max_hp=1, exp=0,
        gold_min=0, gold_max=0, drop_item=0, resurrection_vnum=0, summon=0;

    START TRANSACTION;
    INSERT INTO mob_proto SELECT * FROM woodcutting_tree_template
        WHERE NOT EXISTS(SELECT 1 FROM mob_proto WHERE vnum=34090);
    -- Keep the established WEAPON/SWORD wire representation so all old equipment
    -- packet layouts remain unchanged. These are tools by the dedicated vnum rules;
    -- ITEM_PICK and mining's ore/progression tables are not modified.
    UPDATE item_proto SET name=CONCAT('Woodcutting Axe +',vnum-210),
        locale_name=CONCAT('Woodcutting Axe +',vnum-210),
        locale_name_en=CONCAT('Woodcutting Axe +',vnum-210),
        locale_name_hu=CONCAT('Favágó balta +',vnum-210),
        locale_name_cz=CONCAT('Dřevorubecká sekera +',vnum-210),
        locale_name_de=CONCAT('Holzfälleraxt +',vnum-210),
        locale_name_es=CONCAT('Hacha de leñador +',vnum-210),
        locale_name_it=CONCAT('Ascia da boscaiolo +',vnum-210),
        locale_name_pl=CONCAT('Siekiera drwala +',vnum-210),
        locale_name_pt=CONCAT('Machado de lenhador +',vnum-210),
        locale_name_ro=CONCAT('Topor de tăietor +',vnum-210),
        locale_name_tr=CONCAT('Oduncu baltası +',vnum-210),
        type=1, subtype=0, size=2, antiflag=0, flag=1, wearflag=16,
        refined_vnum=0, refine_set=0, addon_type=0,
        limittype0=0, limitvalue0=0, limittype1=0, limitvalue1=0,
        applytype0=0, applyvalue0=0, applytype1=0, applyvalue1=0,
        applytype2=0, applyvalue2=0, applytype3=0, applyvalue3=0,
        applytype4=0, applyvalue4=0,
        value0=0, value1=0, value2=0, value3=0, value4=0, value5=0,
        socket_pct=0
      WHERE vnum BETWEEN 210 AND 219;
    COMMIT;
    DROP TEMPORARY TABLE woodcutting_tree_template;
END$$
CALL migrate_woodcutting()$$
DROP PROCEDURE migrate_woodcutting$$
DELIMITER ;

-- Rollback (with all services stopped) restores only the backed-up prototype
-- columns/rows. Do NOT delete player items, and do not blindly delete a tree row
-- if another system has subsequently adopted vnum 34090.
