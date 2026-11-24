"""Apply a catalog or editor drag to a private copy of a quest graph."""

from dataclasses import dataclass

from quest_flow import TYPES, connect, fresh_id, node, ports


@dataclass(frozen=True)
class Asset:
    kind: str
    name: str
    subject: str = ""
    line: str = ""
    speaker: str = ""
    count: int = 1
    origin: str = ""
    mission: str = ""


def defaults(kind):
    return {
        "dialogue": {
            "speaker": "Küldetésadó",
            "line": "Segítesz nekem?",
            "answers": ["Igen.", "Mesélj még."],
        },
        "text": {"speaker": "Mesélő", "line": "Írd ide a játékosnak szóló szöveget."},
        "task": {"event": "mob_killed", "subject": "*", "goal": 1},
        "condition": {"test": ""},
        "reward": {"bundle": {"schema": 3, "experience": 100, "gold": 0}},
        "timer": {"duration": 1000},
    }[kind]


def reward_subject(row):
    return f"item.{row['category']}.{row['local']}.{row['level']}"


def change_stack(bundle, subject, delta):
    if bundle.get("schema", 1) not in (2, 3):
        raise ValueError("Ez a jutalom régi formátumú.")
    rows = bundle.get("typed_items", [])
    row = next((r for r in rows if reward_subject(r) == subject), None)
    count = (row["count"] if row else 0) + delta
    if not 0 <= count <= 50000:
        raise ValueError("Egy tárgyból legfeljebb 50 000 darab adható.")
    if row:
        if count:
            row["count"] = count
        else:
            rows.remove(row)
    elif count:
        if (
            len(rows) >= 16
            or len(rows) + sum(len(bundle.get(f, [])) for f in ("typed_requirements", "typed_consume_items"))
            >= 32
        ):
            raise ValueError("Megtelt a jutalom tárgylistája.")
        _, category, local, level = subject.split(".")
        bundle.setdefault("typed_items", []).append(
            {"category": int(category), "local": int(local), "level": int(level), "count": count}
        )


def insertion(flow, key, target, edge):
    """Splice one wire, or all incoming wires when dropping directly on a step."""
    edges = flow["wires"]
    if target == "start" and not edge:
        edge = ("start", "next")
    if edge:
        a, p = edge
        target = edges.get(a, {}).get(p)
        connect(flow, a, p, key)
    elif target:
        for a, p in [(a, p) for a, outputs in edges.items() for p, dest in outputs.items() if dest == target]:
            connect(flow, a, p, key)
    if target:
        for p in ports(flow["nodes"][key]):
            connect(flow, key, p, target)


def drop_action(asset, target_kind, on_wire=False):
    if asset.kind == "taskref":
        return "Feladat hozzárendelése a feltételhez" if target_kind == "condition" and not on_wire else ""
    if asset.kind == "answer":
        return "Válasz összekötése" if target_kind and target_kind != "start" and not on_wire else ""
    if asset.kind == "stack":
        return "Jutalomtárgy áthelyezése" if target_kind == "reward" and not on_wire else ""
    if asset.kind == "npc" and not on_wire:
        if target_kind == "start":
            return "Küldetésadó beállítása"
        if target_kind in ("dialogue", "text"):
            return "Beszélő beállítása"
    if asset.kind in ("npc", "enemy", "item") and target_kind == "task" and not on_wire:
        return "Feladat célpontjának beállítása"
    if asset.kind == "item" and target_kind == "reward" and not on_wire:
        return "Tárgy hozzáadása a jutalomhoz"
    if asset.kind == "phrase" and target_kind in ("dialogue", "text") and not on_wire:
        return "Szöveg átvétele"
    return "Lépés beszúrása" if target_kind or on_wire else "Új lépés elhelyezése"


def apply_drop(flow, asset, pos, target=None, edge=None):
    """Caller commits the candidate only on success. Admission changes live outside the flow."""
    ns = flow["nodes"]
    dest = ns.get(target, {}) if not edge else {}
    kind = dest.get("kind")
    if flow.get("readonly") or not drop_action(asset, kind, edge):
        raise ValueError("Ez az elem itt nem helyezhető el.")
    if asset.kind == "answer":
        connect(flow, asset.origin, asset.subject, target)
        return asset.origin
    if asset.kind == "taskref":
        dest["test"] = asset.origin
        return target
    if asset.kind == "stack":
        if target == asset.origin:
            raise ValueError("Válassz másik jutalmat.")
        change_stack(dest["bundle"], asset.subject, asset.count)
        change_stack(ns[asset.origin]["bundle"], asset.subject, -asset.count)
        return target
    if asset.kind == "npc" and kind in ("text", "dialogue"):
        dest.update(speaker=asset.name, speaker_target=asset.subject)
        dest.pop("portrait", None)
        return target
    if asset.kind == "item" and kind == "reward":
        change_stack(dest["bundle"], asset.subject, asset.count)
        return target
    if asset.kind == "phrase" and kind in ("text", "dialogue"):
        dest["line"] = asset.line
        return target
    if asset.kind in ("npc", "item", "enemy"):
        event = {"npc": "npc_interacted", "item": "item_collected", "enemy": "mob_killed"}[asset.kind]
        if kind == "task":
            if (asset.kind, dest["event"]) in (("item", "item_crafted"), ("enemy", "boss_defeated")):
                event = dest["event"]
            dest.update(event=event, subject=asset.subject)
            return target
        new_kind = "task"
        fields = {"event": event, "subject": asset.subject, "goal": asset.count}
        name = {"npc": "Beszélgetés", "item": "Gyűjtés", "enemy": "Vadászat"}[asset.kind] + " · " + asset.name
    elif asset.kind == "phrase":
        new_kind, name = "text", asset.name
        fields = {"speaker": asset.speaker or "Mesélő", "line": asset.line}
    elif asset.kind == "step":
        new_kind = asset.subject
        name = TYPES[new_kind]
        fields = defaults(new_kind)
    else:
        raise ValueError("Ismeretlen elem.")
    if len(ns) >= 512:
        raise ValueError("A gráf megtelt.")
    key = fresh_id()
    ns[key] = node(new_kind, *pos, name=name, **fields)
    insertion(flow, key, target, edge)
    return key


def reorder_answer(flow, key, source_port, target_port):
    n = flow["nodes"][key]
    labels = ports(n)
    order = labels.copy()
    order.remove(source_port)
    order.insert(labels.index(target_port), source_port)
    old = dict(zip(labels, n["answers"]))
    wires = flow["wires"].get(key, {})
    n["answers"] = [old[p] for p in order]
    flow["wires"][key] = {new: wires[p] for new, p in zip(labels, order) if p in wires}
