-- The registry is static content. Runtime users receive SELECT only.
CREATE TABLE protos.item_category (
    category_id SMALLINT UNSIGNED NOT NULL,
    code VARCHAR(48) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
    prototype_table VARCHAR(53) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
    PRIMARY KEY (category_id),
    UNIQUE KEY uq_item_category_code (code),
    UNIQUE KEY uq_item_category_table (prototype_table),
    CONSTRAINT ck_item_category_nonzero CHECK (category_id > 0)
) ENGINE=InnoDB;

-- Only the catalog publisher may reserve IDs. Lock the category row with
-- SELECT ... FOR UPDATE in the publishing transaction, validate the requested
-- range, then advance this counter and commit the content together.
-- 4294967296 is an exhaustion sentinel, never a valid local_id.
-- Deleting/retiring content must never decrement or rebuild this sequence.
CREATE TABLE protos.item_local_id_sequence (
    category_id SMALLINT UNSIGNED NOT NULL,
    next_local_id BIGINT UNSIGNED NOT NULL DEFAULT 1,
    PRIMARY KEY (category_id),
    CONSTRAINT fk_item_local_id_category FOREIGN KEY (category_id)
        REFERENCES protos.item_category (category_id),
    CONSTRAINT ck_item_local_id_range CHECK (next_local_id BETWEEN 1 AND 4294967296)
) ENGINE=InnoDB;
