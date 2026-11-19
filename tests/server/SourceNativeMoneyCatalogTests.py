"""Original native Yang and trusted guild payout safebox/redemption/drop and native SQL/wire."""
import argparse,json,os,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path[:0]=[str(ROOT/'SRC/Tools/ItemCatalogBuilder'),str(ROOT/'db/tools')]
from stage_exported_catalog import stage
from mariadb_client import statements
from legacy_item_migration import file_hash

def main():
    parser=argparse.ArgumentParser()
    for name in ('candidate','baseline','helper','test-executable','work','mariadb-bin'):parser.add_argument('--'+name,required=True,type=Path)
    args=parser.parse_args();candidate=json.loads(args.candidate.read_text());old=json.loads(args.baseline.read_text())
    known={r['source_id'] for r in old['accepted']};added=[r for r in candidate['accepted'] if r['source_id'] not in known];assert {r['source_id'] for r in added}=={1,50026}
    packet=candidate['packet'];rows=lambda table:[dict(zip(packet['columns'][table],r)) for r in packet['tables'][table]]
    bases={(int(r['category_id']),int(r['local_id'])):r for r in rows('item_base')}
    packets=[]
    def verify(sandbox,catalog):
        db=sandbox.client;db.query('CREATE DATABASE player CHARACTER SET utf8mb4 COLLATE utf8mb4_bin')
        db.query("CREATE TABLE player.player(id INT UNSIGNED PRIMARY KEY,exp INT UNSIGNED NOT NULL DEFAULT 0,skill_point SMALLINT NOT NULL DEFAULT 0,sub_skill_point SMALLINT NOT NULL DEFAULT 0,horse_skill_point SMALLINT NOT NULL DEFAULT 0,skill_level BLOB NULL,alignment INT UNSIGNED NOT NULL DEFAULT 0,level SMALLINT UNSIGNED NOT NULL DEFAULT 1,level_step SMALLINT UNSIGNED NOT NULL DEFAULT 0,stat_point SMALLINT UNSIGNED NOT NULL DEFAULT 0,random_hp SMALLINT UNSIGNED NOT NULL DEFAULT 0,random_sp SMALLINT UNSIGNED NOT NULL DEFAULT 0,name VARCHAR(32) NOT NULL DEFAULT '') ENGINE=InnoDB")
        db.query('CREATE TABLE player.player_deleted LIKE player.player')
        db.query('ALTER TABLE player.player ADD account_id INT UNSIGNED NOT NULL DEFAULT 0')
        db.query("INSERT INTO player.player(id,account_id,name) VALUES(23001,71,'moneytest')")
        db.query('CREATE DATABASE account CHARACTER SET utf8mb4 COLLATE utf8mb4_bin')
        db.query('CREATE TABLE account.account(id INT UNSIGNED PRIMARY KEY,login VARCHAR(32) NOT NULL) ENGINE=InnoDB')
        db.query("INSERT INTO account.account VALUES(71,'moneytest'),(72,'wrongtest')")
        paths=sorted((ROOT/'db/migrations/item_instance').glob('[0-9][0-9][0-9]_*.sql'));assert [int(p.name[:3]) for p in paths]==list(range(1,45))
        for path in paths:
            for statement in statements(path.read_text(encoding='utf-8-sig')):db.query(statement)
        db.query("INSERT INTO player.item_persistence_format(singleton,schema_version,catalog_revision,catalog_sha256) VALUES(1,1,1,UNHEX('"+catalog['hash']+"'))")
        db.query("CREATE USER 'game_reader'@'localhost' IDENTIFIED BY '"+sandbox.password+"'")
        for name in ('protos','player','account'):db.query("GRANT SELECT ON "+name+".* TO 'game_reader'@'localhost'")
        for table in ('item_owner_fence','item_wallet','item_operation_receipt','item_instance','item_instance_attribute','item_instance_socket','item_occupied_cell','item_ground_core','item_ground_entry','item_book_learning','item_recovery_schedule','item_soul_strike_event','item_currency_award','item_currency_ground'):
            db.query("GRANT INSERT,UPDATE,DELETE ON player."+table+" TO 'game_reader'@'localhost'")
        db.query("GRANT UPDATE ON player.item_instance_id_allocator TO 'game_reader'@'localhost'");db.query("GRANT UPDATE ON player.player TO 'game_reader'@'localhost'");db.query("GRANT INSERT ON player.item_mission_event TO 'game_reader'@'localhost'")
        output=(args.work/'source-native-money-runtime').resolve()
        stdin=[sandbox.port,sandbox.password,len(catalog['tables']['item_base']),catalog['hash'],0,output,2]
        for r in added:stdin.extend((r['item']['category_id'],r['item']['local_id'],0,0))
        result=subprocess.run([str(args.test_executable.resolve()),'--native-money-catalog'],input='\n'.join(map(str,stdin))+'\n',text=True,capture_output=True,timeout=300,env=dict(os.environ,MARIADB_TLS_DISABLE_PEER_VERIFICATION='1'))
        log=(result.stdout+result.stderr).replace(sandbox.password,'[redacted]');(args.work/'source-native-money-worker.log').write_text(log)
        if result.returncode or 'SOURCE_NATIVE_MONEY_WORKER=SUCCESS sources=2' not in log:raise RuntimeError('Original energy crystal worker failed; see owned worker log')
        key=next(r['item'] for r in added if r['source_id']==50026)
        phases=('bank_received','withdrawn','token_ready','redeemed','redeem_replayed','relogged','drop_ready','dropped','drop_replayed','picked','pickup_replayed','duplicate_denied','cap_denied')
        for phase in phases:
            kinds=['inventory','details']
            if phase=='bank_received':kinds.append('storage')
            if phase=='token_ready':kinds.append('token')
            if phase in ('drop_ready','dropped'):kinds.append('money')
            if phase=='dropped':kinds.append('ground')
            files={kind:Path(str(output)+f'.{phase}.{kind}.hex') for kind in kinds}
            for kind,path in files.items():
                raw=bytes.fromhex(path.read_text());assert raw[0]==dict(inventory=225,details=227,storage=201,token=234,money=216,ground=226)[kind] and int.from_bytes(raw[1:5],'little')==len(raw)
            facts=json.loads(Path(str(output)+f'.{phase}.json').read_text())
            packets.append(dict(category=key['category_id'],local=key['local_id'],phase=phase,icon=bases[key['category_id'],key['local_id']]['icon_asset'],**facts,**{k:str(v) for k,v in files.items()}))
        assert len(packets)==13;sandbox.verify()
    result=stage(args.candidate,args.helper,args.work,args.mariadb_bin,verify)
    evidence=dict(success=True,candidate_sha256=file_hash(args.candidate),catalog_hash=result['catalog_hash'],new_sources=added,packets=packets,actual_game_item_worker=True,full_actor_world_test=False,installed_to_game=False)
    (args.work/'source-native-money-worker-result.json').write_text(json.dumps(evidence,indent=2)+'\n');print(json.dumps(dict(success=True,sources=2,packets=len(packets),installed_to_game=False)))
if __name__=='__main__':main()
