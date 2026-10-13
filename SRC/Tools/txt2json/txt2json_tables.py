#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# Converts the custom server txt tables to JSON (read by the GameServer via
# JsonFile + nlohmann). Faithful ports of the old parsers.
#
# Usage:
#   python txt2json_tables.py blend  <in.txt> <out.json>
#   python txt2json_tables.py cube   <in.txt> <out.json>
#   python txt2json_tables.py fishing <in.txt> <out.json>
#   python txt2json_tables.py gaya   <in.txt> <out.json>
#   python txt2json_tables.py guild  <in.txt> <out.json>
#
# py2.7/py3 compatible (the live host runs python2.7).

from __future__ import print_function

import collections
import io
import json
import sys

MAX_BLEND_ITEM_VALUE = 5
MAX_FISH = 37
MAX_PROB = 4
NUM_USE_RESULT_COUNT = 10
GUILD_LEVEL_MIN = 21
GUILD_LEVEL_MAX = 30          # NSBUZERANT (CommonDefines.h)

DELIM = " \t\r\n"


class ConversionError(Exception):
    pass


def ascii_lower(text):
    return "".join(chr(ord(c) + 32) if "A" <= c <= "Z" else c for c in text)


def str_to_int(text, default=0):
    number = 0
    sign = 1
    text = text.strip()
    if text.startswith("-"):
        sign = -1
        text = text[1:]
    for c in text:
        if c.isdigit():
            number = number * 10 + (ord(c) - 48)
        else:
            break
    return number * sign


def read_text(in_path, encodings=("utf-8", "cp949", "latin-1")):
    raw = open(in_path, "rb").read()

    for encoding in encodings:
        try:
            return raw.decode(encoding)
        except UnicodeDecodeError:
            continue

    return raw.decode(encodings[-1])


def write_json(out_path, doc, note=""):
    text = json.dumps(doc, indent=2, ensure_ascii=False)
    if not isinstance(text, str):
        text = text.decode("utf-8") if isinstance(text, bytes) else unicode(text)

    with io.open(out_path, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(text)
        handle.write(u"\n")

    print("ok  %s -> %s%s" % (out_path, note, ""))


def get_lines(in_path):
    return [line.rstrip("\n") for line in read_text(in_path).replace("\r\n", "\n").replace("\r", "\n").split("\n")]


# ---------------------------------------------------------------- blend

def convert_blend(in_path, out_path):
    blends = []
    current = None

    for line in get_lines(in_path):
        if line.startswith("#"):
            continue

        tokens = line.split(None)  # " \t\r\n" + any whitespace

        if not tokens:
            continue

        key = tokens[0]

        if key == "section":
            current = collections.OrderedDict()
            current["item_vnum"] = 0
            current["apply_type"] = ""
            current["apply_value"] = [0] * MAX_BLEND_ITEM_VALUE
            current["apply_duration"] = [0] * MAX_BLEND_ITEM_VALUE
        elif key in ("item_vnum", "apply_type", "apply_value", "apply_duration", "end"):
            if current is None:
                raise ConversionError("blend: %s before section" % key)

            if key == "item_vnum":
                if len(tokens) < 2:
                    raise ConversionError("blend: item_vnum without value")
                current["item_vnum"] = str_to_int(tokens[1])
            elif key == "apply_type":
                if len(tokens) < 2:
                    raise ConversionError("blend: apply_type without value")
                current["apply_type"] = tokens[1]
            elif key == "apply_value":
                if len(tokens) < 1 + MAX_BLEND_ITEM_VALUE:
                    raise ConversionError("blend: apply_value needs %d values" % MAX_BLEND_ITEM_VALUE)
                current["apply_value"] = [str_to_int(t) for t in tokens[1:1 + MAX_BLEND_ITEM_VALUE]]
            elif key == "apply_duration":
                if len(tokens) < 1 + MAX_BLEND_ITEM_VALUE:
                    raise ConversionError("blend: apply_duration needs %d values" % MAX_BLEND_ITEM_VALUE)
                current["apply_duration"] = [str_to_int(t) for t in tokens[1:1 + MAX_BLEND_ITEM_VALUE]]
            elif key == "end":
                blends.append(current)
                current = None

    write_json(out_path, collections.OrderedDict([("blends", blends)]), " (%d blend)" % len(blends))
    return 0


# ---------------------------------------------------------------- cube

def cube_check(entry):
    for npc in entry["npcs"]:
        if npc == 0:
            return False
    for item in entry["items"] + entry["rewards"]:
        if item["vnum"] == 0 or item["count"] == 0:
            return False
    return True


def convert_cube(in_path, out_path):
    cubes = []
    current = None

    for line in get_lines(in_path):
        if line.startswith("#"):
            continue

        tokens = line.split(None)

        if not tokens:
            continue

        key = tokens[0]
        arg1 = tokens[1] if len(tokens) > 1 else ""
        arg2 = tokens[2] if len(tokens) > 2 else ""

        if key == "section":
            current = collections.OrderedDict()
            current["npcs"] = []
            current["items"] = []
            current["rewards"] = []
            current["percent"] = 0
            current["category"] = "WORLDARD"          # CUBE_RENEWAL_DATA default
            current["allow_copy"] = 0
            current["gaya"] = 0
            current["gold"] = 0
        elif key == "end":
            if current is None:
                continue
            if not cube_check(current):
                print("note: cube section dropped (invalid data)")
            else:
                cubes.append(current)
            current = None
        elif current is None:
            continue
        elif key == "npc":
            current["npcs"].append(str_to_int(arg1))
        elif key == "item":
            current["items"].append(collections.OrderedDict([("vnum", str_to_int(arg1)), ("count", str_to_int(arg2))]))
        elif key == "reward":
            current["rewards"].append(collections.OrderedDict([("vnum", str_to_int(arg1)), ("count", str_to_int(arg2))]))
        elif key == "percent":
            current["percent"] = str_to_int(arg1)
        elif key == "category":
            current["category"] = arg1
        elif key == "allow_copy":
            current["allow_copy"] = str_to_int(arg1)
        elif key == "gaya":
            current["gaya"] = str_to_int(arg1)
        elif key == "gold":
            current["gold"] = str_to_int(arg1)

    write_json(out_path, collections.OrderedDict([("cubes", cubes)]), " (%d cube)" % len(cubes))
    return 0


# ---------------------------------------------------------------- fishing

def convert_fishing(in_path, out_path):
    fish = []
    idx = 0

    for line in get_lines(in_path):
        if line.startswith("#"):
            continue

        if idx == MAX_FISH:
            break

        segments = line.split("\t")

        if len(segments) < 2:
            raise ConversionError("fishing: line without tab: %s" % line[:60])

        entry = collections.OrderedDict()
        entry["name"] = ""
        entry["vnum"] = 0
        entry["dead_vnum"] = 0
        entry["grill_vnum"] = 0
        entry["prob"] = [0] * MAX_PROB
        entry["difficulty"] = 0
        entry["time_type"] = 0
        entry["length_range"] = [0] * 3
        entry["used_table"] = [0] * NUM_USE_RESULT_COUNT

        # The old parser processed only tab-terminated segments (the tail
        # after the last tab was dropped).
        col = 0
        for raw_segment in segments[:-1]:
            value = ascii_lower(raw_segment.strip())

            if not value:
                col += 1
                continue

            if col == 0:
                entry["name"] = value
            elif col == 1:
                entry["vnum"] = str_to_int(value)
            elif col == 2:
                entry["dead_vnum"] = str_to_int(value)
            elif col == 3:
                entry["grill_vnum"] = str_to_int(value)
            elif 4 <= col <= 7:
                entry["prob"][col - 4] = str_to_int(value)
            elif col == 8:
                entry["difficulty"] = str_to_int(value)
            elif col == 9:
                entry["time_type"] = str_to_int(value)
            elif 10 <= col <= 12:
                entry["length_range"][col - 10] = str_to_int(value)
            elif 13 <= col <= 22:
                entry["used_table"][col - 13] = str_to_int(value)

            col += 1

        fish.append(entry)
        idx += 1

    write_json(out_path, collections.OrderedDict([("fish", fish)]), " (%d fish)" % len(fish))
    return 0


# ---------------------------------------------------------------- gaya

def convert_gaya(in_path, out_path):
    items = []
    config = collections.OrderedDict()

    for line in get_lines(in_path):
        if line.startswith("#"):
            continue

        tokens = line.split(None)

        if not tokens:
            continue

        key = tokens[0]
        value1 = str_to_int(tokens[1]) if len(tokens) > 1 else 0
        value2 = str_to_int(tokens[2]) if len(tokens) > 2 else 0
        value3 = str_to_int(tokens[3]) if len(tokens) > 3 else 0

        if key == "ITEM":
            items.append(collections.OrderedDict([("item", value1), ("gaya", value2), ("count", value3)]))
        elif key.isupper():
            config[ascii_lower(key)] = value1

    doc = collections.OrderedDict()
    doc["items"] = items
    doc["config"] = config

    write_json(out_path, doc, " (%d item)" % len(items))
    return 0


# ---------------------------------------------------------------- guild renewal

def convert_guild(in_path, out_path):
    levels = []

    for line in get_lines(in_path):
        line = line.lstrip("\ufeff \t")

        if not line or line[0] in "#;/":
            continue

        tokens = line.split()

        if len(tokens) < 2:
            continue

        lvl = str_to_int(tokens[0])
        yang = str_to_int(tokens[1])

        if lvl < GUILD_LEVEL_MIN or lvl > GUILD_LEVEL_MAX:
            continue

        items = []

        for i in range(5):
            vnum = str_to_int(tokens[2 + i * 2]) if len(tokens) > 2 + i * 2 else 0
            count = str_to_int(tokens[3 + i * 2]) if len(tokens) > 3 + i * 2 else 0
            items.append(collections.OrderedDict([("vnum", vnum), ("count", count)]))

        levels.append(collections.OrderedDict([("level", lvl), ("yang", yang), ("items", items)]))

    if not levels:
        raise ConversionError("guild: no valid rows parsed")

    write_json(out_path, collections.OrderedDict([("levels", levels)]), " (%d level)" % len(levels))
    return 0


CONVERTERS = {
    "blend": convert_blend,
    "cube": convert_cube,
    "fishing": convert_fishing,
    "gaya": convert_gaya,
    "guild": convert_guild,
}


def main():
    if len(sys.argv) < 4:
        print("Usage: python txt2json_tables.py <%s> <in.txt> <out.json>" % "|".join(sorted(CONVERTERS)))
        return 1

    mode = sys.argv[1]
    in_path = sys.argv[2]
    out_path = sys.argv[3]

    converter = CONVERTERS.get(mode)

    if converter is None:
        print("unknown mode: %s" % mode)
        return 1

    try:
        return converter(in_path, out_path)
    except ConversionError as ex:
        print("error: %s" % ex)
        return 1


if __name__ == "__main__":
    sys.exit(main())
