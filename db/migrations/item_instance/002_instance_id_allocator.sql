-- F4.4 (IC-034): durable allocator for new ItemInstanceIds (split stacks,
-- later drops, shops, quests and rewards). Additive; apply after 001 on a
-- stopped target. Every new instance ID comes from a reserved range of this
-- counter, so two GameServers never hand out the same ID; an unused rest of
-- a range is simply skipped after a restart. The offline importer (F6) must
-- move next_id above every ID it writes.
CREATE TABLE player.item_instance_id_allocator (
    singleton TINYINT UNSIGNED NOT NULL PRIMARY KEY CHECK (singleton=1),
    next_id BIGINT UNSIGNED NOT NULL CHECK (next_id>0)
) ENGINE=InnoDB;

INSERT INTO player.item_instance_id_allocator(singleton,next_id)
SELECT 1,COALESCE(MAX(instance_id),0)+1 FROM player.item_instance;
