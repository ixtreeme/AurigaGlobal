-- Applied and verified on 2026-11-11 with the server stopped.
-- One-time migration: new pet bonuses change from legacy tenths to actual values.
-- Backup: /Auriga/Server/run/deployments/decimal-release-4e125d1c03c25581/before.sql
SET SESSION sql_mode='STRICT_ALL_TABLES,NO_ENGINE_SUBSTITUTION';
USE `player`;
ALTER TABLE `offlineshop_auctions` MODIFY COLUMN `attrval0` decimal(13,3) NOT NULL, MODIFY COLUMN `attrval1` decimal(13,3) NOT NULL, MODIFY COLUMN `attrval2` decimal(13,3) NOT NULL, MODIFY COLUMN `attrval3` decimal(13,3) NOT NULL, MODIFY COLUMN `attrval4` decimal(13,3) NOT NULL, MODIFY COLUMN `attrval5` decimal(13,3) NOT NULL, MODIFY COLUMN `attrval6` decimal(13,3) NOT NULL, MODIFY COLUMN `attrval7` decimal(13,3) NOT NULL DEFAULT 0;
ALTER TABLE `offlineshop_safebox_items` MODIFY COLUMN `attrval0` decimal(13,3) NOT NULL, MODIFY COLUMN `attrval1` decimal(13,3) NOT NULL, MODIFY COLUMN `attrval2` decimal(13,3) NOT NULL, MODIFY COLUMN `attrval3` decimal(13,3) NOT NULL, MODIFY COLUMN `attrval4` decimal(13,3) NOT NULL, MODIFY COLUMN `attrval5` decimal(13,3) NOT NULL, MODIFY COLUMN `attrval6` decimal(13,3) NOT NULL, MODIFY COLUMN `attrval7` decimal(13,3) NOT NULL DEFAULT 0;
ALTER TABLE `offlineshop_shop_items` MODIFY COLUMN `attrval0` decimal(13,3) NOT NULL, MODIFY COLUMN `attrval1` decimal(13,3) NOT NULL, MODIFY COLUMN `attrval2` decimal(13,3) NOT NULL, MODIFY COLUMN `attrval3` decimal(13,3) NOT NULL, MODIFY COLUMN `attrval4` decimal(13,3) NOT NULL, MODIFY COLUMN `attrval5` decimal(13,3) NOT NULL, MODIFY COLUMN `attrval6` decimal(13,3) NOT NULL, MODIFY COLUMN `attrval7` decimal(13,3) NOT NULL DEFAULT 0;
ALTER TABLE `new_petsystem` MODIFY COLUMN `bonus0` decimal(13,3) NOT NULL DEFAULT 0, MODIFY COLUMN `bonus1` decimal(13,3) NOT NULL DEFAULT 0, MODIFY COLUMN `bonus2` decimal(13,3) NOT NULL DEFAULT 0; UPDATE new_petsystem SET bonus0=bonus0/10,bonus1=bonus1/10,bonus2=bonus2/10;
ALTER TABLE `affect` MODIFY COLUMN `lApplyValue` decimal(19,9) NOT NULL DEFAULT 0.000;
