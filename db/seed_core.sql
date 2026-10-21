-- Minimal configuration for a NEW installation; existing rows are preserved.
-- DB_NAME_COLUMN matches DatabaseConfig's default.
-- Skill power: local share/data/skill_power.txt default (ymir) row, level 0 = 0.
-- Source SHA256: 1f3c533f5101f318128a7a355eec17de24be8357067feb1ebc05fce00a44d16d
-- This is not mob/item/skill/shop content; import your real game reference data.
INSERT IGNORE INTO `common`.`locale` (`mKey`, `mValue`) VALUES
  ('DB_NAME_COLUMN', 'name'),
  ('SKILL_POWER_BY_LEVEL', '0 5 7 9 11 13 15 17 19 20 22 24 26 28 30 32 34 36 38 40 50 52 55 58 61 63 66 69 72 75 80 82 84 87 90 95 100 110 120 130 150');
