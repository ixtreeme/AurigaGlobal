"""Author Mission package sources. The server's MissionCompiler validates every export."""
import json
import os
from hashlib import sha256
from pathlib import Path
import subprocess
import tempfile
from uuid import uuid4


# These authoring spellings are used by ParseDefinition. Each event here has a
# GameServer producer and needs no additional dialogue-choice definitions.
EVENTS = {
    "mob_killed": ("Szörny legyőzése", "mob."),
    "boss_defeated": ("Főellenség legyőzése", "mob."),
    "npc_interacted": ("Beszélgetés NPC-vel", "npc."),
    "item_collected": ("Tárgy gyűjtése", "item."),
    "item_crafted": ("Tárgy készítése", "item."),
}


def new_package():
    return {"schema": 1, "package": "studio." + uuid4().hex, "version": 1,
            "default_locale": "hu", "missions": [],
            "presentation": {"locales": {"hu": {}}, "missions": {}}}


def done_flag(code):
    return "studio.done." + sha256(code.encode("utf-8")).hexdigest()[:32]


def new_mission(source, title, summary):
    mission_id = "quest." + uuid4().hex
    graph = {"schema": 2, "mission": mission_id, "version": 1, "root": "steps",
             "nodes": [{"id": "steps", "kind": "sequence", "children": [],
                        "completion_flags": [done_flag(mission_id)]}]}
    source["missions"].append(graph)
    source["presentation"]["missions"][mission_id] = {
        "title": mission_id + ".title", "summary": mission_id + ".summary", "objectives": []}
    for texts in source["presentation"]["locales"].values():
        texts.update({mission_id + ".title": title, mission_id + ".summary": summary})
    return graph


def edit_translation(source, binding, field, value):
    presentation = source["presentation"]
    locales = presentation["locales"]
    default = locales[source["default_locale"]]
    key = binding[field]
    if default[key] == value:
        return
    references = 0
    for mission in presentation["missions"].values():
        references += (mission["title"] == key) + (mission["summary"] == key)
        references += sum(row["title"] == key for row in mission["objectives"])
    if references > 1:
        separate = "text." + uuid4().hex
        for texts in locales.values():
            texts[separate] = texts[key]
        binding[field] = key = separate
    default[key] = value


def objective_subject(event, target):
    prefix = EVENTS[event][1]
    if target == "*":
        return target
    parts = target.removeprefix(prefix).split(".")
    limits = (65535, 4294967295, 255) if prefix == "item." else (4294967295,)
    if (len(parts) != len(limits) or any(not p.isascii() or not p.isdecimal() or len(p) > 10
                                        for p in parts)):
        raise ValueError("A cél formátuma tárgynál kategória.típus.szint, NPC-nél és szörnynél egy szám.")
    values = list(map(int, parts))
    identifiers = values[:2] if prefix == "item." else values[:1]
    if not all(0 <= n <= limit for n, limit in zip(values, limits)) or 0 in identifiers:
        raise ValueError("A cél számai kívül esnek a megengedett tartományon.")
    return prefix + ".".join(map(str, values))


def add_objective(source, graph, title, event, target, count):
    if not title.strip() or not 1 <= count <= 1_000_000_000:
        raise ValueError("Adj meg egy célnevet és 1–1 000 000 000 közötti darabszámot.")
    root = next(n for n in graph["nodes"] if n["id"] == graph["root"])
    if root["kind"] != "sequence":
        raise ValueError("Ehhez a művelethez sorrendben teljesítendő célokból álló küldetés szükséges.")
    subject = objective_subject(event, target.strip())
    node = "step." + uuid4().hex
    title_key = node + ".title"
    graph["nodes"].append({"id": node, "kind": "objective",
                           "objective": {"event": event, "subject": subject, "goal": count}})
    final = root["children"][-1] if root["children"] else None
    if any(n["id"] == final and n["kind"] == "reward" for n in graph["nodes"]):
        root["children"].insert(-1, node)
    else:
        root["children"].append(node)
    source["presentation"]["missions"][graph["mission"]]["objectives"].append({"node": node, "title": title_key})
    for texts in source["presentation"]["locales"].values():
        texts[title_key] = title


def completion_reward(graph):
    root = next(n for n in graph["nodes"] if n["id"] == graph["root"])
    rewards = [n for n in graph["nodes"] if n["kind"] == "reward"]
    if root["kind"] != "sequence" or (rewards and (len(rewards) != 1 or root["children"][-1:] != [rewards[0]["id"]])):
        raise ValueError("Ebben a küldetésben a jutalmazás elágazó. Az egyszerű zárójutalom itt nem szerkeszthető.")
    if not rewards:
        return root, None
    bundle = graph["rewards"][rewards[0]["reward"]]
    if bundle.get("schema", 1) != 3:
        raise ValueError("A jutalom régebbi sémát használ. A szerkesztéshez verziózott átalakítás szükséges.")
    return root, bundle


def set_completion_reward(source, graph, experience, gold):
    if not any((experience, gold)) or not all(0 <= v <= 2**63 - 1 for v in (experience, gold)):
        raise ValueError("Adj meg pozitív XP- vagy Yang-jutalmat a szerver által kezelt tartományban.")
    root, bundle = completion_reward(graph)
    if bundle is None:
        node = "reward." + uuid4().hex
        root["children"].append(node)
        graph["nodes"].append({"id": node, "kind": "reward", "reward": node})
        bundle = {"schema": 3}
        graph.setdefault("rewards", {})[node] = bundle
        title = node + ".title"
        source["presentation"]["missions"][graph["mission"]]["objectives"].append({"node": node, "title": title})
        for texts in source["presentation"]["locales"].values():
            texts[title] = "Jutalom átvétele"
    bundle.update(experience=experience, gold=gold)


def read_json(path):
    def unique(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise ValueError("Ismétlődő JSON-kulcs: " + key)
            result[key] = value
        return result
    return json.loads(path.read_text(encoding="utf-8-sig"), object_pairs_hook=unique)


def check_source(data):
    if not isinstance(data, dict) or data.get("schema") != 1 or not isinstance(data.get("missions"), list):
        raise ValueError("Mission Studio-csomagforrást válassz.")
    return data


def runtime_config(source, rules):
    if not 1 <= len(source["missions"]) <= 32:
        raise ValueError("A játékbeli napló egy csomagból 1–32 küldetést tud betölteni.")
    rows = []
    for graph in source["missions"]:
        code = graph["mission"]
        npc, level = rules.get(code, (0, 0))
        if type(npc) is not int or not 1 <= npc <= 4294967295 or type(level) is not int or not 0 <= level <= 255:
            title = source["presentation"]["missions"][code]["title"]
            name = source["presentation"]["locales"][source["default_locale"]][title]
            raise ValueError(f"Adj meg érvényes küldetésadót és belépési szintet ehhez a küldetéshez: {name}")
        rows.append({"code": code, "version": graph["version"], "npc_race": npc, "min_level": level})
    return {"version": 1, "package": "mission.aurmission", "missions": rows}


def save_draft(path, source):
    path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(dir=path.parent, delete=False) as temp:
        name = temp.name
    try:
        Path(name).write_text(json.dumps(source, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        os.replace(name, path)
    finally:
        Path(name).unlink(missing_ok=True)


def compile_source(compiler, source, directory, check_events=False):
    # A failed check must leave any earlier reviewed binary intact.
    with tempfile.TemporaryDirectory(dir=directory) as temp:
        src, binary = Path(temp)/"source.json", Path(temp)/"mission.aurmission"
        save_draft(src, source)
        for args in (["compile", src, binary], ["check-events" if check_events else "check", binary]):
            result = subprocess.run([compiler, *args], capture_output=True, text=True, encoding="utf-8",
                                    errors="replace", timeout=45, creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
            if result.returncode:
                raise ValueError((result.stderr or result.stdout).strip() or f"Fordítóhiba ({result.returncode})")
        output = directory/"mission.aurmission"
        os.replace(binary, output)
        return output


def build_draft(compiler, source, rules, directory, board=None):
    runtime = runtime_config(source, rules)
    components = [source, runtime]
    if board is not None:
        components.append(board)
    content = json.dumps(components, sort_keys=True, ensure_ascii=False).encode("utf-8")
    target = directory / sha256(content).hexdigest()
    directory.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=directory) as temp:
        bundle = Path(temp) / "csomag"
        bundle.mkdir()
        compile_source(compiler, source, bundle, check_events=True)
        save_draft(bundle / "typed_mission_runtime.json", runtime)
        save_draft(bundle / "forras.json", source)
        if board is not None:
            save_draft(bundle / "kampanyterv.json",
                       {"package": source["package"], "version": source["version"], "campaign": board})
        try:
            bundle.rename(target)
        except OSError:
            if not target.is_dir() or any((target / p.name).read_bytes() != p.read_bytes() for p in bundle.iterdir()):
                raise
    return target
