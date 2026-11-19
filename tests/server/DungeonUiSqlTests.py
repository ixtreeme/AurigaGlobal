"""Dungeon result schema and UTC weekly-boundary checks in an owned scratch database."""
from pathlib import Path
import argparse,sys,uuid
REPO=Path(__file__).resolve().parents[2]
sys.path[:0]=[str(REPO/'SRC/Tools/ServerManager'),str(REPO/'db/tools')]
import item_bench
from mariadb_client import Client,statements

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--package',required=True,type=Path);a=p.parse_args()
    root,_,_,auth=item_bench.connection(a.package.resolve());db=Client(**auth,library=str(root/'bin/libmariadb.dll'));schema='codex_dungeon_sql_'+uuid.uuid4().hex[:12]
    try:
        db.query('CREATE DATABASE '+schema)
        try:
            for statement in statements((REPO/'db/migrations/dungeon/001_dungeon_ui.sql').read_text()):db.query(statement.replace('player.',schema+'.'))
            monday='DATE_SUB(UTC_DATE(),INTERVAL WEEKDAY(UTC_DATE()) DAY)'
            for i,date,seconds in [(1,f'DATE_SUB({monday},INTERVAL 1 MICROSECOND)',10),(2,monday,120),(3,f'DATE_ADD({monday},INTERVAL 1 MICROSECOND)',90)]:
                db.query(f"INSERT INTO {schema}.dungeon_ui_run VALUES(UNHEX('{i:032x}'),66,{date},{date},{seconds},1,0,3)")
                db.query(f"INSERT INTO {schema}.dungeon_ui_member VALUES(UNHEX('{i:032x}'),101,0x417276697A,60,0,5000000000,4000000000,3000000000,2500000000,0,1234,-500,1)")
            query=f'SELECT COUNT(*),MIN(r.elapsed),MAX(m.damage),MAX(m.experience),MIN(m.yang_delta) FROM {schema}.dungeon_ui_run r JOIN {schema}.dungeon_ui_member m ON m.run_id=r.id WHERE r.dungeon=66 AND r.completed_at>={monday} AND m.pid=101'
            expected=[['2','90','5000000000','1234','-500']];assert db.query(query)==expected
            db.query("SET time_zone='+09:00'");assert db.query(query)==expected;db.query("SET time_zone='+00:00'")
            assert db.query(f'SELECT HEX(r.id) FROM {schema}.dungeon_ui_run r JOIN {schema}.dungeon_ui_member m ON m.run_id=r.id WHERE m.pid=999')==[]
            db.query(f"INSERT INTO {schema}.dungeon_ui_loot VALUES(UNHEX('{2:032x}'),101,25769803802,4,99)")
            try:db.query(f"INSERT INTO {schema}.dungeon_ui_loot VALUES(UNHEX('{2:032x}'),999,25769803802,4,99)")
            except RuntimeError as error:assert 'foreign key' in str(error).lower()
            else:raise AssertionError('Loot accepted for a non-participant')
            db.query('START TRANSACTION')
            db.query(f"INSERT INTO {schema}.dungeon_ui_run VALUES(UNHEX('{4:032x}'),66,UTC_TIMESTAMP(3),UTC_TIMESTAMP(3),1,0,0,1)")
            db.query('ROLLBACK');assert db.query(f'SELECT COUNT(*) FROM {schema}.dungeon_ui_run')==[['3']]
            assert db.query(f'SELECT count FROM {schema}.dungeon_ui_loot')==[['99']]
            print('DUNGEON_UI_SQL_TESTS=PASS utc_monday_boundaries timezone_independence 64bit_metrics participant_scopes loot_foreign_keys transaction_rollback')
        finally:db.query('DROP DATABASE '+schema)
    finally:db.close()
if __name__=='__main__':main()
