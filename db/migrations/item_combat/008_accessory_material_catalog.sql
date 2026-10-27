-- Typed child rows, not new copies for +0..+N. One category/base composite key.
-- Accessory socket profiles follow in 009. Ring/belt/unique policies and
-- soul-refinement catalysts require further profiles before content import.
CREATE TABLE protos.item_accessory (
    category_id SMALLINT UNSIGNED NOT NULL DEFAULT 3, local_id INT UNSIGNED NOT NULL,
    equip_slot TINYINT UNSIGNED NOT NULL,
    resistance_profile_id INT UNSIGNED NOT NULL,
    PRIMARY KEY(category_id,local_id),
    FOREIGN KEY(category_id,local_id) REFERENCES protos.item_base(category_id,local_id),
    FOREIGN KEY(resistance_profile_id) REFERENCES protos.item_resistance_profile(profile_id),
    CHECK(category_id=3), CHECK(equip_slot IN (6,7,8))
) ENGINE=InnoDB;

CREATE TABLE protos.item_material (
    category_id SMALLINT UNSIGNED NOT NULL DEFAULT 6, local_id INT UNSIGNED NOT NULL,
    material_kind TINYINT UNSIGNED NOT NULL,
    PRIMARY KEY(category_id,local_id),
    FOREIGN KEY(category_id,local_id) REFERENCES protos.item_base(category_id,local_id),
    CHECK(category_id=6), CHECK(material_kind IN (1,2,3,4,5))
) ENGINE=InnoDB;
-- Material refinement=None and complete base->child coverage are cross-table
-- rules verified by the C++ builder, not by these row-local SQL CHECKs.
