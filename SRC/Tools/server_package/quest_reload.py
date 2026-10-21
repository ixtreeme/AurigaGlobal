"""Durable, all-core quest publication over the package's private filesystem.

No sockets or shell commands. A single atomic current.json is the commit
decision. Before it is published, every participant must acknowledge the same
immutable snapshot. After publication a timeout is pending, never a rollback.
"""
import hashlib
import json
import os
from pathlib import Path
import re
import secrets
import time


def digest(value):
    return hashlib.sha256(value).hexdigest()


def read(path):
    if path.is_symlink() or not path.is_file() or path.stat().st_size > 4 * 1024 * 1024:
        raise RuntimeError('Invalid quest control document')
    return json.loads(path.read_text(encoding='utf-8'))


def roots(api, cfg, names):
    result = set()
    for name in names:
        merged = dict(cfg.get('game_defaults', {}))
        core = cfg['game_servers'][name]
        data = core.get('server', {}).get('data_path', merged.get('server', {}).get('data_path', 'data'))
        quest = core.get('quest', {}).get('dir', merged.get('quest', {}).get('dir'))
        data = (api.ROOT / data).resolve(strict=True)
        quest = (api.ROOT / quest).resolve(strict=True) if quest else data / 'quest'
        result.add((data, quest))
    if len(result) != 1:
        raise RuntimeError('Running cores must share one data/quest source tree')
    return result.pop()


def participants(api):
    cfg = api.config()
    state = api.read_json(api.ROOT / 'run/supervisor.json')
    if state.get('phase') != 'listening':
        raise RuntimeError('Server stack is stopping; quest reload is unavailable')
    children = [c for c in state['children'] if c['name'] in cfg['game_servers']]
    if not children:
        raise RuntimeError('No running game cores')
    result = []
    for child in children:
        if not api.owned(child['identity']):
            raise RuntimeError('A managed game core is unavailable: ' + child['name'])
        directory = api.instance_directory(cfg, child['name'])
        if directory != Path(child['directory']).resolve():
            raise RuntimeError('Game core working directory changed')
        token = child.get('token', '')
        if not re.fullmatch('[a-f0-9]{48}', token):
            raise RuntimeError('Game core has no valid control token')
        result.append(dict(child, directory=directory, token_hash=digest(token.encode('ascii'))))
    return cfg, result


def matches(value, child):
    return (value.get('schema_version') == 1 and value.get('pid') == child['identity']['pid']
            and value.get('instance') == child['name'] and value.get('token_hash') == child['token_hash'])


def core_status(child):
    value = read(child['directory'] / '.auriga-quest-status.json')
    if (not matches(value, child) or value.get('protocol') != 1 or
            abs(time.time() - value.get('updated_at', 0)) > 30):
        raise RuntimeError('Core requires the hotreload-enabled server build: ' + child['name'])
    return value


def status(api):
    try:
        _, children = participants(api)
        rows = [dict(name=c['name'], **core_status(c)) for c in children]
        return dict(available=True, cores=rows)
    except (OSError, ValueError, KeyError, TypeError, RuntimeError):
        return dict(available=False, cores=[])


def current(api):
    path = api.ROOT / 'run/quest-reloads/current.json'
    if not path.exists():
        return None
    value = read(path)
    identity = value.get('id', '')
    if value.get('schema_version') != 1 or not re.fullmatch('[a-f0-9]{32}', identity):
        raise RuntimeError('Invalid committed quest publication')
    ids = value.get('quest_ids')
    if (not re.fullmatch('[a-f0-9]{64}', value.get('revision', '')) or not isinstance(ids, dict) or
            any(not name or type(index) is not int or not 1 <= index <= 65535 for name, index in ids.items()) or
            len(set(ids.values())) != len(ids)):
        raise RuntimeError('Invalid committed quest revision/IDs')
    expected = api.ROOT / 'run/quest-reloads' / identity / 'data'
    if expected.resolve(strict=True) != expected or Path(value['snapshot']).resolve(strict=True) != expected:
        raise RuntimeError('Committed quest snapshot is outside its transaction directory')
    return value


def snapshot(api, data, quest, transaction):
    checker = api.executable('ServerPackageCheck')
    inventory = api.native([checker, '--quest-fingerprint', data, quest])
    if inventory.returncode:
        raise RuntimeError('Cannot capture quest source inventory: ' + inventory.stderr.strip())
    inventory = json.loads(inventory.stdout)
    target = transaction / 'data'
    target.mkdir(parents=True, exist_ok=False, mode=0o700)
    if target.resolve() != target:
        raise RuntimeError('Linked transaction directory rejected')
    for relative, metadata in inventory['files'].items():
        name = Path(relative)
        if name.is_absolute() or '..' in name.parts or '\\' in relative:
            raise RuntimeError('Invalid quest inventory path')
        source = quest / name.relative_to('quest') if name.parts[0] == 'quest' else data / name
        base = quest if name.parts[0] == 'quest' else data
        if source.is_symlink() or not source.resolve(strict=True).is_relative_to(base):
            raise RuntimeError('Linked quest resource rejected')
        raw = source.read_bytes()
        if len(raw) != metadata['size'] or digest(raw) != metadata['sha256']:
            raise RuntimeError('Source changed during quest capture: ' + relative)
        output = target / name
        output.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
        with output.open('xb') as handle:
            handle.write(raw)
            handle.flush()
            os.fsync(handle.fileno())
        output.chmod(0o400)
    verification = api.native([checker, '--quest-fingerprint', target, target / 'quest'])
    if verification.returncode or json.loads(verification.stdout) != inventory:
        raise RuntimeError('Captured quest snapshot checksum mismatch')
    validation = api.native([checker, '--check-libraries', target])
    if validation.returncode:
        raise RuntimeError('Quest library/source validation failed: ' + validation.stderr.strip())
    api.save_json(transaction / 'manifest.json', inventory)
    if os.name != 'nt':
        directories = [p for p in target.rglob('*') if p.is_dir()]
        directories += [target, transaction, transaction.parent, transaction.parent.parent, api.ROOT]
        for directory in directories:
            descriptor = os.open(directory, os.O_RDONLY)
            try: os.fsync(descriptor)
            finally: os.close(descriptor)
    return target, inventory['revision']


def apply_offline(api):
    """Explicitly choose saved sources for the next boot, including shared setup.

    Automatic restart still boots the last committed snapshot. Editing a source
    never implicitly changes that recovery guarantee.
    """
    with api.supervisor_lock(), api.named_lock('quest-publication'):
        state_file = api.ROOT / 'run/supervisor.json'
        if state_file.is_file():
            state = api.read_json(state_file)
            if api.owned(state.get('supervisor')) or any(api.owned(c.get('identity')) for c in state.get('children', [])):
                raise RuntimeError('Stop all managed server processes before selecting the next boot quest package')
        cfg = api.config()
        data, quest = roots(api, cfg, cfg['game_servers'])
        previous = current(api)
        transaction = api.ROOT / 'run/quest-reloads' / secrets.token_hex(16)
        target, revision = snapshot(api, data, quest, transaction)
        def catalog(root):
            result = api.native([api.executable('ServerPackageCheck'), '--inspect-quests', root / 'quest'])
            if result.returncode:
                raise RuntimeError('Quest catalog validation failed: ' + result.stderr.strip())
            return json.loads(result.stdout)
        definitions = catalog(target)
        if previous:
            updated = {row['name']: row['states'] for row in definitions}
            for old in catalog(Path(previous['snapshot'])):
                if old['name'] not in updated or any(updated[old['name']].get(name) != index for name, index in old['states'].items()):
                    raise RuntimeError('Persisted quest/state removal requires a separate data migration: ' + old['name'])
        ids = dict(previous['quest_ids']) if previous else {}
        for row in definitions:
            if row['name'] not in ids:
                ids[row['name']] = max(ids.values(), default=0) + 1
        if any(index > 65535 for index in ids.values()):
            raise RuntimeError('Quest ID space exhausted')
        publication = dict(schema_version=1, id=transaction.name, revision=revision,
                           snapshot=str(target), quest_ids=ids, offline=True)
        api.save_json(api.ROOT / 'run/quest-reloads/current.json', publication)
        (api.ROOT / 'run/quest-reloads/pending.json').unlink(missing_ok=True)
        api.save_json(transaction / 'completed.json', publication)
        print('QUEST_BOOT_PACKAGE_READY ' + revision, flush=True)
        return publication


def request(api, child, publication, phase, timeout):
    api.save_json(child['directory'] / '.auriga-quest-request.json', dict(publication, phase=phase,
        token=child['token'], expires=int(time.time()) + min(120, max(10, int(timeout) + 5))))


def wait(api, children, publication, state, timeout):
    pending = {c['name']: c for c in children}
    acknowledgements = {}
    deadline = time.monotonic() + timeout
    while pending and time.monotonic() < deadline:
        for name, child in list(pending.items()):
            if not api.owned(child['identity']):
                raise RuntimeError('Game core exited during quest publication: ' + name)
            if state == 'activated':
                try:
                    active = core_status(child)
                    if active.get('transaction') == publication['id'] and active.get('revision') == publication['revision']:
                        del pending[name]; print('QUEST_ACTIVATED ' + name, flush=True); continue
                except (OSError, ValueError, RuntimeError):
                    pass
            try:
                result = read(child['directory'] / '.auriga-quest-response.json')
            except (OSError, ValueError, RuntimeError):
                continue
            if not matches(result, child) or result.get('id') != publication['id']:
                continue
            if state != 'aborted' and result.get('status') in ('rejected', 'expired'):
                raise RuntimeError('Quest publication rejected by ' + name + ': ' + str(result.get('error', result['status'])))
            if result.get('status') == state and (state == 'aborted' or result.get('revision') == publication['revision']):
                acknowledgements[name] = result
                del pending[name]; print('QUEST_' + state.upper() + ' ' + name, flush=True)
        if pending:
            time.sleep(0.05)
    if pending:
        raise RuntimeError('Quest publication awaiting cores: ' + ', '.join(sorted(pending)))
    return acknowledgements


def recover(api, children):
    journal = api.ROOT / 'run/quest-reloads/pending.json'
    if not journal.is_file():
        return None
    publication = read(journal)
    decision = current(api)
    if decision and decision['id'] == publication['id']:
        # A committed decision is irreversible: retry acknowledgements, including
        # newly restarted cores that boot directly from current.json.
        wait(api, children, publication, 'activated', 30)
        journal.unlink()
        print('QUEST_RELOAD_RECOVERED ' + publication['revision'], flush=True)
        return publication
    for child in children:
        request(api, child, publication, 'abort', 10)
    wait(api, children, publication, 'aborted', 10)
    journal.unlink()
    print('QUEST_PREVIOUS_PREPARATION_ABORTED', flush=True)
    return None


def reload(api, timeout=45):
    with api.named_lock('quest-publication'):
        cfg, children = participants(api)
        recovered = recover(api, children)
        if recovered:
            return recovered
        versions = {(s['revision'], s['id_hash']) for s in (core_status(child) for child in children)}
        if len(versions) != 1:
            raise RuntimeError('Running cores have different quest versions/IDs; synchronize the server builds first')
        data, quest = roots(api, cfg, [c['name'] for c in children])
        identity = secrets.token_hex(16)
        transaction = api.ROOT / 'run/quest-reloads' / identity
        target, revision = snapshot(api, data, quest, transaction)
        publication = dict(schema_version=1, id=identity, revision=revision, snapshot=str(target))
        api.save_json(api.ROOT / 'run/quest-reloads/pending.json', publication)
        committed = False
        try:
            for child in children:
                request(api, child, publication, 'prepare', timeout)
            acknowledgements = wait(api, children, publication, 'prepared', timeout)
            ids = [value['quest_ids'] for value in acknowledgements.values()]
            if not ids or any(value != ids[0] for value in ids[1:]):
                raise RuntimeError('Prepared cores disagree about quest identifiers')
            publication['quest_ids'] = ids[0]
            api.save_json(api.ROOT / 'run/quest-reloads/pending.json', publication)
            # Re-read the owned participants just before the durable decision.
            _, latest = participants(api)
            if {(c['name'], c['token_hash']) for c in latest} != {(c['name'], c['token_hash']) for c in children}:
                raise RuntimeError('Running core set changed while preparing quests')
            api.save_json(api.ROOT / 'run/quest-reloads/current.json', publication)
            committed = True
            print('QUEST_COMMIT_DECIDED ' + revision, flush=True)
            wait(api, children, publication, 'activated', timeout)
            (api.ROOT / 'run/quest-reloads/pending.json').unlink()
            api.save_json(transaction / 'completed.json', dict(publication, cores=[c['name'] for c in children]))
            print('QUEST_RELOAD_OK ' + revision, flush=True)
            return publication
        except BaseException:
            if not committed:
                try:
                    for child in children:
                        request(api, child, publication, 'abort', 10)
                    wait(api, children, publication, 'aborted', 10)
                    (api.ROOT / 'run/quest-reloads/pending.json').unlink()
                except (OSError, RuntimeError, ValueError):
                    pass  # The next explicit run recovers this durable journal.
            else:
                print('QUEST_COMMIT_PENDING: decision retained; repeat reload-quests to finish acknowledgements', flush=True)
            raise
