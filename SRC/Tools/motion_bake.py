#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# Bakes the client .msa motion data into the player DB tables
# (mob_motion / mob_skill_splash) used by the GameServer CMotionManager.
#
# Usage:
#   python motion_bake.py <data_dir> <out.sql>
#
#   <data_dir>  the client/server data root that contains monster/<folder>/
#               motlist.txt and the .msa files (e.g. share/data)
#
# The script only reads files; the generated SQL is meant to be imported
# into the `player` database. Safe to run repeatedly (REPLACE INTO).
#
# py2/py3 compatible (the live host runs python2.7).

from __future__ import print_function

import io
import os
import sys

SKILL_TYPES = [
    ("SPECIAL", 0),
    ("SPECIAL1", 1),
    ("SPECIAL2", 2),
    ("SPECIAL3", 3),
    ("SPECIAL4", 4),
]

MOTION_EVENT_TYPE_SPECIAL_ATTACKING = 4


def sql_escape(text):
    return text.replace("'", "''")


def parse_msa(path):
    """Returns (duration_sec, accum_y, splashes).

    splashes is a list of (starting_time_sec, position_y) for every
    MotionEventType 4 event using the first SphereData position.
    """
    duration = 0.0
    accum_y = 0.0
    splashes = []

    try:
        with io.open(path, "r", encoding="utf-8", errors="replace") as handle:
            lines = handle.readlines()
    except IOError:
        return None

    stack = []
    pending = None
    current_event = None

    for raw in lines:
        line = raw.strip()

        if not line:
            continue

        if line.startswith("{"):
            stack.append(pending if pending else "block")
            if pending == "event":
                current_event = {"type": None, "start": 0.0, "y": None}
            pending = None
            continue

        if line.startswith("}"):
            kind = stack.pop() if stack else "block"
            if kind == "event" and current_event is not None:
                if current_event["type"] == MOTION_EVENT_TYPE_SPECIAL_ATTACKING and current_event["y"] is not None:
                    splashes.append((current_event["start"], current_event["y"]))
                current_event = None
            continue

        parts = line.split(None, 1)
        key = parts[0]
        rest = parts[1].strip() if len(parts) > 1 else ""
        lkey = key.lower()

        if lkey == "group":
            name = rest.split()[0].lower() if rest else ""
            if name.startswith("event"):
                pending = "event"
            elif name.startswith("sphere"):
                pending = "sphere"
            else:
                pending = "block"
            continue

        if stack and stack[-1] == "event" and current_event is not None:
            if lkey == "motioneventtype":
                try:
                    current_event["type"] = int(rest.split()[0])
                except (ValueError, IndexError):
                    current_event["type"] = None
            elif lkey == "startingtime":
                try:
                    current_event["start"] = float(rest.split()[0])
                except (ValueError, IndexError):
                    current_event["start"] = 0.0
            continue

        if stack and stack[-1] == "sphere" and current_event is not None:
            if lkey == "position" and current_event["y"] is None:
                values = rest.split()
                if len(values) >= 2:
                    try:
                        current_event["y"] = float(values[1])
                    except ValueError:
                        pass
            continue

        if lkey == "motionduration":
            try:
                duration = float(rest.split()[0])
            except (ValueError, IndexError):
                duration = 0.0
        elif lkey == "accumulation":
            values = rest.split()
            if len(values) >= 2:
                try:
                    accum_y = float(values[1])
                except ValueError:
                    accum_y = 0.0

    return duration, accum_y, splashes


def parse_motlist(path):
    """Returns {TYPE_UPPER: msa_file} using the first matching line."""
    entries = {}

    try:
        with io.open(path, "r", encoding="utf-8", errors="replace") as handle:
            lines = handle.readlines()
    except IOError:
        return entries

    for raw in lines:
        parts = raw.split()
        # Some motlists omit the trailing percentage column.
        if len(parts) < 3:
            continue

        motion_type = parts[1].upper()
        if motion_type not in entries:
            entries[motion_type] = parts[2]

    return entries


def motion_speed(msa_path):
    parsed = parse_msa(msa_path)
    if parsed is None:
        return 0

    duration, accum_y, _ = parsed
    if duration <= 0.0 or accum_y >= 0.0:
        return 0

    return int(-accum_y / duration + 0.5)


def motion_duration_ms(msa_path):
    parsed = parse_msa(msa_path)
    if parsed is None:
        return 0

    duration = parsed[0]
    if duration <= 0.0:
        return 0

    return int(duration * 1000.0 + 0.5)


def bake_folder(folder_path, out):
    entries = parse_motlist(os.path.join(folder_path, "motlist.txt"))
    if not entries:
        return 0

    def msa(name):
        return os.path.join(folder_path, name)

    walk_speed = motion_speed(msa(entries["WALK"])) if "WALK" in entries else 0
    run_speed = motion_speed(msa(entries["RUN"])) if "RUN" in entries else 0
    attack_duration = motion_duration_ms(msa(entries["NORMAL_ATTACK"])) if "NORMAL_ATTACK" in entries else 0

    skill_durations = [0, 0, 0, 0, 0]

    for motion_type, skill_index in SKILL_TYPES:
        if motion_type not in entries:
            continue

        skill_durations[skill_index] = motion_duration_ms(msa(entries[motion_type]))

    folder = os.path.basename(folder_path)

    if isinstance(folder, bytes):
        folder = folder.decode("utf-8", "replace")

    out.write(
        "REPLACE INTO `player`.`mob_motion` "
        "(`folder`, `walk_speed`, `run_speed`, `attack_duration`, "
        "`skill1_duration`, `skill2_duration`, `skill3_duration`, `skill4_duration`, `skill5_duration`) "
        "VALUES ('%s', %d, %d, %d, %d, %d, %d, %d, %d);\n"
        % (
            sql_escape(folder),
            walk_speed,
            run_speed,
            attack_duration,
            skill_durations[0],
            skill_durations[1],
            skill_durations[2],
            skill_durations[3],
            skill_durations[4],
        )
    )

    splash_count = 0

    for motion_type, skill_index in SKILL_TYPES:
        if motion_type not in entries:
            continue

        parsed = parse_msa(msa(entries[motion_type]))
        if parsed is None:
            continue

        for event_index, splash in enumerate(parsed[2]):
            starting_time, position_y = splash
            # Truncation matches the old server: 100 + uint32_t(start * 1000).
            timing_ms = 100 + int(starting_time * 1000.0)
            out.write(
                "REPLACE INTO `player`.`mob_skill_splash` "
                "(`folder`, `skill_index`, `event_index`, `timing_ms`, `hit_distance`) "
                "VALUES ('%s', %d, %d, %d, %d);\n"
                % (sql_escape(folder), skill_index, event_index, timing_ms, int(position_y))
            )
            splash_count += 1

    return 1 + splash_count


def main():
    if len(sys.argv) < 3:
        print("Usage: python motion_bake.py <data_dir> <out.sql>")
        return 1

    data_dir = sys.argv[1]
    out_path = sys.argv[2]
    monster_dir = os.path.join(data_dir, "monster")

    if not os.path.isdir(monster_dir):
        print("No monster directory under %s" % data_dir)
        return 1

    folders = 0
    rows = 0

    with io.open(out_path, "w", encoding="utf-8") as out:
        out.write("SET NAMES utf8mb4;\n")
        out.write("-- motion bake: %s\n" % monster_dir)
        out.write("-- generated by SRC/Tools/motion_bake.py\n\n")

        for name in sorted(os.listdir(monster_dir)):
            folder_path = os.path.join(monster_dir, name)
            if not os.path.isdir(folder_path):
                continue

            baked = bake_folder(folder_path, out)
            if baked:
                folders += 1
                rows += baked

        out.write("\n-- folders: %d, rows: %d\n" % (folders, rows))

    print("baked %d folders, %d rows -> %s" % (folders, rows, out_path))
    return 0


if __name__ == "__main__":
    sys.exit(main())
