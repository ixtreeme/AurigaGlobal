"""Exercise the actual offline compiler and loader across filesystem boundaries."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile
import unittest


PARSER = argparse.ArgumentParser()
PARSER.add_argument("--compiler", required=True)
PARSER.add_argument("--source", required=True)
ARGS, REST = PARSER.parse_known_args()


class MissionPackageToolsTests(unittest.TestCase):
    def run_tool(self, *args, code=0):
        result = subprocess.run([ARGS.compiler, *map(str, args)], capture_output=True, text=True, timeout=20)
        self.assertEqual(result.returncode, code, result.stdout + result.stderr)
        return result

    def test_compile_check_and_preserve_good_output(self):
        with tempfile.TemporaryDirectory(prefix="auriga-mission-package-") as directory:
            folder = Path(directory)
            first, second = folder / "first.aurmission", folder / "second.aurmission"
            self.run_tool("compile", ARGS.source, first)
            self.run_tool("compile", ARGS.source, second)
            self.assertEqual(first.read_bytes(), second.read_bytes())
            self.run_tool("check", first)
            original = hashlib.sha256(first.read_bytes()).hexdigest()
            bad = folder / "invalid.json"
            data = json.loads(Path(ARGS.source).read_text(encoding="utf-8"))
            data["missions"][0]["nodes"][0]["children"].append("unknown.node")
            bad.write_text(json.dumps(data), encoding="utf-8")
            self.run_tool("compile", bad, first, code=1)
            self.assertEqual(hashlib.sha256(first.read_bytes()).hexdigest(), original)
            self.assertEqual(list(folder.glob("*.tmp")), [])
            self.run_tool("compile", ARGS.source, first)
            self.assertEqual(hashlib.sha256(first.read_bytes()).hexdigest(), original)
            broken = folder / "broken.aurmission"
            broken.write_bytes(first.read_bytes()[:-1])
            self.run_tool("check", broken, code=1)
            self.run_tool("compile", ARGS.source, folder / "wrong.extension", code=1)

    def test_schema_has_only_implemented_native_primitives(self):
        with tempfile.TemporaryDirectory(prefix="auriga-mission-schema-") as directory:
            output = Path(directory) / "schema.json"
            self.run_tool("schema", output)
            schema = json.loads(output.read_text(encoding="utf-8"))
            self.assertEqual(schema["scripting"], "none")
            ids = [entry["type_id"] for entry in schema["types"]]
            self.assertEqual(len(ids), len(set(ids)))
            self.assertTrue(all(not entry["gameplay_bound"] for entry in schema["types"]))
            self.assertIn("core.await_reward_receipt", [entry["name"] for entry in schema["types"]])


if __name__ == "__main__":
    unittest.main(argv=[__file__, *REST])
