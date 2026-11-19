"""Install a compiled additive catalog into the OWNED, STOPPED local package (C46).

Steps, all under the package supervisor lock with the stack stopped:
  1. the installed catalog must still be the converter's preserved catalog (hash);
  2. backup: catalog JSON and every Unity *_typed_equipment.json;
  3. commit the compiled tables with the expected hash (CatalogEditorTool);
  4. move the item persistence pin to the new catalog (LegacyEquipmentImportTool
     follow_catalog), exactly as the C43/C44 cutovers did;
  5. merge the converter's names/descriptions into the Unity locale files
     (existing keys stay; a key of this packet is added or replaced).
Never touches a running server, a remote host or a player row.
"""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import shutil
import sys

REPO = Path(__file__).resolve().parents[3]
sys.path[:0] = [str(REPO / 'SRC/Tools/ServerManager'), str(REPO / 'SRC/Tools/ItemCatalogBuilder')]
LOCALES = 'Assets/Metin2/Resources/Metin2/Locales'


def merge_locales(project: Path, localization: dict) -> dict:
    counts = {}
    for path in sorted((project / LOCALES).glob('*_typed_equipment.json')):
        language = path.name.split('_', 1)[0]
        document = json.loads(path.read_text(encoding='utf-8-sig'))
        entries = {e['key']: e for e in document['entries']}
        order = [e['key'] for e in document['entries']]
        added = 0
        for record in localization.values():
            local = record['texts'].get(language) or record['texts'].get('en') or {}
            fallback = record['texts'].get('en') or {}
            for key, field in (('name_key', 'name'), ('description_key', 'description')):
                value = local.get(field) or fallback.get(field, '')
                if not value:
                    continue
                if record[key] not in entries:
                    order.append(record[key])
                    added += 1
                entries[record[key]] = dict(key=record[key], text=value, kind=0)
        path.write_text(json.dumps(dict(entries=[entries[k] for k in order]), ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
        counts[language] = added
    return counts


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument('--package', required=True, type=Path)
    parser.add_argument('--compiled', required=True, type=Path)
    parser.add_argument('--unity-project', required=True, type=Path)
    parser.add_argument('--helper', required=True, type=Path, help='CatalogEditorTool')
    parser.add_argument('--importer', required=True, type=Path, help='LegacyEquipmentImportTool')
    parser.add_argument('--name', required=True, help='backup folder name under run/deployments')
    args = parser.parse_args(argv)
    import backend, item_bench, item_catalog
    sys.path.insert(0, str(REPO / 'db/tools'))
    from equipment_cutover import import_tool, install_additive
    compiled = json.loads(args.compiled.read_text(encoding='utf-8'))
    if compiled.get('conversion_kind')=='full_prototype_catalog' and compiled.get('release_ready') is not True:
        raise SystemExit('Full prototype conversion has unresolved content/runtime checks; installation refused')
    root, manager, _, auth = item_bench.connection(args.package.resolve())
    os.environ['PATH'] = str(root / 'bin') + os.pathsep + os.environ['PATH']
    backup = root / 'run/deployments' / args.name
    with manager.supervisor_lock():
        if backend.describe(root)['running']:
            raise SystemExit('Stop the local stack first')
        current = item_catalog.run_tool(dict(action='read', connection=auth), executable=args.helper)
        if current['hash'] != compiled['preserved_hash']:
            raise SystemExit('Installed catalog is not the converter input; recompile')
        backup.mkdir(parents=True)
        (backup / 'catalog-before.json').write_text(json.dumps(current, sort_keys=True), encoding='utf-8')
        (backup / 'locales').mkdir()
        for path in (args.unity_project / LOCALES).glob('*_typed_equipment.json'):
            shutil.copy2(path, backup / 'locales' / path.name)
        # Strictly additive, one SQL transaction, verified by readback (C43/C44 path);
        # the interactive editor commit admits only one new item at a time.
        from mariadb_client import Client
        db = Client(**auth, library=str(root / 'bin/libmariadb.dll'))
        try:
            saved = install_additive(db, args.helper, auth, compiled['packet'])
        finally:
            db.close()
        if saved['hash'] != compiled['packet']['hash']:
            raise SystemExit('Installed catalog hash differs from the compiled packet')
        followed = import_tool(args.importer, auth, dict(action='follow_catalog', offline_owned_package=True))
    counts = merge_locales(args.unity_project, compiled['localization'])
    receipt = dict(at=datetime.now(timezone.utc).isoformat(), before=current['hash'], after=saved['hash'], added=saved['added'], follow=followed,
                   accepted=len(compiled['accepted']), locale_entries_added=counts)
    (backup / 'receipt.json').write_text(json.dumps(receipt, indent=2, sort_keys=True) + '\n', encoding='utf-8')
    print('CATALOG_INSTALL=SUCCESS ' + json.dumps(dict(before=current['hash'][:12], after=saved['hash'][:12], accepted=len(compiled['accepted']),
                                                      locales=len(counts), backup=str(backup))))


if __name__ == '__main__':
    main()
