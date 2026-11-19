START TRANSACTION;
UPDATE player.item_owner_fence f JOIN player.item_storage s USING(owner_kind,owner_id)
 SET f.state_revision=f.state_revision+1
 WHERE s.owner_kind=1 AND s.storage_kind=2 AND s.storage_id=1 AND s.grid_columns=27 AND s.grid_rows=1 AND s.grid_pages=1;
UPDATE player.item_storage SET grid_columns=28
 WHERE owner_kind=1 AND storage_kind=2 AND storage_id=1 AND grid_columns=27 AND grid_rows=1 AND grid_pages=1;
COMMIT;
