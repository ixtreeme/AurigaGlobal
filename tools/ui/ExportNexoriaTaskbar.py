"""Export authored Taskbar PSD layers; requires psd-tools[composite].

Existing output is overwritten only when it matches the previous provenance
manifest. Unity texture GUIDs are preserved across repeated exports.
"""
from pathlib import Path
import argparse
import hashlib
import json
import re
import uuid


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=Path('D:/Nexoria UI/Taskbar/Nexoria_Taskbar.psd'))
    parser.add_argument('--project', type=Path, default=Path('D:/Auriga-Unity6/SRC/AurigaUnity-20261103'))
    args = parser.parse_args()
    from psd_tools import PSDImage
    psd = PSDImage.open(args.source)

    def layer_at(*names):
        layer = psd
        for name in names:
            layer = next(item for item in layer if item.name == name)
        return layer

    out = args.project / 'Assets/Metin2/Resources/Metin2/NexoriaUI'
    manifest_path = out / 'taskbar-source.json'
    previous = {row['asset']: row['sha256'] for row in json.loads(manifest_path.read_text(encoding='utf-8'))} if manifest_path.exists() else {}
    sha = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
    groups = [
        ('nxt_taskbar_hero', layer_at('Karakterkeret', 'Háttér'), None),
        ('nxt_taskbar_system', layer_at('Menü', 'Háttér'), None),
        ('nxt_taskbar_floor', layer_at('Alapsáv'), None),
        ('nxt_taskbar_centre', layer_at('Képességsáv'), {'Emelt panel', 'Kristály szárnyak', 'Kristály', 'Kristály — fazetta'}),
        ('nxt_taskbar_portrait', layer_at('Karakterkeret', 'Portré'), None),
    ]
    # Validate every destination before replacing any existing export.
    for name, _, _ in groups:
        target = out / (name + '.png')
        if target.exists() and sha(target) != previous.get(target.name):
            raise RuntimeError('Unrecorded output edits: ' + str(target))
    template = (out / 'nx_panel_sub.png.meta').read_text(encoding='utf-8')
    manifest = []
    for name, layer, allowed in groups:
        layer_filter = (lambda item: item.name != 'Szint jelvény') if name == 'nxt_taskbar_portrait' else None if allowed is None else lambda item: item is layer or item.name in allowed
        image = layer.composite(layer_filter=layer_filter)
        if image is None:
            raise RuntimeError('Empty PSD composite: ' + name)
        target = out / (name + '.png')
        image.save(target)
        meta = target.with_suffix('.png.meta')
        if not meta.exists():
            meta.write_text(re.sub(r'guid: [a-f0-9]+', 'guid: ' + uuid.uuid4().hex, template, count=1), encoding='utf-8')
        manifest.append(dict(asset=target.name, psd=args.source.as_posix(), layer=layer.name, bounds=layer.bbox,
                             visible_layers=sorted(allowed) if allowed else None, sha256=sha(target)))
    manifest_path.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')
    print('TASKBAR_PSD_LAYERS_EXPORTED=' + str(len(manifest)))


if __name__ == '__main__':
    main()
