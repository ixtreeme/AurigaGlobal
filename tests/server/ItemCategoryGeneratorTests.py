"""Category registry validation, reproducibility and build drift refusal."""
from __future__ import annotations

import copy
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
GENERATOR = ROOT / "SRC/Tools/ItemCatalogBuilder/generate_item_categories.py"
spec = importlib.util.spec_from_file_location("item_category_generator", GENERATOR)
generator = importlib.util.module_from_spec(spec)
spec.loader.exec_module(generator)


class RegistryTests(unittest.TestCase):
    def setUp(self):
        self.document = json.loads((ROOT / "config/item_combat/category_registry.json").read_text(encoding="utf-8"))

    def test_reviewed_registry(self):
        rows = generator.validate_registry(self.document)
        self.assertEqual([row["id"] for row in rows], list(range(1, 19)))
        self.assertEqual(rows[0]["cpp_name"], "Weapon")
        self.assertEqual(rows[-1]["cpp_name"], "ContainerKey")
        header = ROOT / "SRC/Server/common/item/ItemCategory.hpp"
        self.assertEqual(header.read_bytes(), generator.render(self.document))
        seed = ROOT / "db/migrations/item_combat/002_category_seed.sql"
        self.assertEqual(seed.read_bytes(), generator.render_sql(self.document))

    def test_sorted_generation_and_category_gaps(self):
        original = generator.render(self.document)
        original_sql = generator.render_sql(self.document)
        self.document["categories"].reverse()
        self.assertEqual(original, generator.render(self.document))
        self.assertEqual(original_sql, generator.render_sql(self.document))
        # Issued IDs need not be contiguous; deleted IDs must never be compacted.
        self.document["categories"] = [self.document["categories"][0], self.document["categories"][-1]]
        rendered = generator.render(self.document).decode()
        self.assertIn("Weapon = 1,", rendered)
        self.assertIn("ContainerKey = 18,", rendered)
        self.assertNotIn("ContainerKey = 2,", rendered)

    def test_reject_invalid_ids_and_duplicates(self):
        for invalid in [0, -1, 65536, True, 1.0, "1", None]:
            with self.subTest(invalid=invalid):
                document = copy.deepcopy(self.document)
                document["categories"][0]["id"] = invalid
                with self.assertRaises(ValueError):
                    generator.render(document)
        for name in ["id", "code", "cpp_name"]:
            with self.subTest(duplicate=name):
                document = copy.deepcopy(self.document)
                document["categories"][1][name] = document["categories"][0][name]
                if name == "code":
                    document["categories"][1]["table"] = document["categories"][0]["table"]
                with self.assertRaises(ValueError):
                    generator.render(document)

    def test_reject_schema_and_identifier_injection(self):
        for member, value in [("schema_version", True), ("schema_version", 2), ("status", "unknown"),
                              ("categories", []), ("categories", {})]:
            document = copy.deepcopy(self.document)
            document[member] = value
            with self.subTest(member=member, value=value), self.assertRaises(ValueError):
                generator.render(document)
        for name, value in [("code", "x'; DROP TABLE item"), ("code", "Uppercase"),
                            ("cpp_name", 'X }; namespace hacked {'), ("cpp_name", "while"),
                            ("table", "player.item"), ("id", [])]:
            document = copy.deepcopy(self.document)
            document["categories"][0][name] = value
            with self.subTest(member=name, value=value), self.assertRaises(ValueError):
                generator.render(document)
        document = copy.deepcopy(self.document)
        document["categories"][0]["unexpected"] = 1
        with self.assertRaises(ValueError):
            generator.render(document)

    def test_check_is_read_only_and_refuses_stale(self):
        with tempfile.TemporaryDirectory() as directory:
            directory = Path(directory)
            registry = directory / "registry.json"
            output = directory / "generated.hpp"
            seed = directory / "seed.sql"
            registry.write_text(json.dumps(self.document), encoding="utf-8")
            command = [sys.executable, "-B", str(GENERATOR), "--registry", str(registry), "--output", str(output), "--sql-output", str(seed)]
            missing = subprocess.run(command + ["--check"], capture_output=True)
            self.assertEqual(missing.returncode, 1)
            self.assertFalse(output.exists())
            subprocess.run(command, check=True, capture_output=True)
            expected = output.read_bytes()
            subprocess.run(command + ["--check"], check=True, capture_output=True)
            output.write_bytes(b"stale header")
            stale = subprocess.run(command + ["--check"], capture_output=True)
            self.assertEqual(stale.returncode, 1)
            self.assertEqual(output.read_bytes(), b"stale header")
            subprocess.run(command, check=True, capture_output=True)
            self.assertEqual(output.read_bytes(), expected)
            seed.write_bytes(b"stale seed")
            stale_seed = subprocess.run(command + ["--check"], capture_output=True)
            self.assertEqual(stale_seed.returncode, 1)
            self.assertEqual(seed.read_bytes(), b"stale seed")
            subprocess.run(command, check=True, capture_output=True)
            expected_seed = seed.read_bytes()
            registry.write_text('{"schema_version":1,"schema_version":1}', encoding="utf-8")
            invalid = subprocess.run(command, capture_output=True)
            self.assertEqual(invalid.returncode, 2)
            self.assertEqual(output.read_bytes(), expected)
            self.assertEqual(seed.read_bytes(), expected_seed)
            self.assertEqual(sorted(path.name for path in directory.iterdir()), ["generated.hpp", "registry.json", "seed.sql"])


if __name__ == "__main__":
    unittest.main()
