"""Regression coverage for the local native content cutover (no live SQL)."""
from pathlib import Path
import copy
import json
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT/'SRC/Tools/ItemCatalogBuilder'))
from convert_local_runtime_content import chance_ppb, convert_drops, convert_starters, digest


class LocalRuntimeContent(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.catalog = json.loads((ROOT/'config/item_combat/full_proto_converted_catalog.json').read_text())

    def test_original_probability_scale(self):
        self.assertEqual(chance_ppb(400), 1_000_000_000)
        self.assertEqual(chance_ppb(100), 250_000_000)
        self.assertEqual(chance_ppb(0.0001), 250)
        self.assertEqual(chance_ppb(0), 0)
        # Original float32 multiplication truncates to 6, despite decimal 7.
        self.assertEqual(chance_ppb(0.0007), 1500)
        for invalid in (-1, 401, float('nan'), True, '100'):
            with self.assertRaises(ValueError):
                chance_ppb(invalid)

    def test_duplicate_groups_are_independent_rolls(self):
        group = dict(name='same', mob=101, type='drop', items=[dict(item='19',count=1,chance=400)])
        result, audit = convert_drops(self.catalog, dict(groups=[group, copy.deepcopy(group)]))
        self.assertEqual(len(result['packet']['tables']['mob_drop_entry']), 2)
        self.assertNotEqual(*[r[1] for r in result['packet']['tables']['mob_drop_group']])
        self.assertEqual(len(audit['duplicate_groups_preserved']), 1)
        self.assertEqual(self.catalog['packet']['tables']['mob_drop_entry'], [])

    def test_only_missing_or_retired_sources_are_excluded(self):
        deferred = self.catalog['blocked'][0]['source_id']
        group = dict(name='test', mob=101, type='drop', items=[dict(item=str(deferred),count=1,chance=1),dict(item='19',count=1,chance=1)])
        result, audit = convert_drops(self.catalog, dict(groups=[group]))
        self.assertEqual(len(result['packet']['tables']['mob_drop_entry']), 1)
        self.assertEqual(audit['excluded'][0]['reason'], 'merge_deferred')
        for table, rows in self.catalog['packet']['tables'].items():
            if table not in ('mob_drop_group','mob_drop_entry'):
                self.assertEqual(rows, result['packet']['tables'][table])

    def test_unsupported_semantics_fail_closed(self):
        group = dict(name='test',mob=101,type='kill',items=[])
        with self.assertRaises(ValueError):
            convert_drops(self.catalog,dict(groups=[group]))
        group.update(type='drop',items=[dict(item='19',count=2,chance=10)])
        with self.assertRaises(ValueError):
            convert_drops(self.catalog,dict(groups=[group]))

    def test_all_character_choices_have_native_packs(self):
        server = ROOT/'SRC/Server'
        result, audit = convert_starters(self.catalog,
            (ROOT/'config/item_combat/original_character_starters.source.cpp').read_text(encoding='utf-8'),
            (server/'common/length.h').read_text(encoding='utf-8'),
            (server/'common/character_creation.h').read_text(encoding='utf-8'))
        self.assertEqual(len(result['profiles']),16)
        self.assertEqual(len(audit),16)
        for profile in result['profiles']:
            content = dict(profile)
            self.assertEqual(content.pop('contract'),digest(content))
            slots = [v['equipped'] for v in profile['grants'] if 'equipped' in v]
            self.assertEqual(len(slots),len(set(slots)))
            self.assertTrue(all('source_id' not in v and 'vnum' not in v for v in profile['grants']))
            self.assertEqual(sum(v.get('equipped')==1 for v in profile['grants']),1)
            self.assertEqual(sum(v.get('equipped')==2 for v in profile['grants']),1)


if __name__ == '__main__':
    unittest.main()
