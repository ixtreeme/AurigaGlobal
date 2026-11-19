-- Full prototype conversion: talismans, belts and paired unique equipment.
-- Semantic extension only; existing catalog rows retain their identities.
SET @slot_checks=(SELECT GROUP_CONCAT(CONCAT('DROP CONSTRAINT `',CONSTRAINT_NAME,'`') SEPARATOR ',')
 FROM information_schema.CHECK_CONSTRAINTS WHERE CONSTRAINT_SCHEMA='protos' AND TABLE_NAME='item_armor' AND LOWER(CHECK_CLAUSE) LIKE '%equip_slot%');
SET @slot_ddl=CONCAT('ALTER TABLE protos.item_armor ',IF(@slot_checks IS NULL,'',CONCAT(@slot_checks,',')),
 'ADD CONSTRAINT ck_native_armor_slot CHECK(equip_slot IN(2,3,4,5,21))');
PREPARE native_slot_upgrade FROM @slot_ddl; EXECUTE native_slot_upgrade; DEALLOCATE PREPARE native_slot_upgrade;
SET @material_checks=(SELECT GROUP_CONCAT(CONCAT('DROP CONSTRAINT `',CONSTRAINT_NAME,'`') SEPARATOR ',')
 FROM information_schema.CHECK_CONSTRAINTS WHERE CONSTRAINT_SCHEMA='protos' AND TABLE_NAME='item_material' AND LOWER(CHECK_CLAUSE) LIKE '%material_kind%');
SET @material_ddl=CONCAT('ALTER TABLE protos.item_material ',IF(@material_checks IS NULL,'',CONCAT(@material_checks,',')),
 'ADD CONSTRAINT ck_native_material_kind CHECK(material_kind BETWEEN 1 AND 6)');
PREPARE native_material_upgrade FROM @material_ddl; EXECUTE native_material_upgrade; DEALLOCATE PREPARE native_material_upgrade;
SET @slot_checks=(SELECT GROUP_CONCAT(CONCAT('DROP CONSTRAINT `',CONSTRAINT_NAME,'`') SEPARATOR ',')
 FROM information_schema.CHECK_CONSTRAINTS WHERE CONSTRAINT_SCHEMA='protos' AND TABLE_NAME='item_accessory' AND LOWER(CHECK_CLAUSE) LIKE '%equip_slot%');
SET @slot_ddl=CONCAT('ALTER TABLE protos.item_accessory ',IF(@slot_checks IS NULL,'',CONCAT(@slot_checks,',')),
 'ADD CONSTRAINT ck_native_accessory_slot CHECK(equip_slot IN(6,7,8,22,23))');
PREPARE native_slot_upgrade FROM @slot_ddl; EXECUTE native_slot_upgrade; DEALLOCATE PREPARE native_slot_upgrade;
