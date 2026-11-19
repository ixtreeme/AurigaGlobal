-- Required for atomic account bank moves and cross-channel consistency.
ALTER TABLE account_mount_inventory
 ADD PRIMARY KEY (id),
 ADD UNIQUE KEY account_slot (account_id, slot);
