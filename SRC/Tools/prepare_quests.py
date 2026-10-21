"""Prepare a separate Lua 5.5 quest tree; never change the supplied source.

The historical DSL is parsed in memory by the server, so no object files are
generated. --duplicate-policy last reproduces the old sequential QC overwrite.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess


def native(checker, *args):
    result = subprocess.run([str(checker), *map(str, args)], capture_output=True,
                            timeout=30, creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)
    if result.returncode:
        raise ValueError((result.stdout + result.stderr).decode('utf-8', errors='replace').strip())
    return result.stdout.decode('utf-8')


def reconcile(root, checker, legacy=None, duplicate_policy='error'):
    """Validate an already copied tree and rewrite only its quest_list."""
    root, checker = Path(root).resolve(), Path(checker).resolve()
    if duplicate_policy not in ('error', 'last'):
        raise ValueError('Unknown duplicate policy')
    rows = (root / 'quest_list').read_text(encoding='utf-8-sig').splitlines()
    parsed, last, paths, disabled, state_checks = {}, {}, set(), [], 0
    for number, line in enumerate(rows):
        value = line.strip()
        if not value or value.startswith('#'):
            continue
        path = (root / value).resolve(strict=True)
        if not path.is_relative_to(root) or path == root or Path(value).is_absolute():
            raise ValueError('quest_list source outside quest root: ' + value)
        if path in paths:
            raise ValueError('Repeated quest_list path: ' + value)
        paths.add(path)
        metadata = json.loads(native(checker, '--inspect-quest', path))
        name = metadata['id']
        if name in last and duplicate_policy == 'error':
            raise ValueError('Duplicate quest id: ' + name)
        if name in last:
            prior = last[name]
            disabled.append(dict(quest=name, disabled=rows[prior].strip(), retained=value,
                                 reason='last QC definition wins'))
        parsed[number] = metadata
        last[name] = number
    # Retain the stored progress identifiers, including signed CRC32 state IDs.
    if legacy:
        legacy = Path(legacy).resolve()
        for name, index in last.items():
            path = legacy / 'object/state' / name
            if not path.is_file():
                continue
            old = dict((key, int(value)) for key, value in
                       re.findall(r'\[\s*["\']([A-Za-z_][A-Za-z_0-9]*)["\']\s*\]\s*=\s*(-?\d+)',
                                  path.read_text(encoding='latin1')))
            if not old or old.get('start') != 0:
                raise ValueError('Cannot verify legacy state table: ' + name)
            for state, expected in old.items():
                if parsed[index]['states'].get(state) != expected:
                    raise ValueError('Stored state ID changed: ' + name + '.' + state)
                state_checks += 1
    output = []
    for index, row in enumerate(rows):
        metadata = parsed.get(index)
        if metadata and last[metadata['id']] != index:
            row = '# Lua55 migration: superseded by later quest id: ' + row
        output.append(row)
    (root / 'quest_list').write_text('\n'.join(output) + '\n', encoding='utf-8', newline='\n')
    changes = []
    library = root / 'questlib.lua'
    if library.is_file():
        raw = library.read_bytes()
        # Lua 5.0 supplied an implicit `arg` table for varargs. Lua 5.5 does not.
        # Match the known helper only; preserve the file's original byte encoding.
        pattern = rb"(function\s+select\s*\(\s*\.\.\.\s*\)\s*return\s+q\.yield\(\s*['\"]select['\"]\s*,\s*)arg(\s*\)\s*end)"
        updated, count = re.subn(pattern, rb'\g<1>{...}\g<2>', raw)
        if count:
            library.write_bytes(updated)
            changes.append(dict(file='questlib.lua', change='explicit select varargs', replacements=count))
    result = native(checker, '--check-quests', root).strip()
    return dict(runtime='Lua 5.5.1', source_count=len(parsed), active_quests=len(last),
                disabled_duplicates=disabled, preserved_state_ids=state_checks, lua_changes=changes, check=result)


def prepare(source, output, checker, duplicate_policy='error'):
    source, output = Path(source).resolve(strict=True), Path(output).resolve()
    if output.is_relative_to(source) or source.is_relative_to(output):
        raise ValueError('Output must be separate from the original quest tree')
    if output.exists():
        raise FileExistsError('Output already exists: ' + str(output))
    excluded = {'object', 'pre_qc', '__pycache__', '.git', '.plastic'}
    before = {}
    for path in source.rglob('*'):
        relative = path.relative_to(source)
        if any(part in excluded for part in relative.parts):
            continue
        if path.is_symlink() or (hasattr(path, 'is_junction') and path.is_junction()):
            raise ValueError('Linked quest resource rejected: ' + str(relative))
        if path.is_file() and path.name.lower() not in ('qc', 'qc.exe', 'pqc', 'pqc.exe', 'pre_qc.py'):
            before[relative] = hashlib.sha256(path.read_bytes()).hexdigest()
    output.mkdir(parents=True, exist_ok=False)
    for relative, checksum in before.items():
        target = output / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source / relative, target)
        if hashlib.sha256(target.read_bytes()).hexdigest() != checksum:
            raise ValueError('Quest changed during copy: ' + str(relative))
    report = reconcile(output, checker, source, duplicate_policy)
    for relative, checksum in before.items():
        if hashlib.sha256((source / relative).read_bytes()).hexdigest() != checksum:
            raise ValueError('Original quest changed during migration: ' + str(relative))
    report['source_hashes'] = {p.as_posix(): h for p, h in before.items()}
    (output / 'lua55-migration.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--checker', required=True, type=Path)
    parser.add_argument('--duplicate-policy', choices=('error', 'last'), default='error')
    args = parser.parse_args()
    report = prepare(args.source, args.output, args.checker, args.duplicate_policy)
    print(report['check'])
    print('Preserved state IDs:', report['preserved_state_ids'])
    for row in report['disabled_duplicates']:
        print('Duplicate:', row['quest'], 'retained:', row['retained'], 'disabled:', row['disabled'])


if __name__ == '__main__':
    main()
