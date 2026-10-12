-- =====================================================================
-- Mob/NPC motion bake tables (kliens .msa -> DB)
-- Date: 2026-10-12
--
-- A kliens .msa/motlist.txt adatait a SRC/Tools/motion_bake.py suti ki
-- ezekbe a tablakba; a GameServer a CMotionManager::Build()-ben tolti be
-- (nincs tobb .msa/motlist olvasas a szerveren). Kulcs a mob folder, mert
-- tobb vnum is hasznalhatja ugyanazt a mappat.
--
--   mob_motion        : walk/run px/s + normal attack / skill duration (ms)
--   mob_skill_splash  : skill "SPECIAL_ATTACKING" eventek (timing ms + tavolsag)
-- =====================================================================

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
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS `player`.`mob_skill_splash` (
  `folder`       VARCHAR(65)      NOT NULL,
  `skill_index`  TINYINT UNSIGNED NOT NULL,
  `event_index`  TINYINT UNSIGNED NOT NULL,
  `timing_ms`    INT UNSIGNED     NOT NULL DEFAULT 0,
  `hit_distance` INT              NOT NULL DEFAULT 0,   -- nyers signed .msa position.y
  PRIMARY KEY (`folder`, `skill_index`, `event_index`)
) ENGINE=InnoDB;
