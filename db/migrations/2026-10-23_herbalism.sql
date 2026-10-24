-- Offline, backed-up deployment only. Repurposes swords 230..239 as shears.
-- All twelve herb items must already exist. No player items are deleted.
USE player;
DELIMITER $$
DROP PROCEDURE IF EXISTS migrate_herbalism$$
CREATE PROCEDURE migrate_herbalism()
migration: BEGIN
    DECLARE species INT DEFAULT 0;
    DECLARE EXIT HANDLER FOR SQLEXCEPTION BEGIN ROLLBACK; RESIGNAL; END;
    IF (SELECT COUNT(*) FROM item_proto WHERE vnum BETWEEN 230 AND 239) <> 10 THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Missing pruning-shears source items';
    END IF;
    IF (SELECT COUNT(*) FROM item_proto WHERE vnum BETWEEN 50701 AND 50712) <> 12 THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Missing herb items';
    END IF;
    IF NOT EXISTS(SELECT 1 FROM mob_proto WHERE vnum=20047 AND type=1) THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Missing neutral resource template';
    END IF;
    IF EXISTS(SELECT 1 FROM mob_proto WHERE vnum BETWEEN 34800 AND 34811
              AND name<>CONCAT('Herbalism_',50701+vnum-34800)) THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Herbalism NPC range is occupied';
    END IF;
    CREATE TABLE IF NOT EXISTS herbalism_item_proto_backup_20261023 LIKE item_proto;
    INSERT IGNORE INTO herbalism_item_proto_backup_20261023
        SELECT * FROM item_proto WHERE vnum BETWEEN 230 AND 239;
    DROP TEMPORARY TABLE IF EXISTS herbalism_plant_template;
    CREATE TEMPORARY TABLE herbalism_plant_template LIKE mob_proto;
    INSERT INTO herbalism_plant_template SELECT * FROM mob_proto WHERE vnum=20047;
    START TRANSACTION;
    WHILE species<12 DO
        UPDATE herbalism_plant_template t JOIN item_proto i ON i.vnum=50701+species
        SET t.vnum=34800+species,t.name=CONCAT('Herbalism_',i.vnum),
            t.locale_name=i.locale_name,t.locale_name_cz=i.locale_name_cz,
            t.locale_name_de=i.locale_name_de,t.locale_name_en=i.locale_name_en,
            t.locale_name_es=i.locale_name_es,t.locale_name_hu=i.locale_name_hu,
            t.locale_name_it=i.locale_name_it,t.locale_name_pl=i.locale_name_pl,
            t.locale_name_pt=i.locale_name_pt,t.locale_name_ro=i.locale_name_ro,t.locale_name_tr=i.locale_name_tr,
            t.type=1,t.ai_flag=2,t.on_click=0,t.max_hp=1,t.exp=0,
            t.gold_min=0,t.gold_max=0,t.drop_item=0,t.resurrection_vnum=0,t.summon=0;
        INSERT INTO mob_proto SELECT * FROM herbalism_plant_template
            WHERE NOT EXISTS(SELECT 1 FROM mob_proto WHERE vnum=34800+species);
        SET species=species+1;
    END WHILE;
    UPDATE item_proto SET name=CONCAT('Pruning Shears +',vnum-230),
        locale_name=CONCAT('Pruning Shears +',vnum-230),
        locale_name_en=CONCAT('Pruning Shears +',vnum-230),
        locale_name_hu=CONCAT('Metszőolló +',vnum-230),
        locale_name_cz=CONCAT('Zahradnické nůžky +',vnum-230),
        locale_name_de=CONCAT('Gartenschere +',vnum-230),
        locale_name_es=CONCAT('Tijeras de podar +',vnum-230),
        locale_name_it=CONCAT('Cesoie +',vnum-230),
        locale_name_pl=CONCAT('Sekator +',vnum-230),
        locale_name_pt=CONCAT('Tesoura de poda +',vnum-230),
        locale_name_ro=CONCAT('Foarfecă de grădină +',vnum-230),
        locale_name_tr=CONCAT('Budama makası +',vnum-230),
        type=1,subtype=0,size=1,antiflag=0,flag=1,wearflag=16,
        refined_vnum=0,refine_set=0,addon_type=0,
        limittype0=0,limitvalue0=0,limittype1=0,limitvalue1=0,
        applytype0=0,applyvalue0=0,applytype1=0,applyvalue1=0,
        applytype2=0,applyvalue2=0,applytype3=0,applyvalue3=0,applytype4=0,applyvalue4=0,
        value0=0,value1=0,value2=0,value3=0,value4=0,value5=0,socket_pct=0
      WHERE vnum BETWEEN 230 AND 239;
    COMMIT;
    DROP TEMPORARY TABLE herbalism_plant_template;
END$$
CALL migrate_herbalism()$$
DROP PROCEDURE migrate_herbalism$$
DELIMITER ;
