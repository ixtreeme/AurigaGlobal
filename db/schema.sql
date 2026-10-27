-- Auriga server fresh-install schema. Target: MariaDB.
-- Source profile: SRC/Server/common/{service.h,CommonDefines.h}; empty table postfix.
-- See db/README.md for installation, required content, validation and upgrade limits.
-- This file creates missing objects; it does not migrate existing table layouts.
-- Binary/positional column order and ENUM ordinals are part of the source contract.
SET NAMES utf8mb4;
CREATE DATABASE IF NOT EXISTS `player` DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
CREATE DATABASE IF NOT EXISTS `common` DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
CREATE DATABASE IF NOT EXISTS `account` DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
CREATE DATABASE IF NOT EXISTS `log` DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `player`.`player` (
  `id`                INT UNSIGNED       NOT NULL AUTO_INCREMENT,
  `account_id`        INT UNSIGNED       NOT NULL DEFAULT 0,
  `name`              VARCHAR(24)        NOT NULL DEFAULT '',
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
  `hp`                BIGINT UNSIGNED    NOT NULL DEFAULT 0,
  `mp`                BIGINT UNSIGNED    NOT NULL DEFAULT 0,
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
  `exp`               INT UNSIGNED       NOT NULL DEFAULT 0,
  `gold`              BIGINT             NOT NULL DEFAULT 0,
  `gaya`              INT                NOT NULL DEFAULT 0,
  `stat_point`        SMALLINT           NOT NULL DEFAULT 0,
  `skill_point`       SMALLINT           NOT NULL DEFAULT 0,
  `sub_skill_point`   SMALLINT           NOT NULL DEFAULT 0,
  `stat_reset_count`  SMALLINT           NOT NULL DEFAULT 0,
  `ip`                VARCHAR(16)        NOT NULL DEFAULT '',
  `part_base`         TINYINT UNSIGNED   NOT NULL DEFAULT 0,
  `part_main`         SMALLINT UNSIGNED  NOT NULL DEFAULT 0,
  `part_hair`         SMALLINT UNSIGNED  NOT NULL DEFAULT 0,
  `part_acce`         SMALLINT UNSIGNED  NOT NULL DEFAULT 0,
  `last_play`         DATETIME           NULL     DEFAULT NULL,
  `skill_group`       TINYINT UNSIGNED   NOT NULL DEFAULT 0,
  `alignment`         INT UNSIGNED       NOT NULL DEFAULT 0,
  `horse_level`       TINYINT UNSIGNED   NOT NULL DEFAULT 0,
  `horse_riding`      TINYINT UNSIGNED   NOT NULL DEFAULT 0,
  `horse_hp`          SMALLINT           NOT NULL DEFAULT 0,
  `horse_hp_droptime` BIGINT UNSIGNED    NOT NULL DEFAULT 0,
  `horse_stamina`     SMALLINT           NOT NULL DEFAULT 0,
  `horse_skill_point` SMALLINT           NOT NULL DEFAULT 0,
  `envanter`          INT                NOT NULL DEFAULT 0,
  `battle_pass_end`   DATETIME           NULL     DEFAULT NULL,
  `r0`                BIGINT             NOT NULL DEFAULT 0,
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
  `skill_level`       BLOB               NULL,
  `quickslot`         BLOB               NULL,
  `mobile`            VARCHAR(32)        NOT NULL DEFAULT '',
  `change_name`       TINYINT UNSIGNED   NOT NULL DEFAULT 0,
  `skill_victim`      VARCHAR(64)        NOT NULL DEFAULT '',
  `map1_skillmob`     INT                NOT NULL DEFAULT 0,
  PRIMARY KEY (`id`),
  KEY `account_id` (`account_id`),
  CHECK (`skill_level` IS NULL OR OCTET_LENGTH(`skill_level`) = 2550),
  CHECK (`quickslot` IS NULL OR OCTET_LENGTH(`quickslot`) = 108)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `player`.`item` (
  `id`          INT UNSIGNED     NOT NULL,
  `owner_id`    INT UNSIGNED     NOT NULL DEFAULT 0,
  `window`      ENUM('INVENTORY','EQUIPMENT','SAFEBOX','MALL',
                     'MOUNT_INVENTORY','RETIRED_WINDOW_6','BELT_INVENTORY',
                     'EXTRA_INVENTORY','SWITCHBOT','GROUND',
                     'NEW_OFFSHOP','SHOP_SAFEBOX') NOT NULL DEFAULT 'INVENTORY',
  `pos`         SMALLINT UNSIGNED NOT NULL DEFAULT 0,
  `count`       INT UNSIGNED     NOT NULL DEFAULT 0,
  `vnum`        INT UNSIGNED     NOT NULL DEFAULT 0,
  `lockedattr`  SMALLINT         NOT NULL DEFAULT -1,
  `socket0`     INT              NOT NULL DEFAULT 0,
  `socket1`     INT              NOT NULL DEFAULT 0,
  `socket2`     INT              NOT NULL DEFAULT 0,
  `attrtype0`   TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `attrvalue0`  SMALLINT         NOT NULL DEFAULT 0,
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
  `attrtype7`   TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `attrvalue7`  SMALLINT         NOT NULL DEFAULT 0,
  PRIMARY KEY (`id`),
  KEY `owner_id` (`owner_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `player`.`affect` (
  `dwPID`       INT UNSIGNED       NOT NULL DEFAULT 0,
  `bType`       INT UNSIGNED       NOT NULL DEFAULT 0,
  `bApplyOn`    TINYINT UNSIGNED   NOT NULL DEFAULT 0,
  `lApplyValue` INT                NOT NULL DEFAULT 0,
  `dwFlag`      INT UNSIGNED       NOT NULL DEFAULT 0,
  `lDuration`   INT                NOT NULL DEFAULT 0,
  `lSPCost`     INT                NOT NULL DEFAULT 0,
  PRIMARY KEY (`dwPID`, `bType`, `bApplyOn`, `lApplyValue`),
  KEY `dwPID` (`dwPID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `player`.`quest` (
  `dwPID`   INT UNSIGNED   NOT NULL DEFAULT 0,
  `szName`  VARCHAR(32)    NOT NULL DEFAULT '',
  `szState` VARCHAR(64)    NOT NULL DEFAULT '',
  `lValue`  BIGINT         NOT NULL DEFAULT 0,
  UNIQUE KEY `pid_name_state` (`dwPID`, `szName`, `szState`),
  KEY `dwPID` (`dwPID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `player`.`safebox` (
  `account_id` INT UNSIGNED     NOT NULL,
  `size`       TINYINT UNSIGNED NOT NULL DEFAULT 1,
  `password`   VARCHAR(7)       NOT NULL DEFAULT '',
  `gold`       INT UNSIGNED     NOT NULL DEFAULT 0,
  PRIMARY KEY (`account_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `player`.`player_index` (
  `id`      INT UNSIGNED     NOT NULL,
  `hwid`    VARCHAR(64)      NOT NULL DEFAULT '',
  `pid1`    INT UNSIGNED     NOT NULL DEFAULT 0,
  `pid2`    INT UNSIGNED     NOT NULL DEFAULT 0,
  `pid3`    INT UNSIGNED     NOT NULL DEFAULT 0,
  `pid4`    INT UNSIGNED     NOT NULL DEFAULT 0,
  `empire`  TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `ch`      TINYINT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `player`.`mob_proto` (
  `vnum`                INT UNSIGNED     NOT NULL,
  `name`                VARCHAR(49)      NOT NULL DEFAULT '',
  `locale_name` VARCHAR(49) NOT NULL DEFAULT '',
  `locale_name_en`      VARCHAR(49)      NOT NULL DEFAULT '',
  `locale_name_ro`      VARCHAR(49)      NOT NULL DEFAULT '',
  `locale_name_it`      VARCHAR(49)      NOT NULL DEFAULT '',
  `locale_name_tr`      VARCHAR(49)      NOT NULL DEFAULT '',
  `locale_name_de`      VARCHAR(49)      NOT NULL DEFAULT '',
  `locale_name_pl`      VARCHAR(49)      NOT NULL DEFAULT '',
  `locale_name_pt`      VARCHAR(49)      NOT NULL DEFAULT '',
  `locale_name_es`      VARCHAR(49)      NOT NULL DEFAULT '',
  `locale_name_cz`      VARCHAR(49)      NOT NULL DEFAULT '',
  `locale_name_hu`      VARCHAR(49)      NOT NULL DEFAULT '',
  `type`                TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `rank`                TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `battle_type`         TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `level`               TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `size`                SMALLINT UNSIGNED NOT NULL DEFAULT 0,
  `ai_flag`             INT UNSIGNED     NOT NULL DEFAULT 0,
  `setRaceFlag`         INT UNSIGNED     NOT NULL DEFAULT 0,
  `setImmuneFlag`       INT UNSIGNED     NOT NULL DEFAULT 0,
  `on_click`            TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `empire`              TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `drop_item`           INT UNSIGNED     NOT NULL DEFAULT 0,
  `resurrection_vnum`   INT UNSIGNED     NOT NULL DEFAULT 0,
  `folder`              VARCHAR(65)      NOT NULL DEFAULT '',
  `st`                  SMALLINT UNSIGNED NOT NULL DEFAULT 0,
  `dx`                  TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `ht`                  TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `iq`                  TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `damage_min`          INT UNSIGNED     NOT NULL DEFAULT 0,
  `damage_max`          INT UNSIGNED     NOT NULL DEFAULT 0,
  `max_hp`              BIGINT UNSIGNED  NOT NULL DEFAULT 0,
  `regen_cycle`         TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `regen_percent`       TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `exp`                 INT UNSIGNED     NOT NULL DEFAULT 0,
  `gold_min`            INT UNSIGNED     NOT NULL DEFAULT 0,
  `gold_max`            INT UNSIGNED     NOT NULL DEFAULT 0,
  `def`                 SMALLINT UNSIGNED NOT NULL DEFAULT 0,
  `attack_speed`        SMALLINT         NOT NULL DEFAULT 0,
  `move_speed`          SMALLINT         NOT NULL DEFAULT 0,
  `aggressive_hp_pct`   TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `aggressive_sight`    SMALLINT UNSIGNED NOT NULL DEFAULT 0,
  `attack_range`        SMALLINT UNSIGNED NOT NULL DEFAULT 0,
  `polymorph_item`      INT UNSIGNED     NOT NULL DEFAULT 0,
  `enchant_curse`       TINYINT          NOT NULL DEFAULT 0,
  `enchant_slow`        TINYINT          NOT NULL DEFAULT 0,
  `enchant_poison`      TINYINT          NOT NULL DEFAULT 0,
  `enchant_stun`        TINYINT          NOT NULL DEFAULT 0,
  `enchant_critical`    TINYINT          NOT NULL DEFAULT 0,
  `enchant_penetrate`   TINYINT          NOT NULL DEFAULT 0,
  `resist_sword`        TINYINT          NOT NULL DEFAULT 0,
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
  `dam_multiply`        FLOAT            NOT NULL DEFAULT 0,
  `summon`              INT UNSIGNED     NOT NULL DEFAULT 0,
  `drain_sp`            INT UNSIGNED     NOT NULL DEFAULT 0,
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
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `player`.`mob_motion` (
  `folder`           VARCHAR(65)  NOT NULL,
  `walk_speed`       INT          NOT NULL DEFAULT 0,
  `run_speed`        INT          NOT NULL DEFAULT 0,
  `attack_duration`  INT          NOT NULL DEFAULT 0,
  `skill1_duration`  INT          NOT NULL DEFAULT 0,
  `skill2_duration`  INT          NOT NULL DEFAULT 0,
  `skill3_duration`  INT          NOT NULL DEFAULT 0,
  `skill4_duration`  INT          NOT NULL DEFAULT 0,
  `skill5_duration`  INT          NOT NULL DEFAULT 0,
  PRIMARY KEY (`folder`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `player`.`mob_skill_splash` (
  `folder`       VARCHAR(65)      NOT NULL,
  `skill_index`  TINYINT UNSIGNED NOT NULL,
  `event_index`  TINYINT UNSIGNED NOT NULL,
  `timing_ms`    INT UNSIGNED     NOT NULL DEFAULT 0,
  `hit_distance` INT              NOT NULL DEFAULT 0,
  PRIMARY KEY (`folder`, `skill_index`, `event_index`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `player`.`item_proto` (
  `vnum`            INT UNSIGNED     NOT NULL,
  `type`            TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `subtype`         TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `name`            VARCHAR(49)      NOT NULL DEFAULT '',
  `locale_name` VARCHAR(49) NOT NULL DEFAULT '',
  `locale_name_en`  VARCHAR(49)      NOT NULL DEFAULT '',
  `locale_name_ro`  VARCHAR(49)      NOT NULL DEFAULT '',
  `locale_name_it`  VARCHAR(49)      NOT NULL DEFAULT '',
  `locale_name_tr`  VARCHAR(49)      NOT NULL DEFAULT '',
  `locale_name_de`  VARCHAR(49)      NOT NULL DEFAULT '',
  `locale_name_pl`  VARCHAR(49)      NOT NULL DEFAULT '',
  `locale_name_pt`  VARCHAR(49)      NOT NULL DEFAULT '',
  `locale_name_es`  VARCHAR(49)      NOT NULL DEFAULT '',
  `locale_name_cz`  VARCHAR(49)      NOT NULL DEFAULT '',
  `locale_name_hu`  VARCHAR(49)      NOT NULL DEFAULT '',
  `gold`            BIGINT           NOT NULL DEFAULT 0,
  `shop_buy_price`  BIGINT           NOT NULL DEFAULT 0,
  `weight`          TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `size`            TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `flag`            INT UNSIGNED     NOT NULL DEFAULT 0,
  `wearflag`        INT UNSIGNED     NOT NULL DEFAULT 0,
  `antiflag`        INT UNSIGNED     NOT NULL DEFAULT 0,
  `immuneflag`      INT UNSIGNED     NOT NULL DEFAULT 0,
  `refined_vnum`    INT UNSIGNED     NOT NULL DEFAULT 0,
  `refine_set`      SMALLINT UNSIGNED NOT NULL DEFAULT 0,
  `magic_pct`       TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `socket_pct`      TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `addon_type`      SMALLINT         NOT NULL DEFAULT 0,
  `limittype0`      TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `limitvalue0`     INT              NOT NULL DEFAULT 0,
  `limittype1`      TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `limitvalue1`     INT              NOT NULL DEFAULT 0,
  `applytype0`      TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `applyvalue0`     INT              NOT NULL DEFAULT 0,
  `applytype1`      TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `applyvalue1`     INT              NOT NULL DEFAULT 0,
  `applytype2`      TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `applyvalue2`     INT              NOT NULL DEFAULT 0,
  `applytype3`      TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `applyvalue3`     INT              NOT NULL DEFAULT 0,
  `applytype4`      TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `applyvalue4`     INT              NOT NULL DEFAULT 0,
  `value0`          INT              NOT NULL DEFAULT 0,
  `value1`          INT              NOT NULL DEFAULT 0,
  `value2`          INT              NOT NULL DEFAULT 0,
  `value3`          INT              NOT NULL DEFAULT 0,
  `value4`          INT              NOT NULL DEFAULT 0,
  `value5`          INT              NOT NULL DEFAULT 0,
  `vnum_range`      INT UNSIGNED     NOT NULL DEFAULT 0,
  PRIMARY KEY (`vnum`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `player`.`item_extra_proto` (
  `vnum` INT UNSIGNED NOT NULL,
  `rarity` INT NOT NULL DEFAULT 0,
  `bonus_type0` TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `bonus_value0` INT NOT NULL DEFAULT 0,
  `bonus_type1` TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `bonus_value1` INT NOT NULL DEFAULT 0,
  PRIMARY KEY (`vnum`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `player`.`refine_proto` (
  `id`     INT UNSIGNED NOT NULL,
  `cost`   BIGINT       NOT NULL DEFAULT 0,
  `prob`   INT          NOT NULL DEFAULT 0,
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
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `player`.`skill_proto` (
  `dwVnum`                      INT UNSIGNED     NOT NULL,
  `szName`                      VARCHAR(33)      NOT NULL DEFAULT '',
  `bType`                       TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `bMaxLevel`                   TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `dwSplashRange`               INT UNSIGNED     NOT NULL DEFAULT 0,
  `szPointOn`                   VARCHAR(64)      NOT NULL DEFAULT '',
  `szPointPoly`                 VARCHAR(101)     NOT NULL DEFAULT '',
  `szSPCostPoly`                VARCHAR(101)     NOT NULL DEFAULT '',
  `szDurationPoly`              VARCHAR(101)     NOT NULL DEFAULT '',
  `szDurationSPCostPoly`        VARCHAR(101)     NOT NULL DEFAULT '',
  `szCooldownPoly`              VARCHAR(101)     NOT NULL DEFAULT '',
  `szMasterBonusPoly`           VARCHAR(101)     NOT NULL DEFAULT '',
  `setFlag`                     INT UNSIGNED     NOT NULL DEFAULT 0,
  `setAffectFlag`               INT UNSIGNED     NOT NULL DEFAULT 0,
  `szPointOn2`                  VARCHAR(64)      NOT NULL DEFAULT '',
  `szPointPoly2`                VARCHAR(101)     NOT NULL DEFAULT '',
  `szDurationPoly2`             VARCHAR(101)     NOT NULL DEFAULT '',
  `setAffectFlag2`              INT UNSIGNED     NOT NULL DEFAULT 0,
  `szPointOn3`                  VARCHAR(64)      NOT NULL DEFAULT '',
  `szPointPoly3`                VARCHAR(101)     NOT NULL DEFAULT '',
  `szDurationPoly3`             VARCHAR(101)     NOT NULL DEFAULT '',
  `szGrandMasterAddSPCostPoly`  VARCHAR(101)     NOT NULL DEFAULT '',
  `bLevelStep`                  TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `bLevelLimit`                 TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `prerequisiteSkillVnum`       INT UNSIGNED     NOT NULL DEFAULT 0,
  `prerequisiteSkillLevel`      TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `iMaxHit`                     INT              NOT NULL DEFAULT 0,
  `szSplashAroundDamageAdjustPoly` VARCHAR(101)  NOT NULL DEFAULT '',
  `eSkillType`                  TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `dwTargetRange`               INT UNSIGNED     NOT NULL DEFAULT 0,
  PRIMARY KEY (`dwVnum`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `player`.`item_attr` (
  `apply` ENUM('MAX_HP','MAX_SP','CON','INT','STR','DEX','ATT_SPEED','MOV_SPEED','CAST_SPEED','HP_REGEN','SP_REGEN','POISON_PCT','STUN_PCT','SLOW_PCT','CRITICAL_PCT','PENETRATE_PCT','ATTBONUS_HUMAN','ATTBONUS_ANIMAL','ATTBONUS_ORC','ATTBONUS_MILGYO','ATTBONUS_UNDEAD','ATTBONUS_DEVIL','STEAL_HP','STEAL_SP','MANA_BURN_PCT','DAMAGE_SP_RECOVER','BLOCK','DODGE','RESIST_SWORD','RESIST_TWOHAND','RESIST_DAGGER','RESIST_BELL','RESIST_FAN','RESIST_BOW','RESIST_FIRE','RESIST_ELEC','RESIST_MAGIC','RESIST_WIND','REFLECT_MELEE','REFLECT_CURSE','POISON_REDUCE','KILL_SP_RECOVER','EXP_DOUBLE_BONUS','GOLD_DOUBLE_BONUS','ITEM_DROP_BONUS','POTION_BONUS','KILL_HP_RECOVER','IMMUNE_STUN','IMMUNE_SLOW','IMMUNE_FALL','SKILL','BOW_DISTANCE','ATT_GRADE_BONUS','DEF_GRADE_BONUS','MAGIC_ATT_GRADE_BONUS','MAGIC_DEF_GRADE_BONUS','CURSE_PCT','MAX_STAMINA','ATT_BONUS_TO_WARRIOR','ATT_BONUS_TO_ASSASSIN','ATT_BONUS_TO_SURA','ATT_BONUS_TO_SHAMAN','ATT_BONUS_TO_MONSTER','ATT_BONUS','MALL_DEFBONUS','MALL_EXPBONUS','MALL_ITEMBONUS','MALL_GOLDBONUS','MAX_HP_PCT','MAX_SP_PCT','SKILL_DAMAGE_BONUS','NORMAL_HIT_DAMAGE_BONUS','SKILL_DEFEND_BONUS','NORMAL_HIT_DEFEND_BONUS','PC_BANG_EXP_BONUS','PC_BANG_DROP_BONUS','EXTRACT_HP_PCT','RESIST_WARRIOR','RESIST_ASSASSIN','RESIST_SURA','RESIST_SHAMAN','ENERGY','DEF_GRADE','COSTUME_ATTR_BONUS','MAGIC_ATT_BONUS_PER','MELEE_MAGIC_ATT_BONUS_PER','RESIST_ICE','RESIST_EARTH','RESIST_DARK','RESIST_CRITICAL','RESIST_PENETRATE','RESERVED_92','RESERVED_93','RESERVED_94','RESERVED_95','RESERVED_96','ATTBONUS_IRR_SPADA','ATTBONUS_IRR_SPADONE','ATTBONUS_IRR_PUGNALE','ATTBONUS_IRR_FRECCIA','ATTBONUS_IRR_VENTAGLIO','ATTBONUS_IRR_CAMPANA','RESIST_MEZZIUOMINI','DEF_TALISMAN','ATTBONUS_INSECT','ATTBONUS_DESERT','ATTBONUS_FORT_ZODIAC','ATTBONUS_ELEC','ATTBONUS_FIRE','ATTBONUS_ICE','ATTBONUS_WIND','ATTBONUS_EARTH','ATTBONUS_DARK','ACCEDRAIN_RATE','RESIST_MAGIC_REDUCTION','ATTBONUS_METIN','ATTBONUS_BOSS','RESIST_MONSTER','Danni Medi[PVM]','PVM_CRITICAL_PCT','ATTBONUS_RUNE','DOUBLE_DROP_ITEM','IRR_WEAPON_DEFENSE','DESIGN_1','FISHING_RARE','PARTY_DROPEXP') NOT NULL DEFAULT 'MAX_HP',
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
  `talisman`       INT UNSIGNED NOT NULL DEFAULT 0,
  `costume_body`   TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `costume_hair`   TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `costume_weapon` TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `costume_stole`  TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `lv1_costume`    INT              NOT NULL DEFAULT 0,
  `lv2_costume`    INT              NOT NULL DEFAULT 0,
  `lv3_costume`    INT              NOT NULL DEFAULT 0,
  `lv4_costume`    INT              NOT NULL DEFAULT 0,
  `lv5_costume`    INT              NOT NULL DEFAULT 0,
  KEY `apply` (`apply`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `player`.`item_attr_rare` (
  `apply` ENUM('MAX_HP','MAX_SP','CON','INT','STR','DEX','ATT_SPEED','MOV_SPEED','CAST_SPEED','HP_REGEN','SP_REGEN','POISON_PCT','STUN_PCT','SLOW_PCT','CRITICAL_PCT','PENETRATE_PCT','ATTBONUS_HUMAN','ATTBONUS_ANIMAL','ATTBONUS_ORC','ATTBONUS_MILGYO','ATTBONUS_UNDEAD','ATTBONUS_DEVIL','STEAL_HP','STEAL_SP','MANA_BURN_PCT','DAMAGE_SP_RECOVER','BLOCK','DODGE','RESIST_SWORD','RESIST_TWOHAND','RESIST_DAGGER','RESIST_BELL','RESIST_FAN','RESIST_BOW','RESIST_FIRE','RESIST_ELEC','RESIST_MAGIC','RESIST_WIND','REFLECT_MELEE','REFLECT_CURSE','POISON_REDUCE','KILL_SP_RECOVER','EXP_DOUBLE_BONUS','GOLD_DOUBLE_BONUS','ITEM_DROP_BONUS','POTION_BONUS','KILL_HP_RECOVER','IMMUNE_STUN','IMMUNE_SLOW','IMMUNE_FALL','SKILL','BOW_DISTANCE','ATT_GRADE_BONUS','DEF_GRADE_BONUS','MAGIC_ATT_GRADE_BONUS','MAGIC_DEF_GRADE_BONUS','CURSE_PCT','MAX_STAMINA','ATT_BONUS_TO_WARRIOR','ATT_BONUS_TO_ASSASSIN','ATT_BONUS_TO_SURA','ATT_BONUS_TO_SHAMAN','ATT_BONUS_TO_MONSTER','ATT_BONUS','MALL_DEFBONUS','MALL_EXPBONUS','MALL_ITEMBONUS','MALL_GOLDBONUS','MAX_HP_PCT','MAX_SP_PCT','SKILL_DAMAGE_BONUS','NORMAL_HIT_DAMAGE_BONUS','SKILL_DEFEND_BONUS','NORMAL_HIT_DEFEND_BONUS','PC_BANG_EXP_BONUS','PC_BANG_DROP_BONUS','EXTRACT_HP_PCT','RESIST_WARRIOR','RESIST_ASSASSIN','RESIST_SURA','RESIST_SHAMAN','ENERGY','DEF_GRADE','COSTUME_ATTR_BONUS','MAGIC_ATT_BONUS_PER','MELEE_MAGIC_ATT_BONUS_PER','RESIST_ICE','RESIST_EARTH','RESIST_DARK','RESIST_CRITICAL','RESIST_PENETRATE','RESERVED_92','RESERVED_93','RESERVED_94','RESERVED_95','RESERVED_96','ATTBONUS_IRR_SPADA','ATTBONUS_IRR_SPADONE','ATTBONUS_IRR_PUGNALE','ATTBONUS_IRR_FRECCIA','ATTBONUS_IRR_VENTAGLIO','ATTBONUS_IRR_CAMPANA','RESIST_MEZZIUOMINI','DEF_TALISMAN','ATTBONUS_INSECT','ATTBONUS_DESERT','ATTBONUS_FORT_ZODIAC','ATTBONUS_ELEC','ATTBONUS_FIRE','ATTBONUS_ICE','ATTBONUS_WIND','ATTBONUS_EARTH','ATTBONUS_DARK','ACCEDRAIN_RATE','RESIST_MAGIC_REDUCTION','ATTBONUS_METIN','ATTBONUS_BOSS','RESIST_MONSTER','Danni Medi[PVM]','PVM_CRITICAL_PCT','ATTBONUS_RUNE','DOUBLE_DROP_ITEM','IRR_WEAPON_DEFENSE','DESIGN_1','FISHING_RARE','PARTY_DROPEXP') NOT NULL DEFAULT 'MAX_HP',
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
  `talisman` INT UNSIGNED NOT NULL DEFAULT 0,
  KEY `apply` (`apply`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `player`.`item_attr_rare_8` (
  `apply` ENUM('MAX_HP','MAX_SP','CON','INT','STR','DEX','ATT_SPEED','MOV_SPEED','CAST_SPEED','HP_REGEN','SP_REGEN','POISON_PCT','STUN_PCT','SLOW_PCT','CRITICAL_PCT','PENETRATE_PCT','ATTBONUS_HUMAN','ATTBONUS_ANIMAL','ATTBONUS_ORC','ATTBONUS_MILGYO','ATTBONUS_UNDEAD','ATTBONUS_DEVIL','STEAL_HP','STEAL_SP','MANA_BURN_PCT','DAMAGE_SP_RECOVER','BLOCK','DODGE','RESIST_SWORD','RESIST_TWOHAND','RESIST_DAGGER','RESIST_BELL','RESIST_FAN','RESIST_BOW','RESIST_FIRE','RESIST_ELEC','RESIST_MAGIC','RESIST_WIND','REFLECT_MELEE','REFLECT_CURSE','POISON_REDUCE','KILL_SP_RECOVER','EXP_DOUBLE_BONUS','GOLD_DOUBLE_BONUS','ITEM_DROP_BONUS','POTION_BONUS','KILL_HP_RECOVER','IMMUNE_STUN','IMMUNE_SLOW','IMMUNE_FALL','SKILL','BOW_DISTANCE','ATT_GRADE_BONUS','DEF_GRADE_BONUS','MAGIC_ATT_GRADE_BONUS','MAGIC_DEF_GRADE_BONUS','CURSE_PCT','MAX_STAMINA','ATT_BONUS_TO_WARRIOR','ATT_BONUS_TO_ASSASSIN','ATT_BONUS_TO_SURA','ATT_BONUS_TO_SHAMAN','ATT_BONUS_TO_MONSTER','ATT_BONUS','MALL_DEFBONUS','MALL_EXPBONUS','MALL_ITEMBONUS','MALL_GOLDBONUS','MAX_HP_PCT','MAX_SP_PCT','SKILL_DAMAGE_BONUS','NORMAL_HIT_DAMAGE_BONUS','SKILL_DEFEND_BONUS','NORMAL_HIT_DEFEND_BONUS','PC_BANG_EXP_BONUS','PC_BANG_DROP_BONUS','EXTRACT_HP_PCT','RESIST_WARRIOR','RESIST_ASSASSIN','RESIST_SURA','RESIST_SHAMAN','ENERGY','DEF_GRADE','COSTUME_ATTR_BONUS','MAGIC_ATT_BONUS_PER','MELEE_MAGIC_ATT_BONUS_PER','RESIST_ICE','RESIST_EARTH','RESIST_DARK','RESIST_CRITICAL','RESIST_PENETRATE','RESERVED_92','RESERVED_93','RESERVED_94','RESERVED_95','RESERVED_96','ATTBONUS_IRR_SPADA','ATTBONUS_IRR_SPADONE','ATTBONUS_IRR_PUGNALE','ATTBONUS_IRR_FRECCIA','ATTBONUS_IRR_VENTAGLIO','ATTBONUS_IRR_CAMPANA','RESIST_MEZZIUOMINI','DEF_TALISMAN','ATTBONUS_INSECT','ATTBONUS_DESERT','ATTBONUS_FORT_ZODIAC','ATTBONUS_ELEC','ATTBONUS_FIRE','ATTBONUS_ICE','ATTBONUS_WIND','ATTBONUS_EARTH','ATTBONUS_DARK','ACCEDRAIN_RATE','RESIST_MAGIC_REDUCTION','ATTBONUS_METIN','ATTBONUS_BOSS','RESIST_MONSTER','Danni Medi[PVM]','PVM_CRITICAL_PCT','ATTBONUS_RUNE','DOUBLE_DROP_ITEM','IRR_WEAPON_DEFENSE','DESIGN_1','FISHING_RARE','PARTY_DROPEXP') NOT NULL DEFAULT 'MAX_HP',
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
  `talisman` INT UNSIGNED NOT NULL DEFAULT 0,
  KEY `apply` (`apply`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `player`.`object_proto` (
  `vnum`                INT UNSIGNED NOT NULL,
  `price`               INT UNSIGNED NOT NULL DEFAULT 0,
  `materials`           TEXT         NULL,
  `upgrade_vnum`        INT UNSIGNED NOT NULL DEFAULT 0,
  `upgrade_limit_time`  INT UNSIGNED NOT NULL DEFAULT 0,
  `life`                INT          NOT NULL DEFAULT 0,
  `reg_1`               INT          NOT NULL DEFAULT 0,
  `reg_2`               INT          NOT NULL DEFAULT 0,
  `reg_3`               INT          NOT NULL DEFAULT 0,
  `reg_4`               INT          NOT NULL DEFAULT 0,
  `npc`                 INT UNSIGNED NOT NULL DEFAULT 0,
  `group_vnum`          INT UNSIGNED NOT NULL DEFAULT 0,
  `dependent_group`     INT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (`vnum`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
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
  `enable`            VARCHAR(3)       NOT NULL DEFAULT 'YES',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `player`.`object` (
  `id`        INT UNSIGNED NOT NULL AUTO_INCREMENT,
  `land_id`   INT UNSIGNED NOT NULL DEFAULT 0,
  `vnum`      INT UNSIGNED NOT NULL DEFAULT 0,
  `map_index` INT          NOT NULL DEFAULT 0,
  `x`         INT          NOT NULL DEFAULT 0,
  `y`         INT          NOT NULL DEFAULT 0,
  `x_rot`     FLOAT        NOT NULL DEFAULT 0,
  `y_rot`     FLOAT        NOT NULL DEFAULT 0,
  `z_rot`     FLOAT        NOT NULL DEFAULT 0,
  `life`      INT          NOT NULL DEFAULT 0,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `player`.`banword` (
  `word` VARCHAR(25) NOT NULL DEFAULT '',
  PRIMARY KEY (`word`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `player`.`quest_item_proto` (
  `vnum`        INT UNSIGNED NOT NULL,
  `name`        VARCHAR(49)  NOT NULL DEFAULT '',
  `locale_name` VARCHAR(49)  NOT NULL DEFAULT '',
  PRIMARY KEY (`vnum`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `player`.`shop` (
  `vnum`     INT UNSIGNED NOT NULL,
  `npc_vnum` INT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (`vnum`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `player`.`shop_item` (
  `shop_vnum`   INT UNSIGNED NOT NULL DEFAULT 0,
  `item_vnum`   INT UNSIGNED NOT NULL DEFAULT 0,
  `count`       INT          NOT NULL DEFAULT 0,
  `gold`        BIGINT       NOT NULL DEFAULT 0,
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
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
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
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `player`.`guild` (
  `id`           INT          NOT NULL AUTO_INCREMENT,
  `name`         VARCHAR(13)  NOT NULL DEFAULT '',
  `master`       INT UNSIGNED NOT NULL DEFAULT 0,
  `sp`           INT          NOT NULL DEFAULT 0,
  `level`        TINYINT UNSIGNED NOT NULL DEFAULT 1,
  `exp`          INT UNSIGNED NOT NULL DEFAULT 0,
  `skill_point`  INT          NOT NULL DEFAULT 0,
  `skill`        BINARY(12) NOT NULL DEFAULT 0x00,
  `ladder_point` INT          NOT NULL DEFAULT 0,
  `win`          INT UNSIGNED NOT NULL DEFAULT 0,
  `draw`         INT UNSIGNED NOT NULL DEFAULT 0,
  `loss`         INT UNSIGNED NOT NULL DEFAULT 0,
  `gold`         BIGINT       NOT NULL DEFAULT 0,
  `trophies`     INT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (`id`),
  KEY `idx_name` (`name`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `player`.`guild_member` (
  `pid`        INT UNSIGNED NOT NULL,
  `guild_id`   INT UNSIGNED NOT NULL DEFAULT 0,
  `grade`      TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `is_general` TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `offer`      INT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (`pid`),
  KEY `idx_guild_id` (`guild_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `player`.`guild_grade` (
  `guild_id` INT UNSIGNED NOT NULL,
  `grade`    TINYINT UNSIGNED NOT NULL,
  `name`     VARCHAR(13) NOT NULL DEFAULT '',
  `auth`     INT         NOT NULL DEFAULT 0,
  PRIMARY KEY (`guild_id`, `grade`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `player`.`guild_comment` (
  `id`       INT UNSIGNED NOT NULL AUTO_INCREMENT,
  `guild_id` INT UNSIGNED NOT NULL,
  `name`     VARCHAR(25)  NOT NULL DEFAULT '',
  `notice`   TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `content`  VARCHAR(51)  NOT NULL DEFAULT '',
  `time`     DATETIME     NOT NULL,
  PRIMARY KEY (`id`),
  KEY `idx_guild_id` (`guild_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `player`.`guild_invite_limit` (
  `id`    INT UNSIGNED NOT NULL,
  `value` INT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
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
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `player`.`guild_war_bet` (
  `war_id` INT UNSIGNED NOT NULL,
  `login`  VARCHAR(31)  NOT NULL DEFAULT '',
  `gold`   BIGINT       NOT NULL DEFAULT 0,
  `guild`  INT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (`war_id`, `login`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `player`.`guild_renewal_money` (
  `guild_id` INT UNSIGNED NOT NULL,
  `money`    BIGINT       NOT NULL DEFAULT 0,
  PRIMARY KEY (`guild_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `player`.`guild_renewal_tax` (
  `guild_id`         INT UNSIGNED NOT NULL,
  `active`           TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `deadline`         BIGINT       NOT NULL DEFAULT 0,
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
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `player`.`guild_renewal_storage` (
  `guild_id` INT UNSIGNED NOT NULL,
  `slot`     SMALLINT UNSIGNED NOT NULL,
  `vnum`     INT UNSIGNED NOT NULL DEFAULT 0,
  `count`    INT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (`guild_id`, `slot`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `player`.`guild_renewal_contrib` (
  `guild_id`        INT UNSIGNED NOT NULL,
  `pid`             INT UNSIGNED NOT NULL,
  `paid_money`      BIGINT       NOT NULL DEFAULT 0,
  `paid_item_total` BIGINT       NOT NULL DEFAULT 0,
  `paid_flag`       TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `last_pay_time`   BIGINT          NOT NULL DEFAULT 0,
  PRIMARY KEY (`guild_id`, `pid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `player`.`guild_renewal_kisado_item` (
  `guild_id` INT UNSIGNED NOT NULL,
  `pid`      INT UNSIGNED NOT NULL,
  `item0`    INT UNSIGNED NOT NULL DEFAULT 0,
  `item1`    INT UNSIGNED NOT NULL DEFAULT 0,
  `item2`    INT UNSIGNED NOT NULL DEFAULT 0,
  `item3`    INT UNSIGNED NOT NULL DEFAULT 0,
  `item4`    INT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (`guild_id`, `pid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `player`.`marriage` (
  `pid1`       INT UNSIGNED NOT NULL,
  `pid2`       INT UNSIGNED NOT NULL,
  `love_point` INT          NOT NULL DEFAULT 0,
  `time`       BIGINT UNSIGNED NOT NULL DEFAULT 0,
  `is_married` TINYINT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (`pid1`, `pid2`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `player`.`messenger_list` (
  `account`   VARCHAR(31) NOT NULL DEFAULT '',
  `companion` VARCHAR(31) NOT NULL DEFAULT '',
  PRIMARY KEY (`account`, `companion`),
  KEY `idx_companion` (`companion`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `player`.`horse_name` (
  `id`   INT UNSIGNED NOT NULL,
  `name` VARCHAR(25)  NOT NULL DEFAULT '',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `player`.`change_empire` (
  `account_id`   INT UNSIGNED NOT NULL,
  `change_count` INT UNSIGNED NOT NULL DEFAULT 0,
  `last_change`  DATETIME     NOT NULL,
  KEY `account_id` (`account_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `player`.`highscore` (
  `board` VARCHAR(20)  NOT NULL DEFAULT '',
  `pid`   INT UNSIGNED NOT NULL,
  `value` INT          NOT NULL DEFAULT 0,
  PRIMARY KEY (`board`, `pid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `player`.`event_table` (
  `id`          SMALLINT UNSIGNED NOT NULL AUTO_INCREMENT,
  `eventIndex`  TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `startTime`   DATETIME     NULL,
  `endTime`     DATETIME     NULL,
  `empireFlag`  TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `channelFlag` TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `value0`      INT UNSIGNED NOT NULL DEFAULT 0,
  `value1`      INT UNSIGNED NOT NULL DEFAULT 0,
  `value2`      INT UNSIGNED NOT NULL DEFAULT 0,
  `value3`      INT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `player`.`daily_reward_status` (
  `pid`           INT UNSIGNED NOT NULL,
  `time`          DATETIME     NOT NULL,
  `reward`        VARCHAR(16)  NOT NULL DEFAULT '0',
  `total_rewards` INT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (`pid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `player`.`daily_reward_claim_hwid` (
  `hwkey`      VARCHAR(128) NOT NULL DEFAULT '',
  `claim_day`  DATE         NOT NULL,
  `pid`        INT UNSIGNED NOT NULL DEFAULT 0,
  `account_id` INT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (`hwkey`, `claim_day`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `player`.`daily_reward_items` (
  `reward` VARCHAR(64)  NOT NULL DEFAULT '',
  `items`  VARCHAR(255) NOT NULL DEFAULT '',
  `count`  INT          NOT NULL DEFAULT 0
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `player`.`myshop_pricelist` (
  `owner_id`  INT UNSIGNED NOT NULL,
  `item_vnum` INT UNSIGNED NOT NULL,
  `price`     INT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (`owner_id`, `item_vnum`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `player`.`new_petsystem` (
  `id`         INT UNSIGNED NOT NULL AUTO_INCREMENT,
  `name`       VARCHAR(25)  NOT NULL DEFAULT '',
  `level`      TINYINT UNSIGNED NOT NULL DEFAULT 1,
  `exp`        INT UNSIGNED NOT NULL DEFAULT 0,
  `expi`       INT UNSIGNED NOT NULL DEFAULT 0,
  `evolution`  TINYINT UNSIGNED NOT NULL DEFAULT 0,
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
  `minAge`     INT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (`id`),
  KEY `idx_name` (`name`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `player`.`battle_pass` (
  `player_id`      INT UNSIGNED NOT NULL,
  `mission_id`     INT UNSIGNED NOT NULL,
  `battle_pass_id` INT UNSIGNED NOT NULL DEFAULT 0,
  `extra_info`     INT UNSIGNED NOT NULL DEFAULT 0,
  `completed`      TINYINT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (`player_id`, `mission_id`, `battle_pass_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `player`.`battle_pass_ranking` (
  `player_name`    VARCHAR(25) NOT NULL DEFAULT '',
  `battle_pass_id` TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `finish_time`    DATETIME    NOT NULL,
  PRIMARY KEY (`player_name`, `battle_pass_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `player`.`dungeon_ranking` (
  `acc_id`        INT UNSIGNED NOT NULL DEFAULT 0,
  `pid`           INT UNSIGNED NOT NULL,
  `dungeon_index` INT          NOT NULL,
  `completed`     INT UNSIGNED NOT NULL DEFAULT 0,
  `time`          INT UNSIGNED NOT NULL DEFAULT 0,
  `damage`        INT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (`pid`, `dungeon_index`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `player`.`ishop_data` (
  `id`              INT UNSIGNED NOT NULL AUTO_INCREMENT,
  `categoryType`    TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `categorySubType` TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `itemVnum`        INT UNSIGNED NOT NULL DEFAULT 0,
  `itemPrice`       INT UNSIGNED NOT NULL DEFAULT 0,
  `discount`        TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `offerTime`       DATETIME NOT NULL DEFAULT '1970-01-01 00:00:01',
  `addedTime`       DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `sellCount`       INT UNSIGNED NOT NULL DEFAULT 0,
  `week_limit`      INT          NOT NULL DEFAULT 0,
  `month_limit`     INT          NOT NULL DEFAULT 0,
  `socket0`         INT          NOT NULL DEFAULT 0,
  `socket1`         INT          NOT NULL DEFAULT 0,
  `socket2`         INT          NOT NULL DEFAULT 0,
  `attrtype0`       TINYINT UNSIGNED NOT NULL DEFAULT 0, `attrvalue0` SMALLINT NOT NULL DEFAULT 0,
  `attrtype1`       TINYINT UNSIGNED NOT NULL DEFAULT 0, `attrvalue1` SMALLINT NOT NULL DEFAULT 0,
  `attrtype2`       TINYINT UNSIGNED NOT NULL DEFAULT 0, `attrvalue2` SMALLINT NOT NULL DEFAULT 0,
  `attrtype3`       TINYINT UNSIGNED NOT NULL DEFAULT 0, `attrvalue3` SMALLINT NOT NULL DEFAULT 0,
  `attrtype4`       TINYINT UNSIGNED NOT NULL DEFAULT 0, `attrvalue4` SMALLINT NOT NULL DEFAULT 0,
  `attrtype5`       TINYINT UNSIGNED NOT NULL DEFAULT 0, `attrvalue5` SMALLINT NOT NULL DEFAULT 0,
  `attrtype6`       TINYINT UNSIGNED NOT NULL DEFAULT 0, `attrvalue6` SMALLINT NOT NULL DEFAULT 0,
  `attrtype7`       TINYINT UNSIGNED NOT NULL DEFAULT 0, `attrvalue7` SMALLINT NOT NULL DEFAULT 0,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
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
  KEY `idx_limit` (`buyDate`, `accountID`, `itemVnum`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
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
  KEY `idx_taken_time` (`taken_time`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `player`.`money_log` (
  `time` DATETIME     NOT NULL,
  `type` TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `vnum` INT UNSIGNED NOT NULL DEFAULT 0,
  `gold` BIGINT       NOT NULL DEFAULT 0
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `player`.`lotto_list` (
  `id`     INT UNSIGNED NOT NULL AUTO_INCREMENT,
  `server` VARCHAR(32)  NOT NULL DEFAULT '',
  `pid`    INT UNSIGNED NOT NULL DEFAULT 0,
  `time`   DATETIME     NOT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `player`.`offlineshop_shops` (
  `owner_id` INT UNSIGNED NOT NULL,
  `name`     VARCHAR(65)  NOT NULL DEFAULT '',
  `duration` INT UNSIGNED NOT NULL DEFAULT 0,
  `npc`      INT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (`owner_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
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
  `locked_attr` INT          NOT NULL DEFAULT 0,
  `expiration`  TINYINT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (`item_id`),
  KEY `idx_owner` (`owner_id`),
  KEY `idx_sold` (`is_sold`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
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
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
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
  `locked_attr` INT          NOT NULL DEFAULT 0,
  `expiration`  TINYINT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (`owner_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `player`.`offlineshop_auction_offers` (
  `owner_id`   INT UNSIGNED NOT NULL DEFAULT 0,
  `buyer_id`   INT UNSIGNED NOT NULL DEFAULT 0,
  `buyer_name` VARCHAR(25)  NOT NULL DEFAULT '',
  `yang`       BIGINT       NOT NULL DEFAULT 0,
  KEY `idx_owner` (`owner_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
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
  `locked_attr` INT          NOT NULL DEFAULT 0,
  `expiration`  TINYINT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (`item_id`),
  KEY `idx_owner` (`owner_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `player`.`offlineshop_logs` (
  `owner_id` INT UNSIGNED NOT NULL DEFAULT 0,
  `item_id`  INT UNSIGNED NOT NULL DEFAULT 0,
  `what`     VARCHAR(1024) NOT NULL DEFAULT '',
  `when`     DATETIME     NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `player`.`player_deleted` LIKE `player`.`player`;
CREATE TABLE IF NOT EXISTS `player`.`savepoint` (
  `id`    INT UNSIGNED NOT NULL,
  `slot`  INT          NOT NULL DEFAULT 0,
  `name`  VARCHAR(32)  NOT NULL DEFAULT '',
  `map`   INT          NOT NULL DEFAULT 0,
  `x`     INT          NOT NULL DEFAULT 0,
  `y`     INT          NOT NULL DEFAULT 0,
  `g_x`   INT          NOT NULL DEFAULT 0,
  `g_y`   INT          NOT NULL DEFAULT 0,
  PRIMARY KEY (`id`, `slot`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `player`.`whisper_system_message` (
  `who`   VARCHAR(24) NOT NULL DEFAULT '',
  `date`  DATETIME    NOT NULL,
  `text`  TEXT        NULL,
  `lang`  VARCHAR(8)  NOT NULL DEFAULT '',
  `color` INT         NOT NULL DEFAULT 0
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `common`.`locale` (
  `mKey`   VARCHAR(32)  NOT NULL DEFAULT '',
  `mValue` TEXT         NOT NULL,
  PRIMARY KEY (`mKey`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `common`.`gmlist` (
  `mID`        INT          NOT NULL,
  `mAccount`   VARCHAR(32)  NOT NULL DEFAULT '',
  `mName`      VARCHAR(32)  NOT NULL DEFAULT '',
  `mContactIP` VARCHAR(16)  NOT NULL DEFAULT '',
  `mServerIP`  VARCHAR(16)  NOT NULL DEFAULT '',
  `mAuthority` VARCHAR(16)  NOT NULL DEFAULT '',
  PRIMARY KEY (`mID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `common`.`gmhost` (
  `mIP` VARCHAR(16) NOT NULL DEFAULT ''
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `common`.`helperlist` (
  `mName` VARCHAR(32) NOT NULL DEFAULT ''
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `common`.`exp_table` (
  `level` INT UNSIGNED NOT NULL,
  `exp`   INT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (`level`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `common`.`priv_settings` (
  `priv_type` VARCHAR(16)      NOT NULL DEFAULT '',
  `id`        INT UNSIGNED     NOT NULL DEFAULT 0,
  `type`      TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `value`     INT              NOT NULL DEFAULT 0,
  `duration`  DATETIME         NULL,
  PRIMARY KEY (`priv_type`,`id`, `type`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `common`.`spam_db` (
  `word`  VARCHAR(64) NOT NULL DEFAULT '',
  `type`  VARCHAR(16) NOT NULL DEFAULT 'SPAM',
  `score` INT         NOT NULL DEFAULT 0,
  KEY `type` (`type`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `account`.`account` (
  `id`                      INT UNSIGNED     NOT NULL AUTO_INCREMENT,
  `login`                   VARCHAR(30)      NOT NULL,
  `password`                VARCHAR(128)      NOT NULL DEFAULT '',
  `social_id`               VARCHAR(18)      NOT NULL DEFAULT '',
  `status`                  VARCHAR(8)       NOT NULL DEFAULT 'OK',
  `hwid`                    VARCHAR(65)      NOT NULL DEFAULT '',
  `coins`                   BIGINT           NOT NULL DEFAULT 0,
  `language`                TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `vote_time`               DATETIME         NULL DEFAULT NULL,
  `availDt`                 DATETIME         NULL DEFAULT NULL,
  `silver_expire`           DATETIME         NULL DEFAULT NULL,
  `gold_expire`             DATETIME         NULL DEFAULT NULL,
  `safebox_expire`          DATETIME         NULL DEFAULT NULL,
  `autoloot_expire`         DATETIME         NULL DEFAULT NULL,
  `fish_mind_expire`        DATETIME         NULL DEFAULT NULL,
  `marriage_fast_expire`    DATETIME         NULL DEFAULT NULL,
  `money_drop_rate_expire`  DATETIME         NULL DEFAULT NULL,
  `create_time`             DATETIME         NULL DEFAULT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `login` (`login`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `player`.`account_mount_inventory` (
  `id`          INT UNSIGNED      NOT NULL,
  `account_id`  INT UNSIGNED      NOT NULL,
  `slot`        SMALLINT UNSIGNED NOT NULL,
  `vnum`        INT UNSIGNED      NOT NULL,
  `count`       INT UNSIGNED      NOT NULL DEFAULT 1,
  `socket0`     INT               NULL DEFAULT 0,
  `socket1`     INT               NULL DEFAULT 0,
  `socket2`     INT               NULL DEFAULT 0,
  `attrtype0`   TINYINT UNSIGNED  NULL DEFAULT 0,
  `attrvalue0`  SMALLINT          NULL DEFAULT 0,
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
  `attrtype6`   TINYINT UNSIGNED  NULL DEFAULT 0,
  `attrvalue6`  SMALLINT          NULL DEFAULT 0,
  `attrtype7`   TINYINT UNSIGNED  NOT NULL DEFAULT 0,
  `attrvalue7`  SMALLINT          NOT NULL DEFAULT 0,
  PRIMARY KEY (`id`),
  KEY `account_id` (`account_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `account`.`antifarm` (
  `hwid`   VARCHAR(65) NULL DEFAULT NULL,
  `login`  VARCHAR(30) NULL DEFAULT NULL,
  `status` INT         NOT NULL DEFAULT 0,
  KEY `idx_login` (`login`),
  KEY `idx_hwid` (`hwid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `account`.`blocked_hwids` (
  `hwid` VARCHAR(65) NOT NULL,
  PRIMARY KEY (`hwid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `player`.`sms_pool` (
  `id`       INT          NOT NULL AUTO_INCREMENT,
  `server`   INT          NOT NULL DEFAULT 0,
  `sender`   VARCHAR(32)  NULL DEFAULT NULL,
  `receiver` VARCHAR(100) NOT NULL DEFAULT '',
  `mobile`   VARCHAR(32)  NULL DEFAULT NULL,
  `sent`     ENUM('N','Y') NOT NULL DEFAULT 'N',
  `msg`      VARCHAR(80)  NULL DEFAULT NULL,
  PRIMARY KEY (`id`),
  KEY `sent_idx` (`sent`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `log`.`log` (
  `type` VARCHAR(16)      NOT NULL,
  `time` DATETIME         NOT NULL,
  `who`  INT UNSIGNED     NOT NULL,
  `x`    INT UNSIGNED     NOT NULL DEFAULT 0,
  `y`    INT UNSIGNED     NOT NULL DEFAULT 0,
  `what` INT UNSIGNED     NOT NULL,
  `how`  VARCHAR(255)     NULL DEFAULT NULL,
  `hint` VARCHAR(1024)    NULL DEFAULT NULL,
  `ip`   VARCHAR(45)      NULL DEFAULT NULL,
  `vnum` INT UNSIGNED     NULL DEFAULT 0
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `log`.`loginlog` (
  `type`       VARCHAR(10)      NOT NULL,
  `time`       DATETIME         NOT NULL,
  `channel`    TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `account_id` INT UNSIGNED     NOT NULL,
  `pid`        INT UNSIGNED     NOT NULL,
  `level`      TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `job`        TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `playtime`   INT UNSIGNED     NOT NULL DEFAULT 0
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `log`.`loginlog2` (
  `id`             INT UNSIGNED     NOT NULL AUTO_INCREMENT,
  `type`           VARCHAR(10)      NOT NULL,
  `is_gm`          CHAR(1)          NOT NULL DEFAULT 'N',
  `login_time`     DATETIME         NOT NULL,
  `logout_time`    DATETIME         NULL DEFAULT NULL,
  `playtime`       TIME             NULL DEFAULT NULL,
  `channel`        TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `account_id`     INT UNSIGNED     NOT NULL,
  `pid`            INT UNSIGNED     NOT NULL,
  `ip`             INT UNSIGNED     NOT NULL,
  `client_version` VARCHAR(32)      NULL DEFAULT NULL,
  PRIMARY KEY (`id`),
  KEY `account_id_pid` (`account_id`, `pid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `log`.`money_log` (
  `time` DATETIME         NOT NULL,
  `type` TINYINT UNSIGNED NOT NULL,
  `vnum` BIGINT     NOT NULL,
  `gold` BIGINT           NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `log`.`goldlog` (
  `date` DATE          NOT NULL,
  `time` TIME          NOT NULL,
  `pid`  INT UNSIGNED  NOT NULL,
  `what` INT UNSIGNED  NOT NULL,
  `how`  VARCHAR(32)   NOT NULL DEFAULT '',
  `hint` VARCHAR(1024) NULL DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `log`.`hack_log` (
  `time`   DATETIME      NOT NULL,
  `login`  VARCHAR(30)   NULL DEFAULT NULL,
  `name`   VARCHAR(24)   NULL DEFAULT NULL,
  `ip`     VARCHAR(45)   NULL DEFAULT NULL,
  `server` VARCHAR(64)   NULL DEFAULT NULL,
  `why`    VARCHAR(1024) NULL DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `log`.`hack_crc_log` (
  `time`   DATETIME      NOT NULL,
  `login`  VARCHAR(30)   NULL DEFAULT NULL,
  `name`   VARCHAR(24)   NULL DEFAULT NULL,
  `ip`     VARCHAR(45)   NULL DEFAULT NULL,
  `server` VARCHAR(64)   NULL DEFAULT NULL,
  `why`    VARCHAR(1024) NULL DEFAULT NULL,
  `crc`    INT UNSIGNED  NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `log`.`bootlog` (
  `time`     DATETIME         NOT NULL,
  `hostname` VARCHAR(64)      NULL DEFAULT NULL,
  `channel`  TINYINT UNSIGNED NOT NULL DEFAULT 0
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `log`.`cube` (
  `pid`        INT UNSIGNED     NOT NULL,
  `time`       DATETIME         NOT NULL,
  `x`          INT UNSIGNED     NOT NULL,
  `y`          INT UNSIGNED     NOT NULL,
  `item_vnum`  INT UNSIGNED     NOT NULL,
  `item_uid`   INT UNSIGNED     NOT NULL,
  `item_count` INT              NOT NULL,
  `success`    TINYINT UNSIGNED NOT NULL DEFAULT 0
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `log`.`speed_hack` (
  `pid`        INT UNSIGNED NOT NULL,
  `time`       DATETIME     NOT NULL,
  `x`          INT UNSIGNED NOT NULL,
  `y`          INT UNSIGNED NOT NULL,
  `hack_count` INT          NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `log`.`change_name` (
  `pid`      INT UNSIGNED NOT NULL,
  `old_name` VARCHAR(24)  NOT NULL,
  `new_name` VARCHAR(24)  NOT NULL,
  `time`     DATETIME     NOT NULL,
  `ip`       VARCHAR(45)  NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `log`.`command_log` (
  `userid`   INT UNSIGNED     NOT NULL,
  `server`   INT              NOT NULL DEFAULT 999,
  `ip`       VARCHAR(45)      NOT NULL,
  `port`     TINYINT UNSIGNED NOT NULL,
  `username` VARCHAR(30)      NOT NULL,
  `command`  VARCHAR(1024)    NOT NULL,
  `date`     DATETIME         NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `log`.`refinelog` (
  `pid`        INT UNSIGNED NOT NULL,
  `item_name`  VARCHAR(255) NULL DEFAULT NULL,
  `item_id`    INT UNSIGNED NOT NULL,
  `step`       INT          NOT NULL,
  `time`       DATETIME     NOT NULL,
  `is_success` INT          NOT NULL,
  `setType`    VARCHAR(32)  NULL DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `log`.`shout_log` (
  `time`    DATETIME         NOT NULL,
  `channel` TINYINT UNSIGNED NOT NULL,
  `empire`  TINYINT UNSIGNED NOT NULL,
  `text`    VARCHAR(1024)    NULL DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `log`.`levellog` (
  `name`       VARCHAR(24)  NOT NULL,
  `level`      INT UNSIGNED NOT NULL,
  `time`       DATETIME     NOT NULL,
  `account_id` INT UNSIGNED NOT NULL,
  `pid`        INT UNSIGNED NOT NULL,
  `playtime`   INT          NOT NULL,
  KEY `name` (`name`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `log`.`fish_log` (
  `time`         DATETIME     NOT NULL,
  `pid`          INT UNSIGNED NOT NULL,
  `prob_idx`     INT          NOT NULL,
  `fish_id`      INT          NOT NULL,
  `fish_level`   INT          NOT NULL,
  `milliseconds` INT UNSIGNED NOT NULL,
  `vnum`         INT UNSIGNED NOT NULL,
  `value`        INT UNSIGNED NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `log`.`quest_reward_log` (
  `quest_name` VARCHAR(64)  NOT NULL,
  `pid`        INT UNSIGNED NOT NULL,
  `level`      INT UNSIGNED NOT NULL,
  `type`       INT          NOT NULL,
  `value1`     INT          NOT NULL,
  `value2`     INT          NOT NULL,
  `time`       DATETIME     NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `log`.`dragon_slay_log` (
  `guild_id`    INT UNSIGNED NOT NULL,
  `dragon_vnum` INT UNSIGNED NOT NULL,
  `start_time`  DATETIME     NOT NULL,
  `end_time`    DATETIME     NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `log`.`chat_log` (
  `where`     INT UNSIGNED NOT NULL,
  `who_id`    INT UNSIGNED NOT NULL,
  `who_name`  VARCHAR(24)  NOT NULL,
  `whom_id`   INT UNSIGNED NOT NULL,
  `whom_name` VARCHAR(24)  NULL DEFAULT NULL,
  `type`      VARCHAR(16)  NOT NULL,
  `msg`       TEXT         NULL DEFAULT NULL,
  `when`      DATETIME     NOT NULL,
  `ip`        VARCHAR(45)  NULL DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `log`.`invalid_server_log` (
  `locale_type` INT          NOT NULL,
  `log_date`    DATETIME     NOT NULL,
  `ip`          VARCHAR(45)  NULL DEFAULT NULL,
  `revision`    VARCHAR(1024) NULL DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `log`.`acce` (
  `pid`             INT UNSIGNED     NOT NULL,
  `time`            DATETIME         NOT NULL,
  `x`               INT UNSIGNED     NOT NULL,
  `y`               INT UNSIGNED     NOT NULL,
  `item_vnum`       INT UNSIGNED     NOT NULL,
  `item_uid`        INT UNSIGNED     NOT NULL,
  `item_count`      INT              NOT NULL,
  `item_abs_chance` INT              NOT NULL,
  `success`         TINYINT UNSIGNED NOT NULL DEFAULT 0
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `log`.`attr_transfer` (
  `pid`       INT UNSIGNED NOT NULL,
  `time`      DATETIME     NOT NULL,
  `x`         INT UNSIGNED NOT NULL,
  `y`         INT UNSIGNED NOT NULL,
  `item_vnum` INT UNSIGNED NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `log`.`cheat_log` (
  `player_id` INT          NOT NULL,
  `ip`        VARCHAR(45)  NULL DEFAULT NULL,
  `reason`    TEXT         NULL DEFAULT NULL,
  `time`      DATETIME     NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `player`.`offlineshop_safebox_valutes` (
  `owner_id` INT UNSIGNED NOT NULL,
  `yang` BIGINT NOT NULL DEFAULT 0,
  `cheque` BIGINT NOT NULL DEFAULT 0,
  PRIMARY KEY (`owner_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
CREATE TABLE IF NOT EXISTS `player`.`string` (
  `name` VARCHAR(64) NOT NULL DEFAULT '',
  `text` TEXT NULL,
  PRIMARY KEY (`name`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
