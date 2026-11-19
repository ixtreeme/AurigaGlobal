"""Export the character, character-details and special-inventory artwork with native Photoshop effects (C47).

Same contract as ExportNexoriaInventoryStorage.py: Photoshop renders isolated layer
states of a temporary duplicate (the source PSD is never saved), the clean canvas
drops every sample value (names, levels, numbers, bar fills, effect icons, items),
and controls keep their authored rectangles. Existing texture GUIDs are preserved;
other windows in the shared manifest are kept. No version-control operation.
"""
from pathlib import Path
import argparse, hashlib, json, re, shutil, subprocess, uuid
import NexoriaLiveText

KARAKTER_TABS = ['Karakter', 'Képesség', 'Érzelmek', 'Küldetések']
KARAKTER_PAGES = ['Oldal — ' + tab for tab in KARAKTER_TABS]
DETAIL_TABS = ['PVM bónuszok', 'PVP bónuszok']
SPECIAL_TYPES = ['Fejlesztés', 'Könyvek', 'Kövek', 'Ládák', 'Esszenciák', 'Egyéb']


def main():
    from psd_tools import PSDImage
    from PIL import Image
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument('--project', type=Path, default=Path('D:/Auriga-Unity6/SRC/AurigaUnity-20261103'))
    parser.add_argument('--design', type=Path, default=Path('D:/Nexoria UI'))
    parser.add_argument('--cache', type=Path, default=Path('build/c47-native-cache'))
    args = parser.parse_args()
    output = args.project / 'Assets/Metin2/Resources/Metin2/NexoriaUI'
    manifest = output / 'item-windows-source.json'
    old = json.loads(manifest.read_text(encoding='utf-8'))
    prior = {a['Asset']: a['Sha256'] for w in old['Windows'] for a in w['Assets']}
    sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
    template = (output / 'nx_panel_sub.png.meta').read_text(encoding='utf-8')
    scratch = args.cache.resolve(); scratch.mkdir(parents=True, exist_ok=True)
    windows = []
    documents = [('Karakter', 'Nexoria_Karakter', 'character'), ('Karakter_Reszletek', 'Nexoria_Karakter_Reszletek', 'details'),
                 ('Extra_Leltar', 'Nexoria_Extra_Leltar', 'special')]
    for folder, stem, key in documents:
        source = args.design / folder / (stem + '.psd')
        script = next((args.design / folder / 'scripts').glob('nx_build_*.jsx'))
        psd = PSDImage.open(source)

        def walk(group, path=()):
            for layer in group:
                p = path + (layer.name,); yield layer, p
                if layer.is_group(): yield from walk(layer, p)
        layers = list(walk(psd)); specs = []; assets = []; controls = []

        def find(*names):
            """Full paths of layers named exactly (any depth); each name must exist once."""
            found = []
            for name in names:
                hits = [p for _, p in layers if p[-1] == name]
                if len(hits) != 1: raise RuntimeError(f'{stem}: layer {name!r} found {len(hits)}x')
                found.append(hits[0])
            return found

        def asset(name, predicate):
            name = 'nxi_' + key + '_' + name
            target = output / (name + '.png')
            if target.exists() and sha(target) != prior.get(name): raise RuntimeError('Unrecorded export edit: ' + name)
            specs.append(dict(Asset=name, Path=[], States=[dict(Path=list(p), Visible=bool(predicate(l, p))) for l, p in layers]))
            assets.append(dict(Asset=name)); return name

        def only(*paths):
            paths = [tuple(p) for p in paths]
            return lambda layer, p: any(p[:len(q)] == q or q[:len(p)] == p for q in paths) and 'rejtett' not in layer.name.lower()

        def base_without(rules):
            def keep(layer, p):
                if not layer.visible: return False
                return not any(rule(name, p) for name in p for rule in rules)
            return keep
        named = lambda *patterns: (lambda name, p: any(re.search(x, name) for x in patterns))
        top = lambda *groups: (lambda name, p: p[0] in groups)

        if key == 'character':
            # One window, four tabs: the base is the frame, the profile card and the shared content
            # background; every tab page is its own asset. The resting minus buttons and the
            # attribute value boxes belong to the Karakter page; its sample numbers, fills and the
            # live plus buttons are dropped.
            sample_value = lambda name, p: name.startswith('Érték — ') and 'Alap tulajdonságok' not in p
            base = asset('base', base_without([top('Fülek', *KARAKTER_PAGES), named(
                r'^Portré — portré$', r'^Név$', r'^Szint és kaszt$', r'^Céh jel', r'^Céh$', r'^Birodalom', r'^Tapasztalat — érték$',
                r'— (kitöltés|csillanás)$')]))
            page_keep = base_without([sample_value, named(
                r'^Szabad pontok', r'^Növelés — ', r'^Érték szám — ', r'— (kitöltés|csillanás)$',
                r'^(Életpont|Mana|Kitartás) — érték$', r'^Részletes értékek — ', r'^Hatás \d+ — ', r'^Hatás ideje ')])
            asset('page_0', lambda layer, p: p[0] == KARAKTER_PAGES[0] and page_keep(layer, p))

            def under(page, *names):
                """Paths of layers named exactly inside one tab page (names repeat across pages)."""
                found = []
                for name in names:
                    hits = [p for _, p in layers if p[0] == page and p[-1] == name]
                    if len(hits) != 1: raise RuntimeError(f'{stem}: {page} / {name!r} found {len(hits)}x')
                    found.append(hits[0])
                return found
            skills, emotions, quests = KARAKTER_PAGES[1:]
            # Képesség: static frame + hint; one card's parts are the repeatable pieces (no sample skill).
            asset('page_1', only(*under(skills, 'Képességcsoport — keret', 'Tipp')))
            for name, parts in (('skill_segment', ['Képességcsoport — kijelölés']), ('skill_points', ['Képességpontok']),
                                ('skill_card', ['Kártya — Dühroham']), ('skill_icon', ['Ikon — Dühroham']),
                                ('skill_rarity_1', ['Ikon — Háromirányú vágás — ritkaság']), ('skill_rarity_2', ['Ikon — Kardpörgetés — ritkaság']),
                                ('skill_rarity_3', ['Ikon — Előretörés — ritkaság']), ('skill_badge', ['Fokozat — Háromirányú vágás']),
                                ('skill_badge_perfect', ['Fokozat — Előretörés']), ('skill_track', ['Fejlődés — Dühroham — sín']),
                                ('skill_fill', ['Fejlődés — Előretörés — kitöltés', 'Fejlődés — Előretörés — csillanás']),
                                ('skill_plus', ['Fejlesztés — Dühroham', 'Fejlesztés — Dühroham — ikon']),
                                ('passive_card', ['Kártya — Vezetés']), ('passive_icon', ['Ikon — Vezetés']),
                                ('section_line', ['Szakasz díszvonal — Aktív képességek']), ('section_diamond', ['Szakasz gyémánt — Aktív képességek'])):
                asset(name, only(*under(skills, *parts)))
            # Érzelmek: hint + one tile.
            asset('page_2', only(*under(emotions, 'Tipp')))
            asset('emotion_tile', only(*under(emotions, 'Csempe — Koccintás')))
            # Küldetések: details backdrop; list row (idle/selected), icon frame, progress, clock, task
            # checks, reward slot, coin and the two button bases without captions.
            asset('page_3', only(*under(quests, 'Hátlap')))
            for name, parts in (('quest_row', ['Sor — Valmar hívása']), ('quest_row_active', ['Sor — A kristályok ébredése']),
                                ('quest_icon', ['Ikon — Valmar hívása']), ('quest_track', ['Haladás — Valmar hívása — sín']),
                                ('quest_fill', ['Haladás — Napi vadászat — kitöltés', 'Haladás — Napi vadászat — csillanás']),
                                ('quest_clock', ['Idő ikon — Napi vadászat']),
                                ('quest_check_on', ['Pipa — Beszélj Elarával', 'Pipa — Beszélj Elarával — pipa']),
                                ('quest_check_off', ['Pipa — Gyűjts 5 kristályszilánkot']), ('quest_reward', ['Jutalom 1']),
                                ('quest_coin', ['Yang jutalom — érme']),
                                ('quest_button', ['Követés gomb — alap', 'Követés gomb — fényes sáv']),
                                ('quest_button_dark', ['Feladás gomb — alap'])):
                asset(name, only(*under(quests, *parts)))
            for i, tab in enumerate(KARAKTER_TABS):
                asset(f'tab_{i}', only(*find(f'Fül — {tab}', f'Fül — {tab} — felirat', f'Fül — {tab} — ikon')))
                asset(f'tab_{i}_active', only(*find(f'Fül — {tab} (aktív)', f'Fül — {tab} — felirat', f'Fül — {tab} — ikon')))
            asset('minus', only(*find('Csökkentés — Életerő', 'Csökkentés — Életerő — ikon')))
            asset('plus', only(*find('Növelés — Életerő', 'Növelés — Életerő — ikon')))
            asset('free_points', only(*find('Szabad pontok')))
            for bar, part in (('hp', 'Életpont'), ('sp', 'Mana'), ('stamina', 'Kitartás'), ('exp', 'Tapasztalat')):
                asset(bar + '_fill', only(*find(part + ' — kitöltés', part + ' — csillanás')))
            asset('guild_icon', only(*find('Céh jel — pajzs', 'Céh jel — kristály', 'Céh jel — kristály — fazetta')))
            asset('empire', only(*find('Birodalom')))
            details = find('Részletes értékek — alap', 'Részletes értékek — felirat', 'Részletes értékek — ikon')
            controls.append(dict(Key='details', Asset=asset('button_details', only(*details)), Rect=[99, 1179, 472, 54], DynamicCaption=False))
            close = [1069, 81, 46, 46]
        elif key == 'details':
            pages = [p for _, p in layers if len(p) == 1 and p[0].startswith('Oldal — ')]
            base = asset('base', base_without([top('Fülek', *[p[0] for p in pages]), named(r'^Alcím$')]))
            for i, tab in enumerate(DETAIL_TABS):
                asset(f'tab_{i}', only(*find(f'Fül — {tab}', f'Fül — {tab} — felirat', f'Fül — {tab} — ikon')))
                asset(f'tab_{i}_active', only(*find(f'Fül — {tab} (aktív)', f'Fül — {tab} — felirat', f'Fül — {tab} — ikon')))
            for i, page in enumerate(pages):
                rows = [p for _, p in layers if p[0] == page[0] and not p[-1].endswith('— érték')]
                asset(f'page_{i}', lambda layer, p, rows=set(rows): p in rows and 'rejtett' not in layer.name.lower())
            close = [1089, 81, 46, 46]
        else:
            base = asset('base', base_without([top('Kategóriák (Cat)'), named(
                r'^Oldalak \(Inventory_Tab\)$', r'^Rendezés', r'^Frissítés', r'^Tárgyhelyek \(ItemSlot\)$', r'^Zárolt sor \(cover\)$',
                r'^Foglalt hely — (kitöltés|csillanás|érték)$', r'^Kategória$', r'^Alcím$')]))
            for i, kind in enumerate(SPECIAL_TYPES):
                asset(f'cat_{i}', only(*find(f'Menüpont — {kind}', f'Menüpont — {kind} — ikon', f'Menüpont — {kind} — felirat')))
                asset(f'cat_{i}_active', only(*find(f'Menüpont — {kind} (aktív)', f'Menüpont — {kind} — ikon', f'Menüpont — {kind} — felirat')))
            asset('page_active', only(*find('Oldal 1')))
            asset('page_idle', only(*find('Oldal 2')))
            asset('cell', only(*find('Hely 4')))
            asset('capacity_fill', only(*find('Foglalt hely — kitöltés', 'Foglalt hely — csillanás')))
            for name, label, rect in (('safebox', 'Raktár (Safebox_cat)', [59, 759, 262, 54]), ('mall', 'Tárgypiac raktár (Mall_cat)', [59, 827, 262, 54])):
                parts = find(f'{label} — alap', f'{label} — felirat', f'{label} — ikon')
                controls.append(dict(Key=name, Asset=asset('button_' + name, only(*parts)), Rect=rect, DynamicCaption=False))
            close = [1189, 81, 46, 46]
        NexoriaLiveText.prepare(layers, specs, assets, asset)
        config = dict(Source=source.as_posix(), Output=scratch.as_posix(), Specs=specs, LiveText=NexoriaLiveText.VERSION)
        cache_key = hashlib.sha256((json.dumps(config, sort_keys=True) + sha(source) + sha(script)).encode()).hexdigest()
        cached = scratch / (key + '.record.json')
        if cached.exists():
            saved = json.loads(cached.read_text(encoding='utf-8'))
            if saved['CacheKey'] == cache_key and all((scratch / (a['Asset'] + '.cropped.png')).exists() and sha(scratch / (a['Asset'] + '.cropped.png')) == a['Sha256'] for a in saved['Window']['Assets']):
                NexoriaLiveText.refresh(saved['Window']['Assets'], assets)
                windows.append(saved['Window']); print('NEXORIA_CHARACTER_WINDOWS=CACHED ' + key, flush=True); continue
        code = 'var NX_EXPORT=' + json.dumps(config, ensure_ascii=True) + ';\n' + (Path(__file__).parent / 'ExportNexoriaSettings.jsx').read_text(encoding='utf-8')
        jsx = scratch / (key + '.jsx'); jsx.write_text(code, encoding='utf-8')
        subprocess.run(['powershell', '-NoProfile', '-STA', '-ExecutionPolicy', 'Bypass', '-File', str(Path(__file__).parent / 'RunPhotoshopExport.ps1'), '-ScriptPath', str(jsx)], check=True)
        for record in assets:
            with Image.open(scratch / (record['Asset'] + '.png')) as rendered:
                target = scratch / (record['Asset'] + '.cropped.png')
                record.update(Bounds=NexoriaLiveText.crop(record, rendered.convert('RGBA'), target), Sha256=sha(target))
        window = dict(Key=key, Width=psd.width, Height=psd.height, Source=source.as_posix(), SourceSha256=sha(source), GeometrySource=script.as_posix(),
                      GeometrySha256=sha(script), NativePhotoshop=True, Base=base, CloseRect=close, Assets=assets, Controls=controls)
        windows.append(window); cached.write_text(json.dumps(dict(CacheKey=cache_key, Window=window), ensure_ascii=False, indent=2), encoding='utf-8')
        print('NEXORIA_CHARACTER_WINDOWS=RENDERED ' + key, flush=True)
    for window in windows:
        for record in window['Assets']:
            target = output / (record['Asset'] + '.png')
            if target.exists() and sha(target) != prior.get(record['Asset']): raise RuntimeError('Concurrent export edit: ' + str(target))
            shutil.copy2(scratch / (record['Asset'] + '.cropped.png'), target)
            meta = target.with_suffix('.png.meta')
            if not meta.exists(): meta.write_text(re.sub(r'guid: [a-f0-9]+', 'guid: ' + uuid.uuid4().hex, template, count=1), encoding='utf-8')
    keys = {w['Key'] for w in windows}
    old['Windows'] = [w for w in old['Windows'] if w['Key'] not in keys] + windows
    manifest.write_text(json.dumps(old, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')
    print('NEXORIA_CHARACTER_WINDOWS=EXPORTED documents=' + str(len(windows)) + ' assets=' + str(sum(len(w['Assets']) for w in windows)), flush=True)


if __name__ == '__main__':
    main()
