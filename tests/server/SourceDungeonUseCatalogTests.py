"""Actual native SQL consumption/replay/relogin of twelve original dungeon stage items."""
import argparse,json,os,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path[:0]=[str(ROOT/'SRC/Tools/ItemCatalogBuilder'),str(ROOT/'db/tools')]
from stage_exported_catalog import stage
from mariadb_client import statements
from legacy_item_migration import file_hash
SOURCES={30001,30002,30003,30004,30005,30007,30931,30932,30933,33015,33016,33018}
def main():
    p=argparse.ArgumentParser()
    for name in ('candidate','helper','test-executable','work','mariadb-bin'):p.add_argument('--'+name,required=True,type=Path)
    args=p.parse_args();candidate=json.loads(args.candidate.read_text());sources=[r for r in candidate['accepted'] if r['source_id'] in SOURCES];assert {r['source_id'] for r in sources}==SOURCES;packets=[];wallet_packets=[]
    def verify(sandbox,catalog):
        db=sandbox.client;db.query('CREATE DATABASE player CHARACTER SET utf8mb4 COLLATE utf8mb4_bin')
        db.query("CREATE TABLE player.player(id INT UNSIGNED PRIMARY KEY,exp INT UNSIGNED NOT NULL DEFAULT 0,skill_point SMALLINT NOT NULL DEFAULT 0,sub_skill_point SMALLINT NOT NULL DEFAULT 0,horse_skill_point SMALLINT NOT NULL DEFAULT 0,skill_level BLOB NULL,alignment INT UNSIGNED NOT NULL DEFAULT 0,level SMALLINT UNSIGNED NOT NULL DEFAULT 1,level_step SMALLINT UNSIGNED NOT NULL DEFAULT 0,stat_point SMALLINT UNSIGNED NOT NULL DEFAULT 0,random_hp SMALLINT UNSIGNED NOT NULL DEFAULT 0,random_sp SMALLINT UNSIGNED NOT NULL DEFAULT 0,name VARCHAR(32) NOT NULL DEFAULT '') ENGINE=InnoDB")
        db.query('CREATE TABLE player.player_deleted LIKE player.player')
        paths=sorted((ROOT/'db/migrations/item_instance').glob('[0-9][0-9][0-9]_*.sql'));assert [int(p.name[:3]) for p in paths]==list(range(1,45))
        for path in paths:
            for statement in statements(path.read_text(encoding='utf-8-sig')):db.query(statement)
        db.query("INSERT INTO player.item_persistence_format(singleton,schema_version,catalog_revision,catalog_sha256) VALUES(1,1,1,UNHEX('"+catalog['hash']+"'))")
        db.query("CREATE USER 'game_reader'@'localhost' IDENTIFIED BY '"+sandbox.password+"'")
        for name in ('protos','player'):db.query("GRANT SELECT ON "+name+".* TO 'game_reader'@'localhost'")
        for table in ('item_owner_fence','item_wallet','item_operation_receipt','item_instance','item_instance_attribute','item_instance_socket','item_occupied_cell','item_ground_core','item_ground_entry','item_book_learning','item_recovery_schedule','item_soul_strike_event','item_currency_award','item_currency_ground','item_private_shop','item_private_shop_price','item_storage','item_dungeon_entry'):
            db.query("GRANT INSERT,UPDATE,DELETE ON player."+table+" TO 'game_reader'@'localhost'")
        db.query("GRANT UPDATE ON player.item_instance_id_allocator TO 'game_reader'@'localhost'");db.query("GRANT UPDATE ON player.player TO 'game_reader'@'localhost'");db.query("GRANT INSERT ON player.item_mission_event TO 'game_reader'@'localhost'")
        output=(args.work/'source-dungeon-use-runtime').resolve();stdin=[sandbox.port,sandbox.password,len(catalog['tables']['item_base']),catalog['hash'],0,output,len(sources)]
        for r in sources:stdin.extend((r['item']['category_id'],r['item']['local_id'],0,0))
        result=subprocess.run([str(args.test_executable.resolve()),'--dungeon-use-catalog'],input='\n'.join(map(str,stdin))+'\n',text=True,capture_output=True,timeout=300,env=dict(os.environ,MARIADB_TLS_DISABLE_PEER_VERIFICATION='1'))
        log=(result.stdout+result.stderr).replace(sandbox.password,'[redacted]');(args.work/'source-dungeon-use-worker.log').write_text(log)
        if result.returncode or 'SOURCE_DUNGEON_USE_WORKER=SUCCESS sources=12' not in log:raise RuntimeError('Dungeon entry worker failed; see owned worker log')
        packet=candidate['packet'];bases={(int(r[0]),int(r[1])):dict(zip(packet['columns']['item_base'],r)) for r in packet['tables']['item_base']}
        for n,source in enumerate(sources):
            key=source['item']
            for phase in ('ready','used','replayed','relogged'):
                files={kind:Path(str(output)+f'.{n}.{phase}.{kind}.hex') for kind in ('inventory','details')}
                for kind,path in files.items():
                    raw=bytes.fromhex(path.read_text());assert raw[0]==dict(inventory=225,details=227)[kind] and int.from_bytes(raw[1:5],'little')==len(raw)
                packets.append(dict(source=source['source_id'],category=key['category_id'],local=key['local_id'],phase=phase,expected_count=2 if phase=='ready' else 1,icon=bases[key['category_id'],key['local_id']]['icon_asset'],**{k:str(v) for k,v in files.items()}))
        for phase in ('before','paid','replayed','reward','relogged'):
            inventory=Path(str(output)+'.wallet.'+phase+'.inventory.hex');facts=json.loads(Path(str(output)+'.wallet.'+phase+'.json').read_text());assert facts['balance']==(1000 if phase in ('paid','replayed') else 7000001001)
            wallet_packets.append(dict(phase=phase,inventory=str(inventory),**facts))
        sandbox.verify()
    result=stage(args.candidate,args.helper,args.work,args.mariadb_bin,verify)
    evidence=dict(success=True,candidate_sha256=file_hash(args.candidate),catalog_hash=result['catalog_hash'],sources=sources,packets=packets,wallet_packets=wallet_packets,actual_game_item_worker=True,full_actor_world_test=False,installed_to_game=False)
    (args.work/'source-dungeon-use-worker-result.json').write_text(json.dumps(evidence,indent=2)+'\n');print(json.dumps(dict(success=True,sources=12,packets=len(packets),installed_to_game=False)))
if __name__=='__main__':main()
