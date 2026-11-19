"""Retirement must refuse nonempty old state and preserve all native rows."""
from pathlib import Path
import importlib.util
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('retire', ROOT/'db/tools/retire_legacy_item_state.py')
retire = importlib.util.module_from_spec(spec)
spec.loader.exec_module(retire)


class Database:
    def __init__(self, old_rows=0, corrupt=False):
        self.tables = dict(item=[['old']]*old_rows, safebox=[], item_award=[], myshop_pricelist=[],
                           item_persistence_format=[['1', '1']], item_instance=[['93', 'live']],
                           item_wallet=[['1', '12345']])
        for name in retire.LEGACY_TABLES:
            self.tables.setdefault(name, [])
        self.columns = ['singleton', 'schema_version']
        self.writes = []
        self.corrupt = corrupt
        self.dependencies = []

    def query(self, sql):
        if 'information_schema.key_column_usage' in sql:
            return self.dependencies
        if 'information_schema.tables' in sql:
            return [[name] for name in self.tables]
        if 'information_schema.columns' in sql:
            return [[name] for name in self.columns]
        if sql.startswith('SELECT COUNT(*) FROM player.'):
            return [[str(len(self.tables[sql.split('`')[1]]))]]
        if sql.startswith('SELECT * FROM player.'):
            return self.tables[sql.split('`')[1]]
        self.writes.append(sql)
        if 'ALTER TABLE player.item_persistence_format' in sql:
            self.columns.append('native_runtime_version')
            return []
        if 'CREATE TABLE player.item_native_award' in sql:
            self.tables['item_native_award'] = []
            return []
        if sql.startswith('DROP TABLE '):
            for name in retire.LEGACY_TABLES:
                self.tables.pop(name, None)
            if self.corrupt:
                self.tables['item_wallet'] = [['1', '0']]
            return []
        raise AssertionError(sql)


class Retirement(unittest.TestCase):
    def test_dry_run_does_not_modify_schema_or_native_items(self):
        db = Database()
        result = retire.retire(db)
        self.assertFalse(result['applied'])
        self.assertEqual(db.writes, [])
        self.assertEqual(result['legacy_tables'], dict.fromkeys(retire.LEGACY_TABLES, 0))

    def test_old_rows_refuse_even_the_format_migration(self):
        db = Database(old_rows=1)
        with self.assertRaisesRegex(RuntimeError, 'not empty'):
            retire.retire(db, True)
        self.assertEqual(db.writes, [])

    def test_nonempty_secondary_legacy_storages_also_refuse_retirement(self):
        for table in ('new_petsystem','account_mount_inventory','offlineshop_safebox_items','item_legacy_import'):
            db=Database();db.tables[table]=[['kept']]
            with self.subTest(table=table), self.assertRaisesRegex(RuntimeError,'not empty'):
                retire.retire(db,True)
            self.assertEqual(db.writes,[])

    def test_only_empty_old_tables_are_removed(self):
        db = Database()
        result = retire.retire(db, True)
        self.assertTrue(result['native_state_unchanged'])
        self.assertEqual(len(db.writes), 3)
        self.assertEqual(db.tables['item_instance'], [['93', 'live']])
        self.assertEqual(db.tables['item_wallet'], [['1', '12345']])
        self.assertTrue(all(name not in db.tables for name in retire.LEGACY_TABLES))
        retire.retire(db, True)
        self.assertEqual(len(db.writes), 3, 'repeated retirement must be idempotent')

    def test_native_state_change_is_detected(self):
        with self.assertRaisesRegex(RuntimeError, 'Native state changed'):
            retire.retire(Database(corrupt=True), True)

    def test_missing_native_schema_refuses_drop(self):
        db = Database()
        del db.tables['item_persistence_format']
        with self.assertRaisesRegex(RuntimeError, 'not installed'):
            retire.retire(db, True)
        self.assertEqual(db.writes, [])

    def test_unknown_foreign_key_child_refuses_before_any_ddl(self):
        db=Database();db.dependencies=[['player','keep_me','item_legacy_import']]
        with self.assertRaisesRegex(RuntimeError,'Unexpected dependent'):
            retire.retire(db,True)
        self.assertEqual(db.writes,[])

    def test_archive_is_dropped_before_its_import_parent(self):
        db=Database();db.dependencies=[['player','item_legacy_archive','item_legacy_import']]
        retire.retire(db,True)
        ddl=next(s for s in db.writes if s.startswith('DROP TABLE '))
        self.assertLess(ddl.index('`item_legacy_archive`'),ddl.index('`item_legacy_import`'))


if __name__ == '__main__':
    unittest.main()
