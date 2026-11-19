"""Owned local encrypted-history fixture. No existing user accounts, items or money are changed."""
from pathlib import Path
import argparse,json,secrets,subprocess,sys
REPO=Path(__file__).resolve().parents[2]
sys.path[:0]=[str(REPO/'SRC/Tools/ServerManager'),str(REPO/'db/tools'),str(Path(__file__).parent)]
import backend,item_bench
from mariadb_client import Client,statements
from CostumeUnityLiveFixture import literal

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('action',choices=('prepare','verify'));p.add_argument('--package',type=Path,required=True);p.add_argument('--fixture',type=Path,required=True);a=p.parse_args()
    path=a.fixture.resolve();assert path.is_relative_to(REPO/'build') and path.name.endswith('.private.json')
    root,manager,_,auth=item_bench.connection(a.package.resolve())
    if a.action=='prepare':
        assert not backend.describe(root)['running'] and not path.exists()
        subprocess.run([sys.executable,'-B',str(REPO/'tests/server/SocialUnityLiveFixture.py'),'seed','--package',str(root),'--fixture',str(path)],check=True)
    db=Client(**auth,library=str(root/'bin/libmariadb.dll'))
    try:
        c=json.loads(path.read_text(encoding='utf-8'));actors=c['Accounts'];assert all(actor['Login']=='codexsocial'+c['Token']+str(i) for i,actor in enumerate(actors))
        if a.action=='verify':
            assert len(actors)==3;ids=','.join(str(actor['Pid']) for actor in actors)
            rows=db.query(f'SELECT COUNT(*),SUM(kind=1),SUM(kind=2) FROM player.social_chat_message WHERE sender_id IN({ids})');assert int(rows[0][1])>=45 and int(rows[0][2])>=1,rows
            assert db.query(f'SELECT COUNT(*) FROM player.social_chat_message WHERE sender_id IN({ids}) AND (LOCATE('+literal('HISTORY_'+c['Token'])+',ciphertext)>0 OR OCTET_LENGTH(nonce)<>12 OR OCTET_LENGTH(auth_tag)<>16)')==[['0']]
            target=int(db.query(f'SELECT MIN(id) FROM player.social_chat_message WHERE sender_id IN({ids})')[0][0])
            for sql in (f'UPDATE player.social_chat_message SET ciphertext=0x00 WHERE id={target}',f'DELETE FROM player.social_chat_message WHERE id={target}'):
                try:db.query(sql)
                except RuntimeError as error:assert 'append-only' in str(error)
                else:raise AssertionError('Message modification unexpectedly succeeded')
            assert db.query(f'SELECT owner_id,balance FROM player.item_wallet WHERE owner_kind=1 AND owner_id IN({ids}) AND currency=1 ORDER BY owner_id')==[[str(actor['Pid']),'1000000'] for actor in actors]
            first,second=actors[0]['Pid'],actors[1]['Pid']
            private_head=db.query(f'SELECT MAX(id) FROM player.social_chat_message WHERE kind=1 AND peer_a={first} AND peer_b={second}')[0][0]
            guild_head=db.query(f'SELECT MAX(id) FROM player.social_chat_message WHERE kind=2 AND peer_a={c["GuildId"]}')[0][0]
            assert db.query(f'SELECT last_id FROM player.social_chat_read WHERE player_id={second} AND kind=1 AND peer_a={first} AND peer_b={second}')==[[private_head]]
            assert db.query(f'SELECT last_id FROM player.social_chat_read WHERE player_id={second} AND kind=2 AND peer_a={c["GuildId"]} AND peer_b=0')==[[guild_head]]
            print('SOCIAL_HISTORY_SQL=SUCCESS encrypted_rows append_only_update_delete_refused native_wallets_unchanged=1');return
        for statement in statements((REPO/'db/migrations/social/002_social_history.sql').read_text(encoding='utf-8')):db.query(statement)
        if any(row[0]=='created_ms' for row in db.query('SHOW COLUMNS FROM player.social_chat_message')):
            for statement in statements((REPO/'db/migrations/social/003_social_history_datetime.sql').read_text(encoding='utf-8')):db.query(statement)
        columns=[row[0] for row in db.query('SHOW COLUMNS FROM player.player') if row[0]!='id' and 'GENERATED' not in row[5]]
        login='codexsocial'+c['Token']+'2';password=secrets.token_hex(8);name='SO'+c['Token']+'2'
        db.query('START TRANSACTION')
        try:
            db.query('INSERT INTO account.account(login,password,social_id,status,create_time,coins) VALUES('+literal(login)+',PASSWORD('+literal(password)+"),'1234567','OK',NOW(),0)");aid=int(db.query('SELECT LAST_INSERT_ID()')[0][0])
            overrides=dict(account_id=str(aid),name=name,job='1',level='60',x=str(c['X']+200))
            projection=[literal(overrides[k]) if k in overrides else '`'+k+'`' for k in columns]
            db.query('INSERT INTO player.player('+','.join('`'+k+'`' for k in columns)+') SELECT '+','.join(projection)+' FROM player.player WHERE id='+str(actors[0]['Pid']));pid=int(db.query('SELECT LAST_INSERT_ID()')[0][0]);db.query(f'INSERT INTO player.player_index(id,pid1,empire) VALUES({aid},{pid},1)')
            c['Accounts'].append(dict(Login=login,Password=password,Name=name,Pid=pid,AccountId=aid))
            db.query('INSERT INTO player.guild(name,master,sp,level,exp,skill_point,skill) VALUES('+literal('CH'+c['Token'])+f",{actors[0]['Pid']},1000,1,0,0,REPEAT(CHAR(0),12))");gid=int(db.query('SELECT LAST_INSERT_ID()')[0][0]);c['GuildId']=gid
            for i,actor in enumerate(actors[:2]):db.query(f"INSERT INTO player.guild_member(pid,guild_id,grade,is_general,offer) VALUES({actor['Pid']},{gid},{1 if i==0 else 15},0,0)")
            for grade in range(1,16):db.query(f'INSERT INTO player.guild_grade(guild_id,grade,name,auth) VALUES({gid},{grade},'+literal('Master' if grade==1 else 'Member')+f',{255 if grade==1 else 0})')
            db.query('COMMIT')
        except BaseException:db.query('ROLLBACK');raise
        path.write_text(json.dumps(c,indent=2)+'\n',encoding='utf-8')
        tool=root/'manager/_internal/validators/ItemBenchTool.exe'
        for owner,fields in ((pid,dict(action='setup',gold=1000000)),(pid,dict(action='storage_setup',owner_kind=1)),(aid,dict(action='storage_setup',owner_kind=2))):item_bench.run_tool(dict(owner=owner,connection=auth,**fields),tool)
        print('SOCIAL_HISTORY_FIXTURE=READY isolated_accounts=3 isolated_guild=1 additive_schema=installed')
    finally:db.close()
if __name__=='__main__':main()
