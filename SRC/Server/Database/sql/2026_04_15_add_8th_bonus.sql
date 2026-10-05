-- 8th bonus support (slot index 7)
-- Date: 2026-04-15

-- 1) Add storage columns for the new 8th attribute
ALTER TABLE player.item
    ADD COLUMN attrtype7 TINYINT UNSIGNED NOT NULL DEFAULT 0 AFTER attrvalue6,
    ADD COLUMN attrvalue7 SMALLINT NOT NULL DEFAULT 0 AFTER attrtype7;

-- Optional but recommended if you use extended item_award attributes:
ALTER TABLE player.item_award
    ADD COLUMN attrtype7 TINYINT UNSIGNED NOT NULL DEFAULT 0 AFTER attrvalue6,
    ADD COLUMN attrvalue7 SMALLINT NOT NULL DEFAULT 0 AFTER attrtype7;

-- 2) Create separate bonus table for the 8th bonus (same structure as 6-7 bonus table)
CREATE TABLE IF NOT EXISTS player.item_attr_rare_8 LIKE player.item_attr_rare;

-- 2b) The ECS/offlineshop persistence builds its column lists from
--     ITEM_ATTRIBUTE_MAX_NUM, so these tables need the 8th pair too. Skip the
--     statements whose columns already exist.
-- offlineshop_shop_items / offlineshop_safebox_items / offlineshop_auctions
ALTER TABLE player.offlineshop_shop_items
    ADD COLUMN attr7 INT NOT NULL DEFAULT 0,
    ADD COLUMN attrval7 INT NOT NULL DEFAULT 0;
ALTER TABLE player.offlineshop_safebox_items
    ADD COLUMN attr7 INT NOT NULL DEFAULT 0,
    ADD COLUMN attrval7 INT NOT NULL DEFAULT 0;
ALTER TABLE player.offlineshop_auctions
    ADD COLUMN attr7 INT NOT NULL DEFAULT 0,
    ADD COLUMN attrval7 INT NOT NULL DEFAULT 0;

-- Mount inventory (SaveItem builds attrtypeN/attrvalueN for all slots)
ALTER TABLE account_mount_inventory
    ADD COLUMN attrtype7 TINYINT UNSIGNED NOT NULL DEFAULT 0,
    ADD COLUMN attrvalue7 SMALLINT NOT NULL DEFAULT 0;

-- 3) Seed data (optional): copy existing rare bonus rules as starting point
-- TRUNCATE TABLE player.item_attr_rare_8;
-- INSERT INTO player.item_attr_rare_8 SELECT * FROM player.item_attr_rare;

-- 4) Item proto examples (adjust columns to your schema / naming)
-- 30619: add 8th bonus item
-- 30620: reroll 8th bonus item
-- Example intent:
-- type = ITEM_USE
-- These two are handled server-side by vnum checks in char_item.cpp.
