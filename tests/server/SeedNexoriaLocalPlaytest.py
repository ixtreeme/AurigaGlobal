"""Create two NEW native-item playtest accounts in the owned, stopped local stack.

Credentials stay in the requested private build/run file. Existing characters,
legacy inventories and native owners are never reset or converted.
"""
from pathlib import Path
import argparse, json, secrets, sys

REPO = Path(__file__).resolve().parents[2]
sys.path[:0] = [str(REPO / 'SRC/Tools/ServerManager'), str(REPO / 'db/tools'), str(Path(__file__).parent)]
import backend, item_bench, item_catalog
from mariadb_client import Client
from PetPassiveUnityLiveFixture import literal, protected
from CompanionUnityLiveSmoke import validate_local_host


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--package', type=Path, required=True)
    p.add_argument('--template', type=Path, required=True)
    p.add_argument('--fixture', type=Path, required=True)
    a = p.parse_args()
    root, manager, _, auth = item_bench.connection(a.package.resolve())
    destination = a.fixture.resolve()
    assert destination.is_relative_to(REPO / 'build') or destination.is_relative_to(root / 'run')
    assert destination.name.endswith('.private.json') and not destination.exists()
    source = json.loads(a.template.read_text(encoding='utf-8-sig'))
    connection = json.loads((root / 'client-connection.json').read_text(encoding='utf-8-sig'))
    host = json.loads((root / 'server.json').read_text(encoding='utf-8'))['game_defaults']['network']['bind_ip']
    validate_local_host(root, host, True)
    bench = root / 'manager/_internal/validators/ItemBenchTool.exe'
    catalog_tool = root / 'manager/_internal/validators/CatalogEditorTool.exe'
    catalog = item_catalog.Catalog(item_catalog.run_tool(dict(action='read', connection=auth), executable=catalog_tool))
    required = {'WeaponLocal': (1, 'weapon.local_probe_sword'),
                'FlaskLocal': (5, 'consumable.local_probe_flask'),
                'MaterialLocal': (6, 'material.local_probe_ore')}
    keys = {}
    for field, (category, code) in required.items():
        row = next(r for r in catalog.tables['item_base'] if int(r['category_id']) == category and r['content_code'] == code)
        keys[field] = int(row['local_id'])
    token = secrets.token_hex(4)
    config = dict(Nexoria=True, Host=host, Token=token, ServerName=connection['Tls']['ServerName'],
                  Pin=connection['Tls']['PinnedCertificateSha256'], Version=connection['ClientVersion'],
                  AuthPort=connection['AuthPort'], GamePort=30078, MapIndex=41, X=986600, Y=269100,
                  Labels=['Ixtre', 'Razor'], Accounts=[], **keys)
    destination.parent.mkdir(parents=True, exist_ok=True)
    def save():
        destination.write_text(json.dumps(config, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    db = Client(**auth, library=str(root / 'bin/libmariadb.dll'))
    try:
        with manager.supervisor_lock():
            assert not backend.describe(root)['running'], 'Stop the owned game stack first'
            before = protected(db)
            template = source['Accounts'][0]
            assert db.query('SELECT account_id,name FROM player.player WHERE id=' + str(int(template['Pid']))) == [[str(template['AccountId']), template['Name']]]
            columns = [r[0] for r in db.query('SHOW COLUMNS FROM player.player') if r[0] != 'id' and 'GENERATED' not in r[5]]
            for side in range(2):
                login = 'codexitem' + token + str(side)
                password = secrets.token_hex(8)
                name = 'CI' + token + str(side)
                db.query('INSERT INTO account.account(login,password,social_id,status,create_time) VALUES(' + literal(login) + ',PASSWORD(' + literal(password) + "),'1234567','OK',NOW())")
                aid = int(db.query('SELECT LAST_INSERT_ID()')[0][0])
                values = dict(account_id=str(aid), name=name, job='0', level='60', exp='0', level_step='0',
                              stat_point='0', skill_point='0', sub_skill_point='0', horse_skill_point='0',
                              st='10', ht='10', dx='10', iq='10', skill_group='1', random_hp='30000', random_sp='10000',
                              gold='0', hp='30000', mp='10000', map_index='41', x=str(config['X'] + side * 100),
                              y=str(config['Y']), z='0', typed_book_revision='0', horse_level='0', horse_riding='0')
                projection = ['REPEAT(CHAR(0),OCTET_LENGTH(`' + k + '`))' if k in ('skill_level', 'quickslot')
                              else literal(values[k]) if k in values else '`' + k + '`' for k in columns]
                db.query('INSERT INTO player.player(' + ','.join('`' + k + '`' for k in columns) + ') SELECT ' + ','.join(projection) + ' FROM player.player WHERE id=' + str(int(template['Pid'])))
                pid = int(db.query('SELECT LAST_INSERT_ID()')[0][0])
                db.query(f'INSERT INTO player.player_index(id,pid1,empire) VALUES({aid},{pid},1)')
                config['Accounts'].append(dict(Login=login, Password=password, Name=name, Pid=pid, AccountId=aid))
                save()  # Keep exact identities recoverable even if a later native grant fails.
                item_bench.run_tool(dict(action='setup', owner=pid, gold=1000 if side == 0 else 500, connection=auth), bench)
                for category, field, count in ((1, 'WeaponLocal', 1), (5, 'FlaskLocal', 3), (6, 'MaterialLocal', 10)):
                    item_bench.run_tool(dict(action='grant', owner=pid, category=category, local=keys[field], count=count, connection=auth), bench)
            assert protected(db, [row['Pid'] for row in config['Accounts']]) == before, 'Existing native owners changed'
            config['ProtectedOwnersVerified'] = True
            save()
        print('NEXORIA_LOCAL_PLAYTEST=READY accounts=2 existing_owners_preserved=1 credentials=' + str(destination))
    finally:
        db.close()


if __name__ == '__main__':
    main()
