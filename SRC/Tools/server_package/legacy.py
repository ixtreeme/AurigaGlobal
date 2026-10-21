"""Translate legacy CONFIG/conf.txt without evaluating code or exposing secrets."""
import copy
import re
import secrets
import shlex


class PackageError(Exception):
    pass


def read_config(path):
    result = {}
    for number, line in enumerate(path.read_text(encoding='latin1').splitlines(), 1):
        line = line.strip()
        if not line or line.startswith(('#', '//', ';')):
            continue
        match = re.fullmatch(r'([A-Za-z0-9_]+)\s*[:=]\s*(.*?)\s*', line)
        if not match:
            raise PackageError('Invalid config line: %s:%d' % (path.name, number))
        key, value = match.groups()
        key = key.upper()
        if key in result:
            raise PackageError('Duplicate config field: %s:%d' % (path.name, number))
        result[key] = value
    return result


def scalar(value):
    return value[1:-1] if len(value) >= 2 and value[0] == value[-1] == '"' else value


def sql_connection(value, database_style=False):
    fields = shlex.split(scalar(value))
    if len(fields) not in (4, 5):
        raise PackageError('Legacy SQL connection needs four fields and an optional port')
    if database_style:
        host, database, user, password = fields[:4]
    else:
        host, user, password, database = fields[:4]
    return dict(host=host, database=database, user=user, password=password,
                port=int(fields[4]) if len(fields) == 5 else 3306)


GAME_FIELDS = {
    'HOSTNAME': ('server.hostname', str), 'CHANNEL': ('server.channel', int),
    'VIEW_RANGE': ('server.view_range', int), 'PASSES_PER_SEC': ('server.passes_per_sec', int),
    'TEST_SERVER': ('server.test', int), 'MAX_LEVEL': ('server.max_level', int),
    'STONE_CHANCE': ('server.stone_chance', int), 'USER_LIMIT': ('server.user_limit', int),
    'PK_PROTECT_LEVEL': ('server.pk_protect_level', int),
    'SAVE_EVENT_SECOND_CYCLE': ('events.save_second_cycle', int),
    'PING_EVENT_SECOND_CYCLE': ('events.ping_second_cycle', int),
    'CHANGE_ATTR_TIME_LIMIT': ('gameplay.item_bonus_change_time', int),
    'EMOTION_MASK_REQUIRE': ('gameplay.disable_emotion_mask', lambda s: not int(s)),
    'PRISM_ITEM_REQUIRE': ('gameplay.disable_prism_need', lambda s: not int(s)),
    'SHOP_PRICE_3X_TAX': ('gameplay.empire_shop_price_triple_disable', lambda s: not int(s)),
    'EMPIRE_LANGUAGE_CHECK': ('gameplay.disable_empire_language_check', lambda s: not int(s)),
    'GLOBAL_SHOUT': ('gameplay.global_shout_enable', lambda s: bool(int(s))),
    'ITEM_COUNT_LIMIT': ('gameplay.item_count_limit', int),
    'STATUS_POINT_GET_LEVEL_LIMIT': ('gameplay.status_point_get_level_limit', int),
    'STATUS_POINT_SET_MAX_VALUE': ('gameplay.status_point_set_max_value', int),
    'SHOUT_LIMIT_LEVEL': ('gameplay.shout_limit_level', int),
    'DB_LOG_LEVEL': ('logs.db_level', int),
    'PORT': ('network.port', int), 'P2P_PORT': ('network.p2p_port', int),
    'DB_PORT': ('network.db_port', int), 'DB_ADDR': ('network.db_addr', str),
    'BIND_IP': ('network.bind_ip', str),
    'MAP_ALLOW': ('map_allow', lambda s: [int(n) for n in s.split()]),
}
DB_FIELDS = {
    'TABLE_POSTFIX': ('server.table_postfix', str),
    'CLIENT_HEART_FPS': ('server.client_heart_fps', int),
    'PLAYER_ID_START': ('server.player_id_start', int),
    'LOCALE': ('server.locale', str), 'TEST_SERVER': ('server.test', lambda s: bool(int(s))),
    'LOG': ('server.log', lambda s: bool(int(s))),
    'PROTO_FROM_DB': ('proto.from_db', lambda s: bool(int(s))),
    'MIRROR2DB': ('proto.mirror2db', lambda s: bool(int(s))),
    'BIND_PORT': ('network.bind_port', int), 'BIND_IP': ('network.bind_ip', str),
    'ITEM_ID_RANGE': ('item_id_range', lambda s: [int(n) for n in s.split()]),
}


def translate(fields, mapping):
    result = {}
    for key, (destination, convert) in mapping.items():
        if key not in fields:
            continue
        parent = result
        parts = destination.split('.')
        for part in parts[:-1]:
            parent = parent.setdefault(part, {})
        try:
            parent[parts[-1]] = convert(scalar(fields[key]))
        except (ValueError, TypeError):
            raise PackageError('Invalid legacy field type: ' + key) from None
    return result


def central_config(source, sql_port):
    database = read_config(source / 'db/conf.txt')
    auth = read_config(source / 'auth/CONFIG')
    paths = sorted((source / 'chan').glob('ch*/core*/CONFIG'))
    if not paths:
        raise PackageError('No channel CONFIG files found')
    for role in ('PLAYER', 'ACCOUNT', 'COMMON'):
        sql_connection(database['SQL_' + role], database_style=True)
    db = translate(database, DB_FIELDS)
    db['working_directory'] = 'run/database'
    db.setdefault('network', {})['bind_ip'] = '127.0.0.1'
    db['proto'] = dict(from_db=True, mirror2db=False)
    locale = db.setdefault('server', {}).pop('locale', 'latin1')
    postfix = db['server'].pop('table_postfix', '')
    password = secrets.token_urlsafe(32)
    sql = {name: dict(host='127.0.0.1', port=sql_port, user='auriga_local',
                      password=password, database=name) for name in ('player', 'account', 'common', 'log')}
    common = dict(locale=locale, table_postfix=postfix, sql=sql,
                  interserver_key=secrets.token_hex(32),
                  tls=dict(cert_file='tls/server.crt', key_file='tls/server.key'))
    games, unused = {}, {}
    for index, path in enumerate(paths, 1):
        fields = read_config(path)
        for name in ('PLAYER', 'COMMON', 'LOG'):
            sql_connection(fields[name + '_SQL'])
        name = path.parent.parent.name + '-' + path.parent.name
        game = translate(fields, GAME_FIELDS)
        game['working_directory'] = 'run/' + name
        game.setdefault('server', {})['server_id'] = index
        game['network']['bind_ip'] = '127.0.0.1'
        game['network'].pop('db_addr', None)
        if game['network'].get('db_port') != db['network']['bind_port']:
            raise PackageError('Legacy core uses a different Database port: ' + name)
        game['network'].pop('db_port', None)
        games[name] = game
        unused[name] = sorted(set(fields) - set(GAME_FIELDS) - {'PLAYER_SQL', 'COMMON_SQL', 'LOG_SQL'})
    defaults = dict(server=dict(data_path='data'), version=dict(check_server=True, value='20251204'))
    for section in ('server', 'network', 'events', 'gameplay', 'logs'):
        first = next(iter(games.values())).get(section, {})
        for key, value in list(first.items()):
            if key in ('hostname', 'channel', 'server_id', 'port', 'p2p_port'):
                continue
            if all(g.get(section, {}).get(key) == value for g in games.values()):
                defaults.setdefault(section, {})[key] = copy.deepcopy(value)
                for game in games.values():
                    game[section].pop(key)
    login = dict(port=int(auth['PORT']), working_directory='run/login')
    result = dict(schema_version=1, common=common, database=db, login=login,
                  game_defaults=defaults, game_servers=games)
    retired = {'ADMINPAGE_IP1', 'ADMINPAGE_PASSWORD'}
    unexpected = {name: [key for key in keys if key not in retired] for name, keys in unused.items()}
    if any(unexpected.values()):
        raise PackageError('Unmapped legacy core fields: ' + repr(unexpected))
    return result, dict(core_count=len(games), legacy_locale=locale, retired_core_fields=unused,
                        retired_database_fields=sorted(set(database) - set(DB_FIELDS) - {'SQL_PLAYER','SQL_ACCOUNT','SQL_COMMON'}))
