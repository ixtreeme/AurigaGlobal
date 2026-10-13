#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# Converts a regen txt file (map/regen.txt, npc.txt, boss.txt, stone.txt,
# dungeon regen files) to the JSON format read by GameServer/world/regen.cpp.
#
# Usage:
#   python txt2json_regen.py <in.txt> <out.json>
#
# Faithful port of the old get_word/read_line token stream:
#   - words are whitespace separated, "//" starts a comment to the end of line,
#   - fields in order: type x y range_x range_y z dir time percent count vnum
#     ("e"/exception entries end after z; anything after that starts the next
#      entry, exactly like the old parser),
#   - time is kept raw ("10s", "1h30m"), percent is kept raw (read but unused),
#   - an entry cut off by EOF is dropped.
#
# py2.7/py3 compatible (the live host runs python2.7).

from __future__ import print_function

import collections
import io
import json
import sys

MODES = ("type", "x", "y", "range_x", "range_y", "z", "dir", "time", "percent", "count", "vnum")
WHITESPACE = " \t\r\n"


class ConversionError(Exception):
    pass


def read_text(in_path):
    raw = open(in_path, "rb").read()

    for encoding in ("utf-8", "latin-1"):
        try:
            return raw.decode(encoding)
        except UnicodeDecodeError:
            continue

    return raw.decode("latin-1")


def tokenize(text):
    tokens = []
    i = 0
    length = len(text)

    while i < length:
        c = text[i]

        if c in WHITESPACE:
            i += 1
            continue

        if text.startswith("//", i):
            end = text.find("\n", i)
            i = length if end < 0 else end + 1
            continue

        if c == '"':
            end = text.find('"', i + 1)
            if end < 0:
                break
            tokens.append(text[i + 1:end])
            i = end + 1
            continue

        end = i
        while end < length and text[end] not in WHITESPACE:
            end += 1
        tokens.append(text[i:end])
        i = end

    return tokens


def str_to_int(text):
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


def consume_entry(tokens, index):
    """Returns (entry, next_index) or (None, index) on EOF mid-entry."""
    entry = collections.OrderedDict()

    mode = 0
    while index < len(tokens):
        token = tokens[index]
        index += 1

        name = MODES[mode]

        if mode == 0:
            if not token or token[0] not in "mgers":
                raise ConversionError("unknown regen type '%s'" % token)
            entry[name] = token
        elif mode == 7 or mode == 8:
            entry[name] = token
        else:
            entry[name] = str_to_int(token)

        if mode == 5 and entry.get("type", "m")[0] == "e":
            return entry, index

        if mode == 10:
            return entry, index

        mode += 1

    return None, index


def convert(in_path, out_path):
    tokens = tokenize(read_text(in_path))
    regens = []

    index = 0
    while index < len(tokens):
        entry, index = consume_entry(tokens, index)
        if entry is None:
            print("note: %s: entry cut off at EOF dropped" % in_path)
            break
        regens.append(entry)

    text = json.dumps(collections.OrderedDict([("regens", regens)]), indent=2, ensure_ascii=False)
    if not isinstance(text, str):
        text = text.decode("utf-8") if isinstance(text, bytes) else unicode(text)

    with io.open(out_path, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(text)
        handle.write(u"\n")

    print("ok  %s -> %s (%d regen)" % (in_path, out_path, len(regens)))
    return 0


def main():
    if len(sys.argv) < 3:
        print("Usage: python txt2json_regen.py <in.txt> <out.json>")
        return 1

    try:
        return convert(sys.argv[1], sys.argv[2])
    except ConversionError as ex:
        print("error: %s: %s" % (sys.argv[1], ex))
        return 1


if __name__ == "__main__":
    sys.exit(main())
