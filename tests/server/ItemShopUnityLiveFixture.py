"""C45 item shop on the native wallet: own a fresh local account/character for the TLS flow.

seed   (stack stopped): new account + character on the native item system, an empty
       native account wallet and 5000 legacy coins in account.account.coins (the inbox).
verify (after METIN2_ITEMSHOP_LIVE=SUCCESS): exact SQL end state; no other owner changed.
"""
from pathlib import Path
from datetime import datetime, timezone
import argparse, json, os, secrets, subprocess, sys

REPO = Path(__file__).resolve().parents[2]
sys.path[:0] = [str(REPO / 'SRC/Tools/ServerManager'), str(REPO / 'db/tools')]
import item_bench, backend
from mariadb_client import Client
from PetPassiveUnityLiveFixture import protected
from CostumeUnityLiveFixture import read, write, literal

START_COINS = 5000
BUY_COUNT = 2


def others(db, c):
    owners = {(1, int(c['Accounts'][0]['Pid'])), (2, int(c['Accounts'][0]['AccountId']))}
    return {k: [r for r in rows if (int(r[0]), int(r[1])) not in owners] for k, rows in protected(db).items()}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('action', choices=('seed', 'verify', 'ring', 'content', 'verify-content'))
    p.add_argument('--package', type=Path, required=True)
    p.add_argument('--fixture', type=Path, required=True)
    p.add_argument('--log', type=Path)
    a = p.parse_args()
    fixture = a.fixture.resolve()
    assert fixture.is_relative_to(REPO / 'build')
    root, manager, ownership, auth = item_bench.connection(a.package.resolve())
    os.environ['PATH'] = str(root / 'bin') + os.pathsep + os.environ['PATH']
    db = Client(**auth, library=str(root / 'bin/libmariadb.dll'))
    tool = root / 'manager/_internal/validators/ItemBenchTool.exe'
    try:
        if a.action == 'verify':
            c = read(fixture); backup = Path(c['Backup'])
            assert 'METIN2_ITEMSHOP_LIVE=SUCCESS' in a.log.read_text(encoding='utf-8', errors='replace')
            assert others(db, c) == read(backup / 'typed-before.json'), 'another owner changed'
            account, pid = int(c['Accounts'][0]['AccountId']), int(c['Accounts'][0]['Pid'])
            paid = BUY_COUNT * int(c['ConvertedPrice'])
            assert db.query(f'SELECT coins FROM account.account WHERE id={account}') == [['0']], 'inbox swept'
            assert db.query(f'SELECT balance FROM player.item_wallet WHERE owner_kind=2 AND owner_id={account} AND currency=2') == [[str(START_COINS - paid)]]
            assert db.query(f'SELECT amount FROM player.item_wallet_legacy_sweep WHERE account_id={account}') == [[str(START_COINS)]], 'one ledger row'
            assert db.query(f'SELECT shop_id,units,paid FROM player.ishop_native_purchase WHERE account_id={account}') == [[str(c['ShopConverted']), str(BUY_COUNT), str(paid)]], 'one logged purchase'
            units = db.query(f"SELECT COALESCE(SUM(stack_count),0) FROM player.item_instance WHERE owner_kind=1 AND owner_id={pid} AND destroyed=0 AND category_id={c['ConvertedCategory']} AND local_id={c['ConvertedLocal']}")
            assert units == [[str(BUY_COUNT)]], units
            print(f'ITEMSHOP_LOCAL_SQL=SUCCESS wallet={START_COINS - paid} swept={START_COINS} units={BUY_COUNT}')
            return
        if a.action in ('content', 'verify-content'):
            # C46: converted shop families (free mount, paid mount, grade-4 stole), same account.
            c = read(fixture); account, pid = int(c['Accounts'][0]['AccountId']), int(c['Accounts'][0]['Pid'])
            if a.action == 'verify-content':
                assert 'METIN2_ITEMSHOP_CONTENT_LIVE=SUCCESS' in a.log.read_text(encoding='utf-8', errors='replace')
                paid = c['ContentMountPrice'] + c['ContentStolePrice']
                assert db.query(f'SELECT balance FROM player.item_wallet WHERE owner_kind=2 AND owner_id={account} AND currency=2') == [[str(c['ContentStartCoins'] - paid)]]
                for local, cat, kind in ((c['ContentMountFreeLocal'], 15, 1), (c['ContentMountLocal'], 15, 1), (c['ContentStoleLocal'], 19, 2)):
                    got = db.query(f'SELECT storage_kind,COUNT(*) FROM player.item_instance WHERE owner_kind=1 AND owner_id={pid} AND destroyed=0 AND category_id={cat} AND local_id={local} GROUP BY storage_kind')
                    assert got == [[str(kind), '1']], (cat, local, got)
                offers = (c['ContentMountFree'], c['ContentMount'], c['ContentStole'])
                ids = ','.join(str(offer) for offer in offers)
                logged = sorted(db.query(f'SELECT shop_id,units,paid FROM player.ishop_native_purchase WHERE account_id={account} AND shop_id IN ({ids})'))
                expected = sorted([[str(offer), '1', str(price)] for offer, price in zip(offers, (0, c['ContentMountPrice'], c['ContentStolePrice']))])
                assert logged == expected, (logged, expected)
                print(f'ITEMSHOP_CONTENT_SQL=SUCCESS wallet={c["ContentStartCoins"] - paid} stole_equipped=1')
                return
            assert 'ContentMount' not in c
            def pick(category, free):
                return db.query('SELECT d.offer_id,m.local_id,d.unit_price FROM player.ishop_offer d JOIN player.ishop_offer_member m ON m.offer_id=d.offer_id '
                                f"WHERE d.enabled=1 AND d.package_title IS NULL AND m.category_id={category} AND d.discount_percent=0 AND d.weekly_limit=0 AND d.monthly_limit=0 AND d.unit_price{'=0' if free else '>0'} ORDER BY d.unit_price,d.offer_id LIMIT 1")[0]
            free, mount, stole = pick(15, True), pick(15, False), pick(19, False)
            balance = int(db.query(f'SELECT balance FROM player.item_wallet WHERE owner_kind=2 AND owner_id={account} AND currency=2')[0][0])
            inbox = 1000  # legacy top-up, swept by the first shop read
            db.query(f'UPDATE account.account SET coins=coins+{inbox} WHERE id={account}')
            inbox = int(db.query(f'SELECT coins FROM account.account WHERE id={account}')[0][0])  # all of it is swept
            c.update(ContentMountFree=int(free[0]), ContentMountFreeLocal=int(free[1]), ContentMount=int(mount[0]), ContentMountLocal=int(mount[1]),
                     ContentMountPrice=int(mount[2]), ContentStole=int(stole[0]), ContentStoleLocal=int(stole[1]), ContentStolePrice=int(stole[2]),
                     ContentStartCoins=balance + inbox)
            assert c['ContentMountPrice'] + c['ContentStolePrice'] <= c['ContentStartCoins']
            write(fixture, c)
            print(f"ITEMSHOP_CONTENT=READY free={free[0]} mount={mount[0]} stole={stole[0]} coins={c['ContentStartCoins']}")
            return
        if a.action == 'ring':
            # After the shop run: one native teleport ring for the (offline) fixture character.
            c = read(fixture); pid = int(c['Accounts'][0]['Pid'])
            assert 'RingLocal' not in c
            key = db.query("SELECT b.category_id,b.local_id FROM protos.item_base b JOIN protos.item_quest q USING(category_id,local_id) WHERE q.group_code='utility.teleport_ring' AND b.retired=0")
            assert len(key) == 1 and key[0][0] == '10', key
            item_bench.run_tool(dict(action='grant', owner=pid, category=10, local=int(key[0][1]), count=1, connection=auth), tool)
            c['RingLocal'] = int(key[0][1]); write(fixture, c)
            print(f"ITEMSHOP_RING=READY local={c['RingLocal']}")
            return
        assert not fixture.exists()
        with manager.supervisor_lock():
            assert not backend.describe(root)['running'], 'Stop the local stack first'
            backup = root / 'run/deployments' / datetime.now(timezone.utc).strftime('c45-itemshop-fixture-%Y%m%dT%H%M%S%f')
            backup.mkdir()
            write(backup / 'typed-before.json', protected(db))
            profile = read(root / 'client-connection.json')
            token = secrets.token_hex(4)
            c = dict(Host='127.0.0.1', ServerName=profile['Tls']['ServerName'], Pin=profile['Tls']['PinnedCertificateSha256'],
                     Version=profile['ClientVersion'], AuthPort=profile['AuthPort'], GamePort=30078,  # ch99-core99 hosts map 41
                     Token=token, Accounts=[], X=987300, Y=268800, Backup=str(backup))
            row = db.query("SELECT d.offer_id,m.category_id,m.local_id,d.unit_price FROM player.ishop_offer d "
                           "JOIN player.ishop_offer_member m ON m.offer_id=d.offer_id "
                           "WHERE d.enabled=1 AND d.package_title IS NULL AND m.category_id=6 AND d.unit_price>250 AND d.unit_price<2500 "
                           "AND d.discount_percent=0 AND d.weekly_limit=0 AND d.monthly_limit=0 ORDER BY d.unit_price,d.offer_id LIMIT 1")[0]
            c.update(ShopConverted=int(row[0]),ConvertedCategory=int(row[1]),ConvertedLocal=int(row[2]),ConvertedPrice=int(row[3]))
            assert BUY_COUNT * c['ConvertedPrice'] < START_COINS < 20 * c['ConvertedPrice'], 'price window for refusal'
            c['ShopUnconverted']=4000000000
            assert db.query("SELECT COUNT(*) FROM player.ishop_offer WHERE offer_id=4000000000")==[['0']], 'reserved absent test offer exists'
            source = int(db.query("SELECT p.id FROM player.player p JOIN account.account a ON a.id=p.account_id WHERE a.login LIKE 'codexitem%' ORDER BY p.id DESC LIMIT 1")[0][0])
            columns = [r[0] for r in db.query('SHOW COLUMNS FROM player.player') if r[0] != 'id' and 'GENERATED' not in r[5]]
            db.query('START TRANSACTION')
            try:
                login = 'codexshop' + token + '0'; password = secrets.token_hex(8); name = 'IS' + token + '0'
                db.query('INSERT INTO account.account(login,password,social_id,status,create_time,coins) VALUES(' + literal(login) + ',PASSWORD(' + literal(password) + f"),'1234567','OK',NOW(),{START_COINS})")
                aid = int(db.query('SELECT LAST_INSERT_ID()')[0][0])
                overrides = dict(job='0', level='60', st='10', ht='10', dx='10', iq='10', random_hp='30000', random_sp='10000', skill_group='1', account_id=str(aid), name=name,
                                 map_index='41', x=str(c['X']), y=str(c['Y']), z='0', gold='0', hp='30000', mp='10000', typed_book_revision='0', horse_level='0', horse_riding='0')
                projection = ['REPEAT(CHAR(0),OCTET_LENGTH(`' + k + '`))' if k in ('skill_level', 'quickslot') else literal(overrides[k]) if k in overrides else '`' + k + '`' for k in columns]
                db.query('INSERT INTO player.player(' + ','.join('`' + k + '`' for k in columns) + ') SELECT ' + ','.join(projection) + f' FROM player.player WHERE id={source}')
                pid = int(db.query('SELECT LAST_INSERT_ID()')[0][0])
                db.query(f'INSERT INTO player.player_index(id,pid1,empire) VALUES({aid},{pid},1)')
                db.query('COMMIT')
            except BaseException:
                db.query('ROLLBACK'); raise
            c['Accounts'].append(dict(Login=login, Password=password, Name=name, AccountId=aid, Pid=pid, Slot=0))
            write(backup / 'fixture-pending.private.json', c)
            bench = lambda action, owner, **f: item_bench.run_tool(dict(action=action, owner=owner, connection=auth, **f), tool)
            bench('setup', pid, gold=0)
            bench('storage_setup', pid, owner_kind=1)
            bench('storage_setup', aid, owner_kind=2)
            assert db.query(f'SELECT balance FROM player.item_wallet WHERE owner_kind=2 AND owner_id={aid} AND currency=2') == [['0']]
            assert others(db, c) == read(backup / 'typed-before.json')
            write(fixture, c)
            print(f"ITEMSHOP_FIXTURE=READY shop_converted={c['ShopConverted']} shop_unconverted={c['ShopUnconverted']} legacy_coins={START_COINS}")
    finally:
        db.close()


if __name__ == '__main__':
    main()
