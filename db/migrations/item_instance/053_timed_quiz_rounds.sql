-- CLI rounds keep their original, open-ended duration. NPC rounds expire on the shared DB clock.
ALTER TABLE player.item_quiz_round
    ADD COLUMN IF NOT EXISTS expires_at DATETIME(6) NULL DEFAULT NULL;
