"""Create an isolated source snapshot for unattended Unity item validation.

The user's open project is read only. This is a validation project, not a
playable asset build: copy production C# sources, assembly/plugin metadata,
locale, UI font, golden fixtures and the artwork used by those fixtures.
It runs the same TLS validators
against the owned local server without a second instance of the open project.
"""
import argparse
import hashlib
import json
import re
from pathlib import Path
import shutil
import subprocess


def ui_dependencies(source):
    """Keep the real UI catalog and its atlases for InitializeOnLoad builders.

    A source-only project must not trip a batch-fatal catalog regeneration
    merely because the source project's already generated images were omitted.
    """
    catalog = source / 'Assets/Metin2/Resources/Metin2UiSpriteCatalog.asset'
    manifest = source / 'Assets/Metin2/Generated/UI/Layouts/extra-sprites.json'
    selected = {path for path in (catalog, manifest) if path.is_file()}
    if manifest.is_file():
        for row in json.loads(manifest.read_text(encoding='utf-8-sig')).get('sprites', []):
            path = (source / row['atlas']).resolve()
            if not path.is_relative_to(source):
                raise ValueError('UI atlas escapes source project')
            if path.is_file():
                selected.add(path)
    if catalog.is_file():
        needed = set(re.findall(r'(?:texture|alphaTexture): \{fileID: \d+, guid: ([0-9a-f]{32})', catalog.read_text(encoding='utf-8-sig')))
        for path in tuple(selected):
            meta = path.with_name(path.name + '.meta')
            if meta.is_file():
                match = re.search(r'^guid: ([0-9a-f]{32})$', meta.read_text(encoding='utf-8-sig'), re.MULTILINE)
                if match:
                    needed.discard(match[1])
        if needed:
            # rg searches the GUID text directly; opening every large model
            # importer .meta through Python is needlessly slow on Windows.
            found = subprocess.run(['rg', '-l', '--hidden', '--no-ignore', '-g', '*.meta',
                                    'guid: (' + '|'.join(sorted(needed)) + ')', 'Assets'],
                                   cwd=source, capture_output=True, text=True, encoding='utf-8')
            if found.returncode not in (0, 1):
                raise RuntimeError('Cannot locate UI catalog texture metadata')
            for relative in found.stdout.splitlines():
                meta = source / relative
                match = re.search(r'^guid: ([0-9a-f]{32})$', meta.read_text(encoding='utf-8-sig', errors='replace'), re.MULTILINE)
                if match and match[1] in needed:
                    selected.add(meta.with_suffix(''))
                    needed.remove(match[1])
                    if not needed:
                        break
        if needed:
            raise ValueError('Missing source UI catalog texture dependencies: ' + str(len(needed)))
    return selected


def prepare(source, output, changeset):
    source = source.resolve()
    output = output.resolve()
    if not (source / 'ProjectSettings/ProjectVersion.txt').is_file():
        raise ValueError('Not a Unity source project')
    if source == output or source in output.parents or output in source.parents:
        raise ValueError('Snapshot must be outside the source project')
    if output.exists():
        raise FileExistsError('Use a fresh snapshot path; existing files are never removed')
    output.mkdir(parents=True)
    copied = []

    def copy(path):
        relative = path.relative_to(source)
        target = output / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, target)
        copied.append(dict(path=relative.as_posix(), sha256=hashlib.sha256(path.read_bytes()).hexdigest()))

    selected = set()
    for path in (source / 'Assets').rglob('*'):
        if not path.is_file():
            continue
        relative = path.relative_to(source).as_posix()
        is_code = path.suffix.lower() in {'.cs', '.asmdef', '.asmref', '.rsp', '.dll'}
        is_fixture = relative.startswith('Assets/Metin2/Editor/Fixtures/')
        is_resource = relative.startswith((
            'Assets/Metin2/Resources/Metin2/Fonts/',
            'Assets/Metin2/Resources/Metin2/Locales/',
        ))
        is_artwork = relative in {
            'Assets/Metin2/GeneratedData/Metin2ItemCatalog.asset',
            'Assets/Metin2/Generated/ItemIcons/icon/item/00010.tga',
        }
        if is_code or is_fixture or is_resource or is_artwork:
            selected.add(path)
    selected.update(ui_dependencies(source))
    for path in tuple(selected):
        if path.suffix != '.meta' and path.with_name(path.name + '.meta').is_file():
            selected.add(path.with_name(path.name + '.meta'))
        for parent in path.parents:
            if parent == source:
                break
            meta = parent.with_name(parent.name + '.meta')
            if meta.is_file():
                selected.add(meta)
    for path in sorted(selected):
        copy(path)
    for path in (source / 'ProjectSettings').rglob('*'):
        if path.is_file():
            copy(path)
    # Include embedded package forks as well as the actual working manifests.
    # Substituting registry versions would compile different production code.
    for path in (source / 'Packages').rglob('*'):
        if path.is_file():
            copy(path)
    (output / 'Temp').mkdir()
    marker = dict(source=str(source), changeset=changeset, files=copied)
    (output / 'validation-source-snapshot.json').write_text(json.dumps(marker, indent=2), encoding='utf-8')
    # The watchdog exists only in this disposable snapshot. It may never exit
    # the user's open Editor and does nothing outside a marked batch project.
    watchdog = output / 'Assets/Editor/AurigaValidationSnapshotExit.cs'
    watchdog.parent.mkdir(parents=True, exist_ok=True)
    watchdog.write_text('''using System.IO;
using UnityEditor;
using UnityEngine;
[InitializeOnLoad]
internal static class AurigaValidationSnapshotExit
{
    [System.Serializable] private sealed class Result { public bool Success; }
    static AurigaValidationSnapshotExit()
    {
        if (Application.isBatchMode && File.Exists("validation-source-snapshot.json"))
            EditorApplication.update += Poll;
    }
    private static void Poll()
    {
        const string result = "Temp/typed-shop-validation-result.json";
        if (!File.Exists(result)) return;
        EditorApplication.update -= Poll;
        File.Copy(result, "validation-source-snapshot-result.json", true);
        EditorApplication.Exit(JsonUtility.FromJson<Result>(File.ReadAllText(result)).Success ? 0 : 1);
    }
}
''', encoding='utf-8')
    print('UNITY_VALIDATION_SNAPSHOT=READY files=' + str(len(copied)) + ' output=' + str(output))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--changeset', type=int, required=True)
    options = parser.parse_args()
    prepare(options.source, options.output, options.changeset)
