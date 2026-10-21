-- Read-only startup prerequisite checks after schema/content import.
-- A READY row proves presence, not the correctness of gameplay formulas/data.
SELECT 'common.locale / SKILL_POWER_BY_LEVEL' AS requirement,
       IF(COUNT(*) = 1 AND MAX(mValue REGEXP '^[[:space:]]*[+-]?[0-9]+([[:space:]]+[+-]?[0-9]+){40}[[:space:]]*$'),
          'READY', 'MISSING_OR_INVALID') AS status
FROM common.locale WHERE mKey = 'SKILL_POWER_BY_LEVEL'
UNION ALL SELECT 'common.locale / DB_NAME_COLUMN', IF(COUNT(*) = 1, 'READY', 'MISSING')
FROM common.locale WHERE mKey = 'DB_NAME_COLUMN'
UNION ALL SELECT 'player.mob_proto', IF(COUNT(*) > 0, 'READY', 'IMPORT_REQUIRED') FROM player.mob_proto
UNION ALL SELECT 'player.item_proto', IF(COUNT(*) > 0, 'READY', 'IMPORT_REQUIRED') FROM player.item_proto
UNION ALL SELECT 'player.skill_proto', IF(COUNT(*) > 0, 'READY', 'IMPORT_REQUIRED') FROM player.skill_proto
UNION ALL SELECT 'player.shop', IF(COUNT(*) > 0, 'READY', 'IMPORT_REQUIRED') FROM player.shop
UNION ALL SELECT 'player.shop_item', IF(COUNT(*) > 0, 'READY', 'IMPORT_REQUIRED') FROM player.shop_item
UNION ALL SELECT 'player.item_attr', IF(COUNT(*) > 0, 'READY', 'IMPORT_REQUIRED') FROM player.item_attr
UNION ALL SELECT 'player.item_attr_rare', IF(COUNT(*) > 0, 'READY', 'IMPORT_REQUIRED') FROM player.item_attr_rare
UNION ALL SELECT 'player.item_attr_rare_8', IF(COUNT(*) > 0, 'READY', 'IMPORT_REQUIRED') FROM player.item_attr_rare_8;
