"""Converted secondary books/elixirs on the actual game item worker, private SQL only."""
import argparse,json,os,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path[:0]=[str(ROOT/'SRC/Tools/ItemCatalogBuilder'),str(ROOT/'db/tools')]
from stage_exported_catalog import stage
from mariadb_client import statements
from legacy_item_migration import file_hash

def main():
    p=argparse.ArgumentParser()
    for name in ('candidate','baseline','helper','test-executable','work','mariadb-bin'):p.add_argument('--'+name,required=True,type=Path)
    args=p.parse_args();candidate=json.loads(args.candidate.read_text());old=json.loads(args.baseline.read_text());known={r['source_id'] for r in old['accepted']}
    added=sorted((r for r in candidate['accepted'] if r['source_id'] not in known),key=lambda r:r['source_id']);assert len(added)==18
    packets=[]
    def verify(sandbox,catalog):
        db=sandbox.client;db.query('CREATE DATABASE player CHARACTER SET utf8mb4 COLLATE utf8mb4_bin')
        db.query("CREATE TABLE player.player(id INT UNSIGNED PRIMARY KEY,exp INT UNSIGNED NOT NULL DEFAULT 0,skill_point SMALLINT NOT NULL DEFAULT 0,sub_skill_point SMALLINT NOT NULL DEFAULT 0,horse_skill_point SMALLINT NOT NULL DEFAULT 0,skill_level BLOB NULL) ENGINE=InnoDB")
        db.query('CREATE TABLE player.player_deleted LIKE player.player')
        paths=sorted((ROOT/'db/migrations/item_instance').glob('[0-9][0-9][0-9]_*.sql'));assert [int(f.name[:3]) for f in paths]==list(range(1,45))
        for path in paths:
            for sql in statements(path.read_text(encoding='utf-8-sig')):db.query(sql)
        db.query("INSERT INTO player.item_persistence_format(singleton,schema_version,catalog_revision,catalog_sha256) VALUES(1,1,1,UNHEX('"+catalog['hash']+"'))")
        db.query("CREATE USER 'game_reader'@'localhost' IDENTIFIED BY '"+sandbox.password+"'");db.query("GRANT SELECT ON protos.* TO 'game_reader'@'localhost'");db.query("GRANT SELECT ON player.* TO 'game_reader'@'localhost'")
        for table in ('item_owner_fence','item_wallet','item_operation_receipt','item_instance','item_instance_attribute','item_instance_socket','item_occupied_cell','item_ground_core','item_ground_entry','item_book_learning'):
            db.query("GRANT INSERT,UPDATE,DELETE ON player."+table+" TO 'game_reader'@'localhost'")
        db.query("GRANT UPDATE ON player.item_instance_id_allocator TO 'game_reader'@'localhost'");db.query("GRANT UPDATE ON player.player TO 'game_reader'@'localhost'");db.query("GRANT INSERT ON player.item_mission_event TO 'game_reader'@'localhost'")
        output=(args.work/'source-service-runtime').resolve()
        stdin='\n'.join([str(sandbox.port),sandbox.password,str(len(catalog['tables']['item_base'])),catalog['hash'],'0',str(output),str(len(added))]+[str(v) for r in added for v in (r['item']['category_id'],r['item']['local_id'])])+'\n'
        result=subprocess.run([str(args.test_executable.resolve()),'--services-catalog'],input=stdin,text=True,capture_output=True,timeout=180,env=dict(os.environ,MARIADB_TLS_DISABLE_PEER_VERIFICATION='1'))
        log=(result.stdout+result.stderr).replace(sandbox.password,'[redacted]');(args.work/'source-service-worker.log').write_text(log,encoding='utf-8')
        if result.returncode or 'SOURCE_SERVICE_WORKER=SUCCESS books=8 elixirs=10' not in log:raise RuntimeError('Source service worker failed; see private work log')
        for n,ref in enumerate(added):
            for path in sorted(args.work.glob('source-service-runtime.'+str(n)+'.*.runtime.hex')):
                phase=path.name.split('.')[-3];paths={kind:Path(str(output)+'.'+str(n)+'.'+phase+'.'+kind+'.hex') for kind in ('inventory','details','runtime')}
                for kind,wire_path in paths.items():
                    wire=bytes.fromhex(wire_path.read_text());assert wire[0]==({'inventory':225,'details':227,'runtime':244 if ref['item']['category_id']==9 else 242}[kind]) and int.from_bytes(wire[1:5],'little')==len(wire)
                packets.append(dict(source=ref['source_id'],category=ref['item']['category_id'],phase=phase,**{k:str(v) for k,v in paths.items()},hashes={k:file_hash(v) for k,v in paths.items()}))
        assert len(packets)==81
        sandbox.verify()
    result=stage(args.candidate,args.helper,args.work,args.mariadb_bin,verify)
    evidence=dict(success=True,candidate_sha256=file_hash(args.candidate),catalog_hash=result['catalog_hash'],sources=added,packets=packets,realms_modified=False,actual_game_item_worker=True,full_actor_world_test=False)
    (args.work/'source-service-worker-result.json').write_text(json.dumps(evidence,indent=2)+'\n');print(json.dumps(dict(success=True,source_rows=len(added),packets=len(packets),installed_to_game=False)))

if __name__=='__main__':main()
