"""Convert legacy per-process JSON configs into one server.json; originals stay intact."""
import argparse
import copy
import json
import os
import re
from pathlib import Path


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError('Duplicate JSON key in an input configuration')
        result[key] = value
    return result


def read(path):
    try:
        value = json.loads(path.read_text(encoding='utf-8-sig'), object_pairs_hook=unique_object)
    except json.JSONDecodeError:
        raise ValueError('Invalid JSON in an input configuration') from None
    if not isinstance(value, dict):
        raise ValueError('Each input configuration must be a JSON object')
    return value


def relative(path, base):
    try:
        return Path(os.path.relpath(path, base)).as_posix()
    except ValueError:  # Windows: different drive letters cannot form a relative path.
        return path.as_posix()


def relocate(section, source, output, role):
    value = copy.deepcopy(section)
    old_base, new_base = source.resolve().parent, output.resolve().parent
    value['working_directory'] = relative(old_base, new_base)
    def path_field(obj, key, default=None):
        text = obj.get(key, default)
        if text is None or text == '':
            return
        if not isinstance(text, str):
            raise ValueError('A configured path must be a string')
        path = Path(text)
        obj[key] = relative((old_base / path).resolve() if not path.is_absolute() else path, new_base)
    tls = value.get('tls') if role == 'login' else value.get('network', {}).get('tls')
    if tls is not None:
        path_field(tls, 'cert_file'); path_field(tls, 'key_file')
    if role == 'game':
        path_field(value.setdefault('server', {}), 'data_path', 'data')
        if 'quest' in value:
            path_field(value['quest'], 'dir')
            if 'object_dir' in value['quest']:
                if not isinstance(value['quest']['object_dir'], list):
                    raise ValueError('quest.object_dir must be an array')
                converted = []
                for directory in value['quest']['object_dir']:
                    item = {'path': directory}; path_field(item, 'path'); converted.append(item['path'])
                value['quest']['object_dir'] = converted
    return value


def merge_configs(database, login, games, output):
    if not games:
        raise ValueError('At least one GameServer configuration is required')
    result = {'schema_version': 1, 'common': {},
              'database': relocate(read(database), database, output, 'database'),
              'login': relocate(read(login), login, output, 'login'),
              'game_defaults': {}, 'game_servers': {}}
    if 'schema_version' in result['database'] or 'schema_version' in result['login']:
        raise ValueError('Inputs must be legacy component configs, not an already merged server.json')
    for name, source in games:
        if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_-]{0,63}', name) or name in ('database', 'login') or name in result['game_servers']:
            raise ValueError('GameServer names must be unique, non-reserved identifiers')
        result['game_servers'][name] = relocate(read(source), source, output, 'game')
    db, auth, cores = result['database'], result['login'], list(result['game_servers'].values())
    # Only share identical values. Different credentials or service-specific settings survive.
    def share(field, locations):
        if all(field in obj for obj in locations) and all(obj[field] == locations[0][field] for obj in locations):
            result['common'][field] = locations[0][field]
            for obj in locations: del obj[field]
    share('interserver_key', [db.setdefault('server', {}), auth] + cores)
    share('locale', [db['server'], auth] + [core['server'] for core in cores])
    share('table_postfix', [db['server']] + [core['server'] for core in cores])
    for name in ('player', 'account', 'common', 'log', 'mission'):
        locations = []
        for component in [db] + cores:
            if name in component.get('sql', {}): locations.append((component['sql'], name))
        if name == 'account' and 'account_sql' in auth: locations.append((auth, 'account_sql'))
        if locations and all(obj[key] == locations[0][0][locations[0][1]] for obj, key in locations):
            result['common'].setdefault('sql', {})[name] = locations[0][0][locations[0][1]]
            for obj, key in locations: del obj[key]
    tls = [(auth, 'tls')] + [(core.setdefault('network', {}), 'tls') for core in cores]
    if all(key in obj for obj, key in tls) and all(obj[key] == tls[0][0][tls[0][1]] for obj, key in tls):
        result['common']['tls'] = tls[0][0][tls[0][1]]
        for obj, key in tls: del obj[key]
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--database', required=True, type=Path)
    parser.add_argument('--login', required=True, type=Path)
    parser.add_argument('--game', action='append', required=True, metavar='NAME=FILE')
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    try:
        games = []
        for game in args.game:
            name, separator, filename = game.partition('=')
            if not separator or not filename: raise ValueError('--game requires NAME=FILE')
            games.append((name, Path(filename)))
        result = merge_configs(args.database, args.login, games, args.output)
        # Never overwrite a source file, existing central config, or live deployment.
        descriptor = os.open(args.output, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
        with os.fdopen(descriptor, 'w', encoding='utf-8', newline='\n') as output:
            json.dump(result, output, ensure_ascii=False, indent=2); output.write('\n')
        print('Central configuration created:', args.output)
        print('Run run_server.py --check-config before starting the services.')
        return 0
    except (OSError, ValueError, TypeError, AttributeError, KeyError):
        parser.error('Conversion failed: verify input paths/types, unique instance names and a new output path. Input values are not printed.')


if __name__ == '__main__':
    raise SystemExit(main())
