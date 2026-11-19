ALTER TABLE protos.item_soul
 ADD COLUMN next_category_id SMALLINT UNSIGNED NULL,
 ADD COLUMN next_local_id INT UNSIGNED NULL,
 ADD CONSTRAINT ck_soul_next_core CHECK(
  (next_category_id IS NULL AND next_local_id IS NULL) OR
  (next_category_id IS NOT NULL AND next_local_id IS NOT NULL AND soul_kind=1 AND next_category_id=14 AND next_local_id>0 AND next_local_id<>local_id)),
 ADD CONSTRAINT fk_soul_next_core FOREIGN KEY(next_category_id,next_local_id)
  REFERENCES protos.item_base(category_id,local_id) ON DELETE RESTRICT ON UPDATE RESTRICT;
-- Migration 034 used anonymous checks; the fourth requires a fixed payload.
-- The complete native snapshot validator now enforces fixed XOR nonempty pool.
ALTER TABLE protos.item_bonus_tool DROP CONSTRAINT CONSTRAINT_4,
 ADD CONSTRAINT ck_bonus_tool_payload CHECK(
  (operation_kind=4 AND (bonus_id IS NULL OR value_scaled<>0)) OR
  (operation_kind<>4 AND bonus_id IS NULL));
CREATE TABLE protos.bonus_tool_choice (
 category_id SMALLINT UNSIGNED NOT NULL,
 local_id INT UNSIGNED NOT NULL,
 choice_no TINYINT UNSIGNED NOT NULL,
 bonus_id INT UNSIGNED NOT NULL,
 value_scaled BIGINT NOT NULL,
 PRIMARY KEY(category_id,local_id,choice_no),
 UNIQUE KEY uq_bonus_tool_choice(category_id,local_id,bonus_id),
 CONSTRAINT ck_bonus_tool_choice CHECK(category_id=20 AND choice_no BETWEEN 1 AND 16 AND value_scaled<>0),
 CONSTRAINT fk_bonus_tool_choice FOREIGN KEY(category_id,local_id)
  REFERENCES protos.item_bonus_tool(category_id,local_id) ON DELETE RESTRICT ON UPDATE RESTRICT,
 CONSTRAINT fk_bonus_tool_choice_bonus FOREIGN KEY(bonus_id)
  REFERENCES protos.bonus_definition(bonus_id) ON DELETE RESTRICT ON UPDATE RESTRICT
) ENGINE=InnoDB;
UPDATE protos.catalog_format SET schema_version=40,canonical_domain='auriga.item_catalog.v40' WHERE singleton=1;
