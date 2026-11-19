-- Read-only startup prerequisite checks after schema/content import.
-- A READY row proves presence, not the correctness of gameplay formulas/data.
-- GameServer consumes levels 0-40; legacy curves may retain additional levels.
SELECT 'common.locale / SKILL_POWER_BY_LEVEL' AS requirement,
       IF(COUNT(*) = 1 AND MAX(mValue REGEXP '^[[:space:]]*[+-]?[0-9]+([[:space:]]+[+-]?[0-9]+){40,}[[:space:]]*$'),
          'READY', 'MISSING_OR_INVALID') AS status
FROM common.locale WHERE mKey = 'SKILL_POWER_BY_LEVEL'
UNION ALL SELECT 'common.locale / DB_NAME_COLUMN', IF(COUNT(*) = 1, 'READY', 'MISSING')
FROM common.locale WHERE mKey = 'DB_NAME_COLUMN'
UNION ALL SELECT 'player.mob_proto', IF(COUNT(*) > 0, 'READY', 'IMPORT_REQUIRED') FROM player.mob_proto
UNION ALL SELECT 'player.skill_proto', IF(COUNT(*) > 0, 'READY', 'IMPORT_REQUIRED') FROM player.skill_proto
UNION ALL SELECT 'player.item_persistence_format / native runtime',
       IF(COUNT(*) = 1 AND MIN(native_runtime_version)=1, 'READY', 'MISSING_OR_INVALID') FROM player.item_persistence_format
UNION ALL SELECT 'player.item_native_award', IF(COUNT(*) >= 0, 'READY', 'MISSING') FROM player.item_native_award;
