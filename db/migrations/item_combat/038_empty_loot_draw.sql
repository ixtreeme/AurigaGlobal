-- Additive semantic extension, no change to the catalog's column format (31).
-- A weighted failed draw still consumes the box, but grants no fake reward.
ALTER TABLE protos.loot_entry DROP CONSTRAINT IF EXISTS CONSTRAINT_2;
ALTER TABLE protos.loot_entry DROP CONSTRAINT IF EXISTS ck_loot_reward_kind;
ALTER TABLE protos.loot_entry ADD CONSTRAINT ck_loot_reward_kind CHECK(reward_kind BETWEEN 1 AND 9);
ALTER TABLE protos.loot_entry DROP CONSTRAINT IF EXISTS ck_loot_empty_amount;
ALTER TABLE protos.loot_entry ADD CONSTRAINT ck_loot_empty_amount CHECK(reward_kind<>9 OR amount=0);
