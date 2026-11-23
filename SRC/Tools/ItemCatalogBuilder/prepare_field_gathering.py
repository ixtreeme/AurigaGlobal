"""Validate the typed field tools and queue them for local Server Manager review."""
import argparse
import base64
import json
import os
from pathlib import Path
import sys


REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO / 'SRC/Tools/ServerManager'))


def documents(catalog, packet, rule):
    model = catalog.Catalog(packet)
    first = int(packet['sequences']['11'])
    for offset, tool in enumerate(rule['tools']):
        document = model.new(11)
        local = first + offset
        document['base'].update(local_id=str(local), content_code=tool['code'],
            name_key=tool['name_key'], description_key=tool['description_key'],
            icon_asset=tool['icon'], use_policy_id=None)
        document['child'].update(local_id=str(local), tool_kind=str(tool['kind']))
        document['use']['use_during_combat'] = '0'
        points = rule['points_by_grade']
        document['refinement']['max_level'] = str(len(points) - 1)
        zero = document['levels'][0]
        document['levels'] = [dict(zero, level=str(grade)) for grade in range(len(points))]
        document['tool_levels'] = []
        for grade, required in enumerate(points):
            profile = model.tool_level(grade, terminal=not required)
            if required:
                profile.update(required_points=str(required),
                    practice_numerator=str(rule['practice_numerator']),
                    practice_denominator=str(rule['practice_denominator']),
                    refine_chance_bp=str(rule['refine_chance_basis_points']),
                    failure_loss_bp=str(rule['failure_point_loss_basis_points']))
            document['tool_levels'].append(profile)
        yield tool, dict(type='item', key=[11, local], slot='catalog:11:' + str(local), document=document)


def shop_draft(source, proposed, price):
    contents = json.loads(source.text)
    shop, = (shop for shop in contents['shops'] if shop['npc_race'] == 20016)
    next_id = max(offer['id'] for store in contents['shops'] for offer in store['offers']) + 1
    for index, (_, operation) in enumerate(proposed):
        category, local = operation['key']
        shop['offers'].append(dict(id=next_id + index, category=category, local=local,
            level=0, count=1, gold=price, costs=[], try_magic=False))
    original = base64.b64encode(source.original).decode('ascii')
    rendered = json.dumps(contents, ensure_ascii=False, indent=2) + '\n'
    content = base64.b64encode(source.encoded(rendered)).decode('ascii')
    return dict(relative=source.relative, expected_hash=source.digest, base_data=original,
                encoding=source.encoding, data=content)


def main():
    argument = argparse.ArgumentParser(description=__doc__)
    argument.add_argument('server', type=Path)
    argument.add_argument('output', type=Path)
    argument.add_argument('--stage', action='store_true')
    args = argument.parse_args()
    from PySide6.QtCore import QCoreApplication, QObject
    import backend
    import item_catalog as catalog
    from pending_changes import PendingChanges, compile_catalog

    app = QCoreApplication([])

    class LocalServer(QObject):
        root = args.server.resolve()
        remote_profile = None

        def target_id(self):
            return 0, 'local', str(self.root)

    owner = LocalServer()
    helper = owner.root / 'manager/_internal/validators/CatalogEditorTool.exe'
    catalog.helper = lambda: helper
    os.environ['PATH'] = str(owner.root / 'bin') + os.pathsep + os.environ.get('PATH', '')
    packet = catalog.request(owner.root)
    rule = json.loads((REPO / 'config/item_combat/field_gathering_tools.json').read_bytes())
    proposed = list(documents(catalog, packet, rule))
    candidate = compile_catalog(packet, [op for _, op in proposed])
    validated = catalog.run_tool(dict(action='validate', tables=candidate['tables']), candidate['columns'])
    source = backend.Document(owner.root, 'data/typed_shops.json')
    shop = shop_draft(source, proposed, rule['starting_price_yang'])

    args.output.mkdir(parents=True, exist_ok=True)
    data = dict(before=packet['hash'], after=validated['hash'], columns=candidate['columns'],
                tables=candidate['tables'], tools={tool['code']: op['key'] for tool, op in proposed})
    (args.output / 'catalog-plan.json').write_text(json.dumps(data, ensure_ascii=True), encoding='utf-8')
    (args.output / 'typed_shops.json').write_bytes(base64.b64decode(shop['data']))
    for feature in ('woodcutting', 'herbalism'):
        settings = json.loads((REPO / ('config/' + feature + '.json.example')).read_bytes())
        settings['enabled'] = True
        (args.output / (feature + '.json')).write_text(json.dumps(settings, indent=2) + '\n', encoding='utf-8')
    print('Validated two typed tools with ten grades each:', validated['hash'])

    if args.stage:
        queue = PendingChanges(owner)
        with queue.write_guard():
            slots = {op['slot'] for _, op in proposed} | {'file:' + source.relative}
            entries = queue.entries()
            if any(entry['slot'] in slots for entry in entries):
                raise RuntimeError('A függő módosítások már érintik az eszközök helyét vagy a kovács kínálatát.')
            existing = [entry['body']['payload']['operation'] for entry in queue.catalog_entries()]
            occupied = {tuple(op['key']) for op in existing if 'key' in op}
            if occupied & {tuple(op['key']) for _, op in proposed}:
                raise RuntimeError('A lefoglalt tárgykulcsot már használja egy függő tervezet.')
            original_index = queue.index.copy()
            queue._editing = True
            queue._superseded = []
            try:
                for tool, op in proposed:
                    queue.put(tool['name'] + ' · típusos gyűjtőeszköz', 'catalog_edit',
                        dict(before=packet, operation=op),
                        [(tool['name'], 'Gyakorlás és fejlesztés', 'Nincs', '+0–+9, 20% gyakorlás, 80% fejlesztés')])
                queue.put('Kovács · favágó balta és metszőolló', 'file', shop,
                    [('Kovács', 'Új eszköz', '', f"{tool['name']} · {rule['starting_price_yang']:,} Yang")
                     for tool, _ in proposed])
                queue.save_index()
            except Exception:
                queue.index = original_index
                raise
            finally:
                queue._editing = False
        print('Three changes are available for approval in Server Manager.')


if __name__ == '__main__':
    main()
