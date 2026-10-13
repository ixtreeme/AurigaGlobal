#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# Converts the map metadata txt files to JSON (GameServer/world/sectree_manager.cpp).
#
# Usage:
#   python txt2json_map.py index    <in.txt> <out.json>   # map/index
#   python txt2json_map.py setting  <in.txt> <out.json>   # <map>/Setting.txt
#   python txt2json_map.py town     <in.txt> <out.json>   # <map>/Town.txt
#   python txt2json_map.py dungeon  <in.txt> <out.json>   # <map>/dungeon.txt
#
# py2.7/py3 compatible (the live host runs python2.7).

from __future__ import print_function

import collections
import io
import json
import sys


def read_text(in_path):
    raw = open(in_path, "rb").read()

    try:
        return raw.decode("utf-8")
    except UnicodeDecodeError:
        return raw.decode("latin-1")


def write_json(out_path, doc):
    text = json.dumps(doc, indent=2, ensure_ascii=False)
    if not isinstance(text, str):
        text = text.decode("utf-8") if isinstance(text, bytes) else unicode(text)

    with io.open(out_path, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(text)
        handle.write(u"\n")

    print("ok  %s" % out_path)


def convert_index(in_path, out_path):
    maps = []

    for line in read_text(in_path).splitlines():
        line = line.strip()

        if not line or line.startswith("//") or line.startswith("#"):
            continue

        parts = line.split()

        if len(parts) < 2:
            continue

        maps.append(collections.OrderedDict([("index", int(parts[0])), ("name", parts[1])]))

    write_json(out_path, collections.OrderedDict([("maps", maps)]))
    return 0


SETTING_KEYS = (
    ("scripttype", "script_type", "str"),
    ("cellscale", "cell_scale", "int"),
    ("heightscale", "height_scale", "float"),
    ("viewradius", "view_radius", "int"),
    ("textureset", "texture_set", "str"),
    ("environment", "environment", "str"),
)


def convert_setting(in_path, out_path):
    doc = collections.OrderedDict()

    for line in read_text(in_path).splitlines():
        parts = line.split()

        if not parts:
            continue

        key = parts[0].lower()

        if key == "mapsize" and len(parts) >= 3:
            doc["map_size"] = [int(parts[1]), int(parts[2])]
        elif key == "baseposition" and len(parts) >= 3:
            doc["base_position"] = [int(parts[1]), int(parts[2])]
        else:
            for source, target, kind in SETTING_KEYS:
                if key == source and len(parts) >= 2:
                    if kind == "int":
                        doc[target] = int(parts[1])
                    elif kind == "float":
                        doc[target] = float(parts[1])
                    else:
                        doc[target] = parts[1]

    write_json(out_path, doc)
    return 0


def convert_town(in_path, out_path):
    values = []

    for line in read_text(in_path).splitlines():
        for part in line.split():
            try:
                values.append(int(part))
            except ValueError:
                pass

    doc = collections.OrderedDict()

    if len(values) >= 2:
        doc["spawn"] = values[0:2]

    if len(values) >= 8:
        doc["empire_spawn"] = [values[2:4], values[4:6], values[6:8]]

    write_json(out_path, doc)
    return 0


def convert_dungeon(in_path, out_path):
    positions = []

    for line in read_text(in_path).splitlines():
        line = line.strip()

        if not line or line.startswith("#") or line.startswith("//"):
            continue

        parts = line.split()

        if len(parts) < 6:
            continue

        positions.append(collections.OrderedDict([
            ("name", parts[0]),
            ("x", int(parts[1])),
            ("y", int(parts[2])),
            ("sx", int(parts[3])),
            ("sy", int(parts[4])),
            ("dir", int(parts[5])),
        ]))

    write_json(out_path, collections.OrderedDict([("positions", positions)]))
    return 0


CONVERTERS = {
    "index": convert_index,
    "setting": convert_setting,
    "town": convert_town,
    "dungeon": convert_dungeon,
}


def main():
    if len(sys.argv) < 4:
        print("Usage: python txt2json_map.py <%s> <in.txt> <out.json>" % "|".join(sorted(CONVERTERS)))
        return 1

    converter = CONVERTERS.get(sys.argv[1])

    if converter is None:
        print("unknown mode: %s" % sys.argv[1])
        return 1

    return converter(sys.argv[2], sys.argv[3])


if __name__ == "__main__":
    sys.exit(main())
