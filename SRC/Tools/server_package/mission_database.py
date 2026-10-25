"""Dedicated Mission schema installation. No gameplay hooks or legacy data deletion."""
import copy
import re

TABLES = ('mission_engine_schema', 'mission_engine_packages', 'mission_engine_definitions',
          'mission_engine_characters', 'mission_engine_checkpoint_refs', 'mission_engine_operations',
          'mission_engine_action_outbox', 'mission_engine_reward_ledger', 'mission_engine_item_awards')
MIGRATIONS = ('2026-10-25_mission_engine_persistence.sql', '2026-10-25_mission_engine_actions.sql',
              '2026-10-25_mission_engine_item_awards.sql')


def identifier(value):
    if (not isinstance(value, str) or not re.fullmatch(r'[A-Za-z0-9_]{1,64}', value)
            or value.lower() in ('mysql', 'information_schema', 'performance_schema', 'sys')):
        raise ValueError('Mission database must use a non-system, simple SQL identifier')
    return '`' + value + '`'


def connection(config):
    # Database is the future canonical reward writer; its role overrides matter.
    shared = config['common']['sql']
    override = config.get('database', {}).get('sql', {})
    player = dict(shared.get('player', {}), **override.get('player', {}))
    mission = dict(player, database='mission')
    mission.update(shared.get('mission', {}))
    mission.update(override.get('mission', {}))
    identifier(mission.get('database'))
    for name in ('player', 'account', 'common', 'log'):
        other = dict(shared.get(name, {}), **override.get(name, {}))
        if mission['database'].lower() == str(other.get('database', '')).lower():
            raise ValueError('Mission must use a separate database')
    if (mission.get('host'), mission.get('port') or 3306) != (player.get('host'), player.get('port') or 3306):
        raise ValueError('Mission and player must use the same MariaDB endpoint')
    return copy.deepcopy(mission), player


def install_sql(db_folder, name):
    """Fully qualify every owned table; no USE or connection-context switching.

    The checked-in migrations remain the single source of the table layouts.
    Only trusted table tokens from our explicit set are rewritten, never values.
    """
    database = identifier(name)
    parts = ['CREATE DATABASE IF NOT EXISTS ' + database +
             ' DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_bin;']
    pattern = re.compile(r'\b(' + '|'.join(TABLES) + r')\b')
    for filename in MIGRATIONS:
        source = (db_folder / 'migrations' / filename).read_text(encoding='utf-8-sig')
        # Comments are not needed in the executable assembly; avoid qualifying prose.
        source = '\n'.join(line for line in source.splitlines() if not line.lstrip().startswith('--'))
        parts.append(pattern.sub(lambda match: database + '.`' + match[0] + '`', source))
    return '\n\n'.join(parts) + '\n'


def legacy_check(player):
    # Refuse a fresh empty target when an earlier experimental installation
    # exists in player. Moving existing Mission data needs a reviewed migration.
    identifier(player)
    names = ','.join("'" + table + "'" for table in TABLES)
    return ('SELECT id FROM ' + identifier(player) + '.`player` LIMIT 0;\n' +
            "SELECT COUNT(*) FROM information_schema.TABLES WHERE TABLE_SCHEMA='" +
            player + "' AND TABLE_NAME IN (" + names + ');')


def local_grants(target, player, account=None):
    """For an already verified, owned local SQL instance, using its admin client."""
    for value in (target.get('user'), target.get('password')):
        if not isinstance(value, str) or '\x00' in value:
            raise ValueError('Invalid Mission SQL credentials')
    account = account or dict(player, database='account')
    literal = lambda value: "'" + value.replace("'", "''") + "'"
    user = literal(target['user']) + "@'127.0.0.1'"
    account_grant = ''
    if (account.get('host'), account.get('port') or 3306) == (target.get('host'), target.get('port') or 3306):
        account_grant = 'GRANT SELECT ON ' + identifier(account['database']) + '.`account` TO ' + user + ';\n'
    return ("SET SESSION sql_mode='NO_BACKSLASH_ESCAPES';\nCREATE USER IF NOT EXISTS " + user +
            ' IDENTIFIED BY ' + literal(target['password']) + ';\nGRANT ALL ON ' +
            identifier(target['database']) + '.* TO ' + user + ';\nGRANT SELECT ON ' +
            identifier(player['database']) + '.`player` TO ' + user + ';\n' + account_grant + 'GRANT SELECT,INSERT ON ' +
            identifier(player['database']) + '.`item_award` TO ' + user + ';\n')
