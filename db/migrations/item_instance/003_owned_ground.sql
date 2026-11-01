-- Each stable core fences its previous process at startup. Ground world state
-- stays memory-lived; the durable links make crash cleanup precise.
CREATE TABLE IF NOT EXISTS player.item_ground_core (
    core_key VARBINARY(128) NOT NULL PRIMARY KEY,
    run_generation BIGINT UNSIGNED NOT NULL
) ENGINE=InnoDB;
CREATE TABLE IF NOT EXISTS player.item_ground_entry (
    instance_id BIGINT UNSIGNED NOT NULL PRIMARY KEY,
    core_key VARBINARY(128) NOT NULL,
    run_generation BIGINT UNSIGNED NOT NULL,
    KEY ground_core_run (core_key, run_generation)
) ENGINE=InnoDB;

-- Existing characters receive the dedicated escrow during the offline schema
-- upgrade. The runtime writer keeps no INSERT privilege on item_storage.
INSERT IGNORE INTO player.item_storage
    (owner_kind,owner_id,storage_kind,storage_id,grid_columns,grid_rows,grid_pages)
SELECT f.owner_kind,f.owner_id,7,18446744073709551614,256,256,1
FROM player.item_owner_fence f
LEFT JOIN player.item_storage s ON s.owner_kind=f.owner_kind AND s.owner_id=f.owner_id
WHERE f.owner_kind=1
GROUP BY f.owner_kind,f.owner_id
HAVING COUNT(s.storage_id)<16;
