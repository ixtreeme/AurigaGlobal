"""Apply audited dungeon sources to a newly created package, preserving custom Lua."""
import hashlib
import json
from pathlib import Path, PurePosixPath

from .legacy import PackageError


RESOURCES = Path(__file__).resolve().parents[2] / 'Server/Resources'


def checksum(raw):
    return hashlib.sha256(raw).hexdigest()


def apply_known_dungeon_fixes(data, resources=None):
    """Called only for fresh package output, after legacy resource path conversion.

    Unknown/custom source versions are reported without replacement. Validate the
    entire resource set before writing, so a corrupt bundle cannot partly apply.
    """
    data = Path(data).resolve()
    resources = Path(resources or RESOURCES).resolve()
    manifest = json.loads((resources / 'dungeon-fixes.json').read_text(encoding='utf-8'))
    if manifest.get('version') != 1:
        raise PackageError('Unsupported dungeon fixes manifest')
    report = dict(applied=[], current=[], custom=[], missing=[])
    pending = []
    for relative, entry in manifest['files'].items():
        path = PurePosixPath(relative)
        if (path.is_absolute() or '..' in path.parts or '\\' in relative or
                path.parts[:3] != ('quest', 'Quest', 'dungeon') or path.suffix != '.lua'):
            raise PackageError('Invalid dungeon resource path: ' + relative)
        source, target = resources / relative, data / relative
        if not source.resolve().is_relative_to(resources) or not target.resolve().is_relative_to(data):
            raise PackageError('Linked dungeon resource outside package: ' + relative)
        if source.is_symlink() or target.is_symlink():
            raise PackageError('Linked dungeon resource: ' + relative)
        replacement = source.read_bytes().replace(b'\r\n', b'\n')
        if checksum(replacement) != entry['fixed_sha256']:
            raise PackageError('Dungeon resource checksum mismatch: ' + relative)
        if not target.exists():
            report['missing'].append(relative)
            continue
        raw = target.read_bytes()
        if checksum(raw.replace(b'\r\n', b'\n')) == entry['fixed_sha256']:
            report['current'].append(relative)
        elif (checksum(raw) == entry['previous_sha256'] or
              checksum(raw.replace(b'\r\n', b'\n')) == entry['previous_lf_sha256']):
            pending.append((target, replacement))
            report['applied'].append(relative)
        else:
            report['custom'].append(relative)
    for target, replacement in pending:
        target.write_bytes(replacement)
    return report
