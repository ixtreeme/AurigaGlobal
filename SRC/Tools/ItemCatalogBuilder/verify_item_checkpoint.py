"""Restore a captured item-migration checkpoint into a NEW owned sandbox.

Never accepts an existing SQL endpoint. Original data and runtime are untouched.
"""
import argparse
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[3]
sys.path[:0] = [str(Path(__file__).resolve().parent), str(ROOT / 'db/tools')]
from capture_legacy_items import digest, identifier, table_input, write_json, owner_keys
from backup_sandbox import Sandbox


def verify_files(source, manifest):
    if manifest.get('format_version') != 1 or manifest.get('complete') is not True:
        raise ValueError('Incomplete or unknown checkpoint format')
    for relative, expected in manifest['files'].items():
        path = (source / relative).resolve()
        if not path.is_relative_to(source) or not path.is_file() or digest(path) != expected:
            raise ValueError('Checkpoint file is missing or changed: ' + relative)


def restore_and_verify(source, work, binaries):
    source = Path(source).resolve()
    manifest = json.loads((source / 'manifest.json').read_text(encoding='utf-8'))
    verify_files(source, manifest)
    sandbox = Sandbox(work, binaries)
    try:
        sandbox.start()
        sandbox.run('mariadb', [*sandbox.cli_args(), '--default-character-set=utf8mb4', '--binary-mode'],
                    'restore-checkpoint', input_file=source / 'checkpoint.sql')
        sandbox.verify()
        counts = {}
        for qualified, expected in manifest['row_counts'].items():
            schema, table = qualified.split('.')
            count = int(sandbox.client.query('SELECT COUNT(*) FROM ' + identifier(schema) + '.' + identifier(table))[0][0])
            if count != expected:
                raise ValueError('Restored row count mismatch: ' + qualified)
            counts[qualified] = count
        exact = {}
        for qualified, info in manifest['tables'].items():
            schema, table = qualified.split('.')
            original = json.loads((source / info['file']).read_text(encoding='utf-8'))
            restored = table_input(sandbox.client, schema, table)
            if original != restored:
                raise ValueError('Restored metadata or cell content mismatch: ' + qualified)
            exact[qualified] = len(restored['rows'])
        result = dict(complete=True, checkpoint_sha256=digest(source / 'checkpoint.sql'),
                      restored_base_tables=len(counts), exact_input_tables=exact, row_counts=counts)
        owners=owner_keys(sandbox.client)
        if 'owner_keys' in manifest and manifest['owner_keys']!=owners:
            raise ValueError('Restored owner identities differ from the checkpoint')
        supplement=dict(format_version=1,checkpoint_sha256=result['checkpoint_sha256'],owner_keys=owners)
        write_json(Path(work)/'ownership-supplement.json',supplement)
        result['ownership_supplement_sha256']=digest(Path(work)/'ownership-supplement.json')
        write_json(Path(work) / 'restore-verification.json', result)
        return result
    finally:
        sandbox.stop()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', required=True, type=Path)
    parser.add_argument('--work', required=True, type=Path)
    parser.add_argument('--mariadb-bin', required=True, type=Path)
    args = parser.parse_args()
    result = restore_and_verify(args.source, args.work, args.mariadb_bin)
    print(json.dumps(dict(complete=result['complete'], restored_base_tables=result['restored_base_tables'],
                          exact_input_tables=len(result['exact_input_tables'])), sort_keys=True))


if __name__ == '__main__':
    main()
