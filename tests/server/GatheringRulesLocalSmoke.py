"""Owned-local catalog/rules checks. --exercise-save rewrites identical bytes only."""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import sys
from unittest import mock

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / 'SRC/Tools/ServerManager'))
import backend as api
import item_bench
import item_catalog


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--package', type=Path, required=True)
    parser.add_argument('--catalog-tool', type=Path, required=True)
    parser.add_argument('--evidence', type=Path, required=True)
    modes = parser.add_mutually_exclusive_group()
    modes.add_argument('--expect-running', action='store_true')
    modes.add_argument('--exercise-save', action='store_true')
    args = parser.parse_args()
    root, manager, _, auth = item_bench.connection(args.package)
    tool = args.catalog_tool.resolve(); assert tool.is_file()
    relative = 'data/typed_gathering.json'; document = api.Document(root, relative)
    original = document.original; valid = json.loads(document.text)
    assert valid['sites'], 'Use an existing local gathering fixture, never invent live content'
    checks = 0

    def check(condition, message):
        nonlocal checks
        checks += 1
        if not condition: raise AssertionError(message)

    def rejected(callback, message, errors=api.EditorError):
        try: callback()
        except errors: check(True, message)
        else: raise AssertionError(message)

    with mock.patch.object(item_catalog, 'helper', return_value=tool):
        packet = item_catalog.run_tool(dict(action='read', connection=auth))
        def validate(text):
            return item_catalog.run_tool(dict(action='validate_gathering', tables=packet['tables'], rules_json=text), packet['columns'])
        result = validate(document.text)
        check(result['sites'] == len(valid['sites']), 'Existing world rules must match the runtime parser')
        check(validate('{"version":2,"sites":[]}')['sites'] == 0, 'Explicitly disabled world rules')
        unknown = copy.deepcopy(valid)
        existing = {(int(row['category_id']), int(row['local_id'])) for row in item_catalog.Catalog(packet).items()}
        reward = unknown['sites'][0]['rewards'][0]
        reward['local'] = 4294967295
        while (reward['category'], reward['local']) in existing: reward['local'] -= 1
        duplicates = copy.deepcopy(valid); duplicates['sites'].append(copy.deepcopy(duplicates['sites'][0]))
        v1water = copy.deepcopy(valid); v1water['version'] = 1; v1water['sites'][0].update(race=0, kind=2, pool=2)
        invalid = [json.dumps(unknown), json.dumps(duplicates), json.dumps(v1water),
            '{"version":1,"version":2,"sites":[]}', json.dumps(dict(valid, version=3))]
        for text in invalid: rejected(lambda text=text: validate(text), 'Native rules must reject malformed/unknown content')
        running = bool(api.describe(root)['running'])
        if args.expect_running:
            check(running, 'The owned stack must be running for this guard test')
            rejected(lambda: api.save_document(root, relative, original, document.digest), 'Running-server save must be rejected', (api.EditorError, RuntimeError))
        if args.exercise_save:
            check(not running, 'Stop the owned stack before exercising the editor save path')
            before = set(api.backup_files(root, relative))
            for text in invalid:
                rejected(lambda text=text: api.save_document(root, relative, document.encoded(text), document.digest), 'Invalid data must never reach the file')
                check(document.path.read_bytes() == original, 'Rejected save must preserve original bytes')
            check(set(api.backup_files(root, relative)) == before, 'Rejected saves must not create backups')
            rejected(lambda: api.save_document(root, relative, original, '0' * 64), 'Stale document must never overwrite content')
            output = api.save_document(root, relative, original, document.digest)
            backups = set(api.backup_files(root, relative)) - before
            check(len(backups) == 1 and next(iter(backups)).read_bytes() == original, 'Exactly one byte-exact original backup')
            check('MENTVE' in output, 'The real atomic save path must finish')
        check(document.path.read_bytes() == original, 'No world content changes')
        after = item_catalog.run_tool(dict(action='read', connection=auth))
        check(after['tables'] == packet['tables'], 'All catalog tables must be unchanged')
        check(after['hash'] == packet['hash'], 'Catalog hash must be unchanged')
    evidence = dict(checks=checks, sites=result['sites'], catalog_unchanged=True,
        world_bytes_unchanged=True, running_guard=args.expect_running,
        atomic_save_and_rejected_writes=args.exercise_save,
        rules_sha256=hashlib.sha256(original).hexdigest())
    args.evidence.write_text(json.dumps(evidence, indent=2) + '\n', encoding='utf-8')
    print('GATHERING_EDITOR_LOCAL=SUCCESS', json.dumps(evidence))


if __name__ == '__main__': main()
