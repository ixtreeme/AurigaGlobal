"""Exercise converted tool prototypes through the game item worker in a NEW DB."""
import argparse,json,os,subprocess,sys
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
sys.path[:0]=[str(ROOT/'SRC/Tools/ItemCatalogBuilder'),str(ROOT/'db/tools')]
from stage_exported_catalog import stage
from mariadb_client import statements
from legacy_item_migration import file_hash

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('candidate','helper','test-executable','work','mariadb-bin'):parser.add_argument('--'+name,required=True,type=Path)
    args=parser.parse_args();document=json.loads(args.candidate.read_text(encoding='utf-8'))
    refs={r['source_id']:r for r in document['accepted']}
    native=[refs[n]['item'] for n in (27580,27590,27570)]
    if any(k['category_id']!=11 for k in native):raise RuntimeError('Source tool graph is not converted')
    packets=[]
    def verify(sandbox,candidate):
        db=sandbox.client;db.query('CREATE DATABASE player CHARACTER SET utf8mb4 COLLATE utf8mb4_bin')
        db.query('CREATE TABLE player.player(id INT UNSIGNED PRIMARY KEY) ENGINE=InnoDB');db.query('CREATE TABLE player.player_deleted LIKE player.player')
        migrations=sorted((ROOT/'db/migrations/item_instance').glob('[0-9][0-9][0-9]_*.sql'))
        assert [int(p.name[:3]) for p in migrations]==list(range(1,45))
        for path in migrations:
            for sql in statements(path.read_text(encoding='utf-8-sig')):db.query(sql)
        db.query("INSERT INTO player.item_persistence_format(singleton,schema_version,catalog_revision,catalog_sha256) VALUES(1,1,1,UNHEX('"+candidate['hash']+"'))")
        db.query("CREATE USER 'game_reader'@'localhost' IDENTIFIED BY '"+sandbox.password+"'")
        db.query("GRANT SELECT ON protos.* TO 'game_reader'@'localhost'");db.query("GRANT SELECT ON player.* TO 'game_reader'@'localhost'")
        for table in ('item_owner_fence','item_wallet','item_operation_receipt','item_instance','item_instance_attribute','item_instance_socket','item_occupied_cell','item_ground_core','item_ground_entry'):
            db.query("GRANT INSERT,UPDATE,DELETE ON player."+table+" TO 'game_reader'@'localhost'")
        db.query("GRANT UPDATE ON player.item_instance_id_allocator TO 'game_reader'@'localhost'");db.query("GRANT INSERT ON player.item_mission_event TO 'game_reader'@'localhost'")
        output=(args.work/'source-tool-runtime').resolve()
        stdin='\n'.join([str(sandbox.port),sandbox.password,str(len(candidate['tables']['item_base'])),candidate['hash'],'0',str(output)]+[str(k['local_id']) for k in native])+'\n'
        result=subprocess.run([str(args.test_executable.resolve()),'--tool-catalog'],input=stdin,text=True,capture_output=True,timeout=120,env=dict(os.environ,MARIADB_TLS_DISABLE_PEER_VERIFICATION='1'))
        log=(result.stdout+result.stderr).replace(sandbox.password,'[redacted]');(args.work/'source-tool-worker.log').write_text(log,encoding='utf-8')
        if result.returncode or 'SOURCE_TOOL_WORKER=SUCCESS' not in log:raise RuntimeError('Source tool worker failed; see owned work log')
        for outcome in ('success','failure'):
            path=Path(str(output)+'.'+outcome+'.hex');packet=bytes.fromhex(path.read_text(encoding='ascii'))
            assert packet[0]==254 and packet[5]==5 and int.from_bytes(packet[1:5],'little')==len(packet)
            packets.append(dict(outcome=outcome,path=str(path),sha256=file_hash(path)))
        sandbox.verify()
    result=stage(args.candidate,args.helper,args.work,args.mariadb_bin,verify)
    evidence=dict(success=True,candidate_sha256=file_hash(args.candidate),catalog_hash=result['catalog_hash'],native_tool_keys=native,packets=packets,realms_modified=False,actual_game_item_worker=True,full_actor_world_test=False)
    (args.work/'source-tool-worker-result.json').write_text(json.dumps(evidence,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(dict(success=True,catalog_hash=result['catalog_hash'],packets=len(packets),installed_to_game=False)))

if __name__=='__main__':main()
