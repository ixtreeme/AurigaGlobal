-- Runtime instance IDs (F4 C7a, IC-035). Every writer that creates items
-- (game cores, the test bench, later the importer) reserves a block here in one
-- transaction; MAX(instance_id)+1 is never used beside a running game server.
-- The floor 2^32 keeps runtime IDs clear of converted 32-bit legacy item IDs.
-- Additive: apply after 001 on a stopped target.
CREATE TABLE player.item_instance_id_allocator (
    singleton TINYINT UNSIGNED NOT NULL PRIMARY KEY CHECK (singleton=1),
    next_instance_id BIGINT UNSIGNED NOT NULL CHECK (next_instance_id>=4294967296)
) ENGINE=InnoDB;

INSERT INTO player.item_instance_id_allocator(singleton,next_instance_id)
SELECT 1,GREATEST(4294967296,COALESCE(MAX(instance_id),0)+1) FROM player.item_instance;
