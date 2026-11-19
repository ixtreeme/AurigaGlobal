"""Verify and apply the lossless UTC DATETIME(3) history upgrade on an owned local package."""
from pathlib import Path
import argparse,sys,uuid
REPO=Path(__file__).resolve().parents[2]
sys.path[:0]=[str(REPO/'SRC/Tools/ServerManager'),str(REPO/'db/tools')]
import backend,item_bench
from mariadb_client import Client,statements

EPOCH="CAST('1970-01-01 00:00:00' AS DATETIME)"
def snapshot(db,table,date):
    timestamp=f'TIMESTAMPDIFF(MICROSECOND,{EPOCH},created_at) DIV 1000' if date else 'created_ms'
    return db.query(f'SELECT id,kind,peer_a,peer_b,sender_id,HEX(sender_name),{timestamp},empire,game_master,HEX(nonce),HEX(ciphertext),HEX(auth_tag) FROM {table} ORDER BY id')
def migrate(db,schema):
    for statement in statements((REPO/'db/migrations/social/003_social_history_datetime.sql').read_text(encoding='utf-8')):db.query(statement.replace('player.',schema+'.'))
def check_triggers(db,table):
    row=db.query(f'SELECT MIN(id) FROM {table}')[0][0]
    if row is None:return
    for statement in (f'UPDATE {table} SET ciphertext=0x00 WHERE id={int(row)}',f'DELETE FROM {table} WHERE id={int(row)}'):
        try:db.query(statement)
        except RuntimeError as error:assert 'append-only' in str(error)
        else:raise AssertionError('Append-only protection was lost')
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--package',type=Path,required=True);p.add_argument('--apply',action='store_true');a=p.parse_args()
    root,manager,_,auth=item_bench.connection(a.package.resolve());db=Client(**auth,library=str(root/'bin/libmariadb.dll'));schema='codex_history_datetime_'+uuid.uuid4().hex[:12]
    try:
        db.query('CREATE DATABASE '+schema)
        try:
            for statement in statements((REPO/'db/migrations/social/002_social_history.sql').read_text(encoding='utf-8')):db.query(statement.replace('player.',schema+'.'))
            samples=[1,999,1000,1700000000001,1711846799123,1794599699124,253402300799000]
            for index,stamp in enumerate(samples,1):db.query(f"INSERT INTO {schema}.social_chat_message VALUES({index},1,10,20,10,'Timestamp',{stamp},1,0,UNHEX(REPEAT('12',12)),0x00414200,UNHEX(REPEAT('34',16)))")
            before=snapshot(db,schema+'.social_chat_message',False);migrate(db,schema);assert snapshot(db,schema+'.social_chat_message',True)==before
            assert db.query(f'SELECT created_at FROM {schema}.social_chat_message WHERE id=6')==[['2026-11-13 19:54:59.124']]
            db.query("SET time_zone='+09:00'");assert snapshot(db,schema+'.social_chat_message',True)==before;db.query("SET time_zone='+00:00'")
            db.query(f"INSERT INTO {schema}.social_chat_message(id,kind,peer_a,peer_b,sender_id,sender_name,created_at,empire,game_master,nonce,ciphertext,auth_tag) VALUES(8,1,10,20,10,'Timestamp',TIMESTAMPADD(MICROSECOND,1794599846926*1000,{EPOCH}),1,0,UNHEX(REPEAT('12',12)),0x00414200,UNHEX(REPEAT('34',16)))")
            assert db.query(f'SELECT created_at FROM {schema}.social_chat_message WHERE id=8')==[['2026-11-13 19:57:26.926']]
            check_triggers(db,schema+'.social_chat_message');check_triggers(db,schema+'.social_chat_message_epoch_backup')
            print('SOCIAL_HISTORY_DATETIME_TESTS=PASS exact_milliseconds utc_timezone_independent new_writes append_only old_ciphertext_preserved')
        finally:db.query('DROP DATABASE '+schema)
        if a.apply:
            with manager.supervisor_lock():
                assert not backend.describe(root)['running'],'Stop game writers before schema replacement'
                columns={row[0]:row[1] for row in db.query('SHOW COLUMNS FROM player.social_chat_message')}
                if 'created_ms' in columns:
                    assert 'created_at' not in columns
                    before=snapshot(db,'player.social_chat_message',False);migrate(db,'player');assert snapshot(db,'player.social_chat_message',True)==before
                    print('SOCIAL_HISTORY_DATETIME_MIGRATION=APPLIED rows='+str(len(before))+' exact_timestamps_and_ciphertext_preserved=1')
                else:assert columns.get('created_at')=='datetime(3)'
                check_triggers(db,'player.social_chat_message')
                print('SOCIAL_HISTORY_DATETIME_PREVIEW',db.query('SELECT id,created_at FROM player.social_chat_message ORDER BY id LIMIT 2'))
    finally:db.close()
if __name__=='__main__':main()
