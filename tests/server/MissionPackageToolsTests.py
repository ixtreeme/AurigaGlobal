"""Exercise the actual offline compiler and loader across filesystem boundaries."""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import subprocess
import sys
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
            self.assertEqual({entry["type_id"] for entry in schema["types"] if entry["gameplay_bound"]},
                             {0x3001, 0x3002, 0x3003, 0x3004, 0x3006, 0x3007})
            self.assertIn("core.await_reward_receipt", [entry["name"] for entry in schema["types"]])

    def test_studio_checks_live_events_separately_from_domain_syntax(self):
        with tempfile.TemporaryDirectory() as directory:
            folder = Path(directory)
            source = folder / "source.json"
            package = folder / "story.aurmission"
            content = json.loads(Path(ARGS.source).read_text(encoding="utf-8"))
            self.run_tool("compile", ARGS.source, package)
            self.assertIn("missing gameplay event adapter", self.run_tool("check-events", package, code=1).stderr)
            for mission in content["missions"]:
                for node in mission["nodes"]:
                    if node.get("objective", {}).get("event") == "puzzle_solved":
                        node["objective"]["event"] = "mob_killed"
            source.write_text(json.dumps(content), encoding="utf-8")
            self.run_tool("compile", source, package)
            self.run_tool("check-events", package)

    def test_campaign_edges_preserve_native_prerequisites_and_layout(self):
        sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "SRC/Tools/MissionStudio"))
        try:
            from campaign import empty_board, gate, links, place_missing, set_link
            from mission_source import add_objective, build_draft, new_mission, new_package
        finally:
            sys.path.pop(0)
        source = new_package()
        rules = {}
        for title in ("First", "Second", "Afterwards"):
            graph = new_mission(source, title, title)
            add_objective(source, graph, title, "npc_interacted", "9003", 1)
            rules[graph["mission"]] = [9003, 1]
        first, second, target = [g["mission"] for g in source["missions"]]
        native_rule = {"op": "not_flag", "key": "existing.story.rule"}
        graph["prerequisite"] = native_rule.copy()
        board = empty_board()
        place_missing(board, source)
        for parent in (first, second):
            set_link(source, parent, target)
        before = copy.deepcopy(source)
        with self.assertRaises(ValueError):
            set_link(source, target, first)
        with self.assertRaises(ValueError):
            set_link(source, second, target)
        self.assertEqual(source, before)
        gate(graph)["op"] = "any_of"
        with tempfile.TemporaryDirectory() as directory:
            folder = build_draft(ARGS.compiler, source, rules, Path(directory), board)
            recovered = json.loads((folder / "forras.json").read_bytes())
            self.assertEqual(links(recovered), [(first, target), (second, target)])
            self.assertEqual(gate(recovered["missions"][-1])["op"], "any_of")
            self.assertEqual(json.loads((folder / "kampanyterv.json").read_bytes())["campaign"], board)
        for parent in (first, second):
            set_link(source, parent, target, remove=True)
        self.assertEqual(graph["prerequisite"], native_rule)
        self.assertFalse(links(source))


if __name__ == "__main__":
    unittest.main(argv=[__file__, *REST])
