import json
import os
from pathlib import Path
from functools import lru_cache
from unicodedata import combining, normalize


def folded(value):
    return "".join(c for c in normalize("NFKD", value.casefold()) if not combining(c))


def make_index(names, catalog=None, actors=None):
    mobs = {key: row["name"] for key, row in names["mobs"].items() if row.get("name")}
    items = {}
    item_profiles, levels = {}, {}
    if catalog:
        columns = catalog["columns"]["item_base"]
        for values in catalog["tables"]["item_base"]:
            row = dict(zip(columns, values))
            label = names["items"].get(row["name_key"])
            if str(row["retired"]) == "0":
                key = f'{row["category_id"]}.{row["local_id"]}'
                items[key] = label or "Névtelen tárgy"
                if "refinement_profile_id" in row:
                    item_profiles[key] = str(row["refinement_profile_id"])
        for values in catalog["tables"].get("refinement_level", []):
            row = dict(zip(catalog["columns"]["refinement_level"], values))
            levels.setdefault(str(row["profile_id"]), []).append(int(row["level"]))
    index = {"mob": mobs, "item": items}
    if levels and item_profiles:
        index.update(item_profiles=item_profiles,levels=levels)
    if actors is not None:
        live = {k: (v.get("name") or mobs.get(k, "")).strip() for k,v in actors.items()}
        index["mob"] = {k:v for k,v in live.items() if v}
        index["npc"] = {k:live[k] for k,v in actors.items() if v["type"] == 1 and live[k]}
        index["enemy"] = {k:live[k] for k,v in actors.items() if v["type"] in (0,2) and live[k]}
    return index


@lru_cache(maxsize=1)
def target_names():
    path = Path(os.environ.get("AURIGA_MISSION_TARGETS") or Path(__file__).with_name("mission_targets.json"))
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
