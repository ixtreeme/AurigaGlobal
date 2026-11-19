"""Copy-on-write channel topology; no services, files or database are mutated."""
import copy
import re
import backend as api

MAP_LIMIT = 64  # common/CommonDefines.h: MAP_ALLOW_LIMIT


def effective(config, name):
    return api.merge(config.get('game_defaults', {}), config['game_servers'][name])


def channels(config):
    return sorted({effective(config, name)['server']['channel'] for name in config['game_servers']})


def members(config, channel):
    return [name for name in config['game_servers'] if effective(config, name)['server']['channel'] == channel]


def map_ids(values):
    if isinstance(values, str):
        try: values = [int(part) for part in re.split(r'[,;\s]+', values.strip()) if part]
        except ValueError: raise api.EditorError('A térképazonosítók egész számok legyenek.') from None
    if not isinstance(values, (list, tuple)) or any(type(v) is not int or not 1 <= v <= 2147483647 for v in values):
        raise api.EditorError('Érvényes, pozitív térképazonosítókat adj meg.')
    if len(values) != len(set(values)):
        raise api.EditorError('Egy térkép csak egyszer szerepelhet a core-ban.')
    if len(values) > MAP_LIMIT:
        raise api.EditorError('Egy core legfeljebb %d térképet szolgálhat ki.' % MAP_LIMIT)
    return list(values)


def next_channel(config):
    return next((c for c in range(1, 256) if c != 99 and c not in channels(config)), None)


def used_ports(config, exclude=None):
    result = set()
    def collect(value):
        if isinstance(value, dict):
            for key, child in value.items():
                if key in ('port', 'p2p_port', 'bind_port') and type(child) is int and child:
                    result.add(child)
                elif isinstance(child, (dict, list)): collect(child)
        elif isinstance(value, list):
            for child in value: collect(child)
    collect({key: value for key, value in config.items() if key != 'game_servers'})
    for name in config['game_servers']:
        if name != exclude: collect(effective(config, name))
    return result


def _append(config, channel, maps):
    if type(channel) is not int or not 1 <= channel <= 255:
        raise api.EditorError('A CH száma 1 és 255 közé essen; a CH99 közös csatorna.')
    maps = map_ids(maps)
    if not maps: raise api.EditorError('Az új core-hoz válassz legalább egy kiszolgálandó térképet.')
    games = config['game_servers']
    number = next(i for i in range(1, len(games) + 2) if 'ch%d-core%d' % (channel, i) not in games)
    name = 'ch%d-core%d' % (channel, number)
    identifiers = {effective(config, key).get('server', {}).get('server_id', 0) for key in games}
    identity = next(i for i in range(1, len(identifiers) + 2) if i not in identifiers)
    taken = used_ports(config)
    port = next((p for p in range(30054, 65535) if p not in taken and p + 1 not in taken), None)
    if port is None: raise api.EditorError('Nem található két szabad konfigurációs port 30054 és 65535 között.')
    directory = 'run/' + name
    occupied = {core.get('working_directory', 'run/' + key).replace('\\', '/').rstrip('/').casefold()
                for key, core in games.items()}
    if directory.casefold() in occupied:
        raise api.EditorError('A core futási mappáját egy másik példány már használja.')
    games[name] = dict(server=dict(hostname=name, channel=channel, server_id=identity),
                       network=dict(port=port, p2p_port=port + 1), map_allow=maps,
                       working_directory=directory)
    return name


def add_channel(config, channel, source=None, maps=None):
    candidate = copy.deepcopy(config)
    if channel in channels(config): raise api.EditorError('Ez a csatorna már létezik.')
    if source is None:
        layouts = [map_ids(maps or [])]
    else:
        if source not in channels(config): raise api.EditorError('A mintaként választott csatorna nem található.')
        layouts = [map_ids(effective(config, name).get('map_allow', [])) for name in members(config, source)]
        layouts = [values for values in layouts if values]  # no idle, empty cores
        if not layouts: raise api.EditorError('A mintacsatornában nincs térképet kiszolgáló core.')
        flattened = [value for values in layouts for value in values]
        if len(flattened) != len(set(flattened)):
            raise api.EditorError('A mintacsatornában több core ugyanazt a térképet szolgálja ki. Előbb rendezd a felosztást.')
    names = [_append(candidate, channel, values) for values in layouts]
    return candidate, names


def assign_maps(config, name, maps, transfer=False):
    if name not in config['game_servers']: raise api.EditorError('A core nem található.')
    values = map_ids(maps)
    if not values: raise api.EditorError('Adj meg legalább egy térképet, vagy töröld a fölösleges core-t.')
    candidate = copy.deepcopy(config)
    channel = effective(candidate, name)['server']['channel']
    for sibling in members(candidate, channel):
        if sibling == name: continue
        current = effective(candidate, sibling).get('map_allow', [])
        overlap = set(current) & set(values)
        if overlap and not transfer:
            raise api.EditorError('Ezeket a térképeket már a(z) %s kezeli: %s. Engedélyezd az áthelyezést.' %
                                  (sibling, ', '.join(map(str, sorted(overlap)))))
        if overlap:
            candidate['game_servers'][sibling]['map_allow'] = [v for v in current if v not in overlap]
    candidate['game_servers'][name]['map_allow'] = values
    return candidate


def add_core(config, channel, maps, transfer=False):
    if channel not in channels(config): raise api.EditorError('Előbb hozz létre egy csatornát.')
    candidate = copy.deepcopy(config)
    name = _append(candidate, channel, maps)
    return assign_maps(candidate, name, maps, transfer), name


def remove(config, channel=None, name=None):
    if name is not None:
        if name not in config['game_servers']: raise api.EditorError('A core nem található.')
        names = [name]
    else:
        names = members(config, channel)
        if not names: raise api.EditorError('A csatorna nem található.')
    remaining = [key for key in config['game_servers'] if key not in names]
    if not any(effective(config, key)['server']['channel'] != 99 for key in remaining):
        raise api.EditorError('Legalább egy normál csatorna egy core-ját meg kell tartani.')
    candidate = copy.deepcopy(config)
    for key in names: del candidate['game_servers'][key]
    return candidate


def compact(config):
    """Remove redundant overrides, preserving every effective core setting."""
    candidate = copy.deepcopy(config)
    keep = {('working_directory',), ('map_allow',), ('server', 'hostname'), ('server', 'channel'),
            ('server', 'server_id'), ('network', 'port'), ('network', 'p2p_port')}
    def sparse(value, base, path=()):
        result = {}
        for key, child in value.items():
            location = path + (key,)
            if isinstance(child, dict):
                reduced = sparse(child, base.get(key, {}) if isinstance(base.get(key), dict) else {}, location)
                if reduced or not isinstance(base.get(key), dict): result[key] = reduced
            elif location in keep or key not in base or type(child) is not type(base[key]) or child != base[key]:
                result[key] = copy.deepcopy(child)
        return result
    for name, core in candidate['game_servers'].items():
        reduced = sparse(core, config.get('game_defaults', {}))
        candidate['game_servers'][name] = reduced
    return candidate


def client_manifest(config):
    result = []
    for channel in channels(config):
        if channel == 99: continue
        cores = members(config, channel)
        name = next((key for key in cores if effective(config, key).get('map_allow')), cores[0])
        result.append(dict(id=channel, name='CH%d' % channel, port=effective(config, name)['network']['port']))
    if not result: raise api.EditorError('A klienshez legalább egy normál CH szükséges.')
    shared = members(config, 99)
    mark_port = effective(config, shared[0])['network']['port'] if shared else result[0]['port']
    return dict(schema_version=1, auth_port=config['login']['port'], mark_port=mark_port, channels=result)
