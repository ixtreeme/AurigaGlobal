"""Generate the draft typed bonus contract and its fresh-catalog SQL seed.

Not a legacy APPLY converter. Unknown units, filters and unsupported semantic
combinations fail before writing. Never reads or contacts a database.
"""
import argparse
import json
import os
from pathlib import Path
import re
import sys
import tempfile

from generate_item_categories import unique_object

ROOT = Path(__file__).resolve().parents[3]
ENUMS = {
    "unit": ("BonusUnit", ["Raw", "BasisPoints", "Integer"]),
    "target": ("BonusTarget", ["PhysicalAttack", "MagicalAttack", "PhysicalDefense", "MagicalDefense",
                               "OutgoingDamage", "IncomingDamage", "FlatPenetration", "PercentPenetration",
                               "Resistance", "CritChance", "CritResistance", "CritMultiplierExtra", "BlockReduction", "Evasion"]),
    "stage": ("BonusStage", ["StatFlat", "StatPercent", "OutgoingAction", "OutgoingMode", "Penetration",
                             "Resistance", "Critical", "IncomingAction", "IncomingMode", "Block", "Accuracy"]),
    "channel": ("BonusChannel", ["Any", "Physical", "Magical"]),
    "action": ("BonusAction", ["Any", "Normal", "Skill"]),
    "mode": ("BonusMode", ["Any", "Pve", "Pvp"]),
    "family": ("BonusFamily", ["Any"]), "rank": ("BonusRank", ["Any"]), "element": ("BonusElement", ["Any"]),
    "stacking": ("BonusStack", ["Add", "Max", "ReplaceByPriority", "UniqueSource"]),
    "aggregate_cap": ("BonusCap", ["RawStat", "StatPercent", "Outgoing", "Incoming", "FlatPenetration",
                                  "PercentPenetration", "Resistance", "CritChance", "CritResistance",
                                  "CritMultiplierExtra", "Block", "Accuracy"]),
}
SOURCES = {"ItemBase": 1, "Refinement": 2}
FIELDS = {"id", "cpp_name", "code", "display_key", "allowed_sources", "minimum", "maximum", *ENUMS}


def number(field, value):
    names = ENUMS[field][1]
    return names.index(value) + (0 if field in ("family", "rank", "element") else 1)


def source_mask(row):
    return sum(SOURCES[name] for name in row["allowed_sources"])


def semantic_contract(row):
    target, stage, channel = (row[name] for name in ("target", "stage", "channel"))
    expected_action, expected_mode = "Any", "Any"
    if target in ("PhysicalAttack", "MagicalAttack", "PhysicalDefense", "MagicalDefense"):
        if stage not in ("StatFlat", "StatPercent"):
            raise ValueError("stat bonus must target a stat stage")
        unit, cap = ("Raw", "RawStat") if stage == "StatFlat" else ("BasisPoints", "StatPercent")
        expected_channel = "Physical" if target.startswith("Physical") else "Magical"
    elif target in ("OutgoingDamage", "IncomingDamage"):
        direction = "Outgoing" if target == "OutgoingDamage" else "Incoming"
        unit, cap, expected_channel = "BasisPoints", direction, "Any"
        if stage == direction + "Action" and row["action"] in ("Normal", "Skill"):
            expected_action = row["action"]
        elif stage == direction + "Mode" and row["mode"] in ("Pve", "Pvp"):
            expected_mode = row["mode"]
        else:
            raise ValueError("damage bonus needs exactly one action or mode filter")
    else:
        contracts = {
            "FlatPenetration": ("Raw", "Penetration", "FlatPenetration"),
            "PercentPenetration": ("BasisPoints", "Penetration", "PercentPenetration"),
            "Resistance": ("BasisPoints", "Resistance", "Resistance"),
            "CritChance": ("BasisPoints", "Critical", "CritChance"),
            "CritResistance": ("BasisPoints", "Critical", "CritResistance"),
            "CritMultiplierExtra": ("BasisPoints", "Critical", "CritMultiplierExtra"),
            "BlockReduction": ("BasisPoints", "Block", "Block"),
            "Evasion": ("BasisPoints", "Accuracy", "Accuracy"),
        }
        unit, expected_stage, cap = contracts[target]
        if stage != expected_stage:
            raise ValueError("bonus target/stage mismatch")
        if target in ("FlatPenetration", "PercentPenetration", "Resistance"):
            if channel not in ("Physical", "Magical"):
                raise ValueError("channel-specific bonus needs an explicit channel")
            expected_channel = channel
        else:
            expected_channel = "Physical" if target == "BlockReduction" else "Any"
    if (row["unit"], row["aggregate_cap"], channel, row["action"], row["mode"]) != (
            unit, cap, expected_channel, expected_action, expected_mode):
        raise ValueError("bonus unit/cap/filter conflicts with semantic target")


def validate(document):
    if not isinstance(document, dict) or set(document) != {"schema_version", "status", "bonuses"}:
        raise ValueError("invalid bonus registry members")
    if type(document["schema_version"]) is not int or document["schema_version"] != 1:
        raise ValueError("unsupported bonus registry schema")
    if document["status"] not in ("draft", "released"):
        raise ValueError("invalid registry status")
    rows = document["bonuses"]
    if not isinstance(rows, list) or not 1 <= len(rows) <= 4096:
        raise ValueError("registry needs 1..4096 definitions")
    seen = {name: set() for name in ("id", "cpp_name", "code", "display_key")}
    for row in rows:
        if not isinstance(row, dict) or set(row) != FIELDS:
            raise ValueError("invalid bonus definition members")
        if type(row["id"]) is not int or not 1 <= row["id"] <= 4294967295:
            raise ValueError("bonus id must be a nonzero uint32")
        for name, pattern in [("cpp_name", r"[A-Z][A-Za-z0-9]{0,63}"),
                              ("code", r"[a-z][a-z0-9_]*(?:\.[a-z0-9_]+)*"),
                              ("display_key", r"[a-z][a-z0-9_]*(?:\.[a-z0-9_]+)*")]:
            if not isinstance(row[name], str) or len(row[name]) > (128 if name == "display_key" else 96) or not re.fullmatch(pattern, row[name]):
                raise ValueError("invalid bonus " + name)
        for name in ENUMS:
            if row[name] not in ENUMS[name][1]:
                raise ValueError("unknown bonus " + name)
        for name in ("minimum", "maximum"):
            bound = 10**15 if row["unit"] == "Raw" else 100000
            if type(row[name]) is not int or not -bound <= row[name] <= bound:
                raise ValueError("invalid individual bonus bound")
        if row["minimum"] > row["maximum"]:
            raise ValueError("inverted bonus bounds")
        sources = row["allowed_sources"]
        if not isinstance(sources, list) or not sources or any(not isinstance(v, str) or v not in SOURCES for v in sources) or len(set(sources)) != len(sources):
            raise ValueError("invalid or duplicate bonus source")
        semantic_contract(row)
        for name in seen:
            if row[name] in seen[name]:
                raise ValueError("duplicate bonus " + name)
            seen[name].add(row[name])
    return sorted(rows, key=lambda row: row["id"])


def render(document):
    rows = validate(document)
    lines = ["// Generated by generate_bonus_registry.py; edit config/item_combat/bonus_registry.json.",
             "// Draft semantic contract; not approved production balance or an APPLY mapping.",
             '#pragma once', '#include "BonusTypes.hpp"', '#include <array>', '', 'namespace auriga::item', '{',
             'enum class BonusId : std::uint32_t', '{']
    lines += [f"    {row['cpp_name']} = {row['id']}," for row in rows]
    lines += ['};', 'inline constexpr std::uint32_t BonusRegistrySchemaVersion = 1;',
              f'inline constexpr std::string_view BonusRegistryStatus = "{document["status"]}";',
              f'inline constexpr std::array<BonusDefinition, {len(rows)}> BonusDefinitions {{{{']
    for row in rows:
        fields = [str(row['id']), '"' + row['code'] + '"', '"' + row['display_key'] + '"']
        fields += [ENUMS[name][0] + '::' + row[name] for name in ENUMS]
        fields += [str(source_mask(row)), str(row['minimum']) + 'LL', str(row['maximum']) + 'LL']
        lines.append('    {' + ', '.join(fields) + '},')
    lines += ['}};', 'constexpr const BonusDefinition* FindBonusDefinition(std::uint32_t id) noexcept', '{',
              '    for (const auto& definition : BonusDefinitions)',
              '        if (definition.id == id) return &definition;', '    return nullptr;', '}',
              'constexpr const BonusDefinition* FindBonusDefinition(BonusId id) noexcept', '{',
              '    return FindBonusDefinition(static_cast<std::uint32_t>(id));', '}', '} // namespace auriga::item', '']
    return '\n'.join(lines).encode('utf-8')


def render_sql(document):
    rows = validate(document)
    lines = ['-- Generated from bonus_registry.json. Fresh draft catalog only; duplicate IDs fail.',
             '-- Bounds are individual structural validation, not aggregated gameplay caps.',
             'START TRANSACTION;', 'INSERT INTO protos.bonus_definition',
             '    (bonus_id,code,display_key,unit_kind,target_kind,stage_kind,channel_filter,action_filter,mode_filter,',
             '     family_filter,rank_filter,element_filter,stack_policy,aggregate_cap,allowed_sources,minimum_value,maximum_value) VALUES']
    values = []
    for row in rows:
        fields = [str(row['id']), "'" + row['code'] + "'", "'" + row['display_key'] + "'"]
        fields += [str(number(name, row[name])) for name in ENUMS]
        fields += [str(source_mask(row)), str(row['minimum']), str(row['maximum'])]
        values.append('    (' + ','.join(fields) + ')')
    lines += [',\n'.join(values) + ';', 'COMMIT;', '']
    return '\n'.join(lines).encode('utf-8')


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--registry', type=Path, default=ROOT / 'config/item_combat/bonus_registry.json')
    parser.add_argument('--output', type=Path, default=ROOT / 'SRC/Server/common/item/BonusRegistry.hpp')
    parser.add_argument('--sql-output', type=Path, default=ROOT / 'db/migrations/item_combat/007_bonus_registry_seed.sql')
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args(argv)
    temporary = None
    try:
        document = json.loads(args.registry.read_text(encoding='utf-8-sig'), object_pairs_hook=unique_object)
        outputs = [(args.output, render(document)), (args.sql_output, render_sql(document))]
        if args.check:
            if any(not path.is_file() or path.read_bytes() != content for path, content in outputs):
                print('Generated bonus header/seed is stale; regenerate before building.', file=sys.stderr)
                return 1
        else:
            for path, content in outputs:
                path.parent.mkdir(parents=True, exist_ok=True)
                with tempfile.NamedTemporaryFile(dir=path.parent, delete=False) as stream:
                    temporary = Path(stream.name)
                    stream.write(content)
                os.replace(temporary, path)
                temporary = None
        return 0
    except (ValueError, OSError) as error:
        print('Bonus registry error: ' + str(error), file=sys.stderr)
        return 2
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)


if __name__ == '__main__':
    raise SystemExit(main())
