-- Optional typed polymorph instance payload. Old item hashes stay unchanged.
ALTER TABLE player.item_instance
 ADD COLUMN IF NOT EXISTS polymorph_form_id INT UNSIGNED NULL,
 ADD COLUMN IF NOT EXISTS polymorph_book_grade SMALLINT UNSIGNED NULL,
 ADD COLUMN IF NOT EXISTS polymorph_practice INT UNSIGNED NULL;
-- Persistent active transformations use the existing hashed recovery sidecar,
-- with conditional RCS3 encoding; no legacy affect row owns their lifetime.
