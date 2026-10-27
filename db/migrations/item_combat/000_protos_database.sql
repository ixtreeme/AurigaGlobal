-- Development foundation for the new catalog, MariaDB 11.4/InnoDB.
-- Not wired into manage.py init-db. Run only in a new owned test instance.
-- This is not the complete P1 item/mob/skill/instance schema or a live upgrade.
-- Deliberately refuse an existing database instead of silently reusing it.
CREATE DATABASE protos CHARACTER SET utf8mb4 COLLATE utf8mb4_bin;
