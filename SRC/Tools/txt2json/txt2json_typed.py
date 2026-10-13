#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# Second step of the group/token table migration: converts the structural
# JSON (txt2json_group.py output) into typed JSON with named fields.
#
# Usage:
#   python txt2json_typed.py <group|group_group|mob_drop|drop_item_group|special_item_group|common_drop_item> <in.json> <out.json>
#
# Differences from the structural file:
#   - "//" starts a comment to the end of line: it is cut off (and the token
#     it was attached to keeps its data part, e.g. "400//metin szilank" -> 400,
#     "5//Az elet vize" -> 5; a token that is only a comment disappears),
#   - fields are typed: numbers as JSON numbers, names/vnums as strings,
#   - every token group has a fixed schema (see the GameServer loaders in
#     entity/mob_manager.cpp and item/item_manager_read_tables.cpp).
#
# py2.7/py3 compatible (the live host runs python2.7).

from __future__ import print_function

import collections
import io
import json
import sys


class ConversionError(Exception):
    pass


def strip_comment(token):
    value = token.split("//", 1)[0].strip()
    return value


def to_int(text, default=0):
    if isinstance(text, bool):
        return default
    if isinstance(text, int):
        return text
    if isinstance(text, float):
        return int(text)

    text = text.strip()

    if not text:
        return default

    sign = 1
    if text.startswith("-"):
        sign = -1
        text = text[1:]

    number = 0
    digits = 0

    for c in text:
        if c.isdigit():
            number = number * 10 + (ord(c) - 48)
            digits += 1
        else:
            break

    return sign * number if digits else default


def to_float(text, default=0.0):
    if isinstance(text, bool):
        return default
    if isinstance(text, (int, float)):
        return float(text)

    text = text.strip()

    if not text:
        return default

    try:
        return float(text)
    except ValueError:
        return default


def read_tokens(group, key):
    values = group.get("tokens", {}).get(key)

    if not values:
        return None

    cleaned = []

    for token in values:
        cleaned.append(strip_comment(token))

    return cleaned


def read_sequence(group, start=1, end=255):
    """Mirrors the old k=1..N loop: stops at the first missing index."""
    result = []
    tokens = group.get("tokens", {})

    for k in range(start, end + 1):
        key = str(k)

        if key not in tokens:
            break

        values = [strip_comment(t) for t in tokens[key]]
        result.append(values)

    return result


def group_name(group):
    return group.get("name", "")


def convert_group(in_path, out_path):
    doc = json.load(io.open(in_path, "r", encoding="utf-8"))
    out = []

    for group in doc.get("groups", []):
        vnum = read_tokens(group, "vnum")

        if not vnum:
            print("note: group %s: no vnum, skipped" % group_name(group))
            continue

        leader = read_tokens(group, "leader")

        if not leader or len(leader) < 2:
            print("note: group %s: no leader, skipped" % group_name(group))
            continue

        members = []

        for values in read_sequence(group):
            if len(values) < 2:
                raise ConversionError("%s: member without vnum" % group_name(group))
            members.append(collections.OrderedDict([
                ("name", values[0]),
                ("vnum", to_int(values[1])),
            ]))

        out.append(collections.OrderedDict([
            ("name", group_name(group)),
            ("vnum", to_int(vnum[0])),
            ("leader", collections.OrderedDict([("name", leader[0]), ("vnum", to_int(leader[1]))])),
            ("members", members),
        ]))

    write_json(out_path, collections.OrderedDict([("groups", out)]), " (%d group)" % len(out))
    return 0


def convert_group_group(in_path, out_path, reference_path=None):
    doc = json.load(io.open(in_path, "r", encoding="utf-8"))

    # The members reference group vnums (group.json). With the reference file
    # the referenced group name is written next to the vnum so the file is
    # readable; the loader only uses vnum/prob.
    names_by_vnum = {}

    if reference_path:
        reference = json.load(io.open(reference_path, "r", encoding="utf-8"))

        for group in reference.get("groups", []):
            names_by_vnum[to_int(group.get("vnum", 0))] = group.get("name", "")

    out = []
    unresolved = 0

    for group in doc.get("groups", []):
        vnum = read_tokens(group, "vnum")

        if not vnum:
            print("note: group_group %s: no vnum, skipped" % group_name(group))
            continue

        members = []

        for values in read_sequence(group):
            member_vnum = to_int(values[0])
            member = collections.OrderedDict([("vnum", member_vnum)])

            if reference_path:
                member_name = names_by_vnum.get(member_vnum)

                if member_name is None:
                    unresolved += 1
                    member_name = "?%d" % member_vnum if member_vnum else "?"

                member["group"] = member_name

            member["prob"] = to_int(values[1], 1) if len(values) > 1 else 1
            members.append(member)

        out.append(collections.OrderedDict([
            ("name", group_name(group)),
            ("vnum", to_int(vnum[0])),
            ("members", members),
        ]))

    if reference_path and unresolved:
        print("note: %d member vnum not found in the reference group table" % unresolved)

    write_json(out_path, collections.OrderedDict([("groups", out)]), " (%d group)" % len(out))
    return 0


def convert_mob_drop(in_path, out_path):
    doc = json.load(io.open(in_path, "r", encoding="utf-8"))
    out = []

    for group in doc.get("groups", []):
        name = group_name(group)

        if name.lower().startswith("kr_"):
            continue

        mob = read_tokens(group, "mob")
        type_tokens = read_tokens(group, "type")

        if mob is None:
            raise ConversionError("%s: no mob" % name)
        if type_tokens is None:
            raise ConversionError("%s: no type" % name)

        drop_type = type_tokens[0]
        entry = collections.OrderedDict([("name", name), ("mob", to_int(mob[0])), ("type", drop_type)])

        if drop_type == "kill":
            kill_drop = read_tokens(group, "kill_drop")

            if kill_drop is None:
                raise ConversionError("%s: kill without kill_drop" % name)

            entry["kill_drop"] = to_int(kill_drop[0])
        elif drop_type == "drop":
            pass
        elif drop_type == "limit":
            level_limit = read_tokens(group, "level_limit")

            if level_limit is None:
                raise ConversionError("%s: limit without level_limit" % name)

            entry["level_limit"] = to_int(level_limit[0])
        elif drop_type == "thiefgloves":
            pass
        else:
            raise ConversionError("%s: invalid type %s" % (name, drop_type))

        items = []

        for values in read_sequence(group):
            if len(values) < 2:
                raise ConversionError("%s: drop item without count" % name)

            item = collections.OrderedDict([("item", values[0])])
            item["count"] = to_int(values[1])

            if drop_type == "kill":
                if len(values) < 3:
                    raise ConversionError("%s: kill item without chance" % name)
                item["chance"] = to_int(values[2])
                item["rare"] = to_int(values[3]) if len(values) > 3 else 0
                item["rare"] = 0 if item["rare"] < 0 else (100 if item["rare"] > 100 else item["rare"])
            else:
                if len(values) < 3:
                    raise ConversionError("%s: drop item without chance" % name)
                item["chance"] = to_float(values[2])

            items.append(item)

        entry["items"] = items
        out.append(entry)

    write_json(out_path, collections.OrderedDict([("groups", out)]), " (%d group)" % len(out))
    return 0


def convert_drop_item_group(in_path, out_path):
    doc = json.load(io.open(in_path, "r", encoding="utf-8"))
    out = []

    for group in doc.get("groups", []):
        name = group_name(group)
        vnum = read_tokens(group, "vnum")
        mob = read_tokens(group, "mob")

        if vnum is None:
            raise ConversionError("%s: no vnum" % name)
        if mob is None:
            raise ConversionError("%s: no mob" % name)

        items = []

        for values in read_sequence(group):
            if len(values) < 2:
                raise ConversionError("%s: item without chance" % name)

            item = collections.OrderedDict([("item", values[0])])
            item["chance"] = to_float(values[1])
            item["count"] = to_int(values[2], 1) if len(values) > 2 else 1
            items.append(item)

        out.append(collections.OrderedDict([
            ("name", name),
            ("vnum", to_int(vnum[0])),
            ("mob", to_int(mob[0])),
            ("items", items),
        ]))

    write_json(out_path, collections.OrderedDict([("groups", out)]), " (%d group)" % len(out))
    return 0


def convert_special_item_group(in_path, out_path):
    doc = json.load(io.open(in_path, "r", encoding="utf-8"))
    out = []

    for group in doc.get("groups", []):
        name = group_name(group)
        vnum = read_tokens(group, "vnum")

        if vnum is None:
            raise ConversionError("%s: no vnum" % name)

        entry = collections.OrderedDict([("name", name), ("vnum", to_int(vnum[0]))])

        type_tokens = read_tokens(group, "type")

        if type_tokens is not None:
            stype = type_tokens[0].lower()

            if stype not in ("pct", "quest", "special", "attr"):
                stype = None

            if stype is not None:
                entry["type"] = stype

        if entry.get("type") == "attr":
            attrs = []

            for values in read_sequence(group):
                if len(values) < 2:
                    raise ConversionError("%s: attr without value" % name)
                attrs.append(collections.OrderedDict([("apply", values[0]), ("value", to_int(values[1]))]))

            entry["attrs"] = attrs

            effect = read_tokens(group, "effect")

            if effect is not None:
                entry["effect"] = effect[0]

            if not attrs:
                print("note: special_item_group %s: attr group without attrs" % name)
        else:
            items = []

            for values in read_sequence(group):
                if len(values) < 2:
                    raise ConversionError("%s: item without count" % name)

                item = collections.OrderedDict([("item", values[0])])
                item["count"] = to_int(values[1])
                item["prob"] = to_int(values[2]) if len(values) > 2 else 0

                if len(values) > 3:
                    item["rare"] = to_int(values[3])

                items.append(item)

            entry["items"] = items

        out.append(entry)

    write_json(out_path, collections.OrderedDict([("groups", out)]), " (%d group)" % len(out))
    return 0


def convert_common_drop_item(in_path, out_path):
    doc = json.load(io.open(in_path, "r", encoding="utf-8"))
    out = []

    for group in doc.get("groups", []):
        name = group_name(group)
        rank = read_tokens(group, "rank")

        if rank is None:
            raise ConversionError("%s: no rank" % name)

        items = []

        for values in read_sequence(group, 1, 1023):
            if len(values) < 4:
                raise ConversionError("%s: common drop item without value" % name)

            item = collections.OrderedDict([
                ("level_start", to_int(values[0])),
                ("level_end", to_int(values[1])),
                ("chance", to_float(values[2])),
                ("item", values[3]),
            ])

            items.append(item)

        out.append(collections.OrderedDict([("name", name), ("rank", to_int(rank[0])), ("items", items)]))

    write_json(out_path, collections.OrderedDict([("groups", out)]), " (%d group)" % len(out))
    return 0


def convert_named_rows(in_path, out_path):
    """Structural rows/columns tree -> row objects with named columns."""
    doc = json.load(io.open(in_path, "r", encoding="utf-8"))

    def walk(node):
        out = collections.OrderedDict()

        if "name" in node:
            out["name"] = node["name"]

        columns = node.get("columns", [])

        if columns:
            out["columns"] = columns

        rows = node.get("tokens")

        if rows:
            named = collections.OrderedDict()

            for key, values in rows.items():
                if columns and len(values) != len(columns):
                    raise ConversionError("row %s: %d values, %d columns" % (key, len(values), len(columns)))

                cells = collections.OrderedDict()

                for column, value in zip(columns, values):
                    cells[column] = strip_comment(value)

                named[key] = cells

            out["rows"] = named

        groups = node.get("groups")

        if groups:
            out["groups"] = [walk(child) for child in groups]

        return out

    result = walk(doc)
    write_json(out_path, result, " (named rows)")
    return 0


def parse_time(text):
    """The old read_line m/h/s parser (seconds)."""
    time = 0
    tmp = 0

    for c in text:
        if c == "h":
            time += tmp * 3600
            tmp = 0
        elif c == "m":
            time += tmp * 60
            tmp = 0
        elif c == "s":
            time += tmp
            tmp = 0
        elif c.isdigit():
            tmp = tmp * 10 + (ord(c) - 48)

    return time


REGEN_TYPES = {
    "m": "mob",
    "g": "group",
    "ga": "group_aggressive",
    "e": "exception",
    "r": "group_group",
    "s": "anywhere",
}


def convert_regen(in_path, out_path):
    doc = json.load(io.open(in_path, "r", encoding="utf-8"))
    out = []

    for entry in doc.get("regens", []):
        token = entry.get("type", "")

        if not token or token[0] not in "mgers":
            raise ConversionError("unknown regen type %s" % token)

        semantic = REGEN_TYPES.get(token[:2]) if token[:2] == "ga" else REGEN_TYPES.get(token[0])

        if semantic is None:
            raise ConversionError("unknown regen type %s" % token)

        out.append(collections.OrderedDict([
            ("type", semantic),
            ("x", to_int(entry.get("x", "0"))),
            ("y", to_int(entry.get("y", "0"))),
            ("range_x", to_int(entry.get("range_x", "0"))),
            ("range_y", to_int(entry.get("range_y", "0"))),
            ("z", to_int(entry.get("z", "0"))),
            ("dir", to_int(entry.get("dir", "0"))),
            ("time", parse_time(entry.get("time", "0"))),
            ("percent", to_int(entry.get("percent", "0"))),
            ("count", to_int(entry.get("count", "0"))),
            ("vnum", to_int(entry.get("vnum", "0"))),
        ]))

    write_json(out_path, collections.OrderedDict([("regens", out)]), " (%d regen)" % len(out))
    return 0


def write_json(out_path, doc, note=""):
    text = json.dumps(doc, indent=2, ensure_ascii=False)
    if not isinstance(text, str):
        text = text.decode("utf-8") if isinstance(text, bytes) else unicode(text)

    with io.open(out_path, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(text)
        handle.write(u"\n")

    print("ok  %s -> %s%s" % (out_path, note, ""))


CONVERTERS = {
    "group": convert_group,
    "group_group": convert_group_group,
    "mob_drop": convert_mob_drop,
    "drop_item_group": convert_drop_item_group,
    "special_item_group": convert_special_item_group,
    "common_drop_item": convert_common_drop_item,
    "named_rows": convert_named_rows,
    "regen": convert_regen,
}


def main():
    if len(sys.argv) < 4:
        print("Usage: python txt2json_typed.py <%s> <in.json> <out.json> [reference.json]" % "|".join(sorted(CONVERTERS)))
        print("  group_group: the optional reference is group.json (member names).")
        return 1

    mode = sys.argv[1]
    converter = CONVERTERS.get(mode)

    if converter is None:
        print("unknown mode: %s" % mode)
        return 1

    try:
        if mode == "group_group" and len(sys.argv) > 4:
            return converter(sys.argv[2], sys.argv[3], sys.argv[4])

        return converter(sys.argv[2], sys.argv[3])
    except ConversionError as ex:
        print("error: %s" % ex)
        return 1


if __name__ == "__main__":
    sys.exit(main())
