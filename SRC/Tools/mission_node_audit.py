"""Rebuild the design-node/legacy-API inventory without running Lua or contacting a server."""
import argparse
from collections import Counter
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import subprocess


CORE = {
    "Start": "manifest.root", "End": "core.success / core.failure",
    "Sequence": "core.sequence", "Branch": "core.branch", "Parallel": "core.parallel",
    "WaitForEvent": "core.wait_event", "WaitForCondition": "core.wait_condition",
    "Delay": "core.delay", "Gate": "core.wait_condition", "Race": "core.parallel (first success)",
    "AND": "condition.all_of", "ALL_OF": "condition.all_of", "OR": "condition.any_of",
    "ANY_OF": "condition.any_of", "NOT": "condition.not", "XOR": "condition.xor (two operands)",
    "AT_LEAST_N": "condition.at_least_n", "EXACTLY_N": "condition.exactly_n",
    "HasStoryFlag": "condition.has_story_flag",
}
EVENTS = {
    "OnMobKilled": "event.mob_killed", "OnBossKilled": "event.boss_defeated",
    "OnItemObtained": "event.item_obtained", "OnItemCrafted": "event.item_crafted",
    "OnPlayerInteract": "event.npc_interacted (NPC subset)",
    "OnAreaDiscovered": "event.area_discovered", "OnPuzzleSolved": "event.puzzle_solved",
}
PARTIAL = {
    "Timeout": "first_terminal + delay/failure; not a dedicated visual node yet",
    "SubGraph": "inline owned tree and named predicates only; external calls/recursion not implemented",
    "Return": "inline completion propagation only",
    "CancelBranch": "implicit race cancellation and whole-instance abandon only",
    "KillObjective": "core.wait_event; solo/party producer contract pending",
    "CollectObjective": "core.wait_event; inventory source/ownership contract pending",
    "TalkObjective": "core.wait_event; authoritative dialogue session pending",
    "ExploreObjective": "core.wait_event; region producer pending",
    "CraftObjective": "core.wait_event; committed craft producer pending",
    "BossObjective": "core.wait_event; encounter/run credit pending",
    "PuzzleObjective": "core.wait_event; server puzzle state pending",
    "EscortObjective": "core.wait_event; authoritative escort service pending",
    "ChoiceObjective": "immutable choice fact; NPC/session/range authorization pending",
    "CompositeObjective": "structured sequence/parallel/recovery; extended objective policy pending",
    "TimedObjective": "delay/first_terminal composition; dedicated typed objective pending",
    "HasMadeChoice": "condition.choice_is; value-specific predicate only",
    "SetStoryFlag": "MissionRuntime.SetFlag / completion_flags; standalone action schema pending",
    "StartMission": "MissionRuntime.Start; graph action binding pending",
    "SaveCheckpoint": "typed in-memory checkpoint; durable store pending",
    "PersistentState": "in-memory story facts/choices; durable store and migration pending",
    "Transaction": "atomic in-memory transitions; external effects are not an ACID transaction",
    "RewardTransaction": "pending ActionKey/receipt barrier only; durable ledger/effects pending",
}
LEGACY_TARGETS = {
    "pc.get_level": "HasLevel", "pc.get_job": "HasClass", "pc.get_empire": "HasFaction",
    "pc.count_item": "HasItem", "pc.get_money": "HasCurrency", "pc.get_gold": "HasCurrency",
    "pc.give_item2": "GiveItem", "pc.give_item": "GiveItem", "pc.remove_item": "RemoveItem",
    "pc.give_exp2": "GiveExperience", "pc.change_gold": "GiveCurrency",
    "pc.warp": "TeleportPlayer", "pc.setqf": "PersistentState", "pc.getqf": "PersistentState",
    "pc.setf": "PersistentState", "pc.getf": "PersistentState",
    "say": "DialogueLine", "select": "PlayerChoice", "notice": "ShowNotification",
    "syschat": "ShowNotification", "server_timer": "Delay / scoped timer",
    "timer": "Delay / character timer", "set_state": "native graph transition",
}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def inventory(root, spec):
    text = spec.read_text(encoding="utf-8-sig")
    catalog = text.split("## 2.", 1)[1].split("## 3.", 1)[0]
    section, nodes = "", []
    for line in catalog.splitlines():
        if line.startswith("### "):
            section = line[4:]
        for name in re.findall(r"`([A-Za-z][A-Za-z0-9_]*)`", line):
            if name in {"Local", "Mission", "Character", "Party", "Dungeon", "World"}:
                continue  # Value scopes, not visual nodes.
            if any(row["name"] == name for row in nodes):
                continue
            status, target = "planned", None
            if name in CORE:
                status, target = "domain_primitive", CORE[name]
            elif name in EVENTS:
                status, target = "domain_event_unbound", EVENTS[name]
            elif name in PARTIAL:
                status, target = "partial_requires_extension", PARTIAL[name]
            nodes.append({"name": name, "family": section, "status": status, "native_mapping": target})
    module_path = root / "SRC/Tools/ServerManager/generate_quest_api.py"
    module_spec = importlib.util.spec_from_file_location("quest_api_inventory", module_path)
    module = importlib.util.module_from_spec(module_spec)
    module_spec.loader.exec_module(module)
    functions, origins = module.catalog(root)
    legacy = []
    for namespace, names in functions.items():
        for name in names:
            api = f"{namespace}.{name}" if namespace else name
            legacy.append({"api": api, "origins": origins[namespace],
                "native_target": LEGACY_TARGETS.get(api),
                "migration": "replace_with_typed_native_service" if api in LEGACY_TARGETS
                    else "audit_native_equivalence_or_authoring_lowering",
                "raw_sql_forbidden_in_new_engine": "mysql" in api.lower()})
    quests = json.loads((root / "docs/mission_engine_audit_2026-10-24_quests.json").read_text(encoding="utf-8"))
    quest_rows = []
    source_root = Path(quests["source_root"])
    for quest in quests["quests"]:
        path = source_root / quest["path"]
        quest_rows.append({"path": quest["path"], "current_sha256": digest(path) if path.is_file() else None,
            "matches_previous_audit": path.is_file() and digest(path) == quest["sha256"],
            "previous_candidate_class": quest["candidate_class"],
            "new_target": "graph_only_candidate" if quest["candidate_class"] == "A" else "native_feature_extension_required",
            "converted": False})
    return {
        "spec_sha256": digest(spec),
        "base_commit": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=root, text=True).strip(),
        "working_tree_implementation": bool(subprocess.check_output([
            "git", "status", "--porcelain", "--", "SRC/Server/GameServer/mission",
            "SRC/Server/GameServer/CMakeLists.txt", "SRC/Tools/MissionCompiler.cpp",
            "tests/server/MissionCoreTests.cpp", "tests/server/MissionPackageTests.cpp"
        ], cwd=root, text=True).strip()),
        "new_runtime_scripting": "none", "seasonal_mission_pass": "preserved",
        "gate_policies": ["any_of", "at_least_n(2)"],
        "gate_policy_selection": "explicit per gate at authoring; no silent default for M06",
        "node_status_counts": dict(Counter(row["status"] for row in nodes)), "nodes": nodes,
        "legacy_api_inventory_note": "Registration declarations include conditional build APIs and opened Lua libraries; not a preprocessed active-build export list.",
        "legacy_api_count": len(legacy), "legacy_apis": legacy,
        "legacy_quests": quest_rows,
        "lua_runtime_version_source": "SRC/3rd/LUA55/src/lua.h (5.5.1)",
        "lua_removal_gate": "No deletion until native parity, progress migration and regression verification, including non-quest Lua users.",
    }


def campaign_manifest(spec):
    text = spec.read_text(encoding="utf-8-sig")
    catalog = text.split("### Act I", 1)[1].split("## Main mission gates", 1)[0]
    act, missions = 1, []
    for line in catalog.splitlines():
        match = re.match(r"### Act (II|III|IV|V)\b", line)
        if match:
            act = {"II": 2, "III": 3, "IV": 4, "V": 5}[match[1]]
        entry = re.match(r"\| ([A-HMS]\d{2}) \| (.*?) \| (.*?) \|", line)
        if entry:
            key, name, objective = entry.groups()
            missions.append({"id": key, "act": act, "title": name, "objective": objective,
                "kind": "main" if key.startswith("M") else "secret" if key.startswith("S") else "side",
                "implementation": "design_only", "content_bindings": None})
    counts = Counter(row["kind"] for row in missions)
    acts = Counter(row["act"] for row in missions)
    if len(missions) != 80 or len({row["id"] for row in missions}) != 80 or counts != {"main": 10, "secret": 6, "side": 64} or any(acts[i] != 16 for i in range(1, 6)):
        raise ValueError(f"Campaign catalog integrity failure: total={len(missions)}, {counts}, {acts}")
    return {"schema": 1, "status": "verified_design_manifest_not_playable_content",
        "source_sha256": digest(spec), "counts": dict(counts), "act_counts": dict(acts),
        "m06_gate": {"supported_policies": ["any_of", "at_least_n"], "threshold_for_two_methods": 2,
                     "selected_policy": None, "selection": "explicit authoring choice before campaign compilation"},
        "missions": missions}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--spec", type=Path, required=True)
    parser.add_argument("--campaign-spec", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--campaign-output", type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    report, campaign = inventory(root, args.spec), campaign_manifest(args.campaign_spec)
    for path, data in [(args.output, report), (args.campaign_output, campaign)]:
        path.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"nodes": len(report["nodes"]), "statuses": report["node_status_counts"],
        "legacy_apis": report["legacy_api_count"], "quests": len(report["legacy_quests"]),
        "campaign": len(campaign["missions"])}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
