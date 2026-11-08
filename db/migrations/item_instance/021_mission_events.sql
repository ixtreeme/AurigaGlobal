-- Item effects and their Mission events share the original item transaction.
CREATE TABLE IF NOT EXISTS player.item_mission_watch (
 character_id INT UNSIGNED NOT NULL PRIMARY KEY CHECK(character_id>0)
) ENGINE=InnoDB;
CREATE TABLE IF NOT EXISTS player.item_mission_event (
 event_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
 operation_id BINARY(16) NOT NULL,
 ordinal SMALLINT UNSIGNED NOT NULL,
 character_id INT UNSIGNED NOT NULL CHECK(character_id>0),
 event_kind TINYINT UNSIGNED NOT NULL CHECK(event_kind IN(1,2)),
 category SMALLINT UNSIGNED NOT NULL CHECK(category>0),
 local_id INT UNSIGNED NOT NULL CHECK(local_id>0),
 refinement_level SMALLINT UNSIGNED NOT NULL CHECK(refinement_level<=255),
 units INT UNSIGNED NOT NULL CHECK(units>0),
 created_at TIMESTAMP(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
 delivered TINYINT UNSIGNED NOT NULL DEFAULT 0 CHECK(delivered IN(0,1)),
 UNIQUE KEY item_mission_operation(operation_id,ordinal),
 KEY item_mission_pending(character_id,delivered,event_id)
) ENGINE=InnoDB;
