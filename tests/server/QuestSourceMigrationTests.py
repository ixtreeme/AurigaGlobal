"""Native source parsing and lossless, isolated quest migration."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / 'SRC/Tools'))
import prepare_quests


class MigrationTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='auriga-quest-migration-')
        self.root = Path(self.temp.name)
        self.source = self.root / 'original'
        (self.source / 'object/state').mkdir(parents=True)
        (self.source / 'libraries').mkdir()
        (self.source / 'a.quest').write_bytes(b'-- \xe1\xe9\nquest retained begin state start begin when login begin local s="!= begin" end end end')
        (self.source / 'b.quest').write_text('quest retained begin state start begin when levelup with !false begin if 1 != 2 then return end end end end')
        (self.source / 'libraries/helper.lua').write_bytes(b'return "\xe1\xe9"')
        (self.source / 'questlib.lua').write_bytes(b"-- \xe1\xe9\r\nfunction select(...)\r\n\treturn q.yield('select', arg)\r\nend\r\n")
        (self.source / 'object/state/retained').write_text('retained={ ["start"] = 0 }')
        (self.source / 'quest_list').write_text('a.quest\nb.quest\n')
        self.output = self.root / 'prepared'

    def tearDown(self):
        self.temp.cleanup()

    def test_duplicate_policy_is_explicit_and_originals_are_preserved(self):
        before = {p.relative_to(self.source): p.read_bytes() for p in self.source.rglob('*') if p.is_file()}
        with self.assertRaisesRegex(ValueError, 'Duplicate quest id'):
            prepare_quests.prepare(self.source, self.output, CHECKER)
        report = prepare_quests.prepare(self.source, self.root / 'compatible', CHECKER, 'last')
        target = self.root / 'compatible'
        self.assertEqual((report['source_count'], report['active_quests'], report['preserved_state_ids']), (2, 1, 1))
        self.assertEqual(report['disabled_duplicates'][0]['retained'], 'b.quest')
        self.assertFalse((target / 'object').exists())
        self.assertEqual(report['lua_changes'][0]['replacements'], 1)
        self.assertIn(b"q.yield('select', {...})", (target / 'questlib.lua').read_bytes())
        self.assertTrue((target / 'quest_list').read_text().startswith('# Lua55 migration:'))
        for relative, original in before.items():
            self.assertEqual((self.source / relative).read_bytes(), original)
            if relative.parts[0] != 'object' and relative.name not in ('quest_list', 'questlib.lua'):
                self.assertEqual((target / relative).read_bytes(), original)
        self.assertEqual(json.loads((target / 'lua55-migration.json').read_text())['source_hashes']['a.quest'],
                         hashlib.sha256(before[Path('a.quest')]).hexdigest())

    def test_state_id_changes_and_missing_states_are_rejected(self):
        (self.source / 'object/state/retained').write_text('retained={["start"]=0,["removed"]=42}')
        with self.assertRaisesRegex(ValueError, 'Stored state ID changed'):
            prepare_quests.prepare(self.source, self.output, CHECKER, 'last')

    def test_overlap_existing_destination_and_missing_sources_are_rejected(self):
        for output in (self.source / 'new', self.root):
            with self.assertRaises(ValueError):
                prepare_quests.prepare(self.source, output, CHECKER, 'last')
        self.output.mkdir()
        (self.output / 'keep').write_text('KEEP')
        with self.assertRaises(FileExistsError):
            prepare_quests.prepare(self.source, self.output, CHECKER, 'last')
        self.assertEqual((self.output / 'keep').read_text(), 'KEEP')
        (self.source / 'quest_list').write_text('missing.quest\n')
        with self.assertRaises(FileNotFoundError):
            prepare_quests.prepare(self.source, self.root / 'missing', CHECKER, 'last')

    def test_native_checks_are_read_only_and_report_bad_source_lines(self):
        path = self.source / 'a.quest'
        path.write_text('quest retained begin\nstate start begin\nwhen login begin\nlocal x = )\nend\nend\nend\n')
        result = subprocess.run([str(CHECKER), '--check-quest', str(path)], capture_output=True,
                                timeout=5, creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('a.quest:4:', result.stderr.decode('utf-8'))
        self.assertFalse((self.source / 'object/notarget').exists())


if __name__ == '__main__':
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument('--checker', required=True, type=Path)
    args, remaining = parser.parse_known_args()
    CHECKER = args.checker.resolve()
    unittest.main(argv=[sys.argv[0], *remaining])
