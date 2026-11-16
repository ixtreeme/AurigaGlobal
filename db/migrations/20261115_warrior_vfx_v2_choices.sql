-- Extend V1 choice constraints for V2. Preserve all rows and progression constraints.
ALTER TABLE warrior_skill_choices
    DROP CONSTRAINT warrior_skill_valid,
    DROP CONSTRAINT warrior_category_valid,
    ADD CONSTRAINT warrior_skill_valid CHECK (skill_id BETWEEN 180 AND 204),
    ADD CONSTRAINT warrior_category_valid CHECK (
        (tier = 0 AND (skill_id BETWEEN 180 AND 183 OR skill_id = 194 OR skill_id BETWEEN 200 AND 204))
        OR (tier BETWEEN 1 AND 3 AND (skill_id BETWEEN 184 AND 189 OR skill_id IN (195,196)))
        OR (tier = 4 AND (skill_id BETWEEN 190 AND 193 OR skill_id BETWEEN 197 AND 199))
    );
