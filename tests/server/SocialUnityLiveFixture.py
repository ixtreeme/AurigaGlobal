"""Seed two isolated local Social accounts; install only the additive Social schema and verify its final rows."""
from pathlib import Path
import argparse,json,secrets,socket,sys
REPO=Path(__file__).resolve().parents[2]
sys.path[:0]=[str(REPO/'SRC/Tools/ServerManager'),str(REPO/'db/tools'),str(Path(__file__).parent)]
import backend,item_bench
from mariadb_client import Client
from CostumeUnityLiveFixture import literal

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('action',choices=('seed','verify'));p.add_argument('--package',type=Path,required=True);p.add_argument('--fixture',type=Path,required=True);a=p.parse_args()
    path=a.fixture.resolve();assert path.is_relative_to(REPO/'build') and path.name.endswith('.private.json')
    root,manager,_,auth=item_bench.connection(a.package.resolve());db=Client(**auth,library=str(root/'bin/libmariadb.dll'))
    try:
        if a.action=='verify':
            c=json.loads(path.read_text(encoding='utf-8'));actors=c['Accounts'];pids=','.join(str(x['Pid']) for x in actors);names=','.join(literal(x['Name']) for x in actors)
            assert all(x['Login']=='codexsocial'+c['Token']+str(i) for i,x in enumerate(actors))
            assert db.query(f'SELECT COUNT(*) FROM player.social_friend_request WHERE requester_id IN({pids}) OR recipient_id IN({pids})')==[['0']]
            assert db.query(f'SELECT COUNT(*) FROM player.social_block WHERE owner_id IN({pids}) OR blocked_id IN({pids})')==[['0']]
            assert db.query(f'SELECT COUNT(*) FROM player.messenger_list WHERE account IN(SELECT name COLLATE utf8mb4_unicode_ci FROM player.player WHERE id IN({pids})) OR companion IN(SELECT name COLLATE utf8mb4_unicode_ci FROM player.player WHERE id IN({pids}))')==[['0']]
            assert db.query(f'SELECT owner_id,balance FROM player.item_wallet WHERE owner_kind=1 AND owner_id IN({pids}) AND currency=1 ORDER BY owner_id')==[[str(x['Pid']),'1000000'] for x in actors]
            print('SOCIAL_LOCAL_SQL=SUCCESS request_answer_remove_block_unblock_reconnect=verified native_wallets_unchanged=1');return
        with manager.supervisor_lock():
            assert not backend.describe(root)['running'] and not path.exists()
            for statement in (REPO/'db/migrations/social/001_social_contacts.sql').read_text(encoding='utf-8').split(';'):
                if statement.strip():db.query(statement)
            assert db.query("SELECT ENGINE FROM information_schema.TABLES WHERE TABLE_SCHEMA='player' AND TABLE_NAME='messenger_list'")==[['InnoDB']], 'Transactional messenger table required'
            token=secrets.token_hex(4);profile=json.loads((root/'client-connection.json').read_text(encoding='utf-8-sig'));host=json.loads((root/'server.json').read_text())['game_defaults']['network']['bind_ip']
            assert host=='127.0.0.1' or host in {r[4][0] for r in socket.getaddrinfo(socket.gethostname(),None,socket.AF_INET)}
            c=dict(Host=host,Token=token,ServerName=profile['Tls']['ServerName'],Pin=profile['Tls']['PinnedCertificateSha256'],Version=profile['ClientVersion'],AuthPort=profile['AuthPort'],GamePort=30078,MapIndex=41,X=986600,Y=269100,Accounts=[])
            source=int(db.query("SELECT p.id FROM player.player p JOIN account.account a ON a.id=p.account_id WHERE a.login LIKE 'codexitem%' ORDER BY p.id DESC LIMIT 1")[0][0])
            columns=[r[0] for r in db.query('SHOW COLUMNS FROM player.player') if r[0]!='id' and 'GENERATED' not in r[5]]
            for i in range(2):
                login='codexsocial'+token+str(i);password=secrets.token_hex(8);name='SO'+token+str(i)
                db.query('START TRANSACTION')
                try:
                    db.query('INSERT INTO account.account(login,password,social_id,status,create_time,coins) VALUES('+literal(login)+',PASSWORD('+literal(password)+"),'1234567','OK',NOW(),0)");aid=int(db.query('SELECT LAST_INSERT_ID()')[0][0])
                    overrides=dict(account_id=str(aid),name=name,job=str(i*2),level=str(60+i),st='10',ht='10',dx='10',iq='10',random_hp='30000',random_sp='10000',skill_group='1',map_index='41',x=str(c['X']+i*100),y=str(c['Y']),z='0',gold='0',hp='30000',mp='10000',typed_book_revision='0',horse_level='0',horse_riding='0')
                    projection=['REPEAT(CHAR(0),OCTET_LENGTH(`'+k+'`))' if k in ('skill_level','quickslot') else literal(overrides[k]) if k in overrides else '`'+k+'`' for k in columns]
                    db.query('INSERT INTO player.player('+','.join('`'+k+'`' for k in columns)+') SELECT '+','.join(projection)+f' FROM player.player WHERE id={source}');pid=int(db.query('SELECT LAST_INSERT_ID()')[0][0]);db.query(f'INSERT INTO player.player_index(id,pid1,empire) VALUES({aid},{pid},1)');db.query('COMMIT')
                except BaseException:db.query('ROLLBACK');raise
                c['Accounts'].append(dict(Login=login,Password=password,Name=name,Pid=pid,AccountId=aid));path.write_text(json.dumps(c,indent=2)+'\n',encoding='utf-8')
                tool=root/'manager/_internal/validators/ItemBenchTool.exe'
                for owner,fields in ((pid,dict(action='setup',gold=1000000)),(pid,dict(action='storage_setup',owner_kind=1)),(aid,dict(action='storage_setup',owner_kind=2))):item_bench.run_tool(dict(owner=owner,connection=auth,**fields),tool)
            print('SOCIAL_FIXTURE=READY dedicated_new_accounts=2 additive_schema=installed')
    finally:db.close()
if __name__=='__main__':main()
