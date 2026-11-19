"""Export the original Nexoria atlas PSD, without sample map markers or character/world values."""
from pathlib import Path
import argparse
import hashlib
import json
import re
import shutil
import subprocess
import uuid

import NexoriaLiveText


def main():
    from psd_tools import PSDImage
    from PIL import Image
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--project', type=Path, default=Path('D:/Auriga-Unity6/SRC/AurigaUnity-20261103'))
    parser.add_argument('--design', type=Path, default=Path('D:/Nexoria UI/Terkep'))
    parser.add_argument('--cache', type=Path, default=Path('build/nexoria-map-export'))
    args = parser.parse_args()
    source = args.design / 'Nexoria_Terkep.psd'
    geometry = args.design / 'scripts/nx_build_terkep.jsx'
    output = args.project / 'Assets/Metin2/Resources/Metin2/NexoriaUI'
    manifest = output / 'item-windows-source.json'
    original = manifest.read_bytes()
    old = json.loads(original)
    prior = {a['Asset']: a['Sha256'] for w in old['Windows'] for a in w['Assets']}
    sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
    source_hash = sha(source)
    psd = PSDImage.open(source)
    scratch = args.cache.resolve()
    scratch.mkdir(parents=True, exist_ok=True)
    specs, assets, controls = [], [], []

    def walk(group, path=()):
        for layer in group:
            p = path + (layer.name,)
            yield layer, p
            if layer.is_group():
                yield from walk(layer, p)
    layers = list(walk(psd))

    def only(*names):
        paths = []
        for name in names:
            hits = [p for _, p in layers if p[-1] == name]
            if len(hits) != 1:
                raise RuntimeError(f'Expected one PSD layer: {name!r}; found {len(hits)}')
            paths.append(hits[0])
        return lambda l, p: l.visible and any(p[:len(q)] == q or q[:len(p)] == p for q in paths)

    def asset(name, keep):
        name = 'nxi_atlas_' + name
        target = output / (name + '.png')
        if target.exists() and sha(target) != prior.get(name):
            raise RuntimeError('Unrecorded artwork edit: ' + name)
        specs.append(dict(Asset=name, Path=[], States=[dict(Path=list(p), Visible=bool(keep(l, p))) for l, p in layers]))
        assets.append(dict(Asset=name))
        return name

    omitted = {'Alcím', 'Térkép neve', 'Térkép adatai', 'Csatorna', 'Csatorna — felirat', 'Koordináta',
               'Keresés — helyőrző',
               'Térkép alap (vágómaszk)', 'Térkép kép (csere)', 'Térkép árnyalat', 'Térkép perem',
               'Nagyítás', 'Nagyítás — ikon', 'Kicsinyítés', 'Kicsinyítés — ikon', 'Középre', 'Középre — ikon'}
    dynamic = lambda n: '— megjelenítés' in n or n.startswith(('Jelölő elhelyezése —', 'Teleportálás —'))
    base = asset('base', lambda l, p: l.visible and p[0] not in ('Jelölők', 'Vezérlők') and not any(n in omitted or dynamic(n) for n in p))
    asset('mask', only('Térkép alap (vágómaszk)'))
    asset('overlay', only('Iránytű', 'Iránytű — észak', 'Iránytű — É'))
    asset('channel', only('Csatorna'))
    asset('coordinates', only('Koordináta háttér'))
    # Kept as an isolated design reference; production always displays the loaded map's own atlas.
    asset('reference', only('Térkép alap (vágómaszk)', 'Térkép kép (csere)', 'Térkép árnyalat'))
    for key, name, y in [('zoom_in', 'Nagyítás', 208), ('zoom_out', 'Kicsinyítés', 264), ('center', 'Középre', 320)]:
        controls.append(dict(Key=key, Asset=asset(key, only(name, name + ' — ikon')), Rect=[804, y, 48, 48], DynamicCaption=False))
    check='Jelmagyarázat — Játékos — megjelenítés'
    asset('check_on', only(check, check+' — pipa'))
    asset('check_off', only(check))
    for key,label in [('player','Játékos'),('party','Csapattag'),('npc','NPC'),('gate','Kapu'),('boss','Főnök'),('quest','Küldetés célpont')]:
        name='Jelmagyarázat — '+label+' — jel'
        asset('marker_'+key,only(name,*([name+' — fazetta'] if key=='player' else [])))
    for key,name,y in [('place_marker','Jelölő elhelyezése',772),('teleport','Teleportálás',836)]:
        parts=[p[-1] for l,p in layers if p[-1].startswith(name+' —') and l.visible]
        controls.append(dict(Key=key,Asset=asset(key,only(*parts)),Rect=[924,y,392,48],DynamicCaption=False))
    NexoriaLiveText.prepare(layers, specs, assets, asset)
    config = dict(Source=source.as_posix(), Output=scratch.as_posix(), Specs=specs, LiveText=NexoriaLiveText.VERSION)
    cache_key = hashlib.sha256((json.dumps(config, sort_keys=True) + source_hash + sha(geometry)).encode()).hexdigest()
    record_path = scratch / 'atlas.record.json'
    saved = json.loads(record_path.read_text(encoding='utf-8')) if record_path.exists() else None
    if saved and saved['CacheKey'] == cache_key and all((scratch / (a['Asset'] + '.cropped.png')).exists() and sha(scratch / (a['Asset'] + '.cropped.png')) == a['Sha256'] for a in saved['Window']['Assets']):
        window = saved['Window']
        NexoriaLiveText.refresh(window['Assets'], assets)
    else:
        jsx = scratch / 'atlas.jsx'
        jsx.write_text('var NX_EXPORT=' + json.dumps(config, ensure_ascii=True) + ';\n' +
                       (Path(__file__).parent / 'ExportNexoriaSettings.jsx').read_text(encoding='utf-8'), encoding='utf-8')
        subprocess.run(['powershell', '-NoProfile', '-STA', '-ExecutionPolicy', 'Bypass', '-File',
                        str(Path(__file__).parent / 'RunPhotoshopExport.ps1'), '-ScriptPath', str(jsx)], check=True)
        for entry in assets:
            with Image.open(scratch / (entry['Asset'] + '.png')) as rendered:
                target = scratch / (entry['Asset'] + '.cropped.png')
                entry.update(Bounds=NexoriaLiveText.crop(entry, rendered.convert('RGBA'), target), Sha256=sha(target))
        window = dict(Key='atlas', Width=psd.width, Height=psd.height, Source=source.as_posix(), SourceSha256=source_hash,
                      GeometrySource=geometry.as_posix(), GeometrySha256=sha(geometry), NativePhotoshop=True,
                      Base=base, CloseRect=[1289, 81, 46, 46], MapRect=[76, 192, 792, 696], Assets=assets, Controls=controls)
        record_path.write_text(json.dumps(dict(CacheKey=cache_key, Window=window), ensure_ascii=False, indent=2), encoding='utf-8')
    if sha(source) != source_hash or manifest.read_bytes() != original:
        raise RuntimeError('Source PSD or shared manifest changed during export')
    template = (output / 'nx_panel_sub.png.meta').read_text(encoding='utf-8')
    for entry in window['Assets']:
        target = output / (entry['Asset'] + '.png')
        shutil.copy2(scratch / (entry['Asset'] + '.cropped.png'), target)
        meta = target.with_suffix('.png.meta')
        if not meta.exists():
            meta.write_text(re.sub(r'guid: [a-f0-9]+', 'guid: ' + uuid.uuid4().hex, template, count=1), encoding='utf-8')
    old['Windows'] = [w for w in old['Windows'] if w['Key'] != 'atlas'] + [window]
    manifest.write_text(json.dumps(old, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print('NEXORIA_MAP=EXPORTED assets=' + str(len(window['Assets'])), flush=True)


if __name__ == '__main__':
    main()
