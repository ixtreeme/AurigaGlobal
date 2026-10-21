"""Exercise the deployment migration on disposable tables in the isolated test DB."""
from source_sql import ROOT


def check_four_slot_migration(client, database):
    passed = []
    def check(ok, message):
        if not ok: raise RuntimeError('Four-slot migration failed: ' + message)
        passed.append(message)
    client.query('USE `' + database + '`')
    text = (ROOT / 'db/migrations/2026-10-19_four_character_slots.sql').read_text(encoding='utf-8')
    routines = text.split('DELIMITER $$', 1)[1].split('DELIMITER ;', 1)[0]
    for statement in routines.split('$$'):
        if statement.strip(): client.query(statement)
    table = 'player_index_four_slot_test'
    try:
        client.query('CREATE TABLE ' + table + ' (id INT PRIMARY KEY,pid1 INT DEFAULT 0,pid2 INT DEFAULT 0,pid3 INT DEFAULT 0,pid4 INT DEFAULT 0,pid5 INT DEFAULT 0,empire INT DEFAULT 3) ENGINE=InnoDB')
        client.query('INSERT INTO ' + table + ' VALUES (1,0,12,13,14,15,3),(2,21,22,23,24,25,3),(3,31,0,33,34,35,3),(4,41,42,0,44,45,3),(5,51,52,53,0,55,3),(6,61,62,63,64,64,3)')
        original = client.query('SELECT * FROM ' + table + ' ORDER BY id')
        try: client.query("CALL migrate_four_character_slots('" + table + "')")
        except RuntimeError as error: check('require reassignment' in str(error), 'full account refuses migration with a useful error')
        else: raise RuntimeError('Five occupied slots were silently accepted')
        check(client.query('SELECT * FROM ' + table + ' ORDER BY id') == original, 'blocked migration preserves every original mapping')
        client.query('UPDATE ' + table + ' SET pid4=0 WHERE id=2')
        client.query("CALL migrate_four_character_slots('" + table + "')")
        actual = client.query('SELECT id,pid1,pid2,pid3,pid4,empire FROM ' + table + ' ORDER BY id')
        expected = [[1,15,12,13,14,3],[2,21,22,23,25,3],[3,31,35,33,34,3],[4,41,42,45,44,3],[5,51,52,53,55,3],[6,61,62,63,64,3]]
        check(actual == [[str(v) for v in row] for row in expected], 'all four relocation positions preserve characters and empire')
        columns = client.query("SELECT COLUMN_NAME FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='" + table + "' ORDER BY ORDINAL_POSITION")
        check([row[0] for row in columns] == ['id','pid1','pid2','pid3','pid4','empire'], 'pid5 removed from the migrated table')
        client.query("CALL migrate_four_character_slots('" + table + "')")
        check(client.query('SELECT id,pid1,pid2,pid3,pid4,empire FROM ' + table + ' ORDER BY id') == actual, 'migration can be repeated safely')
        try: client.query("CALL migrate_four_character_slots('player_index;DROP')")
        except RuntimeError: check(True, 'invalid table identifier rejected')
        else: raise RuntimeError('Invalid table identifier accepted')
    finally:
        client.query('DROP TABLE IF EXISTS ' + table)
        client.query('DROP PROCEDURE IF EXISTS migrate_four_character_slots')
    return passed
