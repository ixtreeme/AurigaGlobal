import json
from pathlib import Path
from functools import lru_cache
from unicodedata import combining, normalize


def folded(value):
    return "".join(c for c in normalize("NFKD", value.casefold()) if not combining(c))


def make_index(names, catalog=None):
    mobs = {key: row["name"] for key, row in names["mobs"].items() if row.get("name")}
    items = {}
    if catalog:
        columns = catalog["columns"]["item_base"]
        for values in catalog["tables"]["item_base"]:
            row = dict(zip(columns, values))
            label = names["items"].get(row["name_key"])
            if label and str(row["retired"]) == "0":
                items[f'{row["category_id"]}.{row["local_id"]}'] = label
    return {"mob": mobs, "item": items}


@lru_cache(maxsize=1)
def target_names():
    path = Path(__file__).with_name("mission_targets.json")
    if path.is_file():
        return json.loads(path.read_text(encoding="utf-8"))
    source = Path(__file__).resolve().parents[3] / "config/server_manager/content_names_hu.json"
    return make_index(json.loads(source.read_text(encoding="utf-8"))) if source.is_file() else {"mob": {}, "item": {}}


def display(subject):
    if subject == "*":
        return "Bármelyik"
    kind, _, key = subject.partition(".")
    if kind == "item":
        base, _, level = key.rpartition(".")
        name = target_names()["item"].get(base)
        if name:
            return name + (" +" + level if level != "0" else "")
    elif kind in ("mob", "npc"):
        return target_names()["mob"].get(key, subject)
    return subject
