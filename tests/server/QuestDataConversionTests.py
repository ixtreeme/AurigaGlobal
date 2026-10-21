"""Converter and C++ consumer parity: python QuestDataConversionTests.py --reader <QuestDataTests.exe>."""
import argparse
import json
import pathlib
import runpy
import subprocess
import sys
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]
CONVERTER = ROOT / "SRC/Tools/txt2json/txt2json_quest.py"
convert = runpy.run_path(str(CONVERTER))["convert"]
READER = None


class QuestConversionTests(unittest.TestCase):
    def test_records_and_boundaries(self):
        self.assertEqual(convert("npc", "1\tblacksmith\n4294967295 Guard_2\n"), {
            "npcs": [{"vnum": 1, "name": "blacksmith"}, {"vnum": 4294967295, "name": "Guard_2"}]})
        self.assertEqual(convert("category", '0 main\n3\t"guild_war_join"\n65535 edge'), {
            "categories": [{"quest": "main", "category": 0}, {"quest": "guild_war_join", "category": 3},
                           {"quest": "edge", "category": 65535}]})

    def test_empty_comments_and_crlf(self):
        self.assertEqual(convert("npc", "\r\n # heading\r\n// other\r\n"), {"npcs": []})
        self.assertEqual(convert("category", "\r\n3\tguild_war_join\r\n"), {
            "categories": [{"quest": "guild_war_join", "category": 3}]})

    def test_bad_rows_have_source_and_line(self):
        for mode, row in [("npc", "1"), ("category", "3"), ("npc", "1 a extra"),
                          ("npc", "0 reserved"), ("npc", "1 notarget"), ("npc", "4294967296 a"),
                          ("category", "65536 a"), ("category", "-1 a"), ("category", "1.5 a"),
                          ("npc", "1 ../outside"), ("npc", "1 a\x00b"), ("npc", "1 100"),
                          ("category", "3 café")]:
            with self.subTest(mode=mode, row=row):
                with self.assertRaisesRegex(ValueError, "source.txt:2:"):
                    convert(mode, "\n" + row, "source.txt")

    def test_duplicates_are_not_overwritten(self):
        for mode, text in [("npc", "1 a\n1 b"), ("npc", "1 a\n2 a"), ("category", "1 a\n2 a")]:
            with self.assertRaisesRegex(ValueError, "duplicate.*first at line 1"):
                convert(mode, text)

    def test_cli_invalid_source_does_not_overwrite_output(self):
        with tempfile.TemporaryDirectory(prefix="auriga-quest-converter-") as tmp:
            root = pathlib.Path(tmp)
            source, output = root / "input.txt", root / "output.json"
            source.write_text("3", encoding="utf-8")
            output.write_text("preserve", encoding="utf-8")
            result = subprocess.run([sys.executable, str(CONVERTER), "category", str(source), str(output)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 1)
            self.assertEqual(output.read_text(encoding="utf-8"), "preserve")
            self.assertIn("input.txt:1:", result.stderr)

    def test_cli_bom_and_check_mode(self):
        with tempfile.TemporaryDirectory(prefix="auriga-quest-converter-") as tmp:
            root = pathlib.Path(tmp)
            source, output = root / "input.txt", root / "output.json"
            source.write_text("3\tguild_war_join\n", encoding="utf-8-sig")
            command = [sys.executable, str(CONVERTER), "category", str(source), str(output)]
            subprocess.run(command, check=True, capture_output=True)
            before = output.read_bytes()
            result = subprocess.run(command + ["--check"], check=True, capture_output=True, text=True)
            self.assertIn("parity verified", result.stdout)
            self.assertEqual(output.read_bytes(), before)
            output.write_text('{"categories": []}', encoding="utf-8")
            result = subprocess.run(command + ["--check"], capture_output=True, text=True)
            self.assertEqual(result.returncode, 1)
            self.assertIn("differ", result.stderr)
            output.write_text('{"categories": [], "categories": [{"quest":"guild_war_join","category":3}]}', encoding="utf-8")
            result = subprocess.run(command + ["--check"], capture_output=True, text=True)
            self.assertEqual(result.returncode, 1)
            self.assertIn("duplicate JSON field", result.stderr)

    def test_converted_records_match_production_cpp_reader(self):
        if not READER:
            self.skipTest("pass --reader for production C++ parity")
        with tempfile.TemporaryDirectory(prefix="auriga-quest-parity-") as tmp:
            root = pathlib.Path(tmp)
            expected = {}
            for mode, name, text in [
                ("npc", "questnpc", "20016 blacksmith\n4294967295 Guard_2\n"),
                ("category", "questcategory", "65535 edge\n3\tguild_war_join\n0 main\n"),
            ]:
                source, output = root / (name + ".txt"), root / (name + ".json")
                source.write_text(text, encoding="utf-8")
                subprocess.run([sys.executable, str(CONVERTER), mode, str(source), str(output)], check=True, capture_output=True)
                expected.update(convert(mode, text))
            actual = json.loads(subprocess.check_output([READER, "--dump", str(root)], text=True))
            expected["categories"].sort(key=lambda row: row["quest"])
            self.assertEqual(actual, expected)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument("--reader")
    options, remaining = parser.parse_known_args()
    READER = options.reader
    unittest.main(argv=[sys.argv[0]] + remaining)
