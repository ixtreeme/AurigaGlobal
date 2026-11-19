ALTER TABLE player.item_instance
 ADD energy_percent TINYINT UNSIGNED NULL,
 ADD CONSTRAINT ck_item_energy_percent CHECK
 (energy_percent IS NULL OR (category_id=5 AND energy_percent BETWEEN 1 AND 100));
