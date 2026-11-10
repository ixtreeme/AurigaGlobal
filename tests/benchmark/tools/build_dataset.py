#!/usr/bin/env python3
"""Build compact benchmark datasets from the exported Auriga sources.

The benchmark deliberately consumes a small, generated snapshot instead of the
10k-row production CSV and the 63 MB SQL dump.  Everything here is derived
mechanically; no gameplay value is invented.

Output directory: build/SRC/tests/benchmark/data/generated
"""
from __future__ import annotations

import csv
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[5]          # .../build/SRC/tests/benchmark/tools -> repo root
SRC = ROOT / "build" / "SRC"
OUT = SRC / "tests" / "benchmark" / "data" / "generated"
SERVER = SRC / "SRC" / "Server"

EXCLUDED_STONES = {28104, 28112, 28100, 28108}
MAIN_CSV = ROOT / "item_proto.csv"
TERA_CSV = ROOT / "build" / "Unity63Backend" / "integration" / "tera_companions" / "item_proto.csv"
WHITELIST_H = SERVER / "GameServer" / "pet" / "mount_inventory_helper.h"
ATTR_CSVS = [
    ("normal", ROOT / "item_attr.csv"),
    ("rare", ROOT / "item_attr_rare.csv"),
    ("extra", ROOT / "item_attr_rare_8.csv"),
]
EXTRA_CSV = ROOT / "item_extra_proto.csv"
SCHEMA_SQL = SRC / "db" / "schema.sql"
EXTRACT = SRC / "tests" / "benchmark" / "data"

ITEM_COLUMNS = [
    "vnum", "name", "type", "subtype", "wearflag", "antiflag", "immuneflag",
    "refine_set", "addon_type", "limit_level", "limit_type", "limit_value",
    "value0", "value1", "value2", "value3", "value4", "value5",
    "apply0_type", "apply0_value", "apply1_type", "apply1_value",
    "apply2_type", "apply2_value", "apply3_type", "apply3_value",
    "apply4_type", "apply4_value", "source", "eligible",
]


def clean(text: str) -> str:
    text = (text or "").replace("\x00", " ").strip()
    return " ".join(text.split())


def as_int(text: str, default: int = 0) -> int:
    try:
        return int(float(str(text).strip() or default))
    except ValueError:
        return default


def read_csv(path: Path) -> list[dict[str, str]]:
    with path.open("r", newline="", encoding="utf-8", errors="replace") as handle:
        reader = csv.DictReader(handle)
        if reader.fieldnames:
            reader.fieldnames = [name.lstrip("\ufeff") for name in reader.fieldnames]
        return [row for row in reader]


def read_tsv(path: Path) -> list[list[str]]:
    with path.open("r", encoding="utf-8", errors="replace") as handle:
        return [line.rstrip("\n").split("\t") for line in handle if line.strip()]


def main_row_to_item(row: dict[str, str], source: str) -> dict[str, str]:
    limit_type = as_int(row.get("limittype0", "0"))
    limit_value = as_int(row.get("limitvalue0", "0"))
    item = {
        "vnum": str(as_int(row.get("vnum", "0"))),
        "name": clean(row.get("name", "")),
        "type": str(as_int(row.get("type", "0"))),
        "subtype": str(as_int(row.get("subtype", "0"))),
        "wearflag": str(as_int(row.get("wearflag", "0"))),
        "antiflag": str(as_int(row.get("antiflag", "0"))),
        "immuneflag": str(as_int(row.get("immuneflag", "0")) or 0),
        "refine_set": str(as_int(row.get("refine_set", "0"))),
        "addon_type": str(as_int(row.get("addon_type", "0"))),
        "limit_level": str(limit_value if limit_type == 1 else 0),
        "limit_type": str(limit_type),
        "limit_value": str(limit_value),
        "source": source,
        "eligible": "1",
    }
    for index in range(6):
        item[f"value{index}"] = str(as_int(row.get(f"value{index}", "0")))
    for index in range(5):
        item[f"apply{index}_type"] = str(as_int(row.get(f"applytype{index}", "0")))
        item[f"apply{index}_value"] = str(as_int(row.get(f"applyvalue{index}", "0")))
    return item


def parse_pvp_table(path: Path) -> list[dict[str, str]]:
    """The extracted TSV still carries the 55-column item_proto layout."""
    columns = None
    rows = []
    for fields in read_tsv(path):
        if len(fields) < 55:
            continue
        row = {
            "vnum": fields[0], "name": fields[1],
            "type": fields[12], "subtype": fields[13],
            "antiflag": fields[17], "flag": fields[18], "wearflag": fields[19],
            "immuneflag": fields[20], "refine_set": fields[23],
            "limittype0": fields[26], "limitvalue0": fields[27],
            "limittype1": fields[28], "limitvalue1": fields[29],
            "addon_type": fields[54],
        }
        for index in range(5):
            row[f"applytype{index}"] = fields[30 + index * 2]
            row[f"applyvalue{index}"] = fields[31 + index * 2]
        for index in range(6):
            row[f"value{index}"] = fields[40 + index]
        rows.append(main_row_to_item(row, path.stem.replace("item_proto_", "variant-")))
    return rows


def load_main_items() -> tuple[list[dict[str, str]], dict[int, dict[str, str]]]:
    items = []
    by_vnum: dict[int, dict[str, str]] = {}
    for row in read_csv(MAIN_CSV):
        item = main_row_to_item(row, "main")
        items.append(item)
        by_vnum[int(item["vnum"])] = item
    return items, by_vnum


def merge_variants(base: dict[int, dict[str, str]], variants: list[dict[str, str]],
                   tag: str) -> dict[int, dict[str, str]]:
    merged = {vnum: dict(item) for vnum, item in base.items()}
    for item in variants:
        item = dict(item)
        item["source"] = tag
        merged[int(item["vnum"])] = item
    return merged


def load_tera_items() -> list[dict[str, str]]:
    if not TERA_CSV.exists():
        return []
    out = []
    for row in read_csv(TERA_CSV):
        item = main_row_to_item(row, "tera")
        out.append(item)
    return out


def load_mount_whitelist() -> set[int]:
    text = WHITELIST_H.read_text(encoding="utf-8", errors="replace")
    start = text.find("mount_allowed_items")
    if start < 0:
        raise SystemExit("mount whitelist not found")
    end = text.find("};", start)
    body = re.sub(r"//[^\n]*", "", text[start:end])
    return {int(value) for value in re.findall(r"\b\d{3,7}\b", body)}


def load_shop_vnums() -> set[int]:
    vnums = set()
    for fields in read_tsv(EXTRACT / "shop_item.tsv"):
        if len(fields) >= 2:
            vnums.add(as_int(fields[1]))
    return vnums


def load_award_vnums() -> set[int]:
    # item_award dump is a single sample row; keep the parser for completeness.
    vnums = set()
    for fields in read_tsv(EXTRACT / "item_award.tsv"):
        if len(fields) >= 4:
            vnums.add(as_int(fields[3]))
    return vnums


def write_csv(name: str, columns: list[str], rows: list[dict[str, object]]) -> None:
    path = OUT / name
    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=columns, extrasaction="ignore")
        writer.writeheader()
        for row in rows:
            writer.writerow(row)
    print(f"{name:28s} {len(rows):6d} rows  {path.stat().st_size:8d} bytes")


def read_attr_apply_enum() -> list[str]:
    """Read the item_attr `apply` ENUM in declaration order.

    The enum order is the authoritative apply index the production database
    sends to the game server, so the benchmark must not invent a table.
    """
    text = SCHEMA_SQL.read_text(encoding="utf-8", errors="replace")
    match = re.search(r"CREATE TABLE IF NOT EXISTS `player`\.`item_attr` \(.*?`apply`\s+ENUM\((.*?)\)",
                      text, re.S)
    if not match:
        raise SystemExit("item_attr apply enum not found in schema.sql")
    body = match.group(1)
    names = []
    current = []
    in_quote = False
    index = 0
    while index < len(body):
        char = body[index]
        if in_quote:
            if char == "'":
                if index + 1 < len(body) and body[index + 1] == "'":
                    current.append("'")
                    index += 2
                    continue
                in_quote = False
            else:
                current.append(char)
        else:
            if char == "'":
                in_quote = True
                current = []
            elif char == ",":
                names.append("".join(current).strip())
                current = []
        index += 1
    if current or in_quote:
        names.append("".join(current).strip())
    names = [name for name in names if name]
    if not names or names[0] != "MAX_HP":
        raise SystemExit(f"unexpected item_attr apply enum: {names[:3]}")
    return names


def build() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    apply_names = read_attr_apply_enum()
    # MySQL ENUM ordinals are 1-based: `SELECT apply+0` (the exact production
    # query in ClientManagerBoot.cpp) returns 1 for the first label, matching
    # APPLY_MAX_HP = 1 in EApplyTypes.  Never emit a 0-based table here.
    write_csv("apply_types.csv", ["index", "name"],
              [{"index": i + 1, "name": name} for i, name in enumerate(apply_names)])
    main_items, main_by_vnum = load_main_items()
    tera_items = load_tera_items()
    pvp_items = []
    pvm_items = []
    for table in ("weapons", "armor", "ekszerek"):
        pvp_items += parse_pvp_table(EXTRACT / f"item_proto_pvp_{table}.tsv")
        pvm_items += parse_pvp_table(EXTRACT / f"item_proto_pvm_{table}.tsv")

    whitelist = load_mount_whitelist()
    shop = load_shop_vnums()
    award = load_award_vnums()

    variants = {
        "main": main_by_vnum,
        "pvp": merge_variants(main_by_vnum, pvp_items, "variant-pvp"),
        "pvm": merge_variants(main_by_vnum, pvm_items, "variant-pvm"),
    }

    all_items = {item["vnum"]: item for item in main_items}
    for item in tera_items:
        all_items[item["vnum"]] = item
    for vnum in whitelist:
        if str(vnum) in all_items:
            all_items[str(vnum)]["eligible"] = "1"

    # Eligibility: shop, award, TERA integration and mount whitelist.  Stones
    # are explicitly excluded from player-obtainable benchmark builds.
    for item in all_items.values():
        vnum = as_int(item["vnum"])
        item["eligible"] = "1" if (
            vnum in shop or vnum in award or vnum in whitelist or item["source"] == "tera"
        ) else "0"
        if vnum in EXCLUDED_STONES:
            item["eligible"] = "0"

    write_csv("items.csv", ITEM_COLUMNS, sorted(all_items.values(), key=lambda i: as_int(i["vnum"])))
    for tag, dataset in variants.items():
        rows = []
        for item in dataset.values():
            item = dict(item)
            vnum = as_int(item["vnum"])
            item["eligible"] = "1" if (
                vnum in shop or vnum in award or vnum in whitelist or item["source"] == "tera"
            ) else "0"
            if vnum in EXCLUDED_STONES:
                item["eligible"] = "0"
            rows.append(item)
        write_csv(f"items_{tag}.csv", ITEM_COLUMNS, sorted(rows, key=lambda i: as_int(i["vnum"])))

    # Attribute tables.  The per-slot columns encode the real legality rules
    # from the production item_attr export.
    slots = ["weapon", "body", "wrist", "foots", "neck", "head", "shield", "ear", "talisman"]
    attr_rows = []
    for set_name, path in ATTR_CSVS:
        for row in read_csv(path):
            entry = {
                "set": set_name,
                "apply": clean(row.get("apply", "")),
                "prob": as_int(row.get("prob", "0")),
            }
            for level in range(1, 6):
                entry[f"lv{level}"] = as_int(row.get(f"lv{level}", "0"))
            for slot in slots:
                entry[slot] = as_int(row.get(slot, "0"))
            if sum(entry[s] for s in slots) > 0:
                attr_rows.append(entry)
    write_csv("attributes.csv", ["set", "apply", "prob"] + [f"lv{i}" for i in range(1, 6)] + slots,
              attr_rows)

    extra_rows = []
    for row in read_csv(EXTRA_CSV):
        entry = {
            "vnum": as_int(row.get("vnum", "0")),
            "rarity": as_int(row.get("rarity", "0")),
        }
        for index in range(2):
            entry[f"bonus{index}_type"] = as_int(row.get(f"bonus_type{index}", "0"))
            entry[f"bonus{index}_value"] = as_int(row.get(f"bonus_value{index}", "0"))
        extra_rows.append(entry)
    write_csv("extra_proto.csv", ["vnum", "rarity", "bonus0_type", "bonus0_value",
                                  "bonus1_type", "bonus1_value"], extra_rows)

    # Mount catalogue: whitelist rows resolved against main + TERA prototypes.
    mount_rows = []
    for vnum in sorted(whitelist):
        item = all_items.get(str(vnum))
        mount_rows.append({
            "vnum": vnum,
            "name": item["name"] if item else "",
            "type": item["type"] if item else "",
            "subtype": item["subtype"] if item else "",
            "source": ("tera" if vnum >= 630000 else ("existing" if vnum >= 611500 else "legacy")),
        })
    write_csv("mounts.csv", ["vnum", "name", "type", "subtype", "source"], mount_rows)

    # Runes: type 28, subtype 10..16.
    rune_rows = []
    for item in all_items.values():
        if as_int(item["type"]) == 28 and 10 <= as_int(item["subtype"]) <= 16:
            rune_rows.append({
                "vnum": item["vnum"], "subtype": item["subtype"],
                "duration": item["value0"], "name": item["name"],
            })
    write_csv("runes.csv", ["vnum", "subtype", "duration", "name"],
              sorted(rune_rows, key=lambda r: as_int(r["vnum"])))

    # Talismans 10750..10850, including the proposed correction for +62..+100.
    talisman_rows = []
    for vnum in range(10750, 10851):
        item = main_by_vnum.get(vnum)
        if not item:
            continue
        apply_type = as_int(item["apply0_type"])
        value = as_int(item["apply0_value"])
        level = vnum - 10750
        talisman_rows.append({
            "vnum": vnum, "level": level, "name": item["name"],
            "apply_as_is": apply_type, "value_as_is": value,
            "apply_proposed": 17 if level >= 62 else apply_type,
            "value_proposed": level if level >= 62 else value,
        })
    write_csv("talismans.csv", ["vnum", "level", "name", "apply_as_is", "value_as_is",
                                "apply_proposed", "value_proposed"], talisman_rows)

    # Skill catalogue (metadata only; no SkillSystem linkage in this build).
    skill_rows = []
    for fields in read_tsv(EXTRACT / "skill_proto.tsv"):
        if len(fields) < 7:
            continue
        skill_rows.append({
            "vnum": as_int(fields[0]), "name": clean(fields[1]),
            "type": as_int(fields[2]), "level_step": as_int(fields[3]),
            "max_level": as_int(fields[4]), "level_limit": as_int(fields[5]),
            "point_on": clean(fields[6]),
        })
    write_csv("skills.csv", ["vnum", "name", "type", "level_step", "max_level",
                             "level_limit", "point_on"], skill_rows)

    # Eligibility audit table.
    write_csv("eligibility.csv", ["vnum", "in_shop", "in_award", "in_mount_whitelist",
                                  "tera", "excluded_stone"],
              [{
                  "vnum": as_int(item["vnum"]),
                  "in_shop": int(as_int(item["vnum"]) in shop),
                  "in_award": int(as_int(item["vnum"]) in award),
                  "in_mount_whitelist": int(as_int(item["vnum"]) in whitelist),
                  "tera": int(item["source"] == "tera"),
                  "excluded_stone": int(as_int(item["vnum"]) in EXCLUDED_STONES),
              } for item in sorted(all_items.values(), key=lambda i: as_int(i["vnum"]))])

    print("\nMount catalogue:", len(mount_rows), "entries (", len(whitelist), "whitelist vnums )")
    print("TERA rows:", len(tera_items), " PvP overrides:", len(pvp_items), " PvM overrides:", len(pvm_items))


if __name__ == "__main__":
    try:
        build()
    except FileNotFoundError as exc:
        print("missing input:", exc, file=sys.stderr)
        raise
