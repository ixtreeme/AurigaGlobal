-- ============================================================================
-- Auriga/GameServer - teljes adatbazis sema (kodbol visszafejtve)
-- ============================================================================
-- Repo: D:\AurigaGlobal\LiveWork\AurigaGlobal
-- Bazis: a SRC/Server fa tenyleges SQL hasznalata (Database, GameServer,
--        LoginServer) + common/tables.h C++ structok + a meglovo migracios
--        SQL-ek. Ez a fajl NEM egy elo MySQL dump helyettese: a kod altal
--        elvart oszlopneveket es a kodbol kovetkeztetett tipusokat tartalmazza.
--
-- FONTOS
--  * Minden oszlopnev literalis SQL-bol szarmazik. Ahol a kod csak
--    pozicionalis INSERT-et hasznal (nincs oszloplista), ott a nev
--    `-- TODO(verify name)` jelolessel szerepel.
--  * A tipusok a C++ struct mezokbol es a printf formatumokbol
--    kovetkeztetettek; a bizonytalan tipusok `-- TODO(verify)` jelzest
--    kaptak. Ezeket elesites elott ellenorizni kell.
--  * Az `-- INCOMPLETE:` jelolesu tablaknal a kod csak az oszlopok egy
--    reszet erinti; a tobbi oszlop a valos DB-ben lehet, hogy letezik.
--  * A tablaneveknel a `%s` table postfix (server.table_postfix, alapert.
--    ures) nelkuli alapnevek szerepelnek.
--  * A build flag-ek kozul ENABLE_NEWSTUFF, ENABLE_NEW_STACK_LIMIT,
--    ENABLE_MAP_TELEPORTER, __ATTR_TRANSFER_SYSTEM__, ATTR_LOCK,
--    ENABLE_BATTLE_PASS, ENABLE_ITEMSHOP, ENABLE_EVENT_MANAGER,
--    __DUNGEON_INFO_SYSTEM__, ENABLE_DAILY_REWARD_HWID_LIMIT_RAZOR93,
--    __ENABLE_NEW_OFFLINESHOP__, ENABLE_NEW_OFFLINESHOP_LOGS,
--    __SKILL_COLOR_SYSTEM__, ENABLE_EXTRA_INVENTORY, ENABLE_SWITCHBOT,
--    ENABLE_ACCE_SYSTEM, ENABLE_HWID, LEADERBOARD_RAZOR93,
--    ADVANCED_GUILD_INFO, ENABLE_GUILD_RENEWAL_BY_RAZOR93 aktiv; a
--    kikapcsolt flag-ekhez tartozo oszlopok itt sem szerepelnek.
--  * A tipusaudit kulon fajlban: db/db_schema_audit.md
--
-- Adatbazisok: player / common / account / log
-- ============================================================================

-- ############################################################################
-- 1. PLAYER DATABASE - CORE (player, item, affect, quest, safebox, player_index)
-- ############################################################################

-- =====================================================================
-- db_player_core.sql
-- DRAFT MySQL DDL for the PLAYER database CORE tables of a Metin2-based
-- server.  Repo: D:\AurigaGlobal\LiveWork\AurigaGlobal
--
-- Generated READ-ONLY from the C++ sources; every column name below
-- comes from a literal SQL statement in the code.  Types are inferred
-- from the mapped C++ struct fields (SRC/Server/common/tables.h) and
-- from the printf conversion specifiers used to build each query.
--
-- All queries in the code append a table postfix ("player%s", "item%s",
-- ...); the base tables are therefore: player, item, affect, quest,
-- safebox, player_index (schema "player").
--
-- Build defines that are ACTIVE in this repo (SRC/Server/common/service.h,
-- CommonDefines.h) and gate optional columns below:
--   ATTR_LOCK, ENABLE_BATTLE_PASS, ENABLE_RANKING, ENABLE_EXTRA_INVENTORY,
--   ENABLE_HWID, LEADERBOARD_RAZOR93, ENABLE_GAYA_SYSTEM,
--   ENABLE_ACCE_SYSTEM, __ENABLE_EXTEND_INVEN_SYSTEM__, ENABLE_SWITCHBOT,
--   __ENABLE_NEW_OFFLINESHOP__
-- NOT active: ENABLE_GENERAL_CH (service.h:127 is commented out)
--             ENABLE_ORDER_BY_LASTPLAY (service.h:154-156 in comment)
--
-- TODO(verify): defaults/NULLability are inferred (an INSERT in
-- ClientManagerPlayer.cpp:1250-1312 omits many NOT NULL columns, so the
-- live schema must have defaults).  Column order matches the struct and
-- query order, not necessarily the real base.
-- =====================================================================

-- =====================================================================
-- TABLE: player.player
-- Column count: 74 with every build-gated column, 48 without them.
-- Evidence:
--   WRITE  SRC/Server/Database/ClientManagerPlayer.cpp:105-270
--          ("UPDATE player%s SET job=..., ... quickslot='%s' WHERE id=%u")
--   WRITE  SRC/Server/Database/ClientManagerPlayer.cpp:1250-1312
--          ("INSERT INTO player%s (id, account_id, name, ...)")
--   WRITE  SRC/Server/Database/ClientManagerLogin.cpp:603-604
--          ("UPDATE player%s SET name=?,change_name=0 WHERE id=?")
--   WRITE  SRC/Server/Database/ClientManager.cpp:1290
--          ("UPDATE player%s SET map_index=%u,x=%u,y=%u WHERE id=%u")
--   WRITE  SRC/Server/GameServer/ecs/systems/CombatSystem.cpp:5726-5730
--          (LEADERBOARD_RAZOR93: skill_victim, map1_skillmob)
--   READ   SRC/Server/Database/ClientManagerPlayer.cpp:472-517
--          (full player SELECT)
--   READ   SRC/Server/Database/ClientManagerLogin.cpp:198-222, 415-440
--          (account character-list SELECT)
--   READ   SRC/Server/GameServer/ecs/systems/CombatSystem.cpp:911,997,1079
--          (r5/r8 ordering, map1_skillmob ordering)
--   READ   SRC/Server/GameServer/social/messenger_manager.cpp:324
--          (name lookup)
--   Types: TPlayerTable tables.h:441-514, TPlayerSkill tables.h:425-430,
--          TQuickslot tables.h:419-423, THorseInfo tables.h:432-439,
--          lengths SRC/Server/common/length.h
-- INCOMPLETE: TPlayerTable::aiPremiumTimes and ::logoff_interval are
--   in-memory only (fed from CLoginData, ClientManagerPlayer.cpp:358/280)
--   and have no corresponding SQL column.
-- NOTE: player.id is proven AUTO_INCREMENT by
--   "INSERT INTO player (id, ...) VALUES(0, ...)" + uiInsertID
--   (ClientManagerPlayer.cpp:1268, 1326).
-- =====================================================================
CREATE TABLE IF NOT EXISTS `player`.`player` (
  `id`                INT UNSIGNED       NOT NULL AUTO_INCREMENT,
  `account_id`        INT UNSIGNED       NOT NULL DEFAULT 0,
  `name`              VARCHAR(24)        NOT NULL DEFAULT '',   -- char[CHARACTER_NAME_MAX_LEN+1]; length.h:25
  `job`               TINYINT UNSIGNED   NOT NULL DEFAULT 0,
  `voice`             TINYINT UNSIGNED   NOT NULL DEFAULT 0,
  `dir`               TINYINT UNSIGNED   NOT NULL DEFAULT 0,
  `x`                 INT                NOT NULL DEFAULT 0,
  `y`                 INT                NOT NULL DEFAULT 0,
  `z`                 INT                NOT NULL DEFAULT 0,
  `map_index`         INT                NOT NULL DEFAULT 0,
  `exit_x`            INT                NOT NULL DEFAULT 0,
  `exit_y`            INT                NOT NULL DEFAULT 0,
  `exit_map_index`    INT                NOT NULL DEFAULT 0,
  `hp`                INT                NOT NULL DEFAULT 0,
  `mp`                INT                NOT NULL DEFAULT 0,
  `stamina`           SMALLINT           NOT NULL DEFAULT 0,
  `random_hp`         SMALLINT           NOT NULL DEFAULT 0,
  `random_sp`         SMALLINT           NOT NULL DEFAULT 0,
  `playtime`          INT                NOT NULL DEFAULT 0,
  `level`             TINYINT UNSIGNED   NOT NULL DEFAULT 1,
  `level_step`        TINYINT UNSIGNED   NOT NULL DEFAULT 0,
  `st`                TINYINT UNSIGNED   NOT NULL DEFAULT 0,
  `ht`                TINYINT UNSIGNED   NOT NULL DEFAULT 0,
  `dx`                TINYINT UNSIGNED   NOT NULL DEFAULT 0,
  `iq`                TINYINT UNSIGNED   NOT NULL DEFAULT 0,
  `exp`               INT UNSIGNED       NOT NULL DEFAULT 0,    -- %u
  `gold`              BIGINT             NOT NULL DEFAULT 0,    -- %lld, int64_t
  `gaya`              INT                NOT NULL DEFAULT 0,    -- ENABLE_GAYA_SYSTEM, %d
  `stat_point`        SMALLINT           NOT NULL DEFAULT 0,
  `skill_point`       SMALLINT           NOT NULL DEFAULT 0,
  `sub_skill_point`   SMALLINT           NOT NULL DEFAULT 0,
  `stat_reset_count`  SMALLINT           NOT NULL DEFAULT 0,
  `ip`                VARCHAR(16)        NOT NULL DEFAULT '',   -- '%s', char[IP_ADDRESS_LENGTH+1]
  `part_base`         TINYINT UNSIGNED   NOT NULL DEFAULT 0,    -- uint8_t
  `part_main`         SMALLINT UNSIGNED  NOT NULL DEFAULT 0,    -- uint16_t parts[PART_MAIN]
  `part_hair`         SMALLINT UNSIGNED  NOT NULL DEFAULT 0,    -- uint16_t parts[PART_HAIR]
  `part_acce`         SMALLINT UNSIGNED  NOT NULL DEFAULT 0,    -- ENABLE_ACCE_SYSTEM, uint16_t
  `last_play`         DATETIME           NULL     DEFAULT NULL, -- NOW() / UNIX_TIMESTAMP(last_play)
  `skill_group`       TINYINT UNSIGNED   NOT NULL DEFAULT 0,
  `alignment`         INT UNSIGNED       NOT NULL DEFAULT 0,    -- %u
  `horse_level`       TINYINT UNSIGNED   NOT NULL DEFAULT 0,
  `horse_riding`      TINYINT UNSIGNED   NOT NULL DEFAULT 0,
  `horse_hp`          SMALLINT           NOT NULL DEFAULT 0,    -- short sHealth
  `horse_hp_droptime` INT UNSIGNED       NOT NULL DEFAULT 0,    -- uint32_t, %u
  `horse_stamina`     SMALLINT           NOT NULL DEFAULT 0,    -- short sStamina
  `horse_skill_point` SMALLINT           NOT NULL DEFAULT 0,
  `envanter`          INT                NOT NULL DEFAULT 0,    -- __ENABLE_EXTEND_INVEN_SYSTEM__, %d
  `battle_pass_end`   DATETIME           NULL     DEFAULT NULL, -- ENABLE_BATTLE_PASS, FROM_UNIXTIME(..)
  `r0`                BIGINT             NOT NULL DEFAULT 0,    -- ENABLE_RANKING, %lld
  `r1`                BIGINT             NOT NULL DEFAULT 0,
  `r2`                BIGINT             NOT NULL DEFAULT 0,
  `r3`                BIGINT             NOT NULL DEFAULT 0,
  `r4`                BIGINT             NOT NULL DEFAULT 0,
  `r5`                BIGINT             NOT NULL DEFAULT 0,
  `r6`                BIGINT             NOT NULL DEFAULT 0,
  `r7`                BIGINT             NOT NULL DEFAULT 0,
  `r8`                BIGINT             NOT NULL DEFAULT 0,
  `r9`                BIGINT             NOT NULL DEFAULT 0,
  `r10`               BIGINT             NOT NULL DEFAULT 0,
  `r11`               BIGINT             NOT NULL DEFAULT 0,
  `r12`               BIGINT             NOT NULL DEFAULT 0,
  `r13`               BIGINT             NOT NULL DEFAULT 0,
  `r14`               BIGINT             NOT NULL DEFAULT 0,
  `r15`               BIGINT             NOT NULL DEFAULT 0,
  `r16`               BIGINT             NOT NULL DEFAULT 0,
  `r17`               BIGINT             NOT NULL DEFAULT 0,
  `r18`               BIGINT             NOT NULL DEFAULT 0,
  `r19`               BIGINT             NOT NULL DEFAULT 0,
  `skill_level`       BLOB               NULL,                  -- escaped TPlayerSkill[SKILL_MAX_NUM] binary
  `quickslot`         BLOB               NULL,                  -- escaped TQuickslot[QUICKSLOT_MAX_NUM] binary
  `mobile`            VARCHAR(32)        NOT NULL DEFAULT '',   -- char[MOBILE_MAX_LEN+1]; length.h:54
  `change_name`       TINYINT UNSIGNED   NOT NULL DEFAULT 0,    -- SELECTed into uint8_t bChangeName
  `skill_victim`      VARCHAR(64)        NOT NULL DEFAULT '',   -- LEADERBOARD_RAZOR93, escaped victim name
  `map1_skillmob`     INT                NOT NULL DEFAULT 0,    -- LEADERBOARD_RAZOR93, %d
  PRIMARY KEY (`id`),
  KEY `account_id` (`account_id`)                               -- login SELECT ... WHERE account_id=%u
) ENGINE=InnoDB;

-- =====================================================================
-- TABLE: player.item
-- Column count: 26 with ATTR_LOCK, 25 without.
-- Evidence:
--   WRITE  SRC/Server/Database/Cache.cpp:133-191
--          ("REPLACE INTO item%s (id, owner_id, window, pos, count, vnum
--            [, lockedattr], [socket0..2], [attrtype0..7/attrvalue0..7])")
--   WRITE  SRC/Server/Database/ClientManager.cpp:1572-1613
--          (SAFEBOX/MALL REPLACE INTO item ... including attrtype7/attrvalue7)
--   WRITE  SRC/Server/Database/ClientManager.cpp:1006-1031
--          (item_award -> item INSERT: id, owner_id, window='%s', pos,
--            vnum, count, socket0..2, attrtype0..7/attrvalue0..7)
--   READ   SRC/Server/Database/ClientManagerPlayer.cpp:426-440, 523-538
--          (inventory/equipment SELECT ... window in ('INVENTORY',...))
--   READ   SRC/Server/Database/ClientManager.cpp:795-811
--          (safebox SELECT ... window='%s')
--   READ   SRC/Server/Database/Cache.cpp:110
--   READ   SRC/Server/Database/ItemIDRangeManager.cpp:94,123 (id ranges)
--   MIGR.  SRC/Server/Database/sql/2026_04_15_add_8th_bonus.sql:5-7
--          (attrtype7 TINYINT UNSIGNED, attrvalue7 SMALLINT)
--   Args:  TPlayerItem / TPlayerItemAttribute tables.h:395-417;
--          ITEM_SOCKET_MAX_NUM=3, ITEM_ATTRIBUTE_MAX_NUM=8
--          (SRC/Server/common/item_length.h:14,28)
-- NOTE: item.id is generator-assigned (ITEM_MANAGER::GetNewID / GainItemID),
--   never AUTO_INCREMENT: ids are passed explicitly in every INSERT.
-- NOTE: owner_id is a player id for INVENTORY/EQUIPMENT rows but an
--   ACCOUNT id for SAFEBOX/MALL rows (ClientManager.cpp:1009,1543-1551).
-- TODO(verify) `window`: the SQL mixes string and numeric forms:
--   Cache.cpp:149 / ClientManager.cpp:1592 write it numerically (%d),
--   while ClientManager.cpp:811 and 1029 compare/write string literals
--   ('SAFEBOX','MALL','INVENTORY',...).  Only a MySQL ENUM (index based)
--   satisfies both.  The value order below mirrors EWindows
--   (length.h:669-690) for the active build defines.
-- =====================================================================
CREATE TABLE IF NOT EXISTS `player`.`item` (
  `id`          INT UNSIGNED     NOT NULL,
  `owner_id`    INT UNSIGNED     NOT NULL DEFAULT 0,
  `window`      ENUM('RESERVED_WINDOW','INVENTORY','EQUIPMENT','SAFEBOX','MALL',
                     'MOUNT_INVENTORY','DRAGON_SOUL_INVENTORY','BELT_INVENTORY',
                     'EXTRA_INVENTORY','SWITCHBOT','GROUND',
                     'NEW_OFFSHOP','SHOP_SAFEBOX') NOT NULL DEFAULT 'INVENTORY',
  `pos`         SMALLINT UNSIGNED NOT NULL DEFAULT 0,           -- uint16_t, %d
  `count`       INT UNSIGNED     NOT NULL DEFAULT 0,            -- %u
  `vnum`        INT UNSIGNED     NOT NULL DEFAULT 0,            -- %u
  `lockedattr`  SMALLINT         NOT NULL DEFAULT -1,           -- ATTR_LOCK, short, %d
  `socket0`     INT              NOT NULL DEFAULT 0,            -- int32_t, %d/%ld
  `socket1`     INT              NOT NULL DEFAULT 0,
  `socket2`     INT              NOT NULL DEFAULT 0,
  `attrtype0`   TINYINT UNSIGNED NOT NULL DEFAULT 0,            -- uint8_t bType
  `attrvalue0`  SMALLINT         NOT NULL DEFAULT 0,            -- short sValue
  `attrtype1`   TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `attrvalue1`  SMALLINT         NOT NULL DEFAULT 0,
  `attrtype2`   TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `attrvalue2`  SMALLINT         NOT NULL DEFAULT 0,
  `attrtype3`   TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `attrvalue3`  SMALLINT         NOT NULL DEFAULT 0,
  `attrtype4`   TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `attrvalue4`  SMALLINT         NOT NULL DEFAULT 0,
  `attrtype5`   TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `attrvalue5`  SMALLINT         NOT NULL DEFAULT 0,
  `attrtype6`   TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `attrvalue6`  SMALLINT         NOT NULL DEFAULT 0,
  `attrtype7`   TINYINT UNSIGNED NOT NULL DEFAULT 0,            -- 2026_04_15 migration
  `attrvalue7`  SMALLINT         NOT NULL DEFAULT 0,            -- 2026_04_15 migration
  PRIMARY KEY (`id`),
  KEY `owner_id` (`owner_id`)                                   -- WHERE owner_id=...
) ENGINE=InnoDB;

-- =====================================================================
-- TABLE: player.affect
-- Column count: 7
-- Evidence:
--   WRITE  SRC/Server/Database/ClientManagerPlayer.cpp:1609-1619
--          ("REPLACE INTO affect%s (dwPID, bType, bApplyOn, lApplyValue,
--            dwFlag, lDuration, lSPCost) VALUES(%u,%u,%u,%d,%u,%d,%d)")
--   WRITE  SRC/Server/Database/ClientManagerPlayer.cpp:1628-1630
--          ("DELETE FROM affect%s WHERE dwPID=%u AND bType=%u AND bApplyOn=%u")
--   READ   SRC/Server/Database/ClientManagerPlayer.cpp:418, 455, 547
--          ("SELECT dwPID,bType,bApplyOn,lApplyValue,dwFlag,lDuration,lSPCost")
--   READ   SRC/Server/Database/ClientManagerPlayer.cpp:909-960 (mapping)
--   READ   SRC/Server/GameServer/command/cmd_gm.cpp:2863 (join on player.id)
--   Types: TPacketAffectElement tables.h:936-944 (uint32_t dwType,
--          uint8_t bApplyOn, int32_t lApplyValue, uint32_t dwFlag,
--          int32_t lDuration, int32_t lSPCost)
-- =====================================================================
CREATE TABLE IF NOT EXISTS `player`.`affect` (
  `dwPID`       INT UNSIGNED       NOT NULL DEFAULT 0,
  `bType`       INT UNSIGNED       NOT NULL DEFAULT 0,   -- uint32_t dwType, %u
  `bApplyOn`    TINYINT UNSIGNED   NOT NULL DEFAULT 0,   -- uint8_t, %u
  `lApplyValue` INT                NOT NULL DEFAULT 0,   -- %d
  `dwFlag`      INT UNSIGNED       NOT NULL DEFAULT 0,   -- %u
  `lDuration`   INT                NOT NULL DEFAULT 0,   -- %d
  `lSPCost`     INT                NOT NULL DEFAULT 0,   -- %d
  PRIMARY KEY (`dwPID`, `bType`, `bApplyOn`),            -- suggestion: REPLACE/DELETE key
  KEY `dwPID` (`dwPID`)
) ENGINE=InnoDB;

-- =====================================================================
-- TABLE: player.quest
-- Column count: 4
-- Evidence:
--   WRITE  SRC/Server/Database/ClientManager.cpp:671-687
--          (DELETE/REPLACE INTO quest%s (dwPID, szName, szState, lValue))
--   WRITE  SRC/Server/Database/ClientManagerEventFlag.cpp:11,51
--          (event flags: dwPID=0)
--   WRITE  SRC/Server/Database/ClientManagerGuild.cpp:76,113
--          (guild_manage rows)
--   READ   SRC/Server/Database/ClientManagerPlayer.cpp:411,447,542
--          ("SELECT dwPID,szName,szState,lValue FROM quest%s WHERE dwPID=%u")
--   READ   SRC/Server/Database/ClientManagerPlayer.cpp:962-996 (mapping)
--   Types: TQuestTable tables.h:668-677 (uint32_t dwPID, char szName[33],
--          char szState[65], int32_t lValue)
-- =====================================================================
CREATE TABLE IF NOT EXISTS `player`.`quest` (
  `dwPID`   INT UNSIGNED   NOT NULL DEFAULT 0,
  `szName`  VARCHAR(32)    NOT NULL DEFAULT '',   -- QUEST_NAME_MAX_LEN=32
  `szState` VARCHAR(64)    NOT NULL DEFAULT '',   -- QUEST_STATE_MAX_LEN=64
  `lValue`  INT            NOT NULL DEFAULT 0,
  UNIQUE KEY `pid_name_state` (`dwPID`, `szName`, `szState`),  -- suggestion: REPLACE key; DELETE also filters szState
  KEY `dwPID` (`dwPID`)
) ENGINE=InnoDB;

-- =====================================================================
-- TABLE: player.safebox
-- Column count: 4
-- Evidence:
--   WRITE  SRC/Server/Database/ClientManager.cpp:1085-1087
--          ("INSERT INTO safebox%s (account_id, size) ..."
--           "UPDATE safebox%s SET size=%u WHERE account_id=%u")
--   WRITE  SRC/Server/Database/ClientManager.cpp:1134
--          ("UPDATE safebox%s SET password=? WHERE account_id=?")
--   WRITE  SRC/Server/Database/ClientManager.cpp:1236-1238
--          ("UPDATE safebox%s SET gold='%u' WHERE account_id=%u")
--   WRITE  SRC/Server/GameServer/ecs/systems/SessionSystem.cpp:886
--          ("UPDATE safebox%s SET size = %d WHERE account_id = %u")
--   READ   SRC/Server/Database/ClientManager.cpp:711-713
--          ("SELECT account_id, size, password FROM safebox%s WHERE account_id=%u")
--   READ   SRC/Server/Database/ClientManager.cpp:1116
--          ("SELECT password FROM safebox%s WHERE account_id=%u")
--   READ   SRC/Server/GameServer/ecs/systems/SessionSystem.cpp:321
--          ("SELECT size FROM safebox%s WHERE account_id = %u")
--   Types: TSafeboxTable tables.h:809-815 (uint32_t dwID -> account_id,
--          uint8_t bSize -> size, uint32_t dwGold -> gold);
--          SAFEBOX_PASSWORD_MAX_LEN=6 tables.h:803
-- INCOMPLETE: TSafeboxTable::wItemCount (uint16_t) is recomputed in memory
--   (ClientManager.cpp:1064) and has no SQL column.  `gold` is only written
--   by the DB layer and never read back (cached client-side).
-- =====================================================================
CREATE TABLE IF NOT EXISTS `player`.`safebox` (
  `account_id` INT UNSIGNED     NOT NULL,
  `size`       TINYINT UNSIGNED NOT NULL DEFAULT 1,   -- bSize, %u/%d
  `password`   VARCHAR(7)       NOT NULL DEFAULT '',  -- '%s' / prepared text, char[7]
  `gold`       INT UNSIGNED     NOT NULL DEFAULT 0,   -- dwGold, '...%u'
  PRIMARY KEY (`account_id`)
) ENGINE=InnoDB;

-- =====================================================================
-- TABLE: player.player_index
-- Column count: 9 with ENABLE_HWID; 8 without `hwid`; 7 if `ch` is also
-- omitted.  `ch` currently compiled out in this build
-- (ENABLE_GENERAL_CH is commented, service.h:127) but appears in literal SQL.
-- Evidence:
--   WRITE  SRC/Server/Database/ClientManagerLogin.cpp:178
--          ("INSERT INTO player_index%s (id) VALUES(%u)")
--   WRITE  SRC/Server/Database/ClientManagerPlayer.cpp:1328,1524
--          ("UPDATE player_index%s SET pid%u=%u WHERE id=%u"
--           "UPDATE player_index%s SET pid%u=0 WHERE pid%u=%u")
--   WRITE  SRC/Server/Database/ClientManager.cpp:1247
--          ("UPDATE player_index%s SET empire=%u WHERE id=%u")
--   WRITE  SRC/Server/Database/ClientManagerLogin.cpp:484
--          ("UPDATE player_index%s SET ch=%d WHERE pid1 IN (...) ...")
--   WRITE  SRC/Server/LoginServer/login_hwid.cpp:63
--          ("UPDATE player.player_index SET hwid=? WHERE id=?")
--   READ   SRC/Server/Database/ClientManagerLogin.cpp:142,166,236
--          ("SELECT pid1, pid2, pid3, pid4, pid5, empire ... WHERE id=%u")
--   READ   SRC/Server/Database/ClientManagerLogin.cpp:496 ("SELECT ch ...")
--   READ   SRC/Server/Database/ClientManagerPlayer.cpp:1192,1431
--          ("SELECT pid%u FROM player_index ...", join pid%u=p.id)
--   READ   SRC/Server/GameServer/ecs/systems/PlayerRuntimeSystem.cpp:286,325
--          (pid1..pid5/empire)
--   Types: uint32_t ids, uint8_t empire / uint8_t ch (Peer.h:55 GetChannel)
--          HWID_LENGTH=64 (length.h:148)
-- =====================================================================
CREATE TABLE IF NOT EXISTS `player`.`player_index` (
  `id`      INT UNSIGNED     NOT NULL,               -- account id
  `hwid`    VARCHAR(64)      NOT NULL DEFAULT '',    -- ENABLE_HWID
  `pid1`    INT UNSIGNED     NOT NULL DEFAULT 0,
  `pid2`    INT UNSIGNED     NOT NULL DEFAULT 0,
  `pid3`    INT UNSIGNED     NOT NULL DEFAULT 0,
  `pid4`    INT UNSIGNED     NOT NULL DEFAULT 0,
  `pid5`    INT UNSIGNED     NOT NULL DEFAULT 0,
  `empire`  TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `ch`      TINYINT UNSIGNED NOT NULL DEFAULT 0,     -- ENABLE_GENERAL_CH (currently disabled)
  PRIMARY KEY (`id`)
) ENGINE=InnoDB;

-- ############################################################################
-- 2. PLAYER DATABASE - PROTO / STATIC TABLE-EK
-- ############################################################################

-- #####################################################################
-- ##  PART 2 -- PLAYER DB (default SQL_PLAYER slot)                   ##
-- #####################################################################

-- ---------------------------------------------------------------------
-- mob_proto
--   DB: player. Table is mob_proto<%postfix%> (GetTablePostfix()).
--   Read:  ClientManagerBoot.cpp:1517-1548  (SELECT, ENABLE_MULTI_NAMES branch)
--          VERIFY map: ClientManagerBoot.cpp:1572-1668
--   Write: ClientManagerBoot.cpp:1279-1384 (replace into mob_proto%s)
--          ClientManagerBoot.cpp:1211-1266 (replace into mob_proto, ENABLE_MULTILANGUAGE_SYSTEM branch -- inactive)
--   Column set is the MERGE of both. 10 locale columns exist because
--   ENABLE_MULTI_NAMES is defined (CommonDefines.h:93); the string array
--   languages[] has exactly 10 entries (ClientManagerBoot.cpp:17-20).
--   NOTE: the active mirror write path (line 1331+) persists only ONE
--   locale column (conf.json name_column), while the read path expects the
--   10 locale_name_* columns -> mirror2db round-trip is lossy. mirror2db
--   defaults to false (DatabaseConfig.h:80).
--   79 columns.
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `player`.`mob_proto` (
  `vnum`                INT UNSIGNED     NOT NULL,
  `name`                VARCHAR(25)      NOT NULL DEFAULT '',  -- CHARACTER_NAME_MAX_LEN(24)+1
  `locale_name_en`      VARCHAR(25)      NOT NULL DEFAULT '',  -- ENABLE_MULTI_NAMES
  `locale_name_ro`      VARCHAR(25)      NOT NULL DEFAULT '',
  `locale_name_it`      VARCHAR(25)      NOT NULL DEFAULT '',
  `locale_name_tr`      VARCHAR(25)      NOT NULL DEFAULT '',
  `locale_name_de`      VARCHAR(25)      NOT NULL DEFAULT '',
  `locale_name_pl`      VARCHAR(25)      NOT NULL DEFAULT '',
  `locale_name_pt`      VARCHAR(25)      NOT NULL DEFAULT '',
  `locale_name_es`      VARCHAR(25)      NOT NULL DEFAULT '',
  `locale_name_cz`      VARCHAR(25)      NOT NULL DEFAULT '',
  `locale_name_hu`      VARCHAR(25)      NOT NULL DEFAULT '',
  `type`                TINYINT UNSIGNED NOT NULL DEFAULT 0,   -- uint8 bType
  `rank`                TINYINT UNSIGNED NOT NULL DEFAULT 0,   -- uint8 bRank
  `battle_type`         TINYINT UNSIGNED NOT NULL DEFAULT 0,   -- uint8 bBattleType
  `level`               TINYINT UNSIGNED NOT NULL DEFAULT 0,   -- uint8 bLevel
  `size`                TINYINT UNSIGNED NOT NULL DEFAULT 0,   -- uint8 bSize
  `ai_flag`             INT UNSIGNED     NOT NULL DEFAULT 0,   -- uint32 dwAIFlag  (SELECT ai_flag+0)
  `setRaceFlag`         INT UNSIGNED     NOT NULL DEFAULT 0,   -- uint32 dwRaceFlag (SELECT setRaceFlag+0)
  `setImmuneFlag`       INT UNSIGNED     NOT NULL DEFAULT 0,   -- uint32 dwImmuneFlag (SELECT setImmuneFlag+0)
  `on_click`            TINYINT UNSIGNED NOT NULL DEFAULT 0,   -- uint8 bOnClickType
  `empire`              TINYINT UNSIGNED NOT NULL DEFAULT 0,   -- uint8 bEmpire
  `drop_item`           INT UNSIGNED     NOT NULL DEFAULT 0,   -- uint32 dwDropItemVnum
  `resurrection_vnum`   INT UNSIGNED     NOT NULL DEFAULT 0,   -- uint32 dwResurrectionVnum
  `folder`              VARCHAR(65)      NOT NULL DEFAULT '',  -- char szFolder[64+1]
  `st`                  TINYINT UNSIGNED NOT NULL DEFAULT 0,   -- uint8 bStr
  `dx`                  TINYINT UNSIGNED NOT NULL DEFAULT 0,   -- uint8 bDex
  `ht`                  TINYINT UNSIGNED NOT NULL DEFAULT 0,   -- uint8 bCon
  `iq`                  TINYINT UNSIGNED NOT NULL DEFAULT 0,   -- uint8 bInt
  `damage_min`          INT UNSIGNED     NOT NULL DEFAULT 0,   -- uint32 dwDamageRange[0]
  `damage_max`          INT UNSIGNED     NOT NULL DEFAULT 0,   -- uint32 dwDamageRange[1]
  `max_hp`              BIGINT UNSIGNED  NOT NULL DEFAULT 0,   -- uint64 dwMaxHP
  `regen_cycle`         TINYINT UNSIGNED NOT NULL DEFAULT 0,   -- uint8 bRegenCycle
  `regen_percent`       TINYINT UNSIGNED NOT NULL DEFAULT 0,   -- uint8 bRegenPercent
  `exp`                 INT UNSIGNED     NOT NULL DEFAULT 0,   -- uint32 dwExp
  `gold_min`            INT UNSIGNED     NOT NULL DEFAULT 0,
  `gold_max`            INT UNSIGNED     NOT NULL DEFAULT 0,
  `def`                 SMALLINT UNSIGNED NOT NULL DEFAULT 0,  -- uint16 wDef
  `attack_speed`        SMALLINT         NOT NULL DEFAULT 0,   -- short sAttackSpeed
  `move_speed`          SMALLINT         NOT NULL DEFAULT 0,   -- short sMovingSpeed
  `aggressive_hp_pct`   TINYINT UNSIGNED NOT NULL DEFAULT 0,   -- uint8 bAggresiveHPPct
  `aggressive_sight`    SMALLINT UNSIGNED NOT NULL DEFAULT 0,  -- uint16 wAggressiveSight
  `attack_range`        SMALLINT UNSIGNED NOT NULL DEFAULT 0,  -- uint16 wAttackRange
  `polymorph_item`      INT UNSIGNED     NOT NULL DEFAULT 0,   -- uint32 dwPolymorphItemVnum
  `enchant_curse`       TINYINT          NOT NULL DEFAULT 0,   -- char cEnchants
  `enchant_slow`        TINYINT          NOT NULL DEFAULT 0,
  `enchant_poison`      TINYINT          NOT NULL DEFAULT 0,
  `enchant_stun`        TINYINT          NOT NULL DEFAULT 0,
  `enchant_critical`    TINYINT          NOT NULL DEFAULT 0,
  `enchant_penetrate`   TINYINT          NOT NULL DEFAULT 0,
  `resist_sword`        TINYINT          NOT NULL DEFAULT 0,   -- char cResists
  `resist_twohand`      TINYINT          NOT NULL DEFAULT 0,
  `resist_dagger`       TINYINT          NOT NULL DEFAULT 0,
  `resist_bell`         TINYINT          NOT NULL DEFAULT 0,
  `resist_fan`          TINYINT          NOT NULL DEFAULT 0,
  `resist_bow`          TINYINT          NOT NULL DEFAULT 0,
  `resist_fire`         TINYINT          NOT NULL DEFAULT 0,
  `resist_elect`        TINYINT          NOT NULL DEFAULT 0,
  `resist_magic`        TINYINT          NOT NULL DEFAULT 0,
  `resist_wind`         TINYINT          NOT NULL DEFAULT 0,
  `resist_poison`       TINYINT          NOT NULL DEFAULT 0,
  `dam_multiply`        FLOAT            NOT NULL DEFAULT 0,   -- float fDamMultiply (mirror uses %f)
  `summon`              INT UNSIGNED     NOT NULL DEFAULT 0,   -- uint32 dwSummonVnum
  `drain_sp`            INT UNSIGNED     NOT NULL DEFAULT 0,   -- uint32 dwDrainSP
  `skill_vnum0`         INT UNSIGNED     NOT NULL DEFAULT 0,
  `skill_level0`        TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `skill_vnum1`         INT UNSIGNED     NOT NULL DEFAULT 0,
  `skill_level1`        TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `skill_vnum2`         INT UNSIGNED     NOT NULL DEFAULT 0,
  `skill_level2`        TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `skill_vnum3`         INT UNSIGNED     NOT NULL DEFAULT 0,
  `skill_level3`        TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `skill_vnum4`         INT UNSIGNED     NOT NULL DEFAULT 0,
  `skill_level4`        TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `sp_berserk`          TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `sp_stoneskin`        TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `sp_godspeed`         TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `sp_deathblow`        TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `sp_revive`           TINYINT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (`vnum`)
) ENGINE=InnoDB;

-- ---------------------------------------------------------------------
-- item_proto
--   DB: player. Table is item_proto<%postfix%>.
--   Read:  ClientManagerBoot.cpp:1726-1762 (SELECT, ENABLE_MULTI_NAMES branch)
--   Write: ClientManagerBoot.cpp:1402-1472 (replace into item_proto%s)
--   MERGE of both. ENABLE_APPLYTYPE5_RAZOR93 (service.h:209) adds
--   applytype3/4 + applyvalue3/4; ENABLE_MULTI_NAMES adds 10 locale_name_*.
--   vnum_range is only selected when ENABLE_AUTODETECT_VNUMRANGE is NOT
--   defined; it IS defined at ClientManagerBoot.cpp:1482, so it is optional
--   here -- TODO(verify): keep/drop vnum_range to match your live schema.
--   47 active columns (+1 optional).
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `player`.`item_proto` (
  `vnum`            INT UNSIGNED     NOT NULL,
  `type`            TINYINT UNSIGNED NOT NULL DEFAULT 0,   -- uint8 bType
  `subtype`         TINYINT UNSIGNED NOT NULL DEFAULT 0,   -- uint8 bSubType
  `name`            VARCHAR(49)      NOT NULL DEFAULT '',  -- ITEM_NAME_MAX_LEN(48)+1
  `locale_name_en`  VARCHAR(49)      NOT NULL DEFAULT '',  -- ENABLE_MULTI_NAMES
  `locale_name_ro`  VARCHAR(49)      NOT NULL DEFAULT '',
  `locale_name_it`  VARCHAR(49)      NOT NULL DEFAULT '',
  `locale_name_tr`  VARCHAR(49)      NOT NULL DEFAULT '',
  `locale_name_de`  VARCHAR(49)      NOT NULL DEFAULT '',
  `locale_name_pl`  VARCHAR(49)      NOT NULL DEFAULT '',
  `locale_name_pt`  VARCHAR(49)      NOT NULL DEFAULT '',
  `locale_name_es`  VARCHAR(49)      NOT NULL DEFAULT '',
  `locale_name_cz`  VARCHAR(49)      NOT NULL DEFAULT '',
  `locale_name_hu`  VARCHAR(49)      NOT NULL DEFAULT '',
  `gold`            BIGINT           NOT NULL DEFAULT 0,   -- int64 dwGold (mirror %lld)
  `shop_buy_price`  BIGINT           NOT NULL DEFAULT 0,   -- int64 dwShopBuyPrice
  `weight`          TINYINT UNSIGNED NOT NULL DEFAULT 0,   -- uint8 bWeight
  `size`            TINYINT UNSIGNED NOT NULL DEFAULT 0,   -- uint8 bSize
  `flag`            INT UNSIGNED     NOT NULL DEFAULT 0,   -- uint32 dwFlags
  `wearflag`        INT UNSIGNED     NOT NULL DEFAULT 0,   -- uint32 dwWearFlags
  `antiflag`        INT UNSIGNED     NOT NULL DEFAULT 0,   -- uint32 dwAntiFlags
  `immuneflag`      INT UNSIGNED     NOT NULL DEFAULT 0,   -- uint32 dwImmuneFlag (SELECT immuneflag+0)
  `refined_vnum`    INT UNSIGNED     NOT NULL DEFAULT 0,   -- uint32 dwRefinedVnum
  `refine_set`      SMALLINT UNSIGNED NOT NULL DEFAULT 0,  -- uint16 wRefineSet
  `magic_pct`       TINYINT UNSIGNED NOT NULL DEFAULT 0,   -- uint8 bAlterToMagicItemPct
  `socket_pct`      TINYINT UNSIGNED NOT NULL DEFAULT 0,   -- uint8 bGainSocketPct
  `addon_type`      SMALLINT         NOT NULL DEFAULT 0,   -- short sAddonType (can be -1)
  `limittype0`      TINYINT UNSIGNED NOT NULL DEFAULT 0,   -- TItemLimit.bType
  `limitvalue0`     INT              NOT NULL DEFAULT 0,   -- TItemLimit.lValue
  `limittype1`      TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `limitvalue1`     INT              NOT NULL DEFAULT 0,
  `applytype0`      TINYINT UNSIGNED NOT NULL DEFAULT 0,   -- TItemApply.bType
  `applyvalue0`     INT              NOT NULL DEFAULT 0,   -- TItemApply.lValue
  `applytype1`      TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `applyvalue1`     INT              NOT NULL DEFAULT 0,
  `applytype2`      TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `applyvalue2`     INT              NOT NULL DEFAULT 0,
  `applytype3`      TINYINT UNSIGNED NOT NULL DEFAULT 0,   -- ENABLE_APPLYTYPE5_RAZOR93
  `applyvalue3`     INT              NOT NULL DEFAULT 0,
  `applytype4`      TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `applyvalue4`     INT              NOT NULL DEFAULT 0,
  `value0`          INT              NOT NULL DEFAULT 0,   -- int32 alValues[0..5]
  `value1`          INT              NOT NULL DEFAULT 0,
  `value2`          INT              NOT NULL DEFAULT 0,
  `value3`          INT              NOT NULL DEFAULT 0,
  `value4`          INT              NOT NULL DEFAULT 0,
  `value5`          INT              NOT NULL DEFAULT 0,
  `vnum_range`      INT UNSIGNED     NOT NULL DEFAULT 0,   -- TODO(verify): selected only if !ENABLE_AUTODETECT_VNUMRANGE
  PRIMARY KEY (`vnum`)
) ENGINE=InnoDB;

-- ---------------------------------------------------------------------
-- item_extra_proto
--   DB: player (default slot).
--   Evidence: ClientManagerBoot.cpp:1174
--             "SELECT * FROM item_extra_proto ORDER by vnum ASC"  (ENABLE_ITEM_EXTRA_PROTO, CommonDefines.h:141)
--   INCOMPLETE: only `vnum` (ORDER BY vnum) is a literal column name.
--   The remaining 5 values are read positionally (ClientManagerBoot.cpp:1190-1195):
--     iRarity(int), ExtraBonus[0].bType(uint8), ExtraBonus[0].lValue(int32),
--     ExtraBonus[1].bType(uint8), ExtraBonus[1].lValue(int32)
--   -- note: code bug at line 1195 reads data[c] without c++ for the last field.
--   No INSERT/SELECT list for those columns exists in the repo -> omitted.
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `player`.`item_extra_proto` (
  `vnum` INT UNSIGNED NOT NULL,
  -- INCOMPLETE: 5 unresolved columns, see comment above / notes.md
  PRIMARY KEY (`vnum`)
) ENGINE=InnoDB;

-- ---------------------------------------------------------------------
-- refine_proto
--   DB: player. Table is refine_proto<%postfix%>.
--   Evidence: ClientManagerBoot.cpp:168-170
--     "SELECT id, cost, prob, vnum0, count0, vnum1, count1, vnum2, count2,
--             vnum3, count3, vnum4, count4 FROM refine_proto%s"
--   C++ struct: common/tables.h:1061-1076 TRefineTable / TRefineMaterial
--     (id uint32, cost int64, prob int, materials[5]{vnum uint32, count int})
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `player`.`refine_proto` (
  `id`     INT UNSIGNED NOT NULL,
  `cost`   BIGINT       NOT NULL DEFAULT 0,   -- int64_t cost (mirror-type)
  `prob`   INT          NOT NULL DEFAULT 0,   -- int prob
  `vnum0`  INT UNSIGNED NOT NULL DEFAULT 0,
  `count0` INT          NOT NULL DEFAULT 0,
  `vnum1`  INT UNSIGNED NOT NULL DEFAULT 0,
  `count1` INT          NOT NULL DEFAULT 0,
  `vnum2`  INT UNSIGNED NOT NULL DEFAULT 0,
  `count2` INT          NOT NULL DEFAULT 0,
  `vnum3`  INT UNSIGNED NOT NULL DEFAULT 0,
  `count3` INT          NOT NULL DEFAULT 0,
  `vnum4`  INT UNSIGNED NOT NULL DEFAULT 0,
  `count4` INT          NOT NULL DEFAULT 0,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB;

-- ---------------------------------------------------------------------
-- skill_proto
--   DB: player. Table is skill_proto<%postfix%>.
--   Evidence: ClientManagerBoot.cpp:545-552 (SELECT ... FROM skill_proto%s ORDER BY dwVnum)
--   Column names are the literal SELECT expressions; "+0" suffixes are casts only.
--   C++ struct: common/tables.h:591-633 TSkillTable.
--   NOT selected (no SQL evidence): bSpecular, dwMobColor.
--   30 columns.
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `player`.`skill_proto` (
  `dwVnum`                      INT UNSIGNED     NOT NULL,
  `szName`                      VARCHAR(33)      NOT NULL DEFAULT '', -- char[32+1]
  `bType`                       TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `bMaxLevel`                   TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `dwSplashRange`               INT UNSIGNED     NOT NULL DEFAULT 0,
  `szPointOn`                   VARCHAR(64)      NOT NULL DEFAULT '', -- char[64]
  `szPointPoly`                 VARCHAR(101)     NOT NULL DEFAULT '', -- char[100+1]
  `szSPCostPoly`                VARCHAR(101)     NOT NULL DEFAULT '',
  `szDurationPoly`              VARCHAR(101)     NOT NULL DEFAULT '',
  `szDurationSPCostPoly`        VARCHAR(101)     NOT NULL DEFAULT '',
  `szCooldownPoly`              VARCHAR(101)     NOT NULL DEFAULT '',
  `szMasterBonusPoly`           VARCHAR(101)     NOT NULL DEFAULT '',
  `setFlag`                     INT UNSIGNED     NOT NULL DEFAULT 0,  -- -> dwFlag
  `setAffectFlag`               INT UNSIGNED     NOT NULL DEFAULT 0,  -- -> dwAffectFlag
  `szPointOn2`                  VARCHAR(64)      NOT NULL DEFAULT '',
  `szPointPoly2`                VARCHAR(101)     NOT NULL DEFAULT '',
  `szDurationPoly2`             VARCHAR(101)     NOT NULL DEFAULT '',
  `setAffectFlag2`              INT UNSIGNED     NOT NULL DEFAULT 0,  -- -> dwAffectFlag2
  `szPointOn3`                  VARCHAR(64)      NOT NULL DEFAULT '',
  `szPointPoly3`                VARCHAR(101)     NOT NULL DEFAULT '',
  `szDurationPoly3`             VARCHAR(101)     NOT NULL DEFAULT '',
  `szGrandMasterAddSPCostPoly`  VARCHAR(101)     NOT NULL DEFAULT '',
  `bLevelStep`                  TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `bLevelLimit`                 TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `prerequisiteSkillVnum`       INT UNSIGNED     NOT NULL DEFAULT 0,  -- -> preSkillVnum
  `prerequisiteSkillLevel`      TINYINT UNSIGNED NOT NULL DEFAULT 0,  -- -> preSkillLevel
  `iMaxHit`                     INT              NOT NULL DEFAULT 0,  -- int32 lMaxHit
  `szSplashAroundDamageAdjustPoly` VARCHAR(101)  NOT NULL DEFAULT '',
  `eSkillType`                  TINYINT UNSIGNED NOT NULL DEFAULT 0,  -- -> bSkillAttrType
  `dwTargetRange`               INT UNSIGNED     NOT NULL DEFAULT 0,
  PRIMARY KEY (`dwVnum`)
) ENGINE=InnoDB;

-- ---------------------------------------------------------------------
-- item_attr
--   DB: player. Table is item_attr<%postfix%>.
--   Evidence: ClientManagerBoot.cpp:661-672
--     "SELECT apply, apply+0, prob, lv1..lv5, weapon, body, wrist, foots,
--             neck, head, shield, ear [, talisman] [, costume_body, costume_hair,
--             costume_weapon [, costume_stole]] [, lv1_costume..lv5_costume]
--      FROM item_attr%s ORDER BY apply"
--   The optional groups are ACTIVE in this build:
--     ENABLE_PENDANT && ENABLE_NEW_BONUS_TALISMAN -> talisman
--     ENABLE_ATTR_COSTUMES -> costume_body/hair/weapon
--     ENABLE_STOLE_COSTUME -> costume_stole + lv*_costume
--   Canonical column list also in root/data/item_attr.sql:1 (INSERT column list).
--   C++ struct: common/tables.h:732 TItemAttrTable (szApply[33], uint32 prob,
--     int32 lValues[10], uint8 bMaxLevelBySet[13]).
--   25 columns.
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `player`.`item_attr` (
  `apply`          VARCHAR(33)      NOT NULL DEFAULT '', -- APPLY_NAME_MAX_LEN(32)+1
  `prob`           INT UNSIGNED     NOT NULL DEFAULT 0,
  `lv1`            INT              NOT NULL DEFAULT 0,
  `lv2`            INT              NOT NULL DEFAULT 0,
  `lv3`            INT              NOT NULL DEFAULT 0,
  `lv4`            INT              NOT NULL DEFAULT 0,
  `lv5`            INT              NOT NULL DEFAULT 0,
  `weapon`         TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `body`           TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `wrist`          TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `foots`          TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `neck`           TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `head`           TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `shield`         TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `ear`            TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `talisman`       TINYINT UNSIGNED NOT NULL DEFAULT 0, -- ENABLE_PENDANT && ENABLE_NEW_BONUS_TALISMAN
  `costume_body`   TINYINT UNSIGNED NOT NULL DEFAULT 0, -- ENABLE_ATTR_COSTUMES
  `costume_hair`   TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `costume_weapon` TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `costume_stole`  TINYINT UNSIGNED NOT NULL DEFAULT 0, -- ENABLE_STOLE_COSTUME
  `lv1_costume`    INT              NOT NULL DEFAULT 0, -- ENABLE_STOLE_COSTUME (lValues[5..9])
  `lv2_costume`    INT              NOT NULL DEFAULT 0,
  `lv3_costume`    INT              NOT NULL DEFAULT 0,
  `lv4_costume`    INT              NOT NULL DEFAULT 0,
  `lv5_costume`    INT              NOT NULL DEFAULT 0,
  PRIMARY KEY (`apply`)
) ENGINE=InnoDB;

-- ---------------------------------------------------------------------
-- item_attr_rare
--   DB: player. Table is item_attr_rare<%postfix%>.
--   Evidence: ClientManagerBoot.cpp:765-778 (SELECT ... FROM item_attr_rare%s ORDER BY apply)
--   Active build (verified):
--     ENABLE_NEW_BONUS_TALISMAN -> talisman
--     ENABLE_ITEM_ATTR_COSTUME  -> NOT DEFINED anywhere in the tree,
--       so costume_body/costume_hair/costume_weapon are NOT selected.
--   16 columns.
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `player`.`item_attr_rare` (
  `apply`    VARCHAR(33)      NOT NULL DEFAULT '',
  `prob`     INT UNSIGNED     NOT NULL DEFAULT 0,
  `lv1`      INT              NOT NULL DEFAULT 0,
  `lv2`      INT              NOT NULL DEFAULT 0,
  `lv3`      INT              NOT NULL DEFAULT 0,
  `lv4`      INT              NOT NULL DEFAULT 0,
  `lv5`      INT              NOT NULL DEFAULT 0,
  `weapon`   TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `body`     TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `wrist`    TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `foots`    TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `neck`     TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `head`     TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `shield`   TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `ear`      TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `talisman` TINYINT UNSIGNED NOT NULL DEFAULT 0,   -- ENABLE_NEW_BONUS_TALISMAN
  PRIMARY KEY (`apply`)
) ENGINE=InnoDB;

-- ---------------------------------------------------------------------
-- item_attr_rare_8
--   DB: player. Table is item_attr_rare_8<%postfix%>.
--   Evidence: ClientManagerBoot.cpp:860-873 (SELECT ... FROM item_attr_rare_8%s ORDER BY apply)
--             SRC/Server/Database/sql/2026_04_15_add_8th_bonus.sql:15
--             "CREATE TABLE IF NOT EXISTS player.item_attr_rare_8 LIKE player.item_attr_rare;"
--   => identical shape to item_attr_rare in this build (16 columns).
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `player`.`item_attr_rare_8` (
  `apply`    VARCHAR(33)      NOT NULL DEFAULT '',
  `prob`     INT UNSIGNED     NOT NULL DEFAULT 0,
  `lv1`      INT              NOT NULL DEFAULT 0,
  `lv2`      INT              NOT NULL DEFAULT 0,
  `lv3`      INT              NOT NULL DEFAULT 0,
  `lv4`      INT              NOT NULL DEFAULT 0,
  `lv5`      INT              NOT NULL DEFAULT 0,
  `weapon`   TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `body`     TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `wrist`    TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `foots`    TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `neck`     TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `head`     TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `shield`   TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `ear`      TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `talisman` TINYINT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (`apply`)
) ENGINE=InnoDB;

-- ---------------------------------------------------------------------
-- object_proto
--   DB: player. Table is object_proto<%postfix%>.
--   Evidence: ClientManagerBoot.cpp:1059-1062
--     "SELECT vnum, price, materials, upgrade_vnum, upgrade_limit_time, life,
--             reg_1, reg_2, reg_3, reg_4, npc, group_vnum, dependent_group
--      FROM object_proto%s ORDER BY vnum"
--   C++ struct: common/building.h:28-46 TObjectProto / TObjectMaterial.
--   `materials` is a packed string "item,count/item,count/..." parsed by
--   parse_pair_number_string (ClientManagerBoot.cpp:1001-1052) -> TEXT.
--   13 columns.
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `player`.`object_proto` (
  `vnum`                INT UNSIGNED NOT NULL,
  `price`               INT UNSIGNED NOT NULL DEFAULT 0,
  `materials`           TEXT         NULL,                     -- packed "10,1/20,3/300,50"
  `upgrade_vnum`        INT UNSIGNED NOT NULL DEFAULT 0,
  `upgrade_limit_time`  INT UNSIGNED NOT NULL DEFAULT 0,
  `life`                INT          NOT NULL DEFAULT 0,       -- int32 lLife
  `reg_1`               INT          NOT NULL DEFAULT 0,       -- int32 lRegion[0]
  `reg_2`               INT          NOT NULL DEFAULT 0,       -- int32 lRegion[1]
  `reg_3`               INT          NOT NULL DEFAULT 0,       -- int32 lRegion[2]
  `reg_4`               INT          NOT NULL DEFAULT 0,       -- int32 lRegion[3]
  `npc`                 INT UNSIGNED NOT NULL DEFAULT 0,       -- uint32 dwNPCVnum
  `group_vnum`          INT UNSIGNED NOT NULL DEFAULT 0,       -- uint32 dwGroupVnum
  `dependent_group`     INT UNSIGNED NOT NULL DEFAULT 0,       -- uint32 dwDependOnGroupVnum
  PRIMARY KEY (`vnum`)
) ENGINE=InnoDB;

-- ---------------------------------------------------------------------
-- land
--   DB: player. Table is land<%postfix%>.
--   Evidence: ClientManagerBoot.cpp:959-961
--     "SELECT id, map_index, x, y, width, height, guild_id, guild_level_limit, price
--      FROM land%s WHERE enable='YES' ORDER BY id"
--             ClientManager.cpp:2209 "UPDATE land%s SET guild_id=%u WHERE id=%u"
--   C++ struct: common/building.h:11-20 TLand.
--   INCOMPLETE: `enable` exists only in the WHERE clause (value 'YES');
--   its type/default is unknown -> TODO(verify) (commonly ENUM('YES','NO')).
--   10 columns.
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `player`.`land` (
  `id`                INT UNSIGNED     NOT NULL,
  `map_index`         INT              NOT NULL DEFAULT 0,
  `x`                 INT              NOT NULL DEFAULT 0,
  `y`                 INT              NOT NULL DEFAULT 0,
  `width`             INT              NOT NULL DEFAULT 0,
  `height`            INT              NOT NULL DEFAULT 0,
  `guild_id`          INT UNSIGNED     NOT NULL DEFAULT 0,
  `guild_level_limit` TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `price`             INT UNSIGNED     NOT NULL DEFAULT 0,
  `enable`            VARCHAR(3)       NOT NULL DEFAULT 'YES', -- TODO(verify) type; queried with enable='YES'
  PRIMARY KEY (`id`)
) ENGINE=InnoDB;

-- ---------------------------------------------------------------------
-- object
--   DB: player. Table is object<%postfix%>.
--   Evidence: ClientManagerBoot.cpp:1128
--     "SELECT id, land_id, vnum, map_index, x, y, x_rot, y_rot, z_rot, life FROM object%s ORDER BY id"
--             ClientManager.cpp:2141-2142 INSERT INTO object%s (land_id, vnum, map_index, x, y, x_rot, y_rot, z_rot)
--             ClientManager.cpp:2176 DELETE FROM object%s WHERE id=%u
--   `id` is DB-generated (INSERT omits it, code uses uiInsertID) -> AUTO_INCREMENT.
--   C++ struct: common/building.h:48-60 TObject.
--   10 columns.
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `player`.`object` (
  `id`        INT UNSIGNED NOT NULL AUTO_INCREMENT,
  `land_id`   INT UNSIGNED NOT NULL DEFAULT 0,
  `vnum`      INT UNSIGNED NOT NULL DEFAULT 0,
  `map_index` INT          NOT NULL DEFAULT 0,
  `x`         INT          NOT NULL DEFAULT 0,
  `y`         INT          NOT NULL DEFAULT 0,
  `x_rot`     FLOAT        NOT NULL DEFAULT 0,   -- float xRot (INSERT uses %f)
  `y_rot`     FLOAT        NOT NULL DEFAULT 0,   -- float yRot
  `z_rot`     FLOAT        NOT NULL DEFAULT 0,   -- float zRot
  `life`      INT          NOT NULL DEFAULT 0,   -- int32 lLife
  PRIMARY KEY (`id`)
) ENGINE=InnoDB;

-- ---------------------------------------------------------------------
-- banword
--   DB: player (default slot). No postfix used in the query.
--   Evidence: ClientManagerBoot.cpp:634 "SELECT word FROM banword"
--   C++ struct: common/tables.h:1078-1081 TBanwordTable { char szWord[BANWORD_MAX_LEN+1] }
--               BANWORD_MAX_LEN = 24 (common/length.h:52) -> 25 chars.
--   1 column.
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `player`.`banword` (
  `word` VARCHAR(25) NOT NULL DEFAULT '',
  PRIMARY KEY (`word`)
) ENGINE=InnoDB;

-- ---------------------------------------------------------------------
-- quest_item_proto
--   DB: player. No postfix used in the query.
--   Evidence: ClientManagerBoot.cpp:388
--     "SELECT vnum, name, %s FROM quest_item_proto ORDER BY vnum"
--     %s is g_stLocaleNameColumn (conf.json "name_column", default "name",
--     Main.cpp:30/193), so the third column name is dynamic.
--   INCOMPLETE / dead code: InitializeQuestItemTable() is NOT called from
--   InitializeTables(); the only call site is commented out
--   (ClientManagerBoot.cpp:501-503).
--   `locale_name` below is a placeholder -- TODO(verify) adjust to your
--   conf.json name_column value.
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `player`.`quest_item_proto` (
  `vnum`        INT UNSIGNED NOT NULL,
  `name`        VARCHAR(49)  NOT NULL DEFAULT '',  -- ITEM_NAME_MAX_LEN(48)+1
  `locale_name` VARCHAR(49)  NOT NULL DEFAULT '',  -- TODO(verify): dynamic column name (g_stLocaleNameColumn)
  PRIMARY KEY (`vnum`)
) ENGINE=InnoDB;

-- ---------------------------------------------------------------------
-- shop / shop_item
--   DB: player (default slot). No postfix used in the query.
--   Evidence: ClientManagerBoot.cpp:293-308
--     "SELECT shop.vnum, shop.npc_vnum, shop_item.item_vnum, shop_item.count
--      [, gold, itemvnum1, itemcount1 ... itemvnum5, itemcount5]
--      FROM shop LEFT JOIN shop_item ON shop.vnum = shop_item.shop_vnum
--      ORDER BY shop.vnum, shop_item.item_vnum"
--   The optional price columns are ACTIVE (ENABLE_BUY_WITH_ITEM, service.h;
--   MAX_SHOP_PRICES = 5, common/length.h:145).
--   C++ structs: common/tables.h:643-666.
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `player`.`shop` (
  `vnum`     INT UNSIGNED NOT NULL,
  `npc_vnum` INT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (`vnum`)
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS `player`.`shop_item` (
  `shop_vnum`   INT UNSIGNED NOT NULL DEFAULT 0,  -- join column (ON shop.vnum = shop_item.shop_vnum)
  `item_vnum`   INT UNSIGNED NOT NULL DEFAULT 0,
  `count`       INT          NOT NULL DEFAULT 0,  -- int (ENABLE_NEW_STACK_LIMIT, service.h:88)
  `gold`        BIGINT       NOT NULL DEFAULT 0,  -- int64 price
  `itemvnum1`   INT UNSIGNED NOT NULL DEFAULT 0,
  `itemcount1`  INT UNSIGNED NOT NULL DEFAULT 0,
  `itemvnum2`   INT UNSIGNED NOT NULL DEFAULT 0,
  `itemcount2`  INT UNSIGNED NOT NULL DEFAULT 0,
  `itemvnum3`   INT UNSIGNED NOT NULL DEFAULT 0,
  `itemcount3`  INT UNSIGNED NOT NULL DEFAULT 0,
  `itemvnum4`   INT UNSIGNED NOT NULL DEFAULT 0,
  `itemcount4`  INT UNSIGNED NOT NULL DEFAULT 0,
  `itemvnum5`   INT UNSIGNED NOT NULL DEFAULT 0,
  `itemcount5`  INT UNSIGNED NOT NULL DEFAULT 0,
  KEY `idx_shop_vnum` (`shop_vnum`)
) ENGINE=InnoDB;

-- ---------------------------------------------------------------------
-- skill_color
--   DB: player. Table is skill_color<%postfix%>.
--   Evidence: ClientManagerPlayer.cpp:315-329 (SELECT s1_col1..s11_col5 ... WHERE player_id=%u)
--             Cache.cpp:35-52 (REPLACE INTO skill_color%s (`player_id`, `s1_col1`, ... `s11_col5`) VALUES(...))
--   11 groups (6 skills + 5 buffs, ESkillColorLength, common/length.h:1052-1059)
--   x 5 effects = 55 color columns, all uint32
--   (common/tables.h:2002-2006 TSkillColor).
--   Note: the SELECT at ClientManagerPlayer.cpp:326 is missing a comma between
--   s10_col5 and s11_col1 (query bug); columns are literal from the REPLACE.
--   56 columns.
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `player`.`skill_color` (
  `player_id` INT UNSIGNED NOT NULL,
  `s1_col1`  INT UNSIGNED NOT NULL DEFAULT 0,
  `s1_col2`  INT UNSIGNED NOT NULL DEFAULT 0,
  `s1_col3`  INT UNSIGNED NOT NULL DEFAULT 0,
  `s1_col4`  INT UNSIGNED NOT NULL DEFAULT 0,
  `s1_col5`  INT UNSIGNED NOT NULL DEFAULT 0,
  `s2_col1`  INT UNSIGNED NOT NULL DEFAULT 0,
  `s2_col2`  INT UNSIGNED NOT NULL DEFAULT 0,
  `s2_col3`  INT UNSIGNED NOT NULL DEFAULT 0,
  `s2_col4`  INT UNSIGNED NOT NULL DEFAULT 0,
  `s2_col5`  INT UNSIGNED NOT NULL DEFAULT 0,
  `s3_col1`  INT UNSIGNED NOT NULL DEFAULT 0,
  `s3_col2`  INT UNSIGNED NOT NULL DEFAULT 0,
  `s3_col3`  INT UNSIGNED NOT NULL DEFAULT 0,
  `s3_col4`  INT UNSIGNED NOT NULL DEFAULT 0,
  `s3_col5`  INT UNSIGNED NOT NULL DEFAULT 0,
  `s4_col1`  INT UNSIGNED NOT NULL DEFAULT 0,
  `s4_col2`  INT UNSIGNED NOT NULL DEFAULT 0,
  `s4_col3`  INT UNSIGNED NOT NULL DEFAULT 0,
  `s4_col4`  INT UNSIGNED NOT NULL DEFAULT 0,
  `s4_col5`  INT UNSIGNED NOT NULL DEFAULT 0,
  `s5_col1`  INT UNSIGNED NOT NULL DEFAULT 0,
  `s5_col2`  INT UNSIGNED NOT NULL DEFAULT 0,
  `s5_col3`  INT UNSIGNED NOT NULL DEFAULT 0,
  `s5_col4`  INT UNSIGNED NOT NULL DEFAULT 0,
  `s5_col5`  INT UNSIGNED NOT NULL DEFAULT 0,
  `s6_col1`  INT UNSIGNED NOT NULL DEFAULT 0,
  `s6_col2`  INT UNSIGNED NOT NULL DEFAULT 0,
  `s6_col3`  INT UNSIGNED NOT NULL DEFAULT 0,
  `s6_col4`  INT UNSIGNED NOT NULL DEFAULT 0,
  `s6_col5`  INT UNSIGNED NOT NULL DEFAULT 0,
  `s7_col1`  INT UNSIGNED NOT NULL DEFAULT 0,
  `s7_col2`  INT UNSIGNED NOT NULL DEFAULT 0,
  `s7_col3`  INT UNSIGNED NOT NULL DEFAULT 0,
  `s7_col4`  INT UNSIGNED NOT NULL DEFAULT 0,
  `s7_col5`  INT UNSIGNED NOT NULL DEFAULT 0,
  `s8_col1`  INT UNSIGNED NOT NULL DEFAULT 0,
  `s8_col2`  INT UNSIGNED NOT NULL DEFAULT 0,
  `s8_col3`  INT UNSIGNED NOT NULL DEFAULT 0,
  `s8_col4`  INT UNSIGNED NOT NULL DEFAULT 0,
  `s8_col5`  INT UNSIGNED NOT NULL DEFAULT 0,
  `s9_col1`  INT UNSIGNED NOT NULL DEFAULT 0,
  `s9_col2`  INT UNSIGNED NOT NULL DEFAULT 0,
  `s9_col3`  INT UNSIGNED NOT NULL DEFAULT 0,
  `s9_col4`  INT UNSIGNED NOT NULL DEFAULT 0,
  `s9_col5`  INT UNSIGNED NOT NULL DEFAULT 0,
  `s10_col1` INT UNSIGNED NOT NULL DEFAULT 0,
  `s10_col2` INT UNSIGNED NOT NULL DEFAULT 0,
  `s10_col3` INT UNSIGNED NOT NULL DEFAULT 0,
  `s10_col4` INT UNSIGNED NOT NULL DEFAULT 0,
  `s10_col5` INT UNSIGNED NOT NULL DEFAULT 0,
  `s11_col1` INT UNSIGNED NOT NULL DEFAULT 0,
  `s11_col2` INT UNSIGNED NOT NULL DEFAULT 0,
  `s11_col3` INT UNSIGNED NOT NULL DEFAULT 0,
  `s11_col4` INT UNSIGNED NOT NULL DEFAULT 0,
  `s11_col5` INT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (`player_id`)
) ENGINE=InnoDB;

-- ############################################################################
-- 3. PLAYER DATABASE - SOCIAL / FEATURE TABLE-EK
-- ############################################################################

-- ============================================================================
-- db_player_social.sql
-- Draft DDL for PLAYER-database social/account-side tables of a Metin2-based
-- server, reverse-engineered from the C++ sources in:
--   D:\AurigaGlobal\LiveWork\AurigaGlobal\SRC\Server
--
-- Method: every column name below appears literally in a SQL string in the
-- source. Types are inferred from C++ struct field types and printf format
-- specifiers (per task rules). Where a name/type/order could not be literally
-- evidenced it is flagged with  -- TODO(verify)  and repeated in
-- db_player_social_notes.md.
--
-- Compile-time feature flags that are DEFINED in this tree (see
-- SRC/Server/common/service.h, CommonDefines.h) and therefore determine the
-- effective column sets:
--   ADVANCED_GUILD_INFO, ENABLE_GUILD_RENEWAL_BY_RAZOR93, ENABLE_BATTLE_PASS,
--   ENABLE_ITEMSHOP, ENABLE_EVENT_MANAGER, __DUNGEON_INFO_SYSTEM__,
--   ENABLE_DAILY_REWARD_HWID_LIMIT_RAZOR93, ENABLE_NEW_OFFLINESHOP_LOGS,
--   __SKILL_COLOR_SYSTEM__, ENABLE_EXTRA_INVENTORY, ENABLE_SWITCHBOT,
--   ENABLE_NEW_PET_EDITS, ENABLE_NEW_BONUS_TALISMAN, ENABLE_PENDANT,
--   ENABLE_ATTR_COSTUMES, ENABLE_STOLE_COSTUME, ENABLE_WEAPON_COSTUME_SYSTEM,
--   ENABLE_EXTEND_ITEM_AWARD, ATTR_LOCK, KASMIR_PAKET_SYSTEM,
--   __ENABLE_NEW_OFFLINESHOP__
-- NOT defined (so the corresponding columns are absent):
--   __ENABLE_CHEQUE_SYSTEM__, __ENABLE_CHANGELOOK_SYSTEM__,
--   ENABLE_ITEM_ATTR_COSTUME
--
-- ITEM_SOCKET_MAX_NUM = 3 and ITEM_ATTRIBUTE_MAX_NUM = 8
--   (SRC/Server/common/item_length.h).
-- ============================================================================

-- ############################################################################
-- 1. GUILD / GUILD RELATED
-- ############################################################################

-- ----------------------------------------------------------------------------
-- guild
-- Evidence:
--   Database/GuildManager.cpp:171,202  SELECT id,name,ladder_point,win,draw,loss,gold,level FROM guild
--   Database/GuildManager.cpp:212      SELECT id,name,ladder_point ... ORDER BY ladder_point DESC
--   Database/GuildManager.cpp:355/369/383  UPDATE guild SET win/loss/draw
--   Database/GuildManager.cpp:737/773      UPDATE guild SET ladder_point / gold=%lld
--   Database/GuildManager.cpp:1203         UPDATE guild SET master=%u
--   GameServer/guild/guild.cpp:146     INSERT INTO guild(name,master,sp,level,exp,skill_point,skill)
--   GameServer/guild/guild.cpp:695     SELECT master,level,exp,name,skill_point,skill,sp,ladder_point,win,draw,loss,gold[,trophies]
--   GameServer/guild/guild.cpp:713,719,738,1566  UPDATE trophies / level,exp,skill_point / sp,skill_point,skill
--   GameServer/ecs/systems/CombatSystem.cpp:951  g.name,g.master,g.win,g.draw,g.loss
--   guild.cpp:147 seeds skill with 12 NUL bytes (GUILD_SKILL_COUNT=12,
--   GameServer/combat/skill.h:160-172)
-- ----------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `player`.`guild` (
  `id`           INT          NOT NULL AUTO_INCREMENT,
  `name`         VARCHAR(13)  NOT NULL DEFAULT '',
  `master`       INT UNSIGNED NOT NULL DEFAULT 0,
  `sp`           INT          NOT NULL DEFAULT 0,
  `level`        TINYINT UNSIGNED NOT NULL DEFAULT 1,
  `exp`          INT UNSIGNED NOT NULL DEFAULT 0,
  `skill_point`  INT          NOT NULL DEFAULT 0,
  `skill`        VARCHAR(12)  NOT NULL DEFAULT '',  -- TODO(verify): raw 12 bytes written via EscapeString; CHAR(12)/VARBINARY may be intended
  `ladder_point` INT          NOT NULL DEFAULT 0,
  `win`          INT UNSIGNED NOT NULL DEFAULT 0,
  `draw`         INT UNSIGNED NOT NULL DEFAULT 0,
  `loss`         INT UNSIGNED NOT NULL DEFAULT 0,
  `gold`         BIGINT       NOT NULL DEFAULT 0,
  `trophies`     INT UNSIGNED NOT NULL DEFAULT 0,  -- ADVANCED_GUILD_INFO (service.h:20)
  PRIMARY KEY (`id`),
  KEY `idx_name` (`name`)  -- SELECT COUNT(*) ... WHERE name=... (guild_manager.cpp:112)
) ENGINE=InnoDB;

-- ----------------------------------------------------------------------------
-- guild_member
-- Evidence:
--   Database/ClientManagerGuild.cpp:32  INSERT INTO guild_member VALUES(pid, guild_id, grade, 0, 0)
--                                       -> exactly 5 columns, positional
--   Database/ClientManagerGuild.cpp:38  SELECT pid,grade,is_general,offer,level,job,name
--                                       FROM guild_member,player WHERE guild_id=... AND pid=id
--                                       (level/job/name come from player, NOT guild_member)
--   Database/ClientManagerGuild.cpp:72  DELETE ... WHERE pid=%u and guild_id=%u
--   Database/GuildManager.cpp:1206/1209 UPDATE grade WHERE pid
--   GameServer/guild/guild.cpp:1880     UPDATE grade,offer,is_general WHERE pid,guild_id
-- Types: pid uint32, grade uint8, is_general uint8, offer uint32 (TPacketDGGuildMember).
-- ----------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `player`.`guild_member` (
  `pid`        INT UNSIGNED NOT NULL,
  `guild_id`   INT UNSIGNED NOT NULL DEFAULT 0,
  `grade`      TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `is_general` TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `offer`      INT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (`pid`),
  KEY `idx_guild_id` (`guild_id`)  -- SELECT COUNT(*)/AVG(level) ... WHERE guild_id (GuildManager.cpp:983,999)
) ENGINE=InnoDB;

-- ----------------------------------------------------------------------------
-- guild_grade
-- Evidence:
--   GameServer/guild/guild.cpp:157  INSERT INTO guild_grade VALUES(?, ?, ?, ?)
--                                   order: guild_id, grade, name, auth (exactly 4 columns)
--   GameServer/guild/guild.cpp:704  SELECT grade,name,auth+0 FROM guild_grade WHERE guild_id
--   GameServer/guild/guild.cpp:866,907,949  ... WHERE guild_id AND grade; UPDATE name/auth
-- Types: guild_id uint32, grade uint8 (1..15), name GUILD_NAME_MAX_LEN=12, auth uint8/int.
-- ----------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `player`.`guild_grade` (
  `guild_id` INT UNSIGNED NOT NULL,
  `grade`    TINYINT UNSIGNED NOT NULL,
  `name`     VARCHAR(13) NOT NULL DEFAULT '',
  `auth`     INT         NOT NULL DEFAULT 0,
  PRIMARY KEY (`guild_id`, `grade`)
) ENGINE=InnoDB;

-- ----------------------------------------------------------------------------
-- guild_comment
-- Evidence:
--   GameServer/guild/guild.cpp:1147  INSERT INTO guild_comment(guild_id,name,notice,content,time) VALUES(?,?,?,?,NOW())
--   GameServer/guild/guild.cpp:1166  DELETE ... WHERE id=%u AND guild_id=%u
--   GameServer/guild/guild.cpp:1170  DELETE ... WHERE id=? AND guild_id=? AND name=?
--   GameServer/guild/guild.cpp:1202  SELECT id,name,content ... ORDER BY notice DESC,id DESC LIMIT 12
--   Database/ClientManagerPlayer.cpp:1574  DELETE FROM guild_comment WHERE name=?
-- Types: notice is 1 when content starts with '!' (guild.cpp:1155), content max 50.
-- ----------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `player`.`guild_comment` (
  `id`       INT UNSIGNED NOT NULL AUTO_INCREMENT,
  `guild_id` INT UNSIGNED NOT NULL,
  `name`     VARCHAR(25)  NOT NULL DEFAULT '',
  `notice`   TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `content`  VARCHAR(51)  NOT NULL DEFAULT '',
  `time`     DATETIME     NOT NULL,
  PRIMARY KEY (`id`),
  KEY `idx_guild_id` (`guild_id`)
) ENGINE=InnoDB;

-- ----------------------------------------------------------------------------
-- guild_invite_limit
-- Evidence:
--   GameServer/guild/guild.cpp:321, guild_input.cpp:272
--       REPLACE INTO guild_invite_limit VALUES(guild_id, get_global_time())
--   GameServer/guild/guild.cpp:2412  SELECT value FROM guild_invite_limit WHERE id=%d
-- Exactly 2 columns (positional insert): id, value. Both ints.
-- ----------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `player`.`guild_invite_limit` (
  `id`    INT UNSIGNED NOT NULL,
  `value` INT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB;

-- ----------------------------------------------------------------------------
-- NOTE: `guild_skill` IS NOT A TABLE in this code base.
-- Every hit for "guild_skill" is a local variable of type TPacketGuildSkillUpdate
-- (GameServer/guild/guild.cpp:175,724). Guild skills are persisted in the
-- `guild`.`skill` (12 bytes), `guild`.`sp` and `guild`.`skill_point` columns.
-- No CREATE/INSERT/SELECT against a guild_skill table exists.
-- ----------------------------------------------------------------------------

-- ----------------------------------------------------------------------------
-- guild_war_reservation
-- Evidence: Database/GuildManager.cpp
--   921/922  SELECT id,guild1,guild2,UNIX_TIMESTAMP(time),type,warprice,initscore,
--                  bet_from,bet_to,power1,power2,handicap ... WHERE started=... winner=-1
--   1082     INSERT INTO guild_war_reservation (guild1,guild2,time,type,warprice,
--                  initscore,power1,power2,handicap) VALUES(...,DATE_ADD(NOW(),INTERVAL 180 SECOND),...)
--   1127     UPDATE ... SET started=1 WHERE id=%u
--   1321     UPDATE ... SET bet_from=%u,bet_to=%u WHERE id=%u
--   1345     UPDATE ... SET started=1,winner=0 WHERE id=%u
--   1395     UPDATE ... SET started=1,winner=%u,result1=%d,result2=%d WHERE id=%u
-- Types: TGuildWarReserve (GuildManager.h) uint32 ids, uint8 type, int32 price/score,
--        time is DATETIME (UNIX_TIMESTAMP / NOW() based).
-- ----------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `player`.`guild_war_reservation` (
  `id`         INT UNSIGNED NOT NULL AUTO_INCREMENT,
  `guild1`     INT UNSIGNED NOT NULL,
  `guild2`     INT UNSIGNED NOT NULL,
  `time`       DATETIME     NOT NULL,
  `type`       TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `warprice`   INT          NOT NULL DEFAULT 0,
  `initscore`  INT          NOT NULL DEFAULT 0,
  `bet_from`   INT UNSIGNED NOT NULL DEFAULT 0,
  `bet_to`     INT UNSIGNED NOT NULL DEFAULT 0,
  `power1`     INT          NOT NULL DEFAULT 0,
  `power2`     INT          NOT NULL DEFAULT 0,
  `handicap`   INT          NOT NULL DEFAULT 0,
  `started`    TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `winner`     INT          NOT NULL DEFAULT -1,
  `result1`    INT          NOT NULL DEFAULT 0,
  `result2`    INT          NOT NULL DEFAULT 0,
  PRIMARY KEY (`id`),
  KEY `idx_started_winner` (`started`, `winner`)
) ENGINE=InnoDB;

-- ----------------------------------------------------------------------------
-- guild_war_bet
-- Evidence: Database/GuildManager.cpp
--   1229  SELECT login,guild,gold FROM guild_war_bet WHERE war_id=%u
--   1305  INSERT INTO guild_war_bet (war_id,login,gold,guild) VALUES(?,?,?,?)
-- Types: war_id uint32, login string (LOGIN_MAX_LEN=30), gold uint32/`SqlParam::Int`,
--        guild uint32.  -- TODO(verify) gold width (uint32 vs BIGINT)
-- ----------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `player`.`guild_war_bet` (
  `war_id` INT UNSIGNED NOT NULL,
  `login`  VARCHAR(31)  NOT NULL DEFAULT '',
  `gold`   BIGINT       NOT NULL DEFAULT 0,  -- int64_t (kod: str_to_number int64, 2026-10-06 migracio)
  `guild`  INT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (`war_id`, `login`)
) ENGINE=InnoDB;

-- ----------------------------------------------------------------------------
-- GUILD RENEWAL (ENABLE_GUILD_RENEWAL_BY_RAZOR93; service.h / CommonDefines.h)
-- Evidence: GameServer/guild/guild_renewal.cpp
-- ----------------------------------------------------------------------------

-- guild_renewal_money  (guild_renewal.cpp:183,193,457)
--   SELECT money FROM guild_renewal_money WHERE guild_id=%u
--   INSERT IGNORE INTO guild_renewal_money(guild_id,money) VALUES(%u,0)
--   REPLACE INTO guild_renewal_money(guild_id,money) VALUES(%u,%lld)
CREATE TABLE IF NOT EXISTS `player`.`guild_renewal_money` (
  `guild_id` INT UNSIGNED NOT NULL,
  `money`    BIGINT       NOT NULL DEFAULT 0,
  PRIMARY KEY (`guild_id`)
) ENGINE=InnoDB;

-- guild_renewal_tax  (guild_renewal.cpp:201-203,222,464-466)
--   SELECT active,deadline,per_member_money,item1_vnum,item1_count,...,item5_count
--   INSERT IGNORE ... (guild_id,active,deadline,per_member_money) VALUES(%u,0,0,0)
--   REPLACE INTO ... (guild_id,active,deadline,per_member_money,item1_vnum,item1_count,
--                     item2_vnum,item2_count,item3_vnum,item3_count,item4_vnum,item4_count,
--                     item5_vnum,item5_count)
CREATE TABLE IF NOT EXISTS `player`.`guild_renewal_tax` (
  `guild_id`         INT UNSIGNED NOT NULL,
  `active`           TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `deadline`         INT          NOT NULL DEFAULT 0,
  `per_member_money` BIGINT       NOT NULL DEFAULT 0,
  `item1_vnum`       INT UNSIGNED NOT NULL DEFAULT 0,
  `item1_count`      INT UNSIGNED NOT NULL DEFAULT 0,
  `item2_vnum`       INT UNSIGNED NOT NULL DEFAULT 0,
  `item2_count`      INT UNSIGNED NOT NULL DEFAULT 0,
  `item3_vnum`       INT UNSIGNED NOT NULL DEFAULT 0,
  `item3_count`      INT UNSIGNED NOT NULL DEFAULT 0,
  `item4_vnum`       INT UNSIGNED NOT NULL DEFAULT 0,
  `item4_count`      INT UNSIGNED NOT NULL DEFAULT 0,
  `item5_vnum`       INT UNSIGNED NOT NULL DEFAULT 0,
  `item5_count`      INT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (`guild_id`)
) ENGINE=InnoDB;

-- guild_renewal_storage  (guild_renewal.cpp:230,486)
--   SELECT slot,vnum,count FROM guild_renewal_storage WHERE guild_id=%u
--   REPLACE INTO guild_renewal_storage(guild_id,slot,vnum,count) VALUES(%u,%u,%u,%u)
CREATE TABLE IF NOT EXISTS `player`.`guild_renewal_storage` (
  `guild_id` INT UNSIGNED NOT NULL,
  `slot`     SMALLINT UNSIGNED NOT NULL,
  `vnum`     INT UNSIGNED NOT NULL DEFAULT 0,
  `count`    INT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (`guild_id`, `slot`)
) ENGINE=InnoDB;

-- guild_renewal_contrib  (guild_renewal.cpp:250,499)
--   SELECT pid,paid_money,paid_item_total,paid_flag,last_pay_time FROM guild_renewal_contrib WHERE guild_id=%u
--   REPLACE INTO guild_renewal_contrib(guild_id,pid,paid_money,paid_item_total,paid_flag,last_pay_time)
CREATE TABLE IF NOT EXISTS `player`.`guild_renewal_contrib` (
  `guild_id`        INT UNSIGNED NOT NULL,
  `pid`             INT UNSIGNED NOT NULL,
  `paid_money`      BIGINT       NOT NULL DEFAULT 0,
  `paid_item_total` BIGINT       NOT NULL DEFAULT 0,
  `paid_flag`       TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `last_pay_time`   INT          NOT NULL DEFAULT 0,
  PRIMARY KEY (`guild_id`, `pid`)
) ENGINE=InnoDB;

-- guild_renewal_kisado_item  (guild_renewal.cpp:107,124)
--   REPLACE INTO guild_renewal_kisado_item(guild_id,pid,item0,item1,item2,item3,item4)
--   SELECT pid,item0,item1,item2,item3,item4 FROM guild_renewal_kisado_item WHERE guild_id=%u
CREATE TABLE IF NOT EXISTS `player`.`guild_renewal_kisado_item` (
  `guild_id` INT UNSIGNED NOT NULL,
  `pid`      INT UNSIGNED NOT NULL,
  `item0`    INT UNSIGNED NOT NULL DEFAULT 0,
  `item1`    INT UNSIGNED NOT NULL DEFAULT 0,
  `item2`    INT UNSIGNED NOT NULL DEFAULT 0,
  `item3`    INT UNSIGNED NOT NULL DEFAULT 0,
  `item4`    INT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (`guild_id`, `pid`)
) ENGINE=InnoDB;

-- ############################################################################
-- 2. SOCIAL / ACCOUNT SIDE
-- ############################################################################

-- ----------------------------------------------------------------------------
-- marriage
-- Evidence: Database/Marriage.cpp
--   40   SELECT pid1,pid2,love_point,time,is_married FROM marriage,player ...
--   102  INSERT INTO marriage(pid1,pid2,love_point,time) VALUES(%u,%u,0,%u)
--   141  UPDATE marriage SET love_point=%d,is_married=%d WHERE pid1=%u AND pid2=%u
--   188  DELETE FROM marriage WHERE pid1=%u AND pid2=%u
--   231  UPDATE marriage SET is_married=1 WHERE pid1=%u AND pid2=%u
-- Types: TMarriage (Marriage.h): uint32 pid1/pid2, int love_point, uint32 time (unix), uint8 is_married.
-- Names of the two characters are NOT stored here (joined from player).
-- ----------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `player`.`marriage` (
  `pid1`       INT UNSIGNED NOT NULL,
  `pid2`       INT UNSIGNED NOT NULL,
  `love_point` INT          NOT NULL DEFAULT 0,
  `time`       INT UNSIGNED NOT NULL DEFAULT 0,
  `is_married` TINYINT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (`pid1`, `pid2`)  -- TODO(verify): PK implied (pid1<pid2 Align, DELETE by pair)
) ENGINE=InnoDB;

-- ----------------------------------------------------------------------------
-- messenger_list
-- Evidence: GameServer/social/messenger_manager.cpp
--   56   SELECT account,companion FROM messenger_list WHERE account=?
--   247  INSERT INTO messenger_list VALUES (?, ?)
--   645  DELETE FROM messenger_list WHERE account=? AND companion=?
--   663  DELETE FROM messenger_list WHERE account=? OR companion=?
-- Types: both are account logins (LOGIN_MAX_LEN=30). Multiple rows per account
--        -> composite key.
-- ----------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `player`.`messenger_list` (
  `account`   VARCHAR(31) NOT NULL DEFAULT '',
  `companion` VARCHAR(31) NOT NULL DEFAULT '',
  PRIMARY KEY (`account`, `companion`),
  KEY `idx_companion` (`companion`)
) ENGINE=InnoDB;

-- ----------------------------------------------------------------------------
-- horse_name
-- Evidence: Database/ClientManagerHorseName.cpp
--   8   REPLACE INTO horse_name VALUES(?, ?)      -> exactly 2 columns
--   18  SELECT name FROM horse_name WHERE id = %u
--   8   SqlParam::UInt(data->dwPlayerID), SqlParam::Text(data->szHorseName)
-- Types: id uint32; name CHARACTER_NAME_MAX_LEN=24.
-- ----------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `player`.`horse_name` (
  `id`   INT UNSIGNED NOT NULL,
  `name` VARCHAR(25)  NOT NULL DEFAULT '',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB;

-- ----------------------------------------------------------------------------
-- change_empire
-- INCOMPLETE: third column name is not literal in any query.
-- Evidence: GameServer/ecs/systems/PlayerRuntimeSystem.cpp
--   234  SELECT change_count FROM change_empire WHERE account_id = %u
--   259  INSERT INTO change_empire VALUES(%u, %d, NOW())   -> exactly 3 columns
--   262  UPDATE change_empire SET change_count=%d WHERE account_id=%u
-- Third value is NOW() -> DATETIME.  -- TODO(verify) name (e.g. last_change/date/time).
-- ----------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `player`.`change_empire` (
  `account_id`   INT UNSIGNED NOT NULL,
  `change_count` INT UNSIGNED NOT NULL DEFAULT 0,
  `last_change`  DATETIME     NOT NULL,  -- TODO(verify) column name invented (only NOW() literal)
  PRIMARY KEY (`account_id`)  -- TODO(verify): implied by per-account read/update
) ENGINE=InnoDB;

-- ----------------------------------------------------------------------------
-- highscore
-- Evidence: Database/ClientManagerPlayer.cpp
--   1639  SELECT value FROM highscore WHERE board=? AND pid = ?
--   1669  INSERT INTO highscore VALUES(?, ?, ?)
--   1691  REPLACE INTO highscore VALUES(?, ?, ?)
-- Types: board char[21] local (szBoard[21]) -> VARCHAR(20); pid uint32; value int.
-- Result handler uses board+pid as the lookup key.  -- TODO(verify) PK/unique key
-- ----------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `player`.`highscore` (
  `board` VARCHAR(20)  NOT NULL DEFAULT '',
  `pid`   INT UNSIGNED NOT NULL,
  `value` INT          NOT NULL DEFAULT 0,
  PRIMARY KEY (`board`, `pid`)  -- TODO(verify)
) ENGINE=InnoDB;

-- ----------------------------------------------------------------------------
-- event_table
-- Evidence: Database/ClientManager.cpp (ENABLE_EVENT_MANAGER)
--   4491  UPDATE player.event_table SET endTime = NOW() WHERE id = %u
--   4559  SELECT id, eventIndex+0, UNIX_TIMESTAMP(startTime), UNIX_TIMESTAMP(endTime),
--                empireFlag, channelFlag, value0, value1, value2, value3 FROM player.event_table
-- Types from TEventManagerData (common/tables.h:2082):
--   eventID uint16, eventIndex uint8, startTime/endTime int (unix, mapped to DATETIME),
--   empireFlag/channelFlag uint8, value[4] uint32.
-- ----------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `player`.`event_table` (
  `id`          SMALLINT UNSIGNED NOT NULL AUTO_INCREMENT,  -- TODO(verify): eventID is uint16 in code
  `eventIndex`  TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `startTime`   DATETIME     NULL,
  `endTime`     DATETIME     NULL,  -- 0/expired handled by code
  `empireFlag`  TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `channelFlag` TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `value0`      INT UNSIGNED NOT NULL DEFAULT 0,
  `value1`      INT UNSIGNED NOT NULL DEFAULT 0,
  `value2`      INT UNSIGNED NOT NULL DEFAULT 0,
  `value3`      INT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB;

-- ############################################################################
-- 3. DAILY REWARD (ENABLE_DAILY_REWARD_HWID_LIMIT_RAZOR93)
-- Evidence: GameServer/command/cmd_general.cpp
-- ############################################################################

-- daily_reward_status (cmd_general.cpp:217,218,227,288,334)
--   DELETE FROM player.daily_reward_status WHERE pid = %u
--   INSERT INTO player.daily_reward_status (pid,time,reward,total_rewards) VALUES(%u,NOW(),0,0)
--   SELECT UNIX_TIMESTAMP(time), reward ... WHERE pid=%u
--   UPDATE daily_reward_status SET reward = CASE WHEN reward=0 THEN '1' ... END,
--          total_rewards=total_rewards+1, time=(NOW()+interval 1 day) WHERE pid=%u
CREATE TABLE IF NOT EXISTS `player`.`daily_reward_status` (
  `pid`           INT UNSIGNED NOT NULL,
  `time`          DATETIME     NOT NULL,
  `reward`        VARCHAR(16)  NOT NULL DEFAULT '0',  -- TODO(verify): values '0'..'6' / reward name
  `total_rewards` INT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (`pid`)
) ENGINE=InnoDB;

-- daily_reward_claim_hwid (cmd_general.cpp:113,122)
--   SELECT 1 FROM player.daily_reward_claim_hwid WHERE hwkey=? AND claim_day=CURDATE() LIMIT 1
--   INSERT INTO player.daily_reward_claim_hwid (hwkey,claim_day,pid,account_id) VALUES(?,CURDATE(),?,?)
CREATE TABLE IF NOT EXISTS `player`.`daily_reward_claim_hwid` (
  `hwkey`      VARCHAR(128) NOT NULL DEFAULT '',  -- TODO(verify): hwid + '|' + host
  `claim_day`  DATE         NOT NULL,
  `pid`        INT UNSIGNED NOT NULL DEFAULT 0,
  `account_id` INT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (`hwkey`, `claim_day`)
) ENGINE=InnoDB;

-- daily_reward_items (cmd_general.cpp:249,326)
--   SELECT items, count FROM player.daily_reward_items WHERE reward = ?
--   SELECT items, count ... WHERE reward = ? ORDER BY RAND() limit 1
-- INCOMPLETE: only 3 columns evidenced; contents/types of `items`/`count` are
-- row strings parsed with str_to_number. No PK evidence.
CREATE TABLE IF NOT EXISTS `player`.`daily_reward_items` (
  `reward` VARCHAR(64)  NOT NULL DEFAULT '',  -- TODO(verify) type/length
  `items`  VARCHAR(255) NOT NULL DEFAULT '',  -- TODO(verify): TEXT?
  `count`  INT          NOT NULL DEFAULT 0     -- TODO(verify): stored as string
) ENGINE=InnoDB;

-- ----------------------------------------------------------------------------
-- myshop_pricelist  (MYSHOP_PRICE_LIST)
-- Evidence:
--   Database/Cache.cpp:290,296  DELETE FROM myshop_pricelist WHERE owner_id=%u
--                               REPLACE myshop_pricelist(owner_id,item_vnum,price) VALUES(...)
--   Database/ClientManager.cpp:2326,2359  SELECT item_vnum,price ... WHERE owner_id=%u
-- Types: TItemPriceInfo (tables.h) dwOwnerID/dwVnum/dwPrice uint32; max 120 rows/owner.
-- ----------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `player`.`myshop_pricelist` (
  `owner_id`  INT UNSIGNED NOT NULL,
  `item_vnum` INT UNSIGNED NOT NULL,
  `price`     INT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (`owner_id`, `item_vnum`)
) ENGINE=InnoDB;

-- ############################################################################
-- 5. NEW PET SYSTEM (ENABLE_NEW_PET_EDITS)
-- ############################################################################

-- ----------------------------------------------------------------------------
-- new_petsystem
-- Evidence:
--   GameServer/pet/pet_input.cpp:370  INSERT INTO new_petsystem VALUES(?, ?, 1, 0, 0, 0, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 0, ?)
--                                     (21 positional values with ENABLE_NEW_PET_EDITS)
--   GameServer/pet/New_PetSystem.cpp:1178  SELECT name,level,exp,expi,bonus0,bonus1,bonus2,skill0,skill0lv,
--                                     skill1,skill1lv,skill2,skill2lv,skill3,skill3lv,duration,tduration,evolution,evocation[,minAge]
--   New_PetSystem.cpp:294  UPDATE ... SET level,evolution,exp,expi,bonus0..2,skill0..skill3lv,duration,tduration
--   New_PetSystem.cpp:1270 UPDATE ... SET evocation=1
--   questlua_petnew.cpp:198 SELECT duration,tduration -- pet_input.cpp UPDATE `name`
-- Layout derived from the positional INSERT:
--   id,name,level,exp,expi,evolution,bonus0,bonus1,bonus2,skill0,skill0lv,skill1,skill1lv,
--   skill2,skill2lv,skill3,skill3lv,duration,tduration,evocation,minAge
--   (the literals 0 at positions 6 and 20 are evolution and evocation -- TODO(verify)).
-- Types: skill slots are int (can be -1), bonus values int (int16 bounds in code),
--        minAge is a unix timestamp (get_global_time()).
-- ----------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `player`.`new_petsystem` (
  `id`         INT UNSIGNED NOT NULL AUTO_INCREMENT,
  `name`       VARCHAR(25)  NOT NULL DEFAULT '',
  `level`      TINYINT UNSIGNED NOT NULL DEFAULT 1,
  `exp`        INT UNSIGNED NOT NULL DEFAULT 0,
  `expi`       INT UNSIGNED NOT NULL DEFAULT 0,
  `evolution`  TINYINT UNSIGNED NOT NULL DEFAULT 0,  -- TODO(verify) physical position
  `bonus0`     SMALLINT     NOT NULL DEFAULT 0,
  `bonus1`     SMALLINT     NOT NULL DEFAULT 0,
  `bonus2`     SMALLINT     NOT NULL DEFAULT 0,
  `skill0`     INT          NOT NULL DEFAULT 0,
  `skill0lv`   TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `skill1`     INT          NOT NULL DEFAULT 0,
  `skill1lv`   TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `skill2`     INT          NOT NULL DEFAULT 0,
  `skill2lv`   TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `skill3`     INT          NOT NULL DEFAULT 0,
  `skill3lv`   TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `duration`   INT UNSIGNED NOT NULL DEFAULT 0,
  `tduration`  INT UNSIGNED NOT NULL DEFAULT 0,
  `evocation`  TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `minAge`     INT UNSIGNED NOT NULL DEFAULT 0,  -- ENABLE_NEW_PET_EDITS; TODO(verify) position
  PRIMARY KEY (`id`),
  KEY `idx_name` (`name`)  -- duplicate-name check (pet_input.cpp:321, questlua_petnew.cpp:283)
) ENGINE=InnoDB;

-- ############################################################################
-- 6. BATTLE PASS (ENABLE_BATTLE_PASS)
-- ############################################################################

-- ----------------------------------------------------------------------------
-- battle_pass
-- Evidence: Database/ClientManagerPlayer.cpp
--   556   SELECT player_id,mission_id,battle_pass_id,extra_info,completed FROM battle_pass WHERE player_id=%u
--   1052  REPLACE INTO battle_pass (player_id,mission_id,battle_pass_id,extra_info,completed) VALUES(...)
-- Types: TPlayerBattlePassMission (tables.h:2010) all uint32 except uint8 completed.
-- NOTE: `battle_pass_mission` is NOT a table; mission definitions are loaded from
--       battle_pass.txt (GameServer/combat/battle_pass.cpp:125).
-- ----------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `player`.`battle_pass` (
  `player_id`      INT UNSIGNED NOT NULL,
  `mission_id`     INT UNSIGNED NOT NULL,
  `battle_pass_id` INT UNSIGNED NOT NULL DEFAULT 0,
  `extra_info`     INT UNSIGNED NOT NULL DEFAULT 0,
  `completed`      TINYINT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (`player_id`, `mission_id`, `battle_pass_id`)  -- TODO(verify): no explicit key evidence
) ENGINE=InnoDB;

-- ----------------------------------------------------------------------------
-- battle_pass_ranking
-- Evidence:
--   Database/ClientManagerPlayer.cpp:1066  SELECT player_name,UNIX_TIMESTAMP(finish_time)
--                                          FROM battle_pass_ranking WHERE battle_pass_id=%d ORDER BY finish_time ASC LIMIT 45
--   Database/ClientManagerPlayer.cpp:1151  REPLACE INTO battle_pass_ranking (player_name,battle_pass_id,finish_time) VALUES(?,?,NOW())
--   GameServer/ecs/systems/ItemSystem_LegacyBridge.cpp:5430  SELECT * FROM battle_pass_ranking WHERE player_name=? AND battle_pass_id=?
-- Types: player_name CHARACTER_NAME_MAX_LEN=24; bBattlePassId uint8; finish_time DATETIME.
-- ----------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `player`.`battle_pass_ranking` (
  `player_name`    VARCHAR(25) NOT NULL DEFAULT '',
  `battle_pass_id` TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `finish_time`    DATETIME    NOT NULL,
  PRIMARY KEY (`player_name`, `battle_pass_id`)  -- TODO(verify): REPLACE requires unique key
) ENGINE=InnoDB;

-- ############################################################################
-- 7. DUNGEON RANKING (__DUNGEON_INFO_SYSTEM__)
-- ############################################################################

-- ----------------------------------------------------------------------------
-- dungeon_ranking
-- Evidence:
--   GameServer/quest/questlua_dungeon.cpp:57,61
--     UPDATE dungeon_ranking SET completed=completed+1,time=%d,damage=%d WHERE pid=%u AND dungeon_index=%d
--     INSERT INTO dungeon_ranking (acc_id,pid,dungeon_index,completed,time,damage) VALUES(...)
--   questlua_dungeon.cpp:142  SELECT d.acc_id,d.pid,p.name,p.level,d.completed,d.time,d.damage ...
--   questlua_dungeon.cpp:242  SELECT completed,time,damage ... WHERE pid=? AND dungeon_index=?
--   GameServer/dungeon/OrcsDungeon.cpp:443/449, RuneDungeon.cpp:292/298, TritonTempleDungeon.cpp:202/208
-- NOTE: questlua_dungeon.cpp:143 also references an unqualified `account_id` in
--       the WHERE clause although all SELECT/INSERT use `acc_id`; treat as an
--       inconsistency / potential extra column.  -- TODO(verify)
-- ----------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `player`.`dungeon_ranking` (
  `acc_id`        INT UNSIGNED NOT NULL DEFAULT 0,
  `pid`           INT UNSIGNED NOT NULL,
  `dungeon_index` INT          NOT NULL,
  `completed`     INT UNSIGNED NOT NULL DEFAULT 0,
  `time`          INT UNSIGNED NOT NULL DEFAULT 0,
  `damage`        INT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (`pid`, `dungeon_index`)  -- TODO(verify)
) ENGINE=InnoDB;

-- ############################################################################
-- 8. ITEM SHOP (ENABLE_ITEMSHOP) - player side (coins live in account.account)
-- ############################################################################

-- ----------------------------------------------------------------------------
-- ishop_data
-- INCOMPLETE: names (except sellCount) are inferred from the SELECT * reading
-- order in Database/ClientManager.cpp:4700-4746 and the TIShopData struct
-- (common/tables.h:2149); the SQL text is "SELECT *" / individual
-- sellCount queries only.  All marked columns are  -- TODO(verify).
-- Evidence:
--   4690  SELECT * FROM player.ishop_data
--   4828  SELECT sellCount FROM player.ishop_data WHERE id = %u
--   4837  UPDATE player.ishop_data SET sellCount = %u WHERE id = %u
-- Read order: id, categoryType, categorySubType, itemVnum, itemPrice, discount,
--   offerTime, addedTime, sellCount, week_limit, month_limit, socket0..2,
--   attrtype0..7/attrvalue0..7 (bounds-guarded loops over ITEM_SOCKET_MAX_NUM /
--   ITEM_ATTRIBUTE_MAX_NUM).
-- ----------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `player`.`ishop_data` (
  `id`              INT UNSIGNED NOT NULL AUTO_INCREMENT,
  `categoryType`    TINYINT UNSIGNED NOT NULL DEFAULT 0,  -- TODO(verify)
  `categorySubType` TINYINT UNSIGNED NOT NULL DEFAULT 0,  -- TODO(verify)
  `itemVnum`        INT UNSIGNED NOT NULL DEFAULT 0,      -- TODO(verify)
  `itemPrice`       INT UNSIGNED NOT NULL DEFAULT 0,      -- TODO(verify)
  `discount`        TINYINT UNSIGNED NOT NULL DEFAULT 0,  -- TODO(verify)
  `offerTime`       DATETIME     NULL,                    -- TODO(verify): read as date string
  `addedTime`       DATETIME     NULL,                    -- TODO(verify): read as date string
  `sellCount`       INT UNSIGNED NOT NULL DEFAULT 0,      -- literal name
  `week_limit`      INT          NOT NULL DEFAULT 0,      -- TODO(verify)
  `month_limit`     INT          NOT NULL DEFAULT 0,      -- TODO(verify)
  `socket0`         INT          NOT NULL DEFAULT 0,      -- TODO(verify)
  `socket1`         INT          NOT NULL DEFAULT 0,      -- TODO(verify)
  `socket2`         INT          NOT NULL DEFAULT 0,      -- TODO(verify)
  `attrtype0`       TINYINT UNSIGNED NOT NULL DEFAULT 0, `attrvalue0` SMALLINT NOT NULL DEFAULT 0,  -- TODO(verify)
  `attrtype1`       TINYINT UNSIGNED NOT NULL DEFAULT 0, `attrvalue1` SMALLINT NOT NULL DEFAULT 0,  -- TODO(verify)
  `attrtype2`       TINYINT UNSIGNED NOT NULL DEFAULT 0, `attrvalue2` SMALLINT NOT NULL DEFAULT 0,  -- TODO(verify)
  `attrtype3`       TINYINT UNSIGNED NOT NULL DEFAULT 0, `attrvalue3` SMALLINT NOT NULL DEFAULT 0,  -- TODO(verify)
  `attrtype4`       TINYINT UNSIGNED NOT NULL DEFAULT 0, `attrvalue4` SMALLINT NOT NULL DEFAULT 0,  -- TODO(verify)
  `attrtype5`       TINYINT UNSIGNED NOT NULL DEFAULT 0, `attrvalue5` SMALLINT NOT NULL DEFAULT 0,  -- TODO(verify)
  `attrtype6`       TINYINT UNSIGNED NOT NULL DEFAULT 0, `attrvalue6` SMALLINT NOT NULL DEFAULT 0,  -- TODO(verify)
  `attrtype7`       TINYINT UNSIGNED NOT NULL DEFAULT 0, `attrvalue7` SMALLINT NOT NULL DEFAULT 0,  -- TODO(verify)
  PRIMARY KEY (`id`)
) ENGINE=InnoDB;

-- ----------------------------------------------------------------------------
-- ishop_log
-- Evidence: Database/ClientManager.cpp
--   5198  INSERT INTO player.ishop_log (accountID,playerName,buyDate,buyTime,ipAdress,itemVnum,itemCount,itemPrice)
--         VALUES(?,?,NOW(),?,?,?,?,?)
--   4868  SELECT * FROM player.ishop_log WHERE accountID = %u
--   4977  ... WHERE buyDate > DATE_SUB(NOW(),INTERVAL 1 WEEK) AND accountID=? AND itemVnum=?
--   5005  ... WHERE buyDate > DATE_SUB(NOW(),INTERVAL 1 MONTH) AND accountID=? AND itemVnum=?
-- SELECT * reads exactly 8 fields (no id column!) in the same order as the INSERT.
-- Types from TIShopLogData (tables.h:2164): accountID uint32, playerName char[25],
--   buyDate char[21] (DATETIME), buyTime int, ipAdress char[16], itemVnum uint32,
--   itemCount int, itemPrice uint32.
-- ----------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `player`.`ishop_log` (
  `accountID`  INT UNSIGNED NOT NULL,
  `playerName` VARCHAR(25)  NOT NULL DEFAULT '',
  `buyDate`    DATETIME     NOT NULL,
  `buyTime`    INT          NOT NULL DEFAULT 0,
  `ipAdress`   VARCHAR(16)  NOT NULL DEFAULT '',
  `itemVnum`   INT UNSIGNED NOT NULL DEFAULT 0,
  `itemCount`  INT          NOT NULL DEFAULT 0,
  `itemPrice`  INT UNSIGNED NOT NULL DEFAULT 0,
  KEY `idx_account` (`accountID`),
  KEY `idx_limit` (`buyDate`, `accountID`, `itemVnum`)  -- weekly/monthly limit queries
) ENGINE=InnoDB;

-- ############################################################################
-- 9. ITEM AWARD
-- ############################################################################

-- ----------------------------------------------------------------------------
-- item_award
-- Evidence:
--   Database/ItemAwardManager.cpp:64-74  SELECT id,login,vnum,count,socket0,socket1,socket2,
--        attrtype0,attrvalue0,...,attrtype7,attrvalue7,mall,why
--        FROM item_award WHERE taken_time IS NULL and id > %d   (ENABLE_EXTEND_ITEM_AWARD)
--   ItemAwardManager.cpp:177  UPDATE item_award SET taken_time=NOW(),item_id=%u WHERE id=%u AND taken_time IS NULL
--   Database/GuildManager.cpp:1171,1180,1356,1435
--        INSERT INTO item_award (login,vnum,socket0,given_time) VALUES(?,?,?,NOW())
--   GameServer/guild/guild.cpp:1618
--        INSERT INTO item_award (login,vnum,count,why,mall) VALUES(?,?,1,'GUILD_REWARD',1)
--   Database/sql/2026_04_15_add_8th_bonus.sql:10-12 adds attrtype7/attrvalue7
-- Types: TItemAward (ItemAwardManager.h): id/vnum/count/socketN uint32, bType uint8,
--        sValue int16, mall bool, why char[50], login char[30].
-- ----------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `player`.`item_award` (
  `id`         INT UNSIGNED NOT NULL AUTO_INCREMENT,
  `login`      VARCHAR(31)  NOT NULL DEFAULT '',
  `vnum`       INT UNSIGNED NOT NULL DEFAULT 0,
  `count`      INT UNSIGNED NOT NULL DEFAULT 1,
  `socket0`    INT UNSIGNED NOT NULL DEFAULT 0,
  `socket1`    INT UNSIGNED NOT NULL DEFAULT 0,
  `socket2`    INT UNSIGNED NOT NULL DEFAULT 0,
  `attrtype0`  TINYINT UNSIGNED NOT NULL DEFAULT 0, `attrvalue0` SMALLINT NOT NULL DEFAULT 0,
  `attrtype1`  TINYINT UNSIGNED NOT NULL DEFAULT 0, `attrvalue1` SMALLINT NOT NULL DEFAULT 0,
  `attrtype2`  TINYINT UNSIGNED NOT NULL DEFAULT 0, `attrvalue2` SMALLINT NOT NULL DEFAULT 0,
  `attrtype3`  TINYINT UNSIGNED NOT NULL DEFAULT 0, `attrvalue3` SMALLINT NOT NULL DEFAULT 0,
  `attrtype4`  TINYINT UNSIGNED NOT NULL DEFAULT 0, `attrvalue4` SMALLINT NOT NULL DEFAULT 0,
  `attrtype5`  TINYINT UNSIGNED NOT NULL DEFAULT 0, `attrvalue5` SMALLINT NOT NULL DEFAULT 0,
  `attrtype6`  TINYINT UNSIGNED NOT NULL DEFAULT 0, `attrvalue6` SMALLINT NOT NULL DEFAULT 0,
  `attrtype7`  TINYINT UNSIGNED NOT NULL DEFAULT 0, `attrvalue7` SMALLINT NOT NULL DEFAULT 0,
  `mall`       TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `why`        VARCHAR(51)  NULL,
  `taken_time` DATETIME     NULL DEFAULT NULL,
  `item_id`    INT UNSIGNED NOT NULL DEFAULT 0,
  `given_time` DATETIME     NULL DEFAULT NULL,
  PRIMARY KEY (`id`),
  KEY `idx_taken_time` (`taken_time`)  -- RequestLoad WHERE taken_time IS NULL
) ENGINE=InnoDB;

-- ############################################################################
-- 11. LOGS / MISC
-- ############################################################################

-- ----------------------------------------------------------------------------
-- money_log
-- INCOMPLETE: only a positional 4-value INSERT; column names are not literal.
-- Evidence: GameServer/core/log.cpp:148
--   INSERT INTO money_log%s VALUES (NOW(), %d, %d, %lld)
--   (PostfixDatabase.cpp:41 shows an older, commented-out string variant.)
-- The 4 values are time, type, vnum, gold (uint8 type, uint32 vnum, int64 gold).
-- -- TODO(verify) all column names.
-- ----------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `player`.`money_log` (
  `time` DATETIME     NOT NULL,             -- TODO(verify) name
  `type` TINYINT UNSIGNED NOT NULL DEFAULT 0,  -- TODO(verify) name
  `vnum` INT UNSIGNED NOT NULL DEFAULT 0,   -- TODO(verify) name
  `gold` BIGINT       NOT NULL DEFAULT 0    -- TODO(verify) name
) ENGINE=InnoDB;

-- ----------------------------------------------------------------------------
-- lotto_list
-- INCOMPLETE: only positional INSERT evidence; column names not literal.
-- Evidence:
--   GameServer/item/item_manager.cpp:1528 and quest/questlua_pc.cpp:2011
--     INSERT INTO lotto_list VALUES(0, 'server%s', %u, NOW())
--   values: 0 (auto id), 'server<postfix>', <player id>, NOW().
-- -- TODO(verify) names, especially the 3rd column (pid vs item vnum).
-- ----------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `player`.`lotto_list` (
  `id`     INT UNSIGNED NOT NULL AUTO_INCREMENT,
  `server` VARCHAR(32)  NOT NULL DEFAULT '',  -- TODO(verify)
  `pid`    INT UNSIGNED NOT NULL DEFAULT 0,   -- TODO(verify): 3rd arg is GetPlayerID()
  `time`   DATETIME     NOT NULL,             -- TODO(verify)
  PRIMARY KEY (`id`)
) ENGINE=InnoDB;

-- ----------------------------------------------------------------------------
-- NOTE: `wiki_info` IS NOT A DB TABLE in this code base.
-- The only occurrences are the protocol struct TWikiInfoTable and its accessor
-- mutable_wiki_info() in common/in_game_wiki.h (and the Launcher copy). No SQL
-- references it (verified repo-wide).
-- ----------------------------------------------------------------------------

-- ############################################################################
-- 12. OFFLINESHOP (__ENABLE_NEW_OFFLINESHOP__)
-- Effective flags: KASMIR_PAKET_SYSTEM -> `npc`; ATTR_LOCK -> `locked_attr`;
-- __ENABLE_CHEQUE_SYSTEM__/__ENABLE_CHANGELOOK_SYSTEM__ are OFF, so no
-- price_cheque / trans columns exist.
-- ############################################################################

-- ----------------------------------------------------------------------------
-- offlineshop_shops
-- Evidence:
--   Database/ClientManagerOfflineshop.cpp:35  SELECT `owner_id`,`name`,`duration`,`npc` FROM `player`.`offlineshop_shops` (KASMIR)
--   OfflineshopCache.cpp:1376  INSERT INTO `player`.`offlineshop_shops` (`owner_id`,`duration`,`name`,`npc`)
--   OfflineshopCache.cpp:1396  UPDATE ... SET `name`=? WHERE `owner_id`=?
--   OfflineshopCache.cpp:1404  UPDATE ... SET `duration`='%d' WHERE `owner_id`=%u
--   ClientManagerOfflineshop.cpp:1864  SELECT owner_id ... WHERE owner_id=...
-- Types: owner_id/duration/npc uint32; name OFFLINE_SHOP_NAME_MAX_LEN=65.
-- ----------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `player`.`offlineshop_shops` (
  `owner_id` INT UNSIGNED NOT NULL,
  `name`     VARCHAR(65)  NOT NULL DEFAULT '',
  `duration` INT UNSIGNED NOT NULL DEFAULT 0,  -- minutes; 0 = infinite (OfflineshopCache.cpp:329)
  `npc`      INT UNSIGNED NOT NULL DEFAULT 0,  -- KASMIR_PAKET_SYSTEM
  PRIMARY KEY (`owner_id`)
) ENGINE=InnoDB;

-- ----------------------------------------------------------------------------
-- offlineshop_shop_items
-- Effective column list (from dynamic builders):
--   OfflineshopCache.cpp:1263-1324 (INSERT), ClientManagerOfflineshop.cpp:1876-1904 (SELECT)
--   item_id, owner_id, price_yang, vnum, count, socket0..2 (ITEM_SOCKET_MAX_NUM=3),
--   attr0,attrval0 .. attr7,attrval7 (ITEM_ATTRIBUTE_MAX_NUM=8), is_sold,
--   locked_attr (ATTR_LOCK), expiration
-- Also: Database/sql/2026_04_15_add_8th_bonus.sql:21-23 adds attr7/attrval7.
-- DELETE/UPDATE/SOLD evidence: OfflineshopCache.cpp:1349,1357,1617,1625,456-458.
-- ----------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `player`.`offlineshop_shop_items` (
  `item_id`     INT UNSIGNED NOT NULL AUTO_INCREMENT,
  `owner_id`    INT UNSIGNED NOT NULL DEFAULT 0,
  `price_yang`  BIGINT       NOT NULL DEFAULT 0,
  `vnum`        INT UNSIGNED NOT NULL DEFAULT 0,
  `count`       INT UNSIGNED NOT NULL DEFAULT 0,
  `socket0`     INT          NOT NULL DEFAULT 0,
  `socket1`     INT          NOT NULL DEFAULT 0,
  `socket2`     INT          NOT NULL DEFAULT 0,
  `attr0`       INT          NOT NULL DEFAULT 0, `attrval0` INT NOT NULL DEFAULT 0,
  `attr1`       INT          NOT NULL DEFAULT 0, `attrval1` INT NOT NULL DEFAULT 0,
  `attr2`       INT          NOT NULL DEFAULT 0, `attrval2` INT NOT NULL DEFAULT 0,
  `attr3`       INT          NOT NULL DEFAULT 0, `attrval3` INT NOT NULL DEFAULT 0,
  `attr4`       INT          NOT NULL DEFAULT 0, `attrval4` INT NOT NULL DEFAULT 0,
  `attr5`       INT          NOT NULL DEFAULT 0, `attrval5` INT NOT NULL DEFAULT 0,
  `attr6`       INT          NOT NULL DEFAULT 0, `attrval6` INT NOT NULL DEFAULT 0,
  `attr7`       INT          NOT NULL DEFAULT 0, `attrval7` INT NOT NULL DEFAULT 0,
  `is_sold`     TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `locked_attr` INT          NOT NULL DEFAULT 0,  -- ATTR_LOCK
  `expiration`  TINYINT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (`item_id`),
  KEY `idx_owner` (`owner_id`),
  KEY `idx_sold` (`is_sold`)  -- DELETE ... WHERE is_sold=1 (OfflineshopCache.cpp:457)
) ENGINE=InnoDB;

-- ----------------------------------------------------------------------------
-- offlineshop_offers
-- Evidence:
--   ClientManagerOfflineshop.cpp:151  SELECT `offer_id`,`owner_id`,`offerer_id`,`item_id`,`price_yang`,`is_notified`,`is_accept`,`buyer_name`
--   OfflineshopCache.cpp:1572  INSERT INTO `player`.`offlineshop_offers` (`offer_id`,`owner_id`,`offerer_id`,`item_id`,`price_yang`,`is_notified`,`is_accept`,`buyer_name`) VALUES(0,...)
--   OfflineshopCache.cpp:1580/1588/1597/1607  UPDATE/DELETE by offer_id / owner_id
-- (price_cheque is commented out in both SELECT and INSERT).
-- ----------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `player`.`offlineshop_offers` (
  `offer_id`    INT UNSIGNED NOT NULL AUTO_INCREMENT,
  `owner_id`    INT UNSIGNED NOT NULL DEFAULT 0,
  `offerer_id`  INT UNSIGNED NOT NULL DEFAULT 0,
  `item_id`     INT UNSIGNED NOT NULL DEFAULT 0,
  `price_yang`  BIGINT       NOT NULL DEFAULT 0,
  `is_notified` TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `is_accept`   TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `buyer_name`  VARCHAR(25)  NOT NULL DEFAULT '',
  PRIMARY KEY (`offer_id`),
  KEY `idx_owner` (`owner_id`),
  KEY `idx_item`  (`item_id`)
) ENGINE=InnoDB;

-- ----------------------------------------------------------------------------
-- offlineshop_auctions
-- Effective column list (dynamic builders):
--   ClientManagerOfflineshop.cpp:1939-1962 (SELECT), OfflineshopCache.cpp:1633-1686 (INSERT)
--   owner_id, duration, name, init_yang, vnum, count, socket0..2,
--   attr0..attr7/attrval0..attrval7, locked_attr (ATTR_LOCK), expiration
--   2026_04_15_add_8th_bonus.sql:27-29 adds attr7/attrval7.
-- ----------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `player`.`offlineshop_auctions` (
  `owner_id`    INT UNSIGNED NOT NULL,
  `duration`    INT UNSIGNED NOT NULL DEFAULT 0,
  `name`        VARCHAR(25)  NOT NULL DEFAULT '',
  `init_yang`   BIGINT       NOT NULL DEFAULT 0,
  `vnum`        INT UNSIGNED NOT NULL DEFAULT 0,
  `count`       INT UNSIGNED NOT NULL DEFAULT 0,
  `socket0`     INT          NOT NULL DEFAULT 0,
  `socket1`     INT          NOT NULL DEFAULT 0,
  `socket2`     INT          NOT NULL DEFAULT 0,
  `attr0`       INT          NOT NULL DEFAULT 0, `attrval0` INT NOT NULL DEFAULT 0,
  `attr1`       INT          NOT NULL DEFAULT 0, `attrval1` INT NOT NULL DEFAULT 0,
  `attr2`       INT          NOT NULL DEFAULT 0, `attrval2` INT NOT NULL DEFAULT 0,
  `attr3`       INT          NOT NULL DEFAULT 0, `attrval3` INT NOT NULL DEFAULT 0,
  `attr4`       INT          NOT NULL DEFAULT 0, `attrval4` INT NOT NULL DEFAULT 0,
  `attr5`       INT          NOT NULL DEFAULT 0, `attrval5` INT NOT NULL DEFAULT 0,
  `attr6`       INT          NOT NULL DEFAULT 0, `attrval6` INT NOT NULL DEFAULT 0,
  `attr7`       INT          NOT NULL DEFAULT 0, `attrval7` INT NOT NULL DEFAULT 0,
  `locked_attr` INT          NOT NULL DEFAULT 0,  -- ATTR_LOCK
  `expiration`  TINYINT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (`owner_id`)  -- one auction per owner (TAuctionInfo keyed by owner)
) ENGINE=InnoDB;

-- ----------------------------------------------------------------------------
-- offlineshop_auction_offers
-- Evidence:
--   ClientManagerOfflineshop.cpp:255  SELECT `owner_id`,`buyer_id`,`buyer_name`,`yang` FROM `player`.`offlineshop_auction_offers`
--   OfflineshopCache.cpp:1699  INSERT INTO `player`.`offlineshop_auction_offers` (`owner_id`,`buyer_id`,`buyer_name`,`yang`) VALUES(?,?,?,?)
--   OfflineshopCache.cpp:1715  DELETE ... WHERE `owner_id`=%u
-- ----------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `player`.`offlineshop_auction_offers` (
  `owner_id`   INT UNSIGNED NOT NULL DEFAULT 0,
  `buyer_id`   INT UNSIGNED NOT NULL DEFAULT 0,
  `buyer_name` VARCHAR(25)  NOT NULL DEFAULT '',
  `yang`       BIGINT       NOT NULL DEFAULT 0,
  KEY `idx_owner` (`owner_id`)
) ENGINE=InnoDB;

-- ----------------------------------------------------------------------------
-- offlineshop_safebox_items
-- Effective column list (dynamic builders):
--   OfflineshopCache.cpp:1417-1469 (INSERT), 1526-1553 (SELECT)
--   item_id, owner_id, vnum, count, socket0..2, attr0..attr7/attrval0..attrval7,
--   locked_attr (ATTR_LOCK), expiration
--   2026_04_15_add_8th_bonus.sql:24-26 adds attr7/attrval7.
-- Note: safebox currency lives in the related (not requested) table
--   `player`.`offlineshop_safebox_valutes` (owner_id, yang, cheque) --
--   see OfflineshopCache.cpp:1475-1523.
-- ----------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `player`.`offlineshop_safebox_items` (
  `item_id`     INT UNSIGNED NOT NULL AUTO_INCREMENT,
  `owner_id`    INT UNSIGNED NOT NULL DEFAULT 0,
  `vnum`        INT UNSIGNED NOT NULL DEFAULT 0,
  `count`       INT UNSIGNED NOT NULL DEFAULT 0,
  `socket0`     INT          NOT NULL DEFAULT 0,
  `socket1`     INT          NOT NULL DEFAULT 0,
  `socket2`     INT          NOT NULL DEFAULT 0,
  `attr0`       INT          NOT NULL DEFAULT 0, `attrval0` INT NOT NULL DEFAULT 0,
  `attr1`       INT          NOT NULL DEFAULT 0, `attrval1` INT NOT NULL DEFAULT 0,
  `attr2`       INT          NOT NULL DEFAULT 0, `attrval2` INT NOT NULL DEFAULT 0,
  `attr3`       INT          NOT NULL DEFAULT 0, `attrval3` INT NOT NULL DEFAULT 0,
  `attr4`       INT          NOT NULL DEFAULT 0, `attrval4` INT NOT NULL DEFAULT 0,
  `attr5`       INT          NOT NULL DEFAULT 0, `attrval5` INT NOT NULL DEFAULT 0,
  `attr6`       INT          NOT NULL DEFAULT 0, `attrval6` INT NOT NULL DEFAULT 0,
  `attr7`       INT          NOT NULL DEFAULT 0, `attrval7` INT NOT NULL DEFAULT 0,
  `locked_attr` INT          NOT NULL DEFAULT 0,  -- ATTR_LOCK
  `expiration`  TINYINT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (`item_id`),
  KEY `idx_owner` (`owner_id`)
) ENGINE=InnoDB;

-- ----------------------------------------------------------------------------
-- offlineshop_logs  (ENABLE_NEW_OFFLINESHOP_LOGS)
-- Evidence: Database/ClientManager.cpp:4406
--   INSERT INTO `player`.`offlineshop_logs` (`owner_id`,`item_id`,`what`,`when`) VALUES(?,?,?,NOW())
-- INCOMPLETE: no SELECT/PK evidence; `when` is a reserved word and is quoted in code.
-- ----------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `player`.`offlineshop_logs` (
  `owner_id` INT UNSIGNED NOT NULL DEFAULT 0,
  `item_id`  INT UNSIGNED NOT NULL DEFAULT 0,
  `what`     VARCHAR(1024) NOT NULL DEFAULT '',
  `when`     DATETIME     NOT NULL
) ENGINE=InnoDB;

-- ----------------------------------------------------------------------------
-- player.player_deleted  (deleted character backup; same structure as player)
-- Evidence: Database/ClientManagerPlayer.cpp:1479
--   "INSERT INTO player_deleted%s SELECT * FROM player%s WHERE id=%u"
-- ----------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `player`.`player_deleted` LIKE `player`.`player`;


-- ----------------------------------------------------------------------------
-- player.savepoint
-- Evidence:
--   READ  GameServer/command/cmd_general2.cpp:570  SELECT slot, name, map, x, y WHERE id
--   READ  GameServer/command/cmd_general2.cpp:665  SELECT g_x, g_y, map WHERE id AND slot
--   READ  GameServer/command/cmd_general2.cpp:736  SELECT * WHERE id AND slot
--   WRITE GameServer/command/cmd_general2.cpp:769  INSERT (id, slot, name, map, x, y, g_x, g_y)
--   DEL   GameServer/command/cmd_general2.cpp:624  DELETE WHERE id AND slot
-- ----------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `player`.`savepoint` (
  `id`    INT UNSIGNED NOT NULL,
  `slot`  INT          NOT NULL DEFAULT 0,
  `name`  VARCHAR(32)  NOT NULL DEFAULT '',  -- TODO(verify) length
  `map`   INT          NOT NULL DEFAULT 0,
  `x`     INT          NOT NULL DEFAULT 0,
  `y`     INT          NOT NULL DEFAULT 0,
  `g_x`   INT          NOT NULL DEFAULT 0,
  `g_y`   INT          NOT NULL DEFAULT 0,
  PRIMARY KEY (`id`, `slot`)  -- suggestion; id prefix serves the list query
) ENGINE=InnoDB;


-- ----------------------------------------------------------------------------
-- player.whisper_system_message
-- Evidence: GameServer/command/whisper_admin.cpp:184
--   INSERT (who, date, text, lang, color) VALUES(?, NOW(), ?, ?, ?)
--   via DBManager (default slot = player DB).
-- ----------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `player`.`whisper_system_message` (
  `who`   VARCHAR(24) NOT NULL DEFAULT '',  -- player name, CHARACTER_NAME_MAX_LEN=24
  `date`  DATETIME    NOT NULL,             -- NOW()
  `text`  TEXT        NULL,
  `lang`  VARCHAR(8)  NOT NULL DEFAULT '',  -- TODO(verify) length
  `color` INT         NOT NULL DEFAULT 0
) ENGINE=InnoDB;


-- ############################################################################
-- 4. COMMON DATABASE
-- ############################################################################

-- =====================================================================
--  db_common_proto.sql  -- DRAFT MySQL DDL reconstructed from C++ source
--  Repo: D:\AurigaGlobal\LiveWork\AurigaGlobal
--  Evidence: literal SQL strings in SRC/Server/** only. Column names are
--  never invented; unverifiable struct fields stay in the notes file.
--
--  DB routing evidence
--  -------------------
--  * CDBManager::DirectQuery/AsyncQuery default slot is SQL_PLAYER
--        (SRC/Server/Database/DBManager.h:41-43)
--  * Main.cpp:226-244 -> SQL_PLAYER = conf.json sql.player, SQL_COMMON = sql.common
--  * SQL_COMMON is used ONLY for: locale, gmlist, gmhost, priv_settings
--        (SRC/Server/Database/ClientManager.cpp:193,196,279,3658,4049,4100)
--  * GameServer AccountDB -> conf.json sql.common (common DB)
--        (SRC/Server/GameServer/core/config.cpp:288-292)
--  * therefore: exp_table is on common (config.cpp:704 AccountDB),
--    helperlist/gmlist are on common (messenger_manager.cpp:59,62)
--  * every proto table below (item_proto, mob_proto, ...) is queried with
--    the default SQL_PLAYER slot -> player DB.
--
--  Table-name postfix: GetTablePostfix() (Main.cpp:273) appends
--  conf.json "table_postfix" to most table names (shown below as NAME<%postfix%>).
--
--  Active build defines relevant to the column sets (verified in tree):
--    ENABLE_PROTO_FROM_DB    ClientManager.h:20
--    ENABLE_MULTI_NAMES      CommonDefines.h:93 / service.h:119
--    ENABLE_MULTI_LANGUAGE   CommonDefines.h:91
--    ENABLE_APPLYTYPE5_RAZOR93 service.h:209 (APPLY_MAX_NUM_=5)
--    ENABLE_NEW_STACK_LIMIT  service.h:88
--    __ATTR_TRANSFER_SYSTEM__ service.h:11
--    ENABLE_ATTR_COSTUMES    service.h:16
--    ENABLE_STOLE_COSTUME    service.h:61
--    ENABLE_PENDANT + ENABLE_NEW_BONUS_TALISMAN CommonDefines.h:66-67
--    ENABLE_BUY_WITH_ITEM    service.h
--    ENABLE_ITEM_EXTRA_PROTO / ENABLE_RARITY_SYSTEM / ENABLE_NEW_EXTRA_BONUS CommonDefines.h:141-143
--    ENABLE_ITEM_ATTR_COSTUME  *** NOT defined anywhere *** -> costume
--      columns of item_attr_rare / item_attr_rare_8 are NOT selected
--    ENABLE_AUTODETECT_VNUMRANGE is #defined at ClientManagerBoot.cpp:1482
--      -> item_proto.vnum_range is NOT read in this build (kept optional).
--
--  All statements use ENGINE=InnoDB as requested.
--  Types are inferred per task rules (see notes md for each uncertainty).
-- =====================================================================

-- #####################################################################
-- ##  PART 1 -- COMMON DB (SQL_COMMON / GameServer AccountDB)         ##
-- #####################################################################

-- ---------------------------------------------------------------------
-- locale
--   DB: common
--   Evidence: ClientManager.cpp:3657-3658 "SELECT mValue, mKey FROM locale"
--             + DirectQuery(..., SQL_COMMON) line 3658
--             GameServer config.cpp:305 "SELECT mKey, mValue FROM locale"
--             (AccountDB == common, config.cpp:288-292)
--             config.cpp:366 "SELECT mValue FROM locale WHERE mKey='SKILL_POWER_BY_LEVEL'"
--             config.cpp:406 "... where mKey='SKILL_POWER_BY_LEVEL_TYPE%d' ORDER BY CAST(mValue AS unsigned)"
--   C++ struct: common/tables.h:1412 tLocale { char szValue[32]; char szKey[32]; }
--   INCOMPLETE: mKey length is only known from tLocale (32); GameServer reads
--   long mValue rows, so mValue is widened to TEXT -- TODO(verify).
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `common`.`locale` (
  `mKey`   VARCHAR(32)  NOT NULL DEFAULT '',   -- TODO(verify) length; tLocale szKey[32]
  `mValue` TEXT         NOT NULL,              -- TODO(verify) tLocale szValue[32] but SKILL_POWER_BY_LEVEL rows are long
  PRIMARY KEY (`mKey`)
) ENGINE=InnoDB;

-- ---------------------------------------------------------------------
-- gmlist
--   DB: common
--   Evidence: ClientManager.cpp:4045-4049 (SQL_COMMON)
--             "SELECT mID,mAccount,mName,mContactIP,mServerIP,mAuthority FROM gmlist WHERE mServerIP='ALL' or mServerIP='%s'"
--             GameServer messenger_manager.cpp:59 "SELECT ?, mName FROM common.gmlist WHERE mName != ?"
--   C++ struct: common/tables.h:1400 TAdminInfo (m_ID int, m_szAccount[32],
--               m_szName[32], m_szContactIP[16], m_szServerIP[16], m_Authority int -> parsed from strings)
--   mAuthority is a text enum ("IMPLEMENTOR","GOD","HIGH_WIZARD","LOW_WIZARD","WIZARD").
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `common`.`gmlist` (
  `mID`        INT          NOT NULL,              -- TODO(verify) auto_increment/primary key not evidenced
  `mAccount`   VARCHAR(32)  NOT NULL DEFAULT '',
  `mName`      VARCHAR(32)  NOT NULL DEFAULT '',
  `mContactIP` VARCHAR(16)  NOT NULL DEFAULT '',   -- TODO(verify) length
  `mServerIP`  VARCHAR(16)  NOT NULL DEFAULT '',   -- TODO(verify) length
  `mAuthority` VARCHAR(16)  NOT NULL DEFAULT '',   -- TODO(verify) length; enum stored as text
  PRIMARY KEY (`mID`)
) ENGINE=InnoDB;

-- ---------------------------------------------------------------------
-- gmhost
--   DB: common
--   Evidence: ClientManager.cpp:4098-4100 (SQL_COMMON) "SELECT mIP FROM gmhost"
--   INCOMPLETE: only mIP is referenced by literal SQL; no struct typing available.
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `common`.`gmhost` (
  `mIP` VARCHAR(16) NOT NULL DEFAULT ''   -- TODO(verify) length (IPv4 strings in tAdminInfo are char[16])
) ENGINE=InnoDB;

-- ---------------------------------------------------------------------
-- helperlist
--   DB: common (fully qualified name in the query)
--   Evidence: messenger_manager.cpp:62 (ENABLE_MESSENGER_HELPER, service.h)
--             "SELECT ?, mName FROM common.helperlist WHERE mName != ?"
--   INCOMPLETE: only mName is referenced by literal SQL; the rest of the
--   table (presumably account/id columns) has no SQL evidence in this repo.
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `common`.`helperlist` (
  `mName` VARCHAR(32) NOT NULL DEFAULT ''   -- TODO(verify) length
) ENGINE=InnoDB;

-- ---------------------------------------------------------------------
-- exp_table
--   DB: common
--   Evidence: GameServer config.cpp:704 (ENABLE_EXPTABLE_FROMDB)
--             AccountDB::DirectQuery("SELECT level, exp FROM exp_table")
--             AccountDB connects to cfg.sql.common (config.cpp:288-292).
--   Loaded into exp_table[PLAYER_MAX_LEVEL_CONST+1] (uint32_t), so both uint32.
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `common`.`exp_table` (
  `level` INT UNSIGNED NOT NULL,
  `exp`   INT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (`level`)
) ENGINE=InnoDB;

-- ---------------------------------------------------------------------
-- priv_settings
--   DB: common
--   Evidence: ClientManager.cpp:193 "DELETE FROM priv_settings WHERE value <= 0 OR duration <= NOW();" (SQL_COMMON)
--             ClientManager.cpp:196 "SELECT priv_type, id, type, value, UNIX_TIMESTAMP(duration) FROM priv_settings" (SQL_COMMON)
--             ClientManager.cpp:279 "REPLACE INTO priv_settings SET priv_type=?, id=?, type=?, value=?, duration=DATE_ADD(NOW(), INTERVAL ? SECOND)" (SQL_COMMON)
--   Types from SqlParam::Text/UInt/UInt/Int and Add*Priv signatures (uint8 type, int value).
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `common`.`priv_settings` (
  `priv_type` VARCHAR(16)      NOT NULL DEFAULT '',  -- 'EMPIRE'|'GUILD'|'PLAYER' -- TODO(verify) length
  `id`        INT UNSIGNED     NOT NULL DEFAULT 0,
  `type`      TINYINT UNSIGNED NOT NULL DEFAULT 0,   -- TODO(verify) uint8 in CPrivManager
  `value`     INT              NOT NULL DEFAULT 0,
  `duration`  DATETIME         NULL,
  PRIMARY KEY (`priv_type`,`id`, `type`)
) ENGINE=InnoDB;

-- ----------------------------------------------------------------------------
-- common.spam_db  (spam filter word list)
-- Evidence: GameServer/network/db.cpp:581,588 (LoadSpamDB / reload event)
--   "SELECT word, score FROM spam_db WHERE type='SPAM'"  (AccountDB = common DB)
--   `type` is evidenced by the WHERE literal.
-- ----------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `common`.`spam_db` (
  `word`  VARCHAR(64) NOT NULL DEFAULT '',      -- TODO(verify) length
  `type`  VARCHAR(16) NOT NULL DEFAULT 'SPAM',  -- TODO(verify) type
  `score` INT         NOT NULL DEFAULT 0,
  KEY `type` (`type`)                            -- suggestion
) ENGINE=InnoDB;


-- ############################################################################
-- 5. ACCOUNT DATABASE
-- ############################################################################

-- =====================================================================
-- DRAFT MySQL DDL - reverse-engineered from SRC/ (READ-ONLY pass)
-- Source: D:\AurigaGlobal\LiveWork\AurigaGlobal
--
-- (A) ACCOUNT database  -> every statement is `account`.`...`
-- (B) LOG database      -> every statement is `log`.`...`
--
-- RULES APPLIED
--  * Column names come only from literal SQL found in the C++ sources,
--    or (where the INSERT has no column list) they are explicitly marked
--    `TODO(verify name)`: the code only supplies positional values.
--  * Types inferred from C++ types / printf specifiers / source strings.
--  * `-- INCOMPLETE:` marks tables where the code touches only a subset
--    of the real table.
--  * `-- TODO(verify)` marks uncertain types / names / keys.
--
-- DB ROUTING EVIDENCE
--  * Account DB: LoginServer opens its `account_sql` connection to it
--      (SRC/Server/LoginServer/login_config.h:14-15, login_db.cpp:11-29).
--    Database server: connectSql(SQL_ACCOUNT, DbConfig().sql.account)
--      (SRC/Server/Database/Main.cpp:234).
--  * Log DB: LogManager::Connect(cfg.sql.log...) - conf.json.sample:49-55
--      "database": "log"; call site SRC/Server/GameServer/core/config.cpp:348-358.
--
-- TABLE POSTFIX
--  * Many log.cpp tables are written as NAME + get_table_postfix()
--      (log.cpp:78,113,136,148,211,218,226,234,244,254,264,277,291,305,344,351,369).
--    get_table_postfix() returns `server.table_postfix` (config.cpp:807-810),
--    default "" in conf.json.sample:6. DDL below uses the base names.
-- =====================================================================

-- #####################################################################
-- (A) ACCOUNT DATABASE
-- #####################################################################

-- ---------------------------------------------------------------------
-- account.account
-- Evidence for every column:
--   LoginServer login SELECT: login_auth.cpp:104-117
--   coins: ClientManager.cpp:4276,4820,4843;
--          PlayerRuntimeSystem.cpp:1579,1600,2111,2127
--   hwid: login_hwid.cpp:49,60; hwidmanager.cpp:50; tables.h:368-369
--   language: login_auth.cpp:106 (language+0); input_main.cpp:1186
--   vote_time: questlua_pc.cpp:4190
--   id/login lookups: guild.cpp:1598; login_auth.cpp:117
-- Lengths: LOGIN_MAX_LEN=30, PASSWD_MAX_LEN=16, SOCIAL_ID_MAX_LEN=18,
--   ACCOUNT_STATUS_MAX_LEN=8, HWID_LENGTH=64 (common/length.h:21,22,55,24,148)
-- -- INCOMPLETE (only columns referenced by code). The real table has
--    more columns (upstream: e.g. empire/safer fields); they are not
--    referenced anywhere in this tree and are intentionally absent.
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `account`.`account` (
  `id`                      INT UNSIGNED     NOT NULL AUTO_INCREMENT, -- login_auth.cpp:163 (id), ClientManager.cpp:4820 WHERE id
  `login`                   VARCHAR(30)      NOT NULL,               -- login_auth.cpp:117 WHERE login=? (LOGIN_MAX_LEN=30)
  `password`                VARCHAR(16)      NOT NULL DEFAULT '',    -- login_auth.cpp:104 PASSWORD(?)/password; old MySQL PASSWORD() = 16 chars TODO(verify)
  `social_id`               VARCHAR(18)      NOT NULL DEFAULT '',    -- login_auth.cpp:104; char[18+1] tables.h:374 TODO(verify: upstream often BIGINT)
  `status`                  VARCHAR(8)       NOT NULL DEFAULT 'OK',  -- login_hwid.cpp:49 status='BAN2'; ACCOUNT_STATUS_MAX_LEN=8
  `hwid`                    VARCHAR(65)      NOT NULL DEFAULT '',    -- login_hwid.cpp:60 SET hwid=?; char[HWID_LENGTH+1]=65 tables.h:369
  `coins`                   BIGINT           NOT NULL DEFAULT 0,     -- ClientManager.cpp:4820 (%u), PlayerRuntimeSystem.cpp:1600 (%lld) TODO(verify: mixed 32/64-bit use)
  `language`                TINYINT UNSIGNED NOT NULL DEFAULT 0,     -- input_main.cpp:1186 SET language=%d; login_auth.cpp:106 language+0 TODO(verify: numeric width; also char[3] under ENABLE_MULTILANGUAGE_SYSTEM tables.h:376)
  `vote_time`               DATETIME         NULL DEFAULT NULL,      -- questlua_pc.cpp:4190 UNIX_TIMESTAMP(vote_time) (ENABLE_VOTE_FOR_BONUS currently disabled, service.h:146)
  `availDt`                 DATETIME         NULL DEFAULT NULL,      -- login_auth.cpp:108 availDt - NOW() > 0
  `silver_expire`           DATETIME         NULL DEFAULT NULL,      -- login_auth.cpp:109 UNIX_TIMESTAMP(silver_expire)
  `gold_expire`             DATETIME         NULL DEFAULT NULL,      -- login_auth.cpp:110
  `safebox_expire`          DATETIME         NULL DEFAULT NULL,      -- login_auth.cpp:111
  `autoloot_expire`         DATETIME         NULL DEFAULT NULL,      -- login_auth.cpp:112
  `fish_mind_expire`        DATETIME         NULL DEFAULT NULL,      -- login_auth.cpp:113
  `marriage_fast_expire`    DATETIME         NULL DEFAULT NULL,      -- login_auth.cpp:114
  `money_drop_rate_expire`  DATETIME         NULL DEFAULT NULL,      -- login_auth.cpp:115
  `create_time`             DATETIME         NULL DEFAULT NULL,      -- login_auth.cpp:116 UNIX_TIMESTAMP(create_time)
  PRIMARY KEY (`id`),                                                -- id used by every account lookup
  UNIQUE KEY `login` (`login`)                                       -- login_auth.cpp:117; UPDATE ... WHERE login (TODO(verify): uniqueness assumed)
) ENGINE=InnoDB;

-- ---------------------------------------------------------------------
-- account.account_mount_inventory  (mount inventory, keyed by account)
-- Evidence:
--   Save:   MountInventory.cpp:172-195  REPLACE INTO (id, account_id, slot,
--           vnum, count, socket0..2, attrtypeN/attrvalueN for N<ITEM_ATTRIBUTE_MAX_NUM)
--   Delete: MountInventory.cpp:204 DELETE WHERE account_id+slot; :210 WHERE id
--   Load:   MountSystem.cpp:390-393 SELECT id, slot, vnum, count, socket0..2,
--           attrtype0..5/attrvalue0..5 ORDER BY slot
--   Legacy on-disk shape: git history commit 72e9bc79 ("player.sql") -
--           exact column set/types/socket0..2 + attr pair 0..5, no keys.
--   8th attr: SRC/Server/Database/sql/2026_04_15_add_8th_bonus.sql:32-34
--           ALTER adds attrtype7/attrvalue7 (attr*6 assumed pre-existing).
--   ITEM_SOCKET_MAX_NUM=3, ITEM_ATTRIBUTE_MAX_NUM=8 (item_length.h:14,28)
-- -- TODO(verify DB ROUTING): the queries above are unqualified and are
--    executed through GameServer DBManager, which is connected to
--    cfg.sql.player (GameServer/core/config.cpp:337). The legacy dump
--    "player.sql" also contains this table. It is emitted under `account`
--    as requested, but may actually live in the player DB in production.
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `account`.`account_mount_inventory` (
  `id`          INT UNSIGNED      NOT NULL,              -- item id, MountInventory.cpp:182 GetItemID
  `account_id`  INT UNSIGNED      NOT NULL,              -- MountInventory.cpp:183
  `slot`        SMALLINT UNSIGNED NOT NULL,              -- MountInventory.cpp:184 pos (dump: smallint)
  `vnum`        INT UNSIGNED      NOT NULL,              -- MountInventory.cpp:185
  `count`       INT UNSIGNED      NOT NULL DEFAULT 1,    -- MountInventory.cpp:186 (dump DEFAULT 1)
  `socket0`     INT               NULL DEFAULT 0,        -- MountInventory.cpp:187
  `socket1`     INT               NULL DEFAULT 0,
  `socket2`     INT               NULL DEFAULT 0,
  `attrtype0`   TINYINT UNSIGNED  NULL DEFAULT 0,        -- attr bType uint8, tables.h:396-399
  `attrvalue0`  SMALLINT          NULL DEFAULT 0,        -- attr sValue short
  `attrtype1`   TINYINT UNSIGNED  NULL DEFAULT 0,
  `attrvalue1`  SMALLINT          NULL DEFAULT 0,
  `attrtype2`   TINYINT UNSIGNED  NULL DEFAULT 0,
  `attrvalue2`  SMALLINT          NULL DEFAULT 0,
  `attrtype3`   TINYINT UNSIGNED  NULL DEFAULT 0,
  `attrvalue3`  SMALLINT          NULL DEFAULT 0,
  `attrtype4`   TINYINT UNSIGNED  NULL DEFAULT 0,
  `attrvalue4`  SMALLINT          NULL DEFAULT 0,
  `attrtype5`   TINYINT UNSIGNED  NULL DEFAULT 0,
  `attrvalue5`  SMALLINT          NULL DEFAULT 0,
  `attrtype6`   TINYINT UNSIGNED  NULL DEFAULT 0,        -- TODO(verify): missing from legacy dump and from 2026_04_15 migration; required by ITEM_ATTRIBUTE_MAX_NUM=8 save loop
  `attrvalue6`  SMALLINT          NULL DEFAULT 0,        -- TODO(verify)
  `attrtype7`   TINYINT UNSIGNED  NOT NULL DEFAULT 0,    -- 2026_04_15_add_8th_bonus.sql:33
  `attrvalue7`  SMALLINT          NOT NULL DEFAULT 0,    -- 2026_04_15_add_8th_bonus.sql:34
  PRIMARY KEY (`id`),                                    -- TODO(verify): legacy dump (72e9bc79) has NO keys, but REPLACE INTO (MountInventory.cpp:173) requires a unique key
  KEY `account_id` (`account_id`)                        -- MountSystem.cpp:393 / MountInventory.cpp:204
) ENGINE=InnoDB;

-- ---------------------------------------------------------------------
-- account.antifarm  (multi-farm/HWID status per logged-in account)
-- Evidence: desc_manager.cpp:538,553,565 (SELECT/INSERT/DELETE),
--           PlayerRuntimeSystem.cpp:3763,3812,3828,3869 (SELECT/UPDATE)
-- -- INCOMPLETE (only columns referenced by code; row id/date if any unknown).
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `account`.`antifarm` (
  `hwid`   VARCHAR(65) NULL DEFAULT NULL, -- desc_manager.cpp:553 INSERT ... hwid=?; HWID_LENGTH=64
  `login`  VARCHAR(30) NULL DEFAULT NULL, -- desc_manager.cpp:553,565; LOGIN_MAX_LEN=30
  `status` INT         NOT NULL DEFAULT 0 -- desc_manager.cpp:553 SqlParam::Int; 0/1 semantics (PlayerRuntimeSystem.cpp:3769) TODO(verify: could be TINYINT)
) ENGINE=InnoDB;

-- KEY `login` (`login`)  -- TODO(verify): lookup key, INSERT/DELETE by login
-- KEY `hwid`  (`hwid`)   -- TODO(verify): SELECT ... WHERE hwid (desc_manager.cpp:538)

-- ---------------------------------------------------------------------
-- account.blocked_hwids  (HWID ban list, loaded at startup)
-- Evidence: login_hwid.cpp:14  "SELECT hwid FROM account.blocked_hwids"
--           hwidmanager.cpp:30 "SELECT hwid FROM account.blocked_hwids"
-- No INSERT/DELETE seen in this tree (managed externally/web-admin).
-- -- INCOMPLETE (only the hwid column is referenced by code).
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `account`.`blocked_hwids` (
  `hwid` VARCHAR(65) NOT NULL -- HWID_LENGTH=64 + NUL (tables.h:369) TODO(verify: real key/layout)
) ENGINE=InnoDB;

-- PRIMARY KEY (`hwid`)  -- TODO(verify): set semantics; no key visible in code

-- ---------------------------------------------------------------------
-- account.sms_pool  (SMS queue)
-- Evidence (INSERT): ClientManager.cpp:1930-1938
--   "INSERT INTO sms_pool (server, sender, receiver, mobile, msg) VALUES(?,?,?,?,?)"
--   server = (m_iPlayerIDStart+2)/3 as SqlParam::Int.
-- Legacy on-disk shape: git history commit 72e9bc79 ("player.sql"):
--   adds `id` (AI PK) and `sent` enum('N','Y') with sent_idx; msg varchar(80).
-- -- TODO(verify DB ROUTING): the INSERT is issued with the default
--    CDBManager slot SQL_PLAYER (DBManager.h:41-50, ClientManager.cpp:1930)
--    and the same table appears in the legacy "player.sql" dump, so the
--    production DB may be player, not account. Emitted under `account`
--    as requested.
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `account`.`sms_pool` (
  `id`       INT          NOT NULL AUTO_INCREMENT,          -- legacy dump 72e9bc79
  `server`   INT          NOT NULL DEFAULT 0,               -- ClientManager.cpp:1933 SqlParam::Int
  `sender`   VARCHAR(32)  NULL DEFAULT NULL,                -- ClientManager.cpp:1934; legacy dump 72e9bc79 TODO(verify length)
  `receiver` VARCHAR(100) NOT NULL DEFAULT '',              -- ClientManager.cpp:1935; legacy dump 72e9bc79
  `mobile`   VARCHAR(32)  NULL DEFAULT NULL,                -- ClientManager.cpp:1936; MOBILE_MAX_LEN=32 (length.h:54)
  `sent`     ENUM('N','Y') NOT NULL DEFAULT 'N',            -- legacy dump 72e9bc79 (not touched by code)
  `msg`      VARCHAR(80)  NULL DEFAULT NULL,                -- ClientManager.cpp:1937 TODO(verify length)
  PRIMARY KEY (`id`),
  KEY `sent_idx` (`sent`)                                   -- legacy dump 72e9bc79
) ENGINE=InnoDB;

-- ############################################################################
-- 6. LOG DATABASE
-- ############################################################################

-- NOTE: loginlog / loginlog2 are written by LogManager (log.cpp:136,322,333,337)
-- through the LogSQL connection -> they are in the LOG database, see below.

-- #####################################################################
-- (B) LOG DATABASE  (`log`, conf.json.sample:49-55)
-- #####################################################################

-- ---------------------------------------------------------------------
-- log.log  (ItemLog + CharLog; base name + postfix, log.cpp:78,113)
-- ItemLog columns : type, time, who, x, y, what, how, hint, ip, vnum
-- CharLog columns : same INSERT minus vnum (log.cpp:113) -> vnum must be
--                   nullable/have a default.
-- hint is escaped into __escape_hint[1024] (log.cpp:15,76,111).
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `log`.`log` (
  `type` VARCHAR(16)      NOT NULL,               -- 'ITEM' / 'CHARACTER'
  `time` DATETIME         NOT NULL,               -- NOW()
  `who`  INT UNSIGNED     NOT NULL,               -- dwPID
  `x`    INT UNSIGNED     NOT NULL DEFAULT 0,     -- log.cpp:78 dwX
  `y`    INT UNSIGNED     NOT NULL DEFAULT 0,     -- log.cpp:78 dwY
  `what` INT UNSIGNED     NOT NULL,               -- dwItemID / dwValue
  `how`  VARCHAR(255)     NULL DEFAULT NULL,      -- c_pszText, not escaped TODO(verify length)
  `hint` VARCHAR(1024)    NULL DEFAULT NULL,      -- __escape_hint[1024] log.cpp:15
  `ip`   VARCHAR(45)      NULL DEFAULT NULL,      -- c_pszIP TODO(verify length/precision)
  `vnum` INT UNSIGNED     NULL DEFAULT 0          -- only ItemLog sends it (log.cpp:79)
) ENGINE=InnoDB;

-- ---------------------------------------------------------------------
-- log.loginlog  (base name + postfix, log.cpp:133-138)
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `log`.`loginlog` (
  `type`       VARCHAR(10)      NOT NULL,           -- 'LOGIN' / 'LOGOUT'
  `time`       DATETIME         NOT NULL,           -- NOW()
  `channel`    TINYINT UNSIGNED NOT NULL DEFAULT 0, -- g_bChannel uint8 (GameServer/core/config.h:61)
  `account_id` INT UNSIGNED     NOT NULL,           -- dwAccountID
  `pid`        INT UNSIGNED     NOT NULL,           -- dwPID
  `level`      TINYINT UNSIGNED NOT NULL DEFAULT 0, -- bLevel uint8
  `job`        TINYINT UNSIGNED NOT NULL DEFAULT 0, -- bJob uint8
  `playtime`   INT UNSIGNED     NOT NULL DEFAULT 0  -- dwPlayTime uint32
) ENGINE=InnoDB;

-- ---------------------------------------------------------------------
-- log.loginlog2  (detailed login/logout; INSERT log.cpp:322-329,
--   UPDATE log.cpp:333-337; id read back via MAX(id))
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `log`.`loginlog2` (
  `id`             INT UNSIGNED     NOT NULL AUTO_INCREMENT, -- log.cpp:333 SELECT MAX(id)
  `type`           VARCHAR(10)      NOT NULL,                -- 'INVALID' / 'VALID'
  `is_gm`          CHAR(1)          NOT NULL DEFAULT 'N',    -- 'Y'/'N' log.cpp:324
  `login_time`     DATETIME         NOT NULL,                -- NOW()
  `logout_time`    DATETIME         NULL DEFAULT NULL,       -- UPDATE log.cpp:337
  `playtime`       TIME             NULL DEFAULT NULL,       -- UPDATE playtime=TIMEDIFF(...) log.cpp:337 TODO(verify: TIME vs INT seconds)
  `channel`        TINYINT UNSIGNED NOT NULL DEFAULT 0,      -- g_bChannel
  `account_id`     INT UNSIGNED     NOT NULL,
  `pid`            INT UNSIGNED     NOT NULL,
  `ip`             INT UNSIGNED     NOT NULL,                -- inet_aton(ip) log.cpp:323
  `client_version` VARCHAR(32)      NULL DEFAULT NULL,       -- CG_Packet.h:596 char timestamp[32+1]
  PRIMARY KEY (`id`),
  KEY `account_id_pid` (`account_id`, `pid`)                 -- log.cpp:333 WHERE account_id=%u AND pid=%u
) ENGINE=InnoDB;

-- ---------------------------------------------------------------------
-- log.money_log  (base name + postfix, log.cpp:140-149)
-- Positional INSERT "VALUES (NOW(), %d, %d, %lld)" - NO column list in
-- code. Names below are candidates marked TODO(verify name).
-- Legacy commented statement (Database/MoneyLog.cpp:41):
--   INSERT INTO money_log%s VALUES('%s', %d, %d, %d)  (first value = time string).
-- Written from GameServer: input_db.cpp:1338-1345 -> LogManager::MoneyLog.
-- type: uint8, gold: int64.
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `log`.`money_log` (
  `time` DATETIME         NOT NULL,           -- TODO(verify name): NOW() / legacy quoted date string
  `type` TINYINT UNSIGNED NOT NULL,           -- TODO(verify name): MONEY_LOG_* uint8 (log.h:59)
  `vnum` INT UNSIGNED     NOT NULL,           -- TODO(verify name)
  `gold` BIGINT           NOT NULL            -- TODO(verify name): int64_t
) ENGINE=InnoDB;

-- ---------------------------------------------------------------------
-- log.goldlog  (base name + postfix, log.cpp:171-213)
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `log`.`goldlog` (
  `date` DATE          NOT NULL,              -- CURDATE()
  `time` TIME          NOT NULL,              -- CURTIME()
  `pid`  INT UNSIGNED  NOT NULL,              -- dwPID
  `what` INT UNSIGNED  NOT NULL,              -- dwItemID
  `how`  VARCHAR(32)   NOT NULL DEFAULT '',   -- szHow[32+1] log.cpp:174; BUY/SELL/SHOP_BUY/SHOP_SELL/EXCHANGE_TAKE/EXCHANGE_GIVE/QUEST
  `hint` VARCHAR(1024) NULL DEFAULT NULL      -- c_pszHint, not escaped TODO(verify length)
) ENGINE=InnoDB;

-- ---------------------------------------------------------------------
-- log.hack_log  (no postfix, log.cpp:151-155)
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `log`.`hack_log` (
  `time`   DATETIME      NOT NULL,          -- NOW()
  `login`  VARCHAR(30)   NULL DEFAULT NULL, -- c_pszLogin TODO(verify length)
  `name`   VARCHAR(24)   NULL DEFAULT NULL, -- c_pszName, CHARACTER_NAME_MAX_LEN=24
  `ip`     VARCHAR(45)   NULL DEFAULT NULL, -- c_pszIP TODO(verify length)
  `server` VARCHAR(64)   NULL DEFAULT NULL, -- g_stHostname TODO(verify length)
  `why`    VARCHAR(1024) NULL DEFAULT NULL  -- escaped __escape_hint[1024]
) ENGINE=InnoDB;

-- ---------------------------------------------------------------------
-- log.hack_crc_log  (no postfix, log.cpp:165-169)
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `log`.`hack_crc_log` (
  `time`   DATETIME      NOT NULL,
  `login`  VARCHAR(30)   NULL DEFAULT NULL,
  `name`   VARCHAR(24)   NULL DEFAULT NULL,
  `ip`     VARCHAR(45)   NULL DEFAULT NULL,
  `server` VARCHAR(64)   NULL DEFAULT NULL,
  `why`    VARCHAR(1024) NULL DEFAULT NULL, -- hack name (not escaped here)
  `crc`    INT UNSIGNED  NOT NULL           -- dwCRC uint32
) ENGINE=InnoDB;

-- ---------------------------------------------------------------------
-- log.bootlog  (no postfix, log.cpp:281-286)
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `log`.`bootlog` (
  `time`     DATETIME         NOT NULL,           -- NOW()
  `hostname` VARCHAR(64)      NULL DEFAULT NULL,  -- g_stHostname TODO(verify length)
  `channel`  TINYINT UNSIGNED NOT NULL DEFAULT 0  -- g_bChannel uint8
) ENGINE=InnoDB;

-- ---------------------------------------------------------------------
-- log.cube  (base name + postfix, log.cpp:215-221)
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `log`.`cube` (
  `pid`        INT UNSIGNED     NOT NULL,
  `time`       DATETIME         NOT NULL,          -- NOW()
  `x`          INT UNSIGNED     NOT NULL,
  `y`          INT UNSIGNED     NOT NULL,
  `item_vnum`  INT UNSIGNED     NOT NULL,
  `item_uid`   INT UNSIGNED     NOT NULL,
  `item_count` INT              NOT NULL,          -- int
  `success`    TINYINT UNSIGNED NOT NULL DEFAULT 0 -- bool -> 0/1
) ENGINE=InnoDB;

-- ---------------------------------------------------------------------
-- log.speed_hack  (base name + postfix, log.cpp:223-229)
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `log`.`speed_hack` (
  `pid`        INT UNSIGNED NOT NULL,
  `time`       DATETIME     NOT NULL, -- NOW()
  `x`          INT UNSIGNED NOT NULL,
  `y`          INT UNSIGNED NOT NULL,
  `hack_count` INT          NOT NULL  -- int
) ENGINE=InnoDB;

-- ---------------------------------------------------------------------
-- log.change_name  (base name + postfix, log.cpp:231-237)
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `log`.`change_name` (
  `pid`      INT UNSIGNED NOT NULL,
  `old_name` VARCHAR(24)  NOT NULL, -- CHARACTER_NAME_MAX_LEN=24
  `new_name` VARCHAR(24)  NOT NULL,
  `time`     DATETIME     NOT NULL, -- NOW()
  `ip`       VARCHAR(45)  NOT NULL  -- TODO(verify length)
) ENGINE=InnoDB;

-- ---------------------------------------------------------------------
-- log.command_log  (base name + postfix, log.cpp:239-247)
-- server column receives literal 999.
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `log`.`command_log` (
  `userid`   INT UNSIGNED     NOT NULL,           -- dwPID
  `server`   INT              NOT NULL DEFAULT 999,-- literal 999 log.cpp:245
  `ip`       VARCHAR(45)      NOT NULL,            -- szIP TODO(verify length)
  `port`     TINYINT UNSIGNED NOT NULL,            -- byChannel uint8 passed as %u TODO(verify: semantic is channel)
  `username` VARCHAR(30)      NOT NULL,            -- szName (account login, LOGIN_MAX_LEN=30)
  `command`  VARCHAR(1024)    NOT NULL,            -- escaped __escape_hint[1024]
  `date`     DATETIME         NOT NULL             -- NOW()
) ENGINE=InnoDB;

-- ---------------------------------------------------------------------
-- log.refinelog  (base name + postfix, log.cpp:249-256)
-- setType values seen: ROD, ROD_HACK (fishing.cpp:791,801,812)
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `log`.`refinelog` (
  `pid`        INT UNSIGNED NOT NULL,
  `item_name`  VARCHAR(255) NULL DEFAULT NULL, -- escaped into __escape_hint[1024] TODO(verify length)
  `item_id`    INT UNSIGNED NOT NULL,
  `step`       INT          NOT NULL,          -- item_refine_level int
  `time`       DATETIME     NOT NULL,          -- NOW()
  `is_success` INT          NOT NULL,          -- int 0/1
  `setType`    VARCHAR(32)  NULL DEFAULT NULL  -- how char* TODO(verify length)
) ENGINE=InnoDB;

-- ---------------------------------------------------------------------
-- log.shout_log  (base name + postfix, log.cpp:259-265)
-- Positional INSERT "VALUES(NOW(), %d, %d,'%s')" - NO column list in code.
-- Argument order: bChannel (uint8), bEmpire (uint8), escaped text.
-- Names below marked TODO(verify name).
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `log`.`shout_log` (
  `time`    DATETIME         NOT NULL,          -- TODO(verify name): NOW()
  `channel` TINYINT UNSIGNED NOT NULL,          -- TODO(verify name): bChannel
  `empire`  TINYINT UNSIGNED NOT NULL,          -- TODO(verify name): bEmpire
  `text`    VARCHAR(1024)    NULL DEFAULT NULL  -- TODO(verify name): __escape_hint[1024]
) ENGINE=InnoDB;

-- ---------------------------------------------------------------------
-- log.levellog  (base name + postfix, log.cpp:267-279, REPLACE INTO)
-- REPLACE INTO implies a unique key; `name` is the natural key.
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `log`.`levellog` (
  `name`       VARCHAR(24)  NOT NULL, -- ecs::PlayerRuntime::GetName
  `level`      INT UNSIGNED NOT NULL,
  `time`       DATETIME     NOT NULL, -- NOW()
  `account_id` INT UNSIGNED NOT NULL,
  `pid`        INT UNSIGNED NOT NULL,
  `playtime`   INT          NOT NULL, -- playhour unsigned int passed %d
  PRIMARY KEY (`name`)                -- TODO(verify): required by REPLACE INTO (log.cpp:277)
) ENGINE=InnoDB;

-- ---------------------------------------------------------------------
-- log.fish_log  (base name + postfix, log.cpp:288-300)
-- Positional INSERT "VALUES(NOW(), %u, %d, %u, %d, %u, %u, %u)" - NO
-- column list in code; names below marked TODO(verify name).
-- Argument order: dwPID(u32), prob_idx(int), fish_id(int), fish_level(int),
--                 dwMiliseconds(u32), dwVnum(u32), dwValue(u32).
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `log`.`fish_log` (
  `time`         DATETIME     NOT NULL,          -- TODO(verify name): NOW()
  `pid`          INT UNSIGNED NOT NULL,          -- TODO(verify name)
  `prob_idx`     INT          NOT NULL,          -- TODO(verify name)
  `fish_id`      INT          NOT NULL,          -- TODO(verify name)
  `fish_level`   INT          NOT NULL,          -- TODO(verify name)
  `milliseconds` INT UNSIGNED NOT NULL,          -- TODO(verify name): dwMiliseconds
  `vnum`         INT UNSIGNED NOT NULL,          -- TODO(verify name): dwVnum
  `value`        INT UNSIGNED NOT NULL           -- TODO(verify name): dwValue
) ENGINE=InnoDB;

-- ---------------------------------------------------------------------
-- log.quest_reward_log  (base name + postfix, log.cpp:302-312)
-- Positional INSERT "VALUES('%s',%u,%u,2,%u,%u,NOW())" - NO column list.
-- Argument order: quest name, dwPID, dwLevel, literal 2, iValue1, iValue2.
-- Names below marked TODO(verify name).
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `log`.`quest_reward_log` (
  `quest_name` VARCHAR(64)  NOT NULL, -- TODO(verify name+length): c_pszQuestName
  `pid`        INT UNSIGNED NOT NULL, -- TODO(verify name)
  `level`      INT UNSIGNED NOT NULL, -- TODO(verify name): dwLevel
  `type`       INT          NOT NULL, -- TODO(verify name): literal 2 (log.cpp:305)
  `value1`     INT          NOT NULL, -- TODO(verify name): iValue1
  `value2`     INT          NOT NULL, -- TODO(verify name): iValue2
  `time`       DATETIME     NOT NULL  -- TODO(verify name): NOW()
) ENGINE=InnoDB;

-- ---------------------------------------------------------------------
-- log.dragon_slay_log  (base name + postfix, log.cpp:341-347)
-- Positional INSERT "VALUES( %d, %d, FROM_UNIXTIME(%d), FROM_UNIXTIME(%d) )"
-- - NO column list. Names below marked TODO(verify name).
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `log`.`dragon_slay_log` (
  `guild_id`    INT UNSIGNED NOT NULL, -- TODO(verify name): dwGuildID
  `dragon_vnum` INT UNSIGNED NOT NULL, -- TODO(verify name): dwDragonVnum
  `start_time`  DATETIME     NOT NULL, -- TODO(verify name): FROM_UNIXTIME(dwStartTime)
  `end_time`    DATETIME     NOT NULL  -- TODO(verify name): FROM_UNIXTIME(dwEndTime)
) ENGINE=InnoDB;

-- ---------------------------------------------------------------------
-- log.chat_log  (base name + postfix, log.cpp:349-355 - all columns backticked)
-- type values: WHISPER/NORMAL/PARTY/GUILD (chat_input.cpp:573..1221)
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `log`.`chat_log` (
  `where`     INT UNSIGNED NOT NULL,          -- map index, ChatLog first arg
  `who_id`    INT UNSIGNED NOT NULL,          -- pid
  `who_name`  VARCHAR(24)  NOT NULL,          -- CHARACTER_NAME_MAX_LEN=24
  `whom_id`   INT UNSIGNED NOT NULL,          -- 0 / pid / guild id
  `whom_name` VARCHAR(24)  NULL DEFAULT NULL, -- party leader name / guild name
  `type`      VARCHAR(16)  NOT NULL,          -- WHISPER/NORMAL/PARTY/GUILD
  `msg`       TEXT         NULL DEFAULT NULL, -- pre-escaped by caller
  `when`      DATETIME     NOT NULL,          -- NOW()
  `ip`        VARCHAR(45)  NULL DEFAULT NULL  -- TODO(verify length)
) ENGINE=InnoDB;

-- ---------------------------------------------------------------------
-- log.invalid_server_log  (no postfix, log.cpp:357-364)
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `log`.`invalid_server_log` (
  `locale_type` INT          NOT NULL,          -- enum eLocalization
  `log_date`    DATETIME     NOT NULL,          -- NOW()
  `ip`          VARCHAR(45)  NULL DEFAULT NULL, -- TODO(verify length)
  `revision`    VARCHAR(1024) NULL DEFAULT NULL -- escaped __escape_hint[1024] TODO(verify: likely short revision hash)
) ENGINE=InnoDB;

-- ---------------------------------------------------------------------
-- log.acce  (base name + postfix, log.cpp:34-37)
-- Compiled when ENABLE_ACCE_SYSTEM is defined - it IS defined
-- (CommonDefines.h:61).
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `log`.`acce` (
  `pid`             INT UNSIGNED     NOT NULL,
  `time`            DATETIME         NOT NULL,          -- NOW()
  `x`               INT UNSIGNED     NOT NULL,
  `y`               INT UNSIGNED     NOT NULL,
  `item_vnum`       INT UNSIGNED     NOT NULL,
  `item_uid`        INT UNSIGNED     NOT NULL,
  `item_count`      INT              NOT NULL,
  `item_abs_chance` INT              NOT NULL,          -- abs_chance int
  `success`         TINYINT UNSIGNED NOT NULL DEFAULT 0 -- bool
) ENGINE=InnoDB;

-- ---------------------------------------------------------------------
-- log.attr_transfer  (base name + postfix, log.cpp:366-371)
-- Compiled when __ATTR_TRANSFER_SYSTEM__ is defined - it IS defined
-- (service.h:11).
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `log`.`attr_transfer` (
  `pid`       INT UNSIGNED NOT NULL,
  `time`      DATETIME     NOT NULL, -- NOW()
  `x`         INT UNSIGNED NOT NULL,
  `y`         INT UNSIGNED NOT NULL,
  `item_vnum` INT UNSIGNED NOT NULL
) ENGINE=InnoDB;

-- ---------------------------------------------------------------------
-- log.cheat_log  (no postfix, log.cpp:389-392)
-- Compiled when ENABLE_ANTICHEAT is defined - currently COMMENTED OUT
-- (CommonDefines.h:126, service.h:134). Kept for completeness.
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `log`.`cheat_log` (
  `player_id` INT          NOT NULL,          -- int32_t pid
  `ip`        VARCHAR(45)  NULL DEFAULT NULL, -- GetHostName string TODO(verify length)
  `reason`    TEXT         NULL DEFAULT NULL, -- std::string reason
  `time`      DATETIME     NOT NULL           -- NOW()
) ENGINE=InnoDB;
