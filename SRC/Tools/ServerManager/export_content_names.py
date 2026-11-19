"""Export the manager's Hungarian names from client locales and reviewed item mappings.

This reads reference data only; it never alters a catalog, locale or database.
Only accepted source mappings are used, so deferred items stay deferred.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re


def clean_name(value):
    value = re.sub(r'\|c[0-9a-fA-F]{8}|\|r', '', value)
    value = re.sub(r'</?(?:color|b|i|size)(?:=[^>]*)?>', '', value)
    return re.sub(r'^[/\\]+', '', value).strip()


def export(locales, compiled):
    locales, compiled = Path(locales), Path(compiled)
    inputs = {}

    def read(path):
        raw = path.read_bytes()
        inputs[path.name] = hashlib.sha256(raw).hexdigest()
        return json.loads(raw.decode('utf-8-sig'))

    catalog = read(compiled)
    source, keyed, mobs = {}, {}, {}
    for language in ('en', 'hu'):
        for suffix in ('items', 'item_extras'):
            for row in read(locales / (language + '_' + suffix + '.json'))['entries']:
                if row.get('name'):
                    source[int(row['vnum'])] = row['name'].strip()
                    for offset in range(1, int(row.get('range') or 0) + 1):
                        source[int(row['vnum']) + offset] = row['name'].strip()
        for row in read(locales / (language + '_typed_equipment.json'))['entries']:
            if row.get('text'):
                keyed.setdefault(language, {})[row['key']] = row['text'].strip()
        for row in read(locales / (language + '_mobs.json'))['entries']:
            if row.get('name'):
                mobs[str(row['vnum'])] = dict(name=row['name'].strip(), level=int(row['level']), rank=int(row['rank']))

    packet = catalog['packet']
    columns = packet['columns']['item_base']
    bases = {(str(row['category_id']), str(row['local_id'])): row
             for cells in packet['tables']['item_base'] for row in [dict(zip(columns, cells))]}
    items = {}
    # Lowest refinement wins for a collapsed family; +N belongs to the instance.
    for row in sorted(catalog['accepted'], key=lambda r: int(r['refinement_level']), reverse=True):
        base = bases[(str(row['item']['category_id']), str(row['item']['local_id']))]
        value = source.get(int(row['source_id']))
        if value:
            items[base['name_key']] = re.sub(r'\s*\+\d+\s*$', '', value).strip()
    for record in catalog.get('localization', {}).values():
        text = record.get('texts', {}).get('hu') or record.get('texts', {}).get('en') or {}
        if text.get('name'):
            items[record['name_key']] = text['name'].strip()
    # Explicit native translations take precedence over migrated source labels.
    for language in ('en', 'hu'):
        for key, value in keyed.get(language, {}).items():
            if key.endswith('.name') and (language == 'hu' or key not in items):
                items[key] = re.sub(r'\s*\+\d+\s*$', '', value).strip()
    items = {key:clean_name(value) for key,value in items.items()}
    return dict(version=1, language='hu', items=dict(sorted(items.items())),
                mobs=dict(sorted(mobs.items(), key=lambda pair: int(pair[0]))), inputs=inputs)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--locales', type=Path, required=True)
    parser.add_argument('--catalog', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    value = export(args.locales, args.catalog)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print('CONTENT_NAMES=SUCCESS items=%d mobs=%d' % (len(value['items']), len(value['mobs'])))


if __name__ == '__main__':
    main()
