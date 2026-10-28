"""Draft bonus registry: semantic validation, generated contract drift, fail-closed output."""
import copy
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'SRC/Tools/ItemCatalogBuilder'))
import generate_bonus_registry as generator


class BonusRegistryTests(unittest.TestCase):
    def setUp(self):
        self.doc = json.loads((ROOT / 'config/item_combat/bonus_registry.json').read_text(encoding='utf-8'))

    def test_current_contract_and_order(self):
        self.assertEqual(len(generator.validate(self.doc)), 34)
        header, sql = generator.render(self.doc), generator.render_sql(self.doc)
        self.assertEqual(header, (ROOT / 'SRC/Server/common/item/BonusRegistry.hpp').read_bytes())
        self.assertEqual(sql, (ROOT / 'db/migrations/item_combat/007_bonus_registry_seed.sql').read_bytes())
        self.doc['bonuses'].reverse()
        self.assertEqual(header, generator.render(self.doc))
        self.assertEqual(sql, generator.render_sql(self.doc))
        self.doc['bonuses'] = self.doc['bonuses'][:1] + self.doc['bonuses'][-1:]
        self.assertIn(b'GlobalOutgoing = 34,', generator.render(self.doc))
        self.assertIn(b'PhysicalAttackFlat = 1,', generator.render(self.doc))

    def test_invalid_values_and_semantic_combinations(self):
        invalid = [('id', v) for v in (0, -1, 4294967296, True, 1.0, '1', None)]
        invalid += [('minimum', v) for v in (-(10**15)-1, True, 1.0, '1', None)]
        invalid += [('maximum', 10**15+1), ('minimum', 10**15), ('maximum', -(10**15)),
                    ('code', "x'; DROP TABLE protos.item_base"), ('code', 'x..y'), ('display_key', 'Upper'),
                    ('unit', 'Integer'), ('target', 'OutgoingDamage'), ('stage', 'Critical'), ('aggregate_cap', 'Resistance'),
                    ('channel', 'Magical'), ('action', 'Normal'), ('mode', 'Pve'), ('family', 'Orc'), ('rank', 'Boss'),
                    ('element', 'Fire'), ('stacking', 'Unknown'), ('allowed_sources', []), ('allowed_sources', ['ItemBase']*2),
                    ('allowed_sources', ['Buff']), ('allowed_sources', [None]), ('allowed_sources', 'ItemBase')]
        for field, value in invalid:
            document = copy.deepcopy(self.doc)
            document['bonuses'][0][field] = value
            # Make the individually valid endpoint changes inverted ranges.
            if field == 'minimum' and value == 10**15:
                document['bonuses'][0]['maximum'] = 0
            if field == 'maximum' and value == -(10**15):
                document['bonuses'][0]['minimum'] = 0
            with self.subTest(field=field, value=value), self.assertRaises(ValueError):
                generator.render(document)
        for field in ('id', 'code', 'cpp_name', 'display_key'):
            document = copy.deepcopy(self.doc)
            document['bonuses'][1][field] = document['bonuses'][0][field]
            with self.subTest(duplicate=field), self.assertRaises(ValueError):
                generator.render(document)

    def test_filter_and_unit_contracts(self):
        for index, field, value in [(6, 'action', 'Any'), (6, 'mode', 'Pve'), (8, 'mode', 'Any'),
                                    (8, 'action', 'Skill'), (14, 'channel', 'Any'), (23, 'channel', 'Any'),
                                    (25, 'unit', 'Integer'), (28, 'unit', 'Raw'), (32, 'stage', 'StatPercent'),
                                    (33, 'action', 'Normal'), (33, 'mode', 'Pve'), (33, 'stage', 'IncomingMode')]:
            doc = copy.deepcopy(self.doc)
            doc['bonuses'][index][field] = value
            with self.subTest(index=index, field=field), self.assertRaises(ValueError):
                generator.validate(doc)
        with self.assertRaises(ValueError):
            json.loads('{"id":1,"id":2}', object_pairs_hook=generator.unique_object)

    def test_cli_check_never_writes_and_invalid_input_preserves_outputs(self):
        with tempfile.TemporaryDirectory() as directory:
            directory = Path(directory)
            source, header, sql = (directory / name for name in ('registry.json', 'Bonus.hpp', 'seed.sql'))
            source.write_text(json.dumps(self.doc), encoding='utf-8')
            command = [sys.executable, '-B', str(Path(generator.__file__)), '--registry', str(source),
                       '--output', str(header), '--sql-output', str(sql)]
            self.assertEqual(subprocess.run(command, capture_output=True).returncode, 0)
            header.write_bytes(b'stale')
            self.assertEqual(subprocess.run(command+['--check'], capture_output=True).returncode, 1)
            self.assertEqual(header.read_bytes(), b'stale')
            before = sql.read_bytes()
            self.doc['bonuses'][0]['unit'] = 'unknown'
            source.write_text(json.dumps(self.doc), encoding='utf-8')
            self.assertEqual(subprocess.run(command, capture_output=True).returncode, 2)
            self.assertEqual(header.read_bytes(), b'stale')
            self.assertEqual(sql.read_bytes(), before)


if __name__ == '__main__':
    unittest.main()
