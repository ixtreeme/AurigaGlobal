"""Editable control flow for one mission, lowered to native structured Mission nodes."""

from copy import deepcopy
from graphlib import CycleError, TopologicalSorter
from hashlib import sha256
from math import isfinite
from uuid import uuid4

from mission_source import EVENTS, objective_subject

TYPES = {
    "start": "Kezdés",
    "dialogue": "NPC-párbeszéd",
    "text": "Szövegdoboz",
    "task": "Feladat",
    "condition": "Feltétel",
    "reward": "Jutalom",
    "timer": "Várakozás",
    "end": "Befejezés",
    "native": "Natív lépés",
}
ANSWER_KEYS = "ABCDEFGH"


def node(kind, x, y, **fields):
    return {"kind": kind, "x": x, "y": y, **fields}


def ports(n):
    if n["kind"] == "end":
        return []
    if n["kind"] == "dialogue":
        return list(ANSWER_KEYS[: len(n["answers"])])
    if n["kind"] == "condition":
        return ["yes", "no"]
    return n.get("ports", ["next"])


def title(n):
    return n.get("name") or TYPES[n["kind"]]


def fresh_id():
    return "n." + uuid4().hex


def empty_flow():
    return {
        "version": 1,
        "nodes": {"start": node("start", 20, 125), "end": node("end", 640, 125)},
        "wires": {"start": {"next": "end"}},
    }


def targets(flow, key):
    return list(flow["wires"].get(key, {}).values())


def validate(flow):
    if not isinstance(flow, dict) or not isinstance(flow.get("wires"), dict):
        raise TypeError("Érvénytelen küldetésgráf.")
    nodes = flow["nodes"]
    if flow.get("version") != 1 or not isinstance(nodes, dict) or not 2 <= len(nodes) <= 512:
        raise ValueError("Érvénytelen küldetésgráf.")
    if nodes.get("start", {}).get("kind") != "start" or nodes.get("end", {}).get("kind") != "end":
        raise ValueError("A gráfból hiányzik a kezdés vagy a befejezés.")
    for key, n in nodes.items():
        if (
            not isinstance(key, str)
            or not key
            or not isinstance(n, dict)
            or n.get("kind") not in TYPES
            or n["kind"] == "native"
            or not isinstance(n.get("name", ""), str)
            or (n["kind"] in ("start", "end") and n["kind"] != key)
            or not all(
                type(n.get(k)) in (int, float) and -100000 <= n[k] <= 100000 and isfinite(n[k])
                for k in ("x", "y")
            )
        ):
            raise ValueError("Érvénytelen csomópont.")
        if n["kind"] == "dialogue" and (
            not isinstance(n["answers"], list)
            or not 2 <= len(n["answers"]) <= len(ANSWER_KEYS)
            or not all(isinstance(v, str) and v.strip() for v in n["answers"])
        ):
            raise ValueError("Egy párbeszédhez 2–8 válasz szükséges.")
        if n["kind"] in ("dialogue", "text") and not all(
            isinstance(n.get(f), str) and n[f].strip() for f in ("speaker", "line")
        ):
            raise ValueError("Add meg a beszélőt és a párbeszéd szövegét.")
        if n["kind"] == "task":
            if (
                n.get("event") not in EVENTS
                or type(n.get("goal")) != int
                or not 1 <= n["goal"] <= 1_000_000_000
            ):
                raise ValueError("Érvénytelen feladat vagy darabszám.")
            if not isinstance(n.get("subject"), str):
                raise ValueError("A feladathoz válassz célpontot.")
            objective_subject(n["event"], n["subject"])
        if n["kind"] == "timer" and (type(n.get("duration")) != int or not 1 <= n["duration"] <= 2**63 - 1):
            raise ValueError("A várakozásnak pozitívnak kell lennie.")
        if n["kind"] == "reward" and not isinstance(n.get("bundle"), dict):
            raise ValueError("Érvénytelen jutalomcsomag.")
        if n["kind"] == "condition" and not isinstance(n.get("test"), str):
            raise ValueError("Érvénytelen feltételhivatkozás.")
    for key, edges in flow["wires"].items():
        if (
            key not in nodes
            or not isinstance(edges, dict)
            or set(edges) - set(ports(nodes[key]))
            or any(t not in nodes or t == "start" for t in edges.values())
        ):
            raise ValueError("A vezeték ismeretlen csomópontra vagy kimenetre mutat.")


def connect(flow, key, port, target):
    if (
        key not in flow["nodes"]
        or target not in flow["nodes"]
        or target == "start"
        or port not in ports(flow["nodes"][key])
    ):
        raise ValueError("Kimenetet egy másik csomópont bemenetére húzz.")
    pending, seen = [target], set()
    while pending:
        current = pending.pop()
        if current == key:
            raise ValueError("Ez a vezeték visszacsatolást hozna létre.")
        if current not in seen:
            seen.add(current)
            pending.extend(targets(flow, current))
    flow["wires"].setdefault(key, {})[port] = target


def drop_node(flow, key):
    if key in ("start", "end"):
        raise ValueError("A kezdés és a befejezés a küldetés része.")
    del flow["nodes"][key]
    flow["wires"].pop(key, None)
    for edges in flow["wires"].values():
        for port in [p for p, v in edges.items() if v == key]:
            del edges[port]


def ordered(flow):
    validate(flow)
    nodes = flow["nodes"]
    if len(nodes) == 2:
        raise ValueError("A kezdés és a befejezés közé adj egy lépést.")
    order = TopologicalSorter()
    for key, n in nodes.items():
        order.add(key)
        for port in ports(n):
            dest = flow["wires"].get(key, {}).get(port)
            if not dest:
                raise ValueError(f"Hiányzó vezeték: {title(n)} · {port}.")
            order.add(dest, key)
    try:
        sequence = list(order.static_order())
    except CycleError as error:
        raise ValueError("A gráfban visszacsatolás van.") from error
    reachable = {"start"}
    for key in sequence:
        if key not in reachable:
            raise ValueError("A kezdéstől nem elérhető: " + title(nodes[key]))
        reachable.update(targets(flow, key))
    return sequence


def import_flow(source, graph):
    """Only translate a linear native source when its original nodes can round-trip."""
    by_id = {n["id"]: n for n in graph["nodes"]}
    root = by_id[graph["root"]]
    binding = source["presentation"]["missions"][graph["mission"]]
    texts = source["presentation"]["locales"][source["default_locale"]]
    names = {r["node"]: texts[r["title"]] for r in binding["objectives"]}
    keys = root.get("children", [])
    simple = (
        root["kind"] == "sequence"
        and len(keys) == len(by_id) - 1
        and all(
            by_id[k]["kind"] in ("objective", "reward", "timer")
            and (by_id[k]["kind"] != "objective" or by_id[k]["objective"]["event"] in EVENTS)
            for k in keys
        )
        and not ({"start", "end"} & by_id.keys())
    )
    flow = empty_flow()
    if simple:
        tail = "start"
        for i, k in enumerate(keys, 1):
            old = by_id[k]
            kind = {"objective": "task", "reward": "reward", "timer": "timer"}[old["kind"]]
            n = node(kind, 20 + i * 200, 125, name=names.get(k, TYPES[kind]), original=deepcopy(old))
            if kind == "task":
                n.update(old["objective"])
            if kind == "reward":
                n["bundle"] = deepcopy(graph.get("rewards", {}).get(old["reward"], {}))
            if kind == "timer":
                n["duration"] = old["duration"]
            flow["nodes"][k] = n
            flow["wires"][tail] = {"next": k}
            tail = k
        flow["wires"][tail] = {"next": "end"}
        flow["nodes"]["end"]["x"] = 20 + 200 * (len(keys) + 1)
        return flow
    flow["readonly"] = True
    flow["wires"] = {}
    pending = [(graph["root"], 0)]
    seen = set()
    rows = {}
    while pending:
        k, depth = pending.pop(0)
        if k in seen:
            continue
        seen.add(k)
        raw = by_id[k]
        children = list(raw.get("children", [])) + [a["child"] for a in raw.get("branches", [])]
        if "fallback" in raw:
            children.append(raw["fallback"])
        labels = [str(i + 1) for i in range(len(children))]
        nk = "native." + k
        flow["nodes"][nk] = node(
            "native",
            210 + 190 * depth,
            30 + 98 * rows.get(depth, 0),
            name=names.get(k, raw["kind"]),
            original=raw,
            ports=labels,
        )
        rows[depth] = rows.get(depth, 0) + 1
        flow["wires"][nk] = dict(zip(labels, ("native." + c for c in children)))
        pending.extend((c, depth + 1) for c in children)
    flow["wires"]["start"] = {"next": "native." + graph["root"]}
    del flow["nodes"]["end"]
    return flow


def materialize(source, graph, flow):
    """Compile a single-entry/single-exit flow in a private copy of the package."""
    sequence = ordered(flow)
    nodes = flow["nodes"]
    wires = flow["wires"]
    # A shared continuation executes once, after the selected arm has finished.
    post = {}
    for k in reversed(sequence):
        children = targets(flow, k)
        post[k] = {k} | (set.intersection(*(post[c] for c in children)) if children else set())
    natives = []
    generated = set()

    def emit(n):
        if n["id"] in generated:
            raise ValueError("A kereszteződő ágakat közös folytatásnál egyesítsd.")
        generated.add(n["id"])
        natives.append(n)
        return n["id"]

    old_root = next(n for n in graph["nodes"] if n["id"] == graph["root"])
    binding = source["presentation"]["missions"][graph["mission"]]
    old_labels = {r["node"]: r for r in binding["objectives"]}
    labels = []
    locales = source["presentation"]["locales"]
    default = source["default_locale"]
    rewards = {}
    choices = graph.setdefault("choices", {})
    for old in graph["nodes"]:
        obj = old.get("objective", {})
        if obj.get("event") == "dialogue_choice" and obj.get("subject") == old["id"] + ".reply":
            choices.pop(obj["subject"], None)

    def caption(k, value):
        if len(value.encode("utf-8")) > 4096:
            raise ValueError("A küldetésnaplóban egy lépés szövege legfeljebb 4096 UTF-8 bájt lehet.")
        entry = old_labels.get(k)
        if entry and locales[default][entry["title"]] == value:
            labels.append(entry)
            return
        # Editing an imported caption must preserve other missions using the same text.
        suffix = sha256((graph["mission"] + "|" + k).encode()).hexdigest()[:32]
        key = "studio.label." + suffix
        if entry:
            for texts in locales.values():
                texts[key] = texts.get(entry["title"], value)
        entry = {"node": k, "title": key}
        labels.append(entry)
        for lang, texts in locales.items():
            if lang == default or entry["title"] not in texts:
                texts[entry["title"]] = value

    visited = set()

    def section(k, stop):
        result = []
        while k != stop:
            if k in visited:
                raise ValueError("A részlegesen összefutó ágakat egyetlen közös folytatásnál kösd össze.")
            visited.add(k)
            n = nodes[k]
            kind = n["kind"]
            if kind in ("dialogue", "condition"):
                branch_key = k + ".branch"
                out = ports(n)
                if kind == "dialogue":
                    subject = k + ".reply"
                    choices[subject] = out
                    result.append(
                        emit(
                            {
                                "id": k,
                                "kind": "objective",
                                "objective": {"event": "dialogue_choice", "subject": subject, "goal": 1},
                            }
                        )
                    )
                    prompt = (
                        n["speaker"]
                        + " · "
                        + n["line"]
                        + "\n"
                        + "\n".join(f"{p}. {s}" for p, s in zip(out, n["answers"]))
                    )
                    caption(k, prompt)
                    conditions = [{"op": "choice_is", "key": subject, "value": p} for p in out[:-1]]
                else:
                    if n.get("test") not in nodes or nodes[n["test"]]["kind"] != "task":
                        raise ValueError(title(n) + " · Válassz egy feladatot a feltételhez.")
                    if graph.get("repeatable"):
                        raise ValueError(
                            "Ismételhető küldetésben a tartós feladatjelző korábbi teljesítésből is megmaradhat. Válassz párbeszédes elágazást."
                        )
                    conditions = [{"op": "has_flag", "key": task_flag(graph, n["test"])}]
                join = next(v for v in sequence if v != k and v in post[k])
                arm_ids = []
                for p in out:
                    ids = section(wires[k][p], join)
                    aid = k + ".arm." + p
                    arm_ids.append(
                        emit(
                            {"id": aid, "kind": "sequence", "children": ids}
                            if ids
                            else {"id": aid, "kind": "success"}
                        )
                    )
                result.append(
                    emit(
                        {
                            "id": branch_key,
                            "kind": "branch",
                            "branches": [{"when": c, "child": v} for c, v in zip(conditions, arm_ids)],
                            "fallback": arm_ids[-1],
                        }
                    )
                )
                k = join
                continue
            native = deepcopy(n.get("original", {}))
            native["id"] = k
            if kind == "text":
                subject = k + ".reply"
                choices[subject] = ["OK"]
                native.update(
                    kind="objective", objective={"event": "dialogue_choice", "subject": subject, "goal": 1}
                )
            elif kind == "task":
                native.update(
                    kind="objective",
                    objective={
                        **native.get("objective", {}),
                        "event": n["event"],
                        "subject": objective_subject(n["event"], n["subject"]),
                        "goal": n["goal"],
                    },
                )
                if any(t.get("test") == k for t in nodes.values()):
                    flags = native.setdefault("completion_flags", [])
                    flag = task_flag(graph, k)
                    if flag not in flags:
                        flags.append(flag)
            elif kind == "reward":
                reward = native.get("reward", k)
                native.update(kind="reward", reward=reward)
                if reward in rewards and rewards[reward] != n["bundle"]:
                    raise ValueError("Két jutalomlépés eltérően módosítana egy közös jutalomcsomagot.")
                rewards[reward] = deepcopy(n["bundle"])
            elif kind == "timer":
                native.update(kind="timer", duration=n["duration"])
            else:
                raise ValueError("Nem fordítható lépés: " + title(n))
            result.append(emit(native))
            caption(k, n["speaker"] + " · " + n["line"] if kind == "text" else title(n))
            k = wires[k]["next"]
        return result

    children = section(wires["start"]["next"], "end")
    emit({**old_root, "kind": "sequence", "children": children})
    graph["nodes"] = natives
    graph["rewards"] = rewards
    binding["objectives"] = labels


def task_flag(graph, key):
    from mission_source import done_flag

    return done_flag(graph["mission"] + "." + key)


def package_source(source, board):
    result = deepcopy(source)
    for graph in result["missions"]:
        flow = board["cards"].get(graph["mission"], {}).get("flow")
        if flow:
            try:
                materialize(result, graph, flow)
            except ValueError as error:
                label = result["presentation"]["missions"][graph["mission"]]["title"]
                name = result["presentation"]["locales"][result["default_locale"]][label]
                raise ValueError(f"{name} · {error}") from error
    return result


def example_flow():
    f = empty_flow()
    talk, hunt, report, big, small = (fresh_id() for _ in range(5))
    f["nodes"] = {
        "start": node("start", 15, 142),
        talk: node(
            "dialogue",
            221,
            142,
            name="A kapitány kérése",
            speaker="Kapitány",
            portrait="kaldor",
            line="A farkasok a határig merészkedtek. Visszaszorítod őket, vagy inkább hírt viszel az őrnek?",
            answers=["Megkeresem a farkasokat.", "Beszélek az őrrel."],
        ),
        hunt: node(
            "task", 452, 58, name="Farkasok a határnál", event="mob_killed", subject="mob.103", goal=10
        ),
        report: node(
            "task", 452, 242, name="Jelentés az őrnek", event="npc_interacted", subject="npc.20374", goal=1
        ),
        big: node(
            "reward", 675, 58, name="A vadász jutalma", bundle={"schema": 3, "experience": 500, "gold": 1000}
        ),
        small: node(
            "reward", 675, 242, name="A hírvivő jutalma", bundle={"schema": 3, "experience": 100, "gold": 250}
        ),
        "end": node("end", 891, 142),
    }
    f["wires"] = {
        "start": {"next": talk},
        talk: {"A": hunt, "B": report},
        hunt: {"next": big},
        report: {"next": small},
        big: {"next": "end"},
        small: {"next": "end"},
    }
    return f
