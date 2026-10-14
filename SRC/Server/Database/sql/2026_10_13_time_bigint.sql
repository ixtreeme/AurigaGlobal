-- =====================================================================
-- Ido mezok BIGINT-re (int64) - chrono Phase 3D
-- Date: 2026-10-13
--
-- A C++ oldal int64-re valtott (quest.lValue: TQuestTable/PC::GetFlag/
-- SetFlag int64), illetve a unix-masodperc jellegu oszlopok
-- eloremenetileg BIGINT-re valtanak (a 32-bites INT 2038/2106 korlat
-- megszuntetese).
-- =====================================================================

-- Quest flag ertekek (idobelyeg + egyeb ertek) - a GameServer/Database
-- oldal mar int64 (TQuestTable::lValue, PC::GetFlag/SetFlag).
ALTER TABLE `player`.`quest`
  MODIFY COLUMN `lValue` BIGINT NOT NULL DEFAULT 0;

-- Unux-masodperc alapu idobelyegek (C++ oldal uint32, a DB oldal
-- eloremenetileg 64-biten tarol).
ALTER TABLE `player`.`marriage`
  MODIFY COLUMN `time` BIGINT UNSIGNED NOT NULL DEFAULT 0;

ALTER TABLE `player`.`player`
  MODIFY COLUMN `horse_hp_droptime` BIGINT UNSIGNED NOT NULL DEFAULT 0;
