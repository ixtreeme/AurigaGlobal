ALTER TABLE player.item_instance
 ADD COLUMN IF NOT EXISTS soul_blob VARBINARY(21) NULL,
 ADD COLUMN IF NOT EXISTS soul_channel TINYINT UNSIGNED NULL,
 ADD COLUMN IF NOT EXISTS soul_active TINYINT UNSIGNED NOT NULL DEFAULT 0,
 ADD COLUMN IF NOT EXISTS soul_active_channel TINYINT UNSIGNED
   GENERATED ALWAYS AS (IF(soul_active=1 AND destroyed=0,soul_channel,NULL)) PERSISTENT;
ALTER TABLE player.item_instance
 ADD CONSTRAINT IF NOT EXISTS ck_typed_soul CHECK
 ((soul_blob IS NULL AND soul_channel IS NULL AND soul_active=0)
 OR (category_id=14 AND OCTET_LENGTH(soul_blob)=21 AND soul_channel IN(1,2)
 AND soul_active IN(0,1) AND (soul_active=0 OR destroyed=0))),
 ADD UNIQUE INDEX IF NOT EXISTS uq_typed_soul_active(owner_kind,owner_id,soul_active_channel);

-- One accepted damage event can debit its owner only once, even with a new operation ID.
CREATE TABLE IF NOT EXISTS player.item_soul_strike_event (
 owner_kind TINYINT UNSIGNED NOT NULL, owner_id BIGINT UNSIGNED NOT NULL,
 event_id BIGINT UNSIGNED NOT NULL, operation_id BINARY(16) NOT NULL,
 target_id BIGINT UNSIGNED NOT NULL,target_revision BIGINT UNSIGNED NOT NULL,
 input_damage_raw BIGINT NOT NULL,output_damage_raw BIGINT NOT NULL,
 PRIMARY KEY(owner_kind,owner_id,event_id),UNIQUE(operation_id),
 CHECK(event_id>0 AND target_id>0 AND target_revision>0 AND input_damage_raw>0 AND output_damage_raw>=input_damage_raw),
 FOREIGN KEY(operation_id) REFERENCES player.item_operation_receipt(operation_id)
) ENGINE=InnoDB;
