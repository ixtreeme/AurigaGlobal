"""Import supplied Nexoria layout/art provenance for existing non-item client windows."""
from pathlib import Path
import argparse
import hashlib
import json
import re
import shutil
import uuid


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--designs', type=Path, default=Path('D:/Nexoria UI'))
    parser.add_argument('--project', type=Path, default=Path('D:/Auriga-Unity6/SRC/AurigaUnity-20261103'))
    args = parser.parse_args()
    designs = args.designs.resolve()
    output = args.project / 'Assets/Metin2/Resources/Metin2/NexoriaUI'
    catalog = args.project / 'Assets/Metin2/GeneratedData/Metin2UiCatalog.asset'
    screens = re.findall(r'^  - screen: (.+)$', catalog.read_text(encoding='utf-8'), re.M)
    excluded = {'minimap', 'chatwindow', 'tooltip', 'imekor', 'gamewindow', 'partymemberinfoboard', 'giftbox', 'energybar',
                'mousebuttonwindow', 'rightmousebuttonwindow', 'inventory_menu', 'expandedmoneytaskbar', 'expandedtaskbar',
                'petminiinformationwindow', 'battleroyale_info', 'pvp_advanced_informations_mini',
                'login', 'loginwindow', 'loading', 'loadingwindow', 'selectempire', 'selectempirewindow',
                'createcharacter', 'createcharacterwindow', 'selectcharacter', 'selectcharacterwindow', 'teleport',
                'inventorywindow', 'inventorywindowex', 'extrainventory', 'beltinventorywindow', 'mountinventory',
                'shopdialog', 'exchangedialog', 'exchangedialog_new', 'safeboxwindow', 'mallwindow', 'guildstorage'}
    primary = {'characterwindow': ('Karakter', 'Karakter'), 'characterdetailswindow': ('Karakter_Reszletek', 'Részletes értékek'),
               'guildwindow': ('Ceh', 'Céh'), 'messengerwindow': ('Social', 'Barátok'),
               'systemoptiondialog': ('Beallitasok', 'Grafika és hang'), 'gameoptiondialog': ('Beallitasok', 'Játékbeállítások'),
               'gameoptiondialog_formobile': ('Beallitasok', 'Játékbeállítások'), 'atlaswindow': ('Terkep', 'Térkép'),
               'whisperdialog': ('Suttogas', 'Suttogás'), 'helpwindow': ('Sugo', 'Súgó'), 'helpwindow2': ('Sugo', 'Érintéses súgó')}
    profiles, art = [], {}
    template = (output / 'nx_panel_sub.png.meta').read_text(encoding='utf-8')
    for screen in screens:
        if screen in excluded or screen.startswith('taskbar'):
            continue
        data_path = designs / 'Ablakok/_data' / (screen + '.js')
        data = json.loads(re.sub(r'^var NXW = |;\s*$', '', data_path.read_text(encoding='utf-8'))) if data_path.exists() else None
        layout_path = args.project / 'Assets/Metin2/Generated/UI/Layouts' / (screen + '.json')
        layout = json.loads(layout_path.read_text(encoding='utf-8')) if layout_path.exists() else None
        if data is None and screen not in primary:
            continue
        if screen in primary:
            folder, title = primary[screen]
            source = next((designs / folder).glob('*.psd'))
        else:
            source = designs / 'Ablakok' / ('Nexoria_' + screen + '.psd')
            if not source.exists():
                matches = list((designs / 'Ablakok').rglob('Nexoria_' + screen + '.psd'))
                if not matches:
                    continue
                source = matches[0]
            title = data['title'] or screen.replace('window', '').replace('dialog', '').replace('_', ' ').strip().capitalize()
        node = layout['nodes'][0] if layout else None
        width = data['w'] if data else node['width']
        height = data['h'] if data else node['height']
        styles = []
        if data:
            paths = {}
            for item in data['nodes']:
                paths[item['i']] = (paths[item['p']] + '/' if item['p'] >= 0 else '') + item['n']
                if 'kind' not in item:
                    continue
                entry = dict(Path=paths[item['i']], Kind=item['kind'], Asset='')
                if item.get('img'):
                    image = Path(item['img']).resolve()
                    assert image.is_relative_to(designs) and image.suffix.lower() == '.png', image
                    if image not in art:
                        name = 'nx_c38_' + hashlib.sha256(image.as_posix().encode()).hexdigest()[:16]
                        target = output / (name + '.png')
                        assert not target.exists() or target.read_bytes() == image.read_bytes(), target
                        shutil.copy2(image, target)
                        meta = target.with_suffix('.png.meta')
                        if not meta.exists():
                            meta.write_text(re.sub(r'guid: [a-f0-9]+', 'guid: ' + uuid.uuid4().hex, template, count=1), encoding='utf-8')
                        art[image] = name
                    entry['Asset'] = art[image]
                styles.append(entry)
        profiles.append(dict(Screen=screen, Title=title, Width=width, Height=height, Source=source.as_posix(), Styles=styles))
    runtime = {'wheelwindow': ('Szerencsekerek', 'Szerencsekerék'),
               'switchbotwindow': ('Bonusz_Muhely', 'Bónuszműhely'),
               'eventcalendarwindow': ('Esemenynaptar', 'Eseménynaptár'),
               'stonecraftwindow': ('Fejlesztes', 'Kőműhely'),
               'guildoffer': ('Ceh', 'Tapasztalat felajánlása'),
               'graphicsadvanced': ('Beallitasok', 'Részletes grafika')}
    for screen, (folder, title) in runtime.items():
        source = next((designs / folder).glob('*.psd'))
        profiles.append(dict(Screen=screen, Title=title, Width=0, Height=0, Source=source.as_posix(), Styles=[]))
    (output / 'window-layouts.json').write_text(json.dumps({'Windows': profiles}, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')
    manifest = [dict(asset=name + '.png', source=path.as_posix(), sha256=hashlib.sha256(path.read_bytes()).hexdigest()) for path, name in art.items()]
    (output / 'window-art-source.json').write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')
    print('NEXORIA_EXISTING_WINDOW_PROFILES=' + str(len(profiles)) + ' provided_art=' + str(len(art)))


if __name__ == '__main__':
    main()
