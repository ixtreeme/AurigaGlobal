-- Existing placements retain their anchors; append the ammunition equipment slot.
UPDATE player.item_storage SET grid_columns=20 WHERE storage_kind=2 AND grid_columns<20;
CREATE TABLE IF NOT EXISTS player.item_ammo_shot_event (
 owner_kind TINYINT UNSIGNED NOT NULL,owner_id BIGINT UNSIGNED NOT NULL,
 event_id BIGINT UNSIGNED NOT NULL,operation_id BINARY(16) NOT NULL,
 bow_id BIGINT UNSIGNED NOT NULL,ammo_id BIGINT UNSIGNED NOT NULL,reserve_id BIGINT UNSIGNED NOT NULL,
 target_id BIGINT UNSIGNED NOT NULL,target_revision BIGINT UNSIGNED NOT NULL,units SMALLINT UNSIGNED NOT NULL,
 input_damage_raw BIGINT NOT NULL,output_damage_raw BIGINT NOT NULL,
 PRIMARY KEY(owner_kind,owner_id,event_id),UNIQUE(operation_id),
 CHECK(event_id>0 AND bow_id>0 AND ammo_id>0 AND target_id>0 AND target_revision>0 AND units>0 AND input_damage_raw>=0 AND output_damage_raw>=input_damage_raw),
 FOREIGN KEY(operation_id) REFERENCES player.item_operation_receipt(operation_id)
) ENGINE=InnoDB;
