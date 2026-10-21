"""Add a built manager to an unchanged server release, preserving credentials."""
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import shutil
import secrets
import sys


def digest(path):
    with path.open('rb') as handle:
        return hashlib.file_digest(handle, 'sha256').hexdigest()


def members(root):
    for path in sorted(root.rglob('*')):
        relative = path.relative_to(root)
        if relative.parts[0] == 'run' or path.name in ('package-manifest.json', 'server-files-ready.json'):
            continue
        if path.is_file():
            yield relative.as_posix(), path


def install(root, bundle):
    root, bundle = root.resolve(), bundle.resolve()
    if not (root / '.auriga-package.json').is_file():
        raise ValueError('Not an owned server package')
    if not (bundle / 'AurigaServerManager.exe').is_file():
        raise ValueError('Missing built manager executable')
    for name in ('manager', 'server_manager.md', 'Szerverkezelo.cmd'):
        if (root / name).exists():
            raise FileExistsError('Manager output already exists: ' + name)
    manifest = json.loads((root / 'package-manifest.json').read_text(encoding='utf-8'))
    current = {relative: path for relative, path in members(root)}
    if set(current) != set(manifest['files']):
        raise ValueError('Release contents differ from the original manifest; install into an unchanged release')
    for relative, expected in manifest['files'].items():
        if digest(current[relative]) != expected:
            raise ValueError('Release file changed: ' + relative)
    shutil.copytree(bundle, root / 'manager')
    source = Path(__file__).resolve().parent
    shutil.copytree(source, root / 'manager/source', ignore=shutil.ignore_patterns('__pycache__', '*.pyc'))
    repo = source.parents[2]
    shutil.copy2(repo / 'docs/server_manager.md', root / 'server_manager.md')
    with (root / 'Szerverkezelo.cmd').open('x', encoding='ascii', newline='') as handle:
        handle.write('@echo off\r\nstart "" "%~dp0manager\\AurigaServerManager.exe" --package "%~dp0."\r\n')
    with (root / 'README.md').open('a', encoding='utf-8') as handle:
        handle.write('\n## Grafikus Qt6 szerverkezelő\n\nIndítás: **Szerverkezelo.cmd** vagy '
                     '**manager/AurigaServerManager.exe**. A grafikus kezelő külön Qt/Python telepítést nem igényel. '
                     'Részletek: [server_manager.md](server_manager.md).\n')
    new_manifest = dict(version=manifest['version'], files={relative: digest(path) for relative, path in members(root)})
    (root / 'package-manifest.json').write_text(json.dumps(new_manifest, indent=2) + '\n', encoding='utf-8')
    print('GUI_INSTALLED files=%d package=%s' % (len(new_manifest['files']), root))


def update(root, bundle):
    """Replace only manifest-owned GUI assets; retain edited server/game files."""
    root, bundle = root.resolve(), bundle.resolve()
    if not (root / '.auriga-package.json').is_file() or not (root / 'manager/AurigaServerManager.exe').is_file():
        raise ValueError('An installed manager in an owned server package is required')
    if not (bundle / 'AurigaServerManager.exe').is_file():
        raise ValueError('Missing built manager executable')
    source = Path(__file__).resolve().parent
    incoming = {'manager/' + path.relative_to(bundle).as_posix(): path
                for path in bundle.rglob('*') if path.is_file()}
    incoming.update({'manager/source/' + path.name: path for path in source.iterdir()
                     if path.is_file() and path.suffix in ('.py', '.txt', '.cs')})
    incoming['server_manager.md'] = source.parents[2] / 'docs/server_manager.md'
    manifest = json.loads((root / 'package-manifest.json').read_text(encoding='utf-8'))
    changed = []
    for relative, original in incoming.items():
        target = root / relative
        if not target.resolve().is_relative_to(root):
            raise ValueError('Manager update target must stay inside the package')
        if target.exists():
            current = digest(target)
            if manifest['files'].get(relative) != current:
                raise ValueError('Manager file was edited outside the release: ' + relative)
            if current == digest(original):
                continue
        if target.is_file() and target.suffix.lower() in ('.exe', '.dll', '.pyd'):
            # Windows refuses write access to an executable loaded by an open
            # GUI. Probe before changing any files; never terminate that GUI.
            with target.open('r+b'):
                pass
        changed.append(relative)
    stamp = datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S-') + secrets.token_hex(4)
    backup = root / 'run/manager-updates' / stamp
    for relative in changed:
        target = root / relative
        if target.is_file():
            previous = backup / relative
            previous.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(target, previous)
    for relative in changed:
        target = root / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        pending = target.with_name(target.name + '.update-' + secrets.token_hex(4))
        try:
            shutil.copy2(incoming[relative], pending)
            os.replace(pending, target)
        finally:
            pending.unlink(missing_ok=True)
        manifest['files'][relative] = digest(target)
    # Only GUI hashes change. The baseline hashes of server.json and data stay
    # as they were, even when the operator has edited those files through GUI.
    temporary = root / ('package-manifest.json.update-' + secrets.token_hex(4))
    try:
        temporary.write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
        os.replace(temporary, root / 'package-manifest.json')
    finally:
        temporary.unlink(missing_ok=True)
    print('GUI_UPDATED files=%d previous_gui=%s' % (len(changed), backup))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--package', required=True, type=Path)
    parser.add_argument('--bundle', required=True, type=Path)
    parser.add_argument('--update', action='store_true', help='Update owned GUI files; preserve server config and data')
    args = parser.parse_args()
    try:
        (update if args.update else install)(args.package, args.bundle)
    except (OSError, ValueError, KeyError) as error:
        print('INSTALL_FAILED:', error, file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
