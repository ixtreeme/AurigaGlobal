#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""Strict questnpc/questcategory conversion; compatible with Python 2.7 and 3.

Usage: txt2json_quest.py npc|category input.txt output.json [--check]
--check compares an existing JSON file without modifying it.
"""
from __future__ import print_function

import io
import json
import os
import re
import sys


IDENTIFIER = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")
INTEGER = re.compile(r"^\+?[0-9]+$")


def convert(mode, text, source="input"):
    if mode not in ("npc", "category"):
        raise ValueError("expected npc or category mode")
    records, ids, names = [], {}, {}
    for number, line in enumerate(text.splitlines(), 1):
        line = line.strip()
        if not line or line.startswith("#") or line.startswith("//"):
            continue
        where = "%s:%d" % (source, number)
        parts = line.split()
        if len(parts) != 2:
            raise ValueError("%s: expected integer and name" % where)
        value, name = parts
        # The renewal reader accepted quoted quest names in tab-separated rows.
        if mode == "category" and name.startswith('"') and name.endswith('"'):
            name = name[1:-1]
        if not INTEGER.match(value):
            raise ValueError("%s: invalid nonnegative integer '%s'" % (where, value))
        value = int(value, 10)
        maximum = 4294967295 if mode == "npc" else 65535
        if value > maximum:
            raise ValueError("%s: integer out of range 0..%d" % (where, maximum))
        if not IDENTIFIER.match(name):
            raise ValueError("%s: expected identifier [A-Za-z_][A-Za-z0-9_]*" % where)
        if mode == "npc" and (value == 0 or name == "notarget"):
            raise ValueError("%s: 0/notarget is reserved" % where)
        if name in names:
            raise ValueError("%s: duplicate name '%s' (first at line %d)" % (where, name, names[name]))
        if mode == "npc" and value in ids:
            raise ValueError("%s: duplicate vnum %d (first at line %d)" % (where, value, ids[value]))
        names[name], ids[value] = number, number
        if mode == "npc":
            records.append({"vnum": value, "name": name})
        else:
            records.append({"quest": name, "category": value})
    return {"npcs" if mode == "npc" else "categories": records}


def canonical(doc):
    return json.dumps(doc, sort_keys=True, ensure_ascii=True)


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError("duplicate JSON field '%s'" % key)
        result[key] = value
    return result


def main(args=None):
    args = sys.argv[1:] if args is None else args
    if len(args) not in (3, 4) or (len(args) == 4 and args[3] != "--check"):
        print(__doc__, file=sys.stderr)
        return 2
    mode, source, destination = args[:3]
    try:
        if os.path.normcase(os.path.realpath(source)) == os.path.normcase(os.path.realpath(destination)):
            raise ValueError("input and output must be different files")
        with io.open(source, "r", encoding="utf-8-sig") as handle:
            doc = convert(mode, handle.read(), source)
        if len(args) == 4:
            with io.open(destination, "r", encoding="utf-8-sig") as handle:
                if canonical(json.load(handle, object_pairs_hook=unique_object)) != canonical(doc):
                    raise ValueError("%s: JSON records differ from TXT input" % destination)
        else:
            text = json.dumps(doc, indent=2, sort_keys=True, ensure_ascii=True)
            with io.open(destination, "w", encoding="utf-8", newline="\n") as handle:
                handle.write(text + u"\n")
        key, field = ("npcs", "vnum") if mode == "npc" else ("categories", "category")
        print("ok %s: %d records, %s sum %d%s" % (
            destination, len(doc[key]), field, sum(row[field] for row in doc[key]),
            " (parity verified)" if len(args) == 4 else ""))
        return 0
    except (ValueError, IOError, UnicodeError) as error:
        print("error: %s" % error, file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
