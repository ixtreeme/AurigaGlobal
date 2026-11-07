"""Owned localhost fixture for real Unity pet-passive combat tests.

Adds isolated accounts, catalog rows and temporary probe monsters. Existing
items/catalog rows are preserved. Restore removes only the temporary map/protos.
Credentials stay in the explicitly selected private fixture file.
"""
from collections import Counter
from datetime import datetime, timezone
from pathlib import Path
import argparse, hashlib, json, os, re, secrets, subprocess, sys

REPO = Path(__file__).resolve().parents[2]
sys.path[:0] = [str(REPO/'SRC/Tools/ServerManager'), str(REPO/'db/tools')]
import backend, item_bench, item_catalog
from mariadb_client import Client


def read(path): return json.loads(path.read_text(encoding='utf-8-sig'))
def write(path, value): path.write_text(json.dumps(value, indent=2)+'\n', encoding='utf-8')
def literal(value): return 'CONVERT(0x'+str(value).encode('utf-8').hex()+' USING utf8mb4)'
def sha(value): return hashlib.sha256(value).hexdigest()


def protected(db, owners=()):
    where = '' if not owners else ' WHERE NOT (owner_kind=1 AND owner_id IN ('+','.join(map(str, owners))+'))'
    return {name: db.query(query+where+' ORDER BY '+order) for name, query, order in [
        ('items', 'SELECT owner_kind,owner_id,instance_id,state_revision,HEX(state_sha256),HEX(last_operation) FROM player.item_instance', 'instance_id'),
        ('wallets', 'SELECT owner_kind,owner_id,currency,state_revision,balance FROM player.item_wallet', 'owner_kind,owner_id,currency'),
        ('learning', 'SELECT owner_kind,owner_id,state_revision,HEX(state_blob),HEX(state_sha256),HEX(last_operation) FROM player.item_book_learning', 'owner_kind,owner_id')]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=['seed', 'verify', 'restore'])
    parser.add_argument('--package', type=Path, required=True)
    parser.add_argument('--fixture', type=Path, required=True)
    parser.add_argument('--template', type=Path)
    parser.add_argument('--catalog-tool', type=Path)
    parser.add_argument('--log', type=Path)
    args = parser.parse_args()
    root, manager, ownership, auth = item_bench.connection(args.package.resolve())
    os.environ['PATH'] = str(root/'bin')+os.pathsep+os.environ['PATH']
    fixture = args.fixture.resolve()
    db = Client(**auth, library=str(root/'bin/libmariadb.dll'))
    tool = root/'manager/_internal/validators/ItemBenchTool.exe'
    try:
        if args.action != 'seed':
            config = read(fixture); backup = Path(config['Backup']).resolve()
            assert backup.parent == root/'run/deployments'
            owners = [int(account['Pid']) for account in config['Accounts']]
            assert all(a['Login'] == 'codexpcombat'+config['Token']+str(i) for i,a in enumerate(config['Accounts']))
            assert protected(db, owners) == read(backup/'typed-before.json')
            if args.action == 'verify':
                assert args.log
                log = args.log.read_text(encoding='utf-8', errors='replace')
                assert 'METIN2_TYPED_PET_COMBAT_LIVE=SUCCESS effects=18' in log
                assert sorted(map(int,re.findall(r'METIN2_TYPED_PET_COMBAT_CASE=SUCCESS effect=(\d+)',log))) == list(range(1,19))
                assert sorted(c['Effect'] for c in config['Cases']) == list(range(1,19))
                for account in config['Accounts']:
                    assert db.query(f"SELECT account_id,job FROM player.player WHERE id={int(account['Pid'])}") == [[str(account['AccountId']), str(account['Race'])]]
                state = item_bench.run_tool(dict(action='status', owner=owners[0], connection=auth), tool)['state']
                pets = [item['growth_pet'] for item in state['items'] if item.get('growth_pet')]
                assert len(pets) == 18 and all(not p['recall_requested'] for p in pets)
                assert sorted(p['skills'][0]['skill'] for p in pets) == list(range(93001, 93019))
                assert all(p['skills'][0]['rank'] == 1 and all(not s['skill'] for s in p['skills'][1:]) for p in pets)
                print('PET_PASSIVE_LOCAL_SQL=SUCCESS effects=18 preserved_items='+str(len(read(backup/'typed-before.json')['items'])))
                return
            with manager.supervisor_lock():
                assert not backend.describe(root)['running']
                manifest = read(root/'package-manifest.json'); relative = config['ProbeMap']
                target = root/relative
                assert target.resolve().is_relative_to(root)
                assert target.read_bytes() == (backup/'regen-installed.json').read_bytes()
                assert sha(target.read_bytes()) == manifest['files'][relative]
                columns = [r[0] for r in db.query('SHOW COLUMNS FROM player.mob_proto')]
                rows = db.query('SELECT '+','.join('`'+c+'`' for c in columns)+' FROM player.mob_proto WHERE vnum IN ('+','.join(map(str, config['ProbeRaces']))+') ORDER BY vnum')
                assert rows == read(backup/'probe-protos.json')
                target.write_bytes((backup/'regen-before.json').read_bytes())
                manifest['files'][relative] = sha(target.read_bytes()); write(root/'package-manifest.json', manifest)
                db.query('DELETE FROM player.mob_proto WHERE vnum IN ('+','.join(map(str, config['ProbeRaces']))+')')
            print('PET_PASSIVE_TEMPORARY_WORLD_RESTORED')
            return

        assert args.template and args.catalog_tool and not fixture.exists()
        assert not backend.describe(root)['running'], 'Stop the owned stack first'
        source = read(args.template)['Accounts'][0]
        source_id = int(source['Pid'])
        assert db.query(f'SELECT account_id,name FROM player.player WHERE id={source_id}') == [[str(source['AccountId']), source['Name']]]
        catalog_tool = args.catalog_tool.resolve()
        def catalog(): return item_catalog.run_tool(dict(action='read', connection=auth), executable=catalog_tool)
        packet = catalog(); before = packet
        token = secrets.token_hex(4)
        backup = root/'run/deployments'/datetime.now(timezone.utc).strftime('pet-passive-combat-%Y%m%dT%H%M%S%f')
        backup.mkdir()
        write(backup/'typed-before.json', protected(db)); write(backup/'catalog-before.json', before)
        with (backup/'player-protos-before.sql').open('wb') as out:
            result = subprocess.run([str(Path(ownership['binary_dir'])/'mariadb-dump.exe'),
                '--defaults-extra-file='+str(root/'run/mariadb/admin.cnf'), '--protocol=tcp', '--host=127.0.0.1',
                '--port='+str(ownership['port']), '--single-transaction', '--databases', 'player', 'protos'],
                stdout=out, stderr=subprocess.PIPE, creationflags=item_bench.NO_WINDOW, timeout=300)
            assert result.returncode == 0 and out.tell() > 0
        profile = read(root/'client-connection.json')
        config = dict(Host='127.0.0.1', Token=token, ServerName=profile['Tls']['ServerName'],
            Pin=profile['Tls']['PinnedCertificateSha256'], Version=profile['ClientVersion'],
            AuthPort=profile['AuthPort'], GamePort=30078, Backup=str(backup), GrowthRace=32500,
            NormalRace=65070, BossRace=65071, StoneRace=65072, MobAttackerRace=65073, RewardRace=65074,
            MobArrowRace=65075, ProbeRaces=list(range(65070,65076)), Cases=[])
        values = {effect: 5000 for effect in range(1,19)}
        values.update({1:250, 2:2500, 13:10000, 14:10000, 15:1000000, 17:10000, 18:10000})
        def save(category, code, configure):
            nonlocal packet
            editor = item_catalog.Catalog(packet)
            existing = next((r for r in editor.tables['item_base'] if r['content_code']==code), None)
            if existing: return int(existing['local_id'])
            doc = editor.new(category)
            doc['base'].update(content_code=code, name_key='item.'+code+'.name', description_key='item.'+code+'.description',
                icon_asset='icon/item/55701.tga', max_stack='100' if category==8 else '1', binding_policy='2')
            doc['trade'].update(allow_trade='0', allow_drop='0', allow_sell='0')
            configure(doc)
            local = int(doc['base']['local_id'])
            tables = editor.apply(doc)
            item_catalog.run_tool(dict(action='commit', connection=auth, tables=tables, expected_hash=packet['hash']), packet['columns'], catalog_tool)
            packet = catalog(); return local
        def growth(doc):
            doc['child'].update(companion_kind='3', clock_kind='4', duration_ms='259200000', riding_form_id=None)
            doc['refinement'].update(profile_kind='1', max_level='0')
            doc['levels']=doc['levels'][:1]
            doc['new_companion_form'] = dict(code='pet.local_combat_form', mob_template_id='32500', model_asset='Metin2RacePrefabs/32500')
            doc.update(growth=dict(maximum_level='3', experience_policy='1'),
                growth_levels=[dict(level=str(i), mob_experience='0' if i==3 else '1000000000', item_experience='0' if i==3 else '1000000000') for i in range(1,4)],
                growth_stages=[dict(stage='0', maximum_level='3', form_id=None)])
            doc.update(item_catalog.Catalog.training_defaults())
            doc['pet_slot_rules'] = [dict(slot=str(i), minimum_stage='0', minimum_age_ms='0' if i==0 else '1296000000') for i in range(4)]
            doc['pet_skills'] = [dict(skill_id=str(93000+i), code='pet.local_combat_'+str(i), effect_kind=str(i)) for i in range(1,19)]
            doc['pet_skill_ranks'] = [dict(skill_id=str(93000+i), skill_rank='1', value_scaled=str(values[i])) for i in range(1,19)]
        config['GrowthLocal'] = save(15, 'companion.local_passive_combat', growth)
        def egg(doc):
            doc['child'].update(container_kind='4', loot_profile_id=None, lock_id='0', hatch_profile_id=None)
            doc['loot'] = None
            doc['hatch'] = dict(item_catalog.Catalog.hatch_defaults(), target_category_id='15', target_local_id=str(config['GrowthLocal']), gold_cost='1')
        config['EggLocal'] = save(8, 'container.local_passive_combat', egg)
        for effect in range(1,19):
            def book(doc, effect=effect):
                doc['supply']['supply_kind']='1'
                doc['pet_study'].update(skill_id=str(93000+effect), selected_allowed='1', known_allowed='0', known_success_bp='0', known_cooldown_ms='0')
            local = save(17, 'special.local_passive_combat_'+str(effect), book)
            config['Cases'].append(dict(Effect=effect, Value=values[effect], BookLocal=local, Skill=93000+effect))
        for field, subtype, attack in [('SwordLocal',1,500000), ('CleanupLocal',1,1000000000000)]:
            def weapon(doc, subtype=subtype, attack=attack):
                doc['child'].update(weapon_subtype=str(subtype), physical_attack_base_raw=str(attack))
                doc['attack']['reach_millimeters']='30000'
            config[field] = save(1, 'weapon.local_passive_combat_'+field.lower(), weapon)
        for name, rows in before['tables'].items():
            assert not Counter(map(tuple, rows))-Counter(map(tuple, packet['tables'][name])), name
        config['CatalogHash'] = packet['hash']
        columns = [r[0] for r in db.query('SHOW COLUMNS FROM player.player') if r[0]!='id' and 'GENERATED' not in r[5]]
        config['Accounts'] = []
        for index, race in enumerate([0,0,1,2,3]):
            login, password, name = 'codexpcombat'+token+str(index), secrets.token_hex(8), 'PC'+token+str(index)
            db.query('START TRANSACTION')
            db.query('INSERT INTO account.account(login,password,social_id,status,create_time) VALUES('+literal(login)+',PASSWORD('+literal(password)+"),'1234567','OK',NOW())")
            account = int(db.query('SELECT LAST_INSERT_ID()')[0][0])
            overrides = dict(account_id=account, name=name, job=race, level=50, level_step=0, exp=0,
                map_index=41, x=986600+index*10, y=269100, z=0, exit_map_index=41, exit_x=986600, exit_y=269100,
                gold=0, hp=30000, mp=10000, random_hp=30000, random_sp=10000, st=60, ht=4, dx=3, iq=3,
                alignment=0, typed_book_revision=0, horse_level=0, horse_riding=0, skill_group=0, part_main=0, part_hair=0)
            projection = ['REPEAT(CHAR(0),OCTET_LENGTH(`'+c+'`))' if c in ('skill_level','quickslot') else literal(overrides[c]) if c in overrides else '`'+c+'`' for c in columns]
            db.query('INSERT INTO player.player('+','.join('`'+c+'`' for c in columns)+') SELECT '+','.join(projection)+f' FROM player.player WHERE id={source_id}')
            pid=int(db.query('SELECT LAST_INSERT_ID()')[0][0])
            db.query(f'INSERT INTO player.player_index(id,pid1,empire) VALUES({account},{pid},1)')
            db.query('COMMIT')
            config['Accounts'].append(dict(Login=login, Password=password, Name=name, Pid=pid, AccountId=account, Race=race))
            item_bench.run_tool(dict(action='setup', owner=pid, gold=1000000, connection=auth), tool)
            for local in [config['SwordLocal']]:
                item_bench.run_tool(dict(action='grant', owner=pid, category=1, local=local, count=1, connection=auth), tool)
            if index==0:
                for category, local, count in [(8,config['EggLocal'],18), (1,config['CleanupLocal'],1)]+[(17,c['BookLocal'],1) for c in config['Cases']]:
                    item_bench.run_tool(dict(action='grant', owner=pid, category=category, local=local, count=count, connection=auth), tool)
        proto_columns = [r[0] for r in db.query('SHOW COLUMNS FROM player.mob_proto')]
        # Persist the private identities before adding temporary world rows, so
        # a provisioning failure never loses access to the isolated accounts.
        write(fixture,config)
        assert not db.query('SELECT vnum FROM player.mob_proto WHERE vnum BETWEEN 65070 AND 65075')
        db.query('START TRANSACTION')
        for race in config['ProbeRaces']:
            overrides = dict(vnum=race, name='PetCombat'+str(race), locale_name='PetCombat'+str(race), type=2 if race==65072 else 0,
                rank=4 if race==65071 else 0, battle_type=1 if race==65075 else 0, level=50, ai_flag=2, setRaceFlag=0, setImmuneFlag=0,
                st=0, dx=0, ht=0, iq=0, damage_min=300 if race in (65073,65075) else 0, damage_max=300 if race in (65073,65075) else 0,
                max_hp=1 if race==65074 else 100000000, regen_cycle=255, regen_percent=0,
                exp=100 if race==65074 else 0, gold_min=100 if race==65074 else 0, gold_max=100 if race==65074 else 0,
                attack_speed=100, move_speed=0, aggressive_hp_pct=0, aggressive_sight=0, attack_range=1000 if race==65075 else 250 if race==65073 else 0,
                resurrection_vnum=0, drop_item=0, summon=0, drain_sp=0, dam_multiply=1)
            overrides.update({c:0 for c in proto_columns if c.startswith(('enchant_','resist_','skill_','sp_'))}); overrides['def']=0
            db.query('INSERT INTO player.mob_proto('+','.join('`'+c+'`' for c in proto_columns)+') SELECT '+
                ','.join(literal(overrides[c]) if c in overrides else '`'+c+'`' for c in proto_columns)+' FROM player.mob_proto WHERE vnum=103')
        db.query('COMMIT')
        write(backup/'probe-protos.json', db.query('SELECT '+','.join('`'+c+'`' for c in proto_columns)+' FROM player.mob_proto WHERE vnum BETWEEN 65070 AND 65075 ORDER BY vnum'))
        relative='data/map/metin2_map_main_razor93/regen.json'; target=root/relative; manifest=read(root/'package-manifest.json')
        assert sha(target.read_bytes())==manifest['files'][relative]
        original=target.read_bytes(); world=json.loads(original)
        for offset,race in enumerate(config['ProbeRaces']):
            world['regens'].append(dict(type='mob', x=650 if race==65074 else 651, y=643 if race in (65073,65074,65075) else 642+offset, range_x=0, range_y=0, z=0, dir=0,
                time=2 if race==65074 else 3600, percent=100, count=1, vnum=race))
        installed=(json.dumps(world,indent=2)+'\n').encode('utf-8')
        (backup/'regen-before.json').write_bytes(original);(backup/'regen-installed.json').write_bytes(installed)
        target.write_bytes(installed);manifest['files'][relative]=sha(installed);write(root/'package-manifest.json',manifest)
        config['ProbeMap']=relative
        owners=[a['Pid'] for a in config['Accounts']]
        assert protected(db,owners)==read(backup/'typed-before.json')
        write(fixture,config)
        print('PET_PASSIVE_COMBAT_FIXTURE_READY effects=18 accounts=5; backup='+str(backup))
    finally: db.close()


if __name__ == '__main__': main()
