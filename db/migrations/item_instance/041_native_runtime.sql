-- Native-only GameServer/Database/Unity cutover. The deployment tool verifies
-- a stopped, owned local target and empty legacy state before dropping tables.
-- Keep this format marker separate from the destructive deployment operation.
ALTER TABLE player.item_persistence_format
 ADD COLUMN native_runtime_version TINYINT UNSIGNED NOT NULL DEFAULT 1
 CHECK (native_runtime_version=1);
