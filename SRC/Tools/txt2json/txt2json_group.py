#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# First step of the group/token table migration: converts the old txt tables
# to a structural JSON (token arrays). The output is fed to txt2json_typed.py,
# which produces the typed JSON the GameServer reads.
#
# Usage:
#   python txt2json_group.py tokens <in.txt> <out.json>
#   python txt2json_group.py rows   <in.txt> <out.json>
#
#   tokens : CTextFileLoader grammar (group.txt, mob_drop_item.txt, ...)
#   rows   : CGroupTextParseTreeLoader grammar ("#--#" column header,
#            dragon_soul_table.txt, shop_table_ex.txt)
#
# The parser is a faithful port of the old C++ loaders:
#   - " \t" delimiters, "#" comment lines (except "#--#"), quoted tokens,
#   - "//" is part of the token (kept verbatim),
#   - keys/group names are lowercased, duplicate keys keep the first value,
#   - a token line without a value stops the current group (same cascade as
#     the old recursive parser).
#
# py2.7/py3 compatible (the live host runs python2.7).

from __future__ import print_function

import collections
import io
import json
import sys

DELIM = " \t"


class ConversionError(Exception):
    pass


def ascii_lower(text):
    return "".join(chr(ord(c) + 32) if "A" <= c <= "Z" else c for c in text)


def find_not_of(line, pos):
    length = len(line)

    while pos < length:
        if line[pos] not in DELIM:
            return pos
        pos += 1

    return -1


def find_of(line, pos):
    length = len(line)

    while pos < length:
        if line[pos] in DELIM:
            return pos
        pos += 1

    return -1


def split_line(line):
    """Port of CMemoryTextFileLoader::SplitLine; None when the line is skipped."""
    tokens = []
    base = 0

    while True:
        begin = find_not_of(line, base)
        if begin < 0:
            return None

        if line[begin] == "#" and line[begin:begin + 4] != "#--#":
            return None

        if line[begin] == '"':
            quoted_begin = begin + 1
            end = line.find('"', quoted_begin)
            if end < 0:
                return None
            base = end + 1
            tokens.append(line[quoted_begin:end])
        else:
            end = find_of(line, begin)
            if end < 0:
                tokens.append(line[begin:])
                return tokens
            base = end
            tokens.append(line[begin:end])

        if find_not_of(line, base) < 0:
            break

    return tokens


def insert_first(node, key, name, values):
    tokens = node.get(key)
    if tokens is None:
        tokens = collections.OrderedDict()
        node[key] = tokens
    if name not in tokens:
        tokens[name] = list(values)


def parse_group_tokens(lines, idx, node, filename):
    while idx < len(lines):
        tokens = split_line(lines[idx])

        if tokens is None:
            idx += 1
            continue

        tokens[0] = ascii_lower(tokens[0])
        head = tokens[0]

        if head[0] == "{":
            idx += 1
            continue

        if head[0] == "}":
            return idx

        if head == "group":
            if len(tokens) != 2:
                raise ConversionError("%s:%d: invalid group syntax (no space in name)" % (filename, idx + 1))

            child = collections.OrderedDict()
            child["name"] = ascii_lower(tokens[1])
            node.setdefault("groups", []).append(child)

            idx = parse_group_tokens(lines, idx + 1, child, filename)
            idx += 1
            continue

        if head == "list":
            if len(tokens) != 2:
                raise ConversionError("%s:%d: list without a name" % (filename, idx + 1))

            name = ascii_lower(tokens[1])
            values = []
            idx += 1

            while idx < len(lines):
                sub = split_line(lines[idx])
                if sub is None:
                    idx += 1
                    continue
                if sub[0][0] == "{":
                    idx += 1
                    continue
                if sub[0][0] == "}":
                    break
                values.extend(sub)
                idx += 1

            insert_first(node, "tokens", name, values)
            idx += 1
            continue

        if len(tokens) == 1:
            return idx

        insert_first(node, "tokens", tokens[0], tokens[1:])
        idx += 1

    return idx


def parse_group_rows(lines, idx, node, filename, child_names):
    while idx < len(lines):
        tokens = split_line(lines[idx])

        if tokens is None:
            idx += 1
            continue

        tokens[0] = ascii_lower(tokens[0])
        head = tokens[0]

        if head[0] == "{":
            idx += 1
            continue

        if head[0] == "}":
            return idx

        if head == "group":
            if len(tokens) != 2:
                raise ConversionError("%s:%d: invalid group syntax (no space in name)" % (filename, idx + 1))

            name = ascii_lower(tokens[1])
            child = collections.OrderedDict()
            child["name"] = name

            idx = parse_group_rows(lines, idx + 1, child, filename, set())
            idx += 1

            if name not in child_names:
                child_names.add(name)
                node.setdefault("groups", []).append(child)
            continue

        if head == "#--#":
            columns = node.setdefault("columns", [])
            for column in tokens[1:]:
                column = ascii_lower(column)
                if column not in columns:
                    columns.append(column)
            idx += 1
            continue

        if len(tokens) == 1:
            return idx

        insert_first(node, "tokens", tokens[0], tokens[1:])
        idx += 1

    return idx


def strip_empty(node):
    for key in ("tokens", "groups", "columns"):
        if key in node and not node[key]:
            del node[key]
    for child in node.get("groups", []):
        strip_empty(child)
    return node


def read_lines(in_path):
    raw = open(in_path, "rb").read()

    try:
        text = raw.decode("utf-8")
    except UnicodeDecodeError:
        # Some legacy tables (group.txt, dragon_soul_table.txt) are not UTF-8.
        # latin-1 maps every byte to one code point, so the byte content is
        # preserved 1:1 (the old loader worked on raw bytes as well) and the
        # output is valid UTF-8.
        text = raw.decode("latin-1")
        print("note: %s is not UTF-8; decoded as latin-1 (bytes preserved)" % in_path)

    return text.replace("\r\n", "\n").replace("\r", "\n").split("\n")


def convert(mode, in_path, out_path):
    lines = read_lines(in_path)

    root = collections.OrderedDict()

    if mode == "tokens":
        parse_group_tokens(lines, 0, root, in_path)
    elif mode == "rows":
        parse_group_rows(lines, 0, root, in_path, set())
    else:
        raise ConversionError("unknown mode: %s" % mode)

    strip_empty(root)

    text = json.dumps(root, indent=2, ensure_ascii=False)
    if not isinstance(text, str):
        text = text.decode("utf-8") if isinstance(text, bytes) else unicode(text)

    with io.open(out_path, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(text)
        handle.write(u"\n")

    groups = len(root.get("groups", []))
    rows = len(root.get("tokens", {}))
    print("ok  %s -> %s (root groups: %d, root rows/tokens: %d)" % (in_path, out_path, groups, rows))
    return 0


def main():
    if len(sys.argv) < 4:
        print("Usage: python txt2json_group.py <tokens|rows> <in.txt> <out.json>")
        return 1

    mode = sys.argv[1]
    in_path = sys.argv[2]
    out_path = sys.argv[3]

    try:
        return convert(mode, in_path, out_path)
    except ConversionError as ex:
        print("error: %s" % ex)
        return 1


if __name__ == "__main__":
    sys.exit(main())
