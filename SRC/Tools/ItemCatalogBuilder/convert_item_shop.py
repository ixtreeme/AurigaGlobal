"""Historical pre-cutover mapping step. Migration 056 and migrate_item_shop_storage.py
consume this archive explicitly; live offers are edited in ishop_offer/member.

OFFLINE one-time conversion of the legacy item shop to native items (C45).

Each player.ishop_data row names a legacy item prototype (itemVnum). The row is
sold to characters on the native item system only through the native item it
was converted to (player.ishop_native_item, db/migrations/item_instance/024).

A legacy vnum resolves only through reviewed conversion evidence, in order:
  0. compiled - the issued list of a reviewed converter (C43/C44 equipment, C46 shop content)
  1. archive  - an instance the owner importer actually converted (vnum -> key, level)
  2. review   - legacy_remaining_review.json (category + code of the installed item)
  3. mapping  - a conversion proposal 'candidate' whose content code is installed
Evidence that names two different installed items is a conflict: the row stays
unconverted. Retired or missing catalog items never resolve. No vnum arithmetic.

Inputs are JSON exports; the tool opens no SQL connection.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path

TOOL_VERSION = 1


class ConversionError(Exception):
    pass


def _rows(path: Path) -> list[dict]:
    data = json.loads(path.read_text(encoding='utf-8-sig'))
    if not isinstance(data, list):
        raise ConversionError(f'{path}: expected a JSON array of rows')
    return data


def _uint(value, name: str) -> int:
    number = int(value)
    if number < 0 or number > 0xFFFFFFFF:
        raise ConversionError(f'{name} out of range: {value}')
    return number


PRESET_FIELDS = tuple(f'socket{i}' for i in range(3)) + tuple(f'attrtype{i}' for i in range(8))


def resolve(shop: list[dict], catalog: list[dict], proposal: dict | None = None,
            review: dict | None = None, archive: list[dict] | None = None,
            compiled: list[list[dict]] | None = None) -> dict:
    """Returns {'rows': [...], 'unconverted': [...], 'summary': {...}} (deterministic)."""
    installed = {}
    for row in catalog:
        if int(row.get('retired', 0)):
            continue
        key = (_uint(row['category_id'], 'category_id'), _uint(row['local_id'], 'local_id'))
        installed[key] = row['content_code']
    by_code: dict[str, tuple[int, int]] = {}
    for key, code in installed.items():
        if code in by_code:
            raise ConversionError(f'duplicate installed content code {code}')
        by_code[code] = key

    evidence: dict[int, dict[tuple[int, int, int], str]] = {}

    def add(vnum: int, key: tuple[int, int], level: int, source: str):
        if key not in installed:
            return
        evidence.setdefault(vnum, {}).setdefault((key[0], key[1], level), source)

    for accepted in compiled or []:  # a converter's issued vnum -> native key list
        for row in accepted:
            add(_uint(row['source_id'], 'source_id'), (_uint(row['item']['category_id'], 'category_id'), _uint(row['item']['local_id'], 'local_id')),
                int(row.get('refinement_level', 0)), 'compiled')
    for row in archive or []:
        add(_uint(row['vnum'], 'vnum'), (_uint(row['category_id'], 'category_id'), _uint(row['local_id'], 'local_id')),
            int(row.get('refinement_level', 0)), 'archive')
    for vnum, item in sorted(((review or {}).get('items') or {}).items(), key=lambda pair: int(pair[0])):
        category, code = int(item['category']), item['code']
        hits = [key for key, installed_code in installed.items()
                if key[0] == category and installed_code.split('.', 1)[-1] == code]
        if len(hits) == 1:
            add(_uint(vnum, 'vnum'), hits[0], 0, 'review')
    for entry in (proposal or {}).get('mapping') or []:
        if entry.get('status') != 'candidate' or not entry.get('content_code'):
            continue
        key = by_code.get(entry['content_code'])
        if key:
            add(_uint(entry['source_id'], 'source_id'), key, int(entry.get('refinement_level') or 0), 'mapping')

    rows, unconverted = [], []
    for row in sorted(shop, key=lambda r: int(r['id'])):
        shop_id, vnum = _uint(row['id'], 'id'), _uint(row['itemVnum'], 'itemVnum')
        targets = evidence.get(vnum, {})
        # The legacy shop stamped these sockets/attributes onto the bought item; the
        # native grant takes its state from the catalog, so such a row stays unsold.
        if targets and any(int(row.get(f) or 0) for f in PRESET_FIELDS):
            unconverted.append(dict(shop_id=shop_id, vnum=vnum, reason='shop_preset_state', candidates=[]))
            continue
        if len(targets) == 1:
            (category, local, level), source = next(iter(targets.items()))
            if not 0 <= level <= 255:
                unconverted.append(dict(shop_id=shop_id, vnum=vnum, reason='refinement_level'))
                continue
            rows.append(dict(shop_id=shop_id, legacy_vnum=vnum, category_id=category, local_id=local,
                             refinement_level=level, source=source, content_code=installed[(category, local)]))
        else:
            unconverted.append(dict(shop_id=shop_id, vnum=vnum,
                                    reason='conflict' if targets else 'no_native_item',
                                    candidates=sorted(installed[(c, l)] for c, l, _ in targets)))
    summary = dict(tool_version=TOOL_VERSION, shop_rows=len(shop), converted=len(rows),
                   unconverted=len(unconverted),
                   conflicts=sum(1 for u in unconverted if u['reason'] == 'conflict'),
                   by_source={s: sum(1 for r in rows if r['source'] == s) for s in ('compiled', 'archive', 'review', 'mapping')})
    return dict(rows=rows, unconverted=unconverted, summary=summary)


def to_sql(result: dict) -> str:
    lines = ['-- Generated by SRC/Tools/ItemCatalogBuilder/convert_item_shop.py; do not edit.',
             '-- ' + json.dumps(result['summary'], sort_keys=True),
             'START TRANSACTION;',
             'DELETE FROM player.ishop_native_item;']
    values = [f"({r['shop_id']},{r['legacy_vnum']},{r['category_id']},{r['local_id']},{r['refinement_level']},'{r['source']}')"
              for r in result['rows']]
    for at in range(0, len(values), 200):
        lines.append('INSERT INTO player.ishop_native_item(shop_id,legacy_vnum,category_id,local_id,refinement_level,source) VALUES')
        lines.append(',\n'.join(values[at:at + 200]) + ';')
    lines.append('COMMIT;')
    return '\n'.join(lines) + '\n'


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument('--shop', required=True, type=Path, help='JSON rows of player.ishop_data (id, itemVnum)')
    parser.add_argument('--catalog', required=True, type=Path, help='JSON rows of protos.item_base')
    parser.add_argument('--proposal', type=Path, help='conversion proposal JSON with a mapping list')
    parser.add_argument('--review', type=Path, help='config/item_combat/legacy_remaining_review.json')
    parser.add_argument('--archive', type=Path, help='JSON rows: vnum, category_id, local_id, refinement_level')
    parser.add_argument('--compiled', type=Path, action='append', default=[], help="converter output with an 'accepted' list (repeatable)")
    parser.add_argument('--sql', required=True, type=Path)
    parser.add_argument('--report', required=True, type=Path)
    args = parser.parse_args(argv)
    load = lambda path: json.loads(path.read_text(encoding='utf-8-sig')) if path else None
    result = resolve(_rows(args.shop), _rows(args.catalog), load(args.proposal), load(args.review),
                     _rows(args.archive) if args.archive else None,
                     [json.loads(p.read_text(encoding='utf-8'))['accepted'] for p in args.compiled])
    with args.sql.open('x', encoding='utf-8', newline='\n') as f:
        f.write(to_sql(result))
    with args.report.open('x', encoding='utf-8', newline='\n') as f:
        json.dump(result, f, ensure_ascii=False, indent=1, sort_keys=True)
        f.write('\n')
    print(json.dumps(result['summary'], sort_keys=True))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
