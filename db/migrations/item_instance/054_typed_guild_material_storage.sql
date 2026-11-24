-- Run during maintenance with the guild writers stopped. Historical numbers are retained for audit.
ALTER TABLE player.guild_renewal_storage ADD COLUMN IF NOT EXISTS item_key BIGINT UNSIGNED NOT NULL DEFAULT 0;

UPDATE player.guild_renewal_storage SET item_key = CASE vnum
    WHEN 8881 THEN (6 << 32) + 611
    WHEN 27987 THEN (8 << 32) + 381
    WHEN 27992 THEN (6 << 32) + 117
    WHEN 27993 THEN (6 << 32) + 118
    WHEN 27994 THEN (6 << 32) + 119
    WHEN 30271 THEN (6 << 32) + 346
    WHEN 30284 THEN (6 << 32) + 355
    WHEN 30617 THEN (5 << 32) + 772
    WHEN 39066 THEN (10 << 32) + 963
    WHEN 71084 THEN (5 << 32) + 346
    WHEN 71123 THEN (6 << 32) + 564
    WHEN 71129 THEN (6 << 32) + 565
    WHEN 88980 THEN (10 << 32) + 1381
    WHEN 99998 THEN (10 << 32) + 1390
    WHEN 99999 THEN (10 << 32) + 1391
    ELSE 0 END
WHERE count > 0 AND item_key = 0;

-- Unknown occupied slots require a reviewed mapping before the new server can start.
ALTER TABLE player.guild_renewal_storage ADD CONSTRAINT IF NOT EXISTS chk_guild_material_key
    CHECK ((count = 0 AND item_key = 0) OR (count > 0 AND (item_key >> 32) BETWEEN 1 AND 65535 AND (item_key & 4294967295) > 0));
