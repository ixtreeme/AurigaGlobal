-- =====================================================================
-- Tipus-egyeztetes migracio (pénz/XP 64-bit, signedness)
-- Date: 2026-10-06
--
-- A kod elvarasaihoz igazitja a DB oszlopokat:
--   - guild bank / guild war bet / account coins / player gold: BIGINT
--   - player hp/mp: BIGINT UNSIGNED (az ECS Health/Mana uint64)
--   - guild win/draw/loss: INT UNSIGNED (nem lehet negativ)
--   - exp: INT UNSIGNED
-- Reszletek: docs/source_type_audit.md, docs/type_alignment_matrix.md
--
-- Futtatas elott: SHOW CREATE TABLE ellenorzes (db/db_schema_audit.md 8. pont).
-- A MODIFY akkor is lefut, ha az oszlop mar jo tipusu (idempotens).
-- =====================================================================

ALTER TABLE `account`.`account`
    MODIFY COLUMN `coins` BIGINT NOT NULL DEFAULT 0;

ALTER TABLE `player`.`player`
    MODIFY COLUMN `gold` BIGINT NOT NULL DEFAULT 0,
    MODIFY COLUMN `exp` INT UNSIGNED NOT NULL DEFAULT 0,
    MODIFY COLUMN `hp` BIGINT UNSIGNED NOT NULL DEFAULT 0,
    MODIFY COLUMN `mp` BIGINT UNSIGNED NOT NULL DEFAULT 0;

ALTER TABLE `player`.`guild`
    MODIFY COLUMN `gold` BIGINT NOT NULL DEFAULT 0,
    MODIFY COLUMN `win` INT UNSIGNED NOT NULL DEFAULT 0,
    MODIFY COLUMN `draw` INT UNSIGNED NOT NULL DEFAULT 0,
    MODIFY COLUMN `loss` INT UNSIGNED NOT NULL DEFAULT 0;

ALTER TABLE `player`.`guild_war_bet`
    MODIFY COLUMN `gold` BIGINT NOT NULL DEFAULT 0;

ALTER TABLE `log`.`money_log`
    MODIFY COLUMN `gold` BIGINT NOT NULL;
