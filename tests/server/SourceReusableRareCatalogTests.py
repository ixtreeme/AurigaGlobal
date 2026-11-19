"""Original reusable rare reroller on actual SQL, with real original adder and three target families."""
import argparse,json,os,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path[:0]=[str(ROOT/'SRC/Tools/ItemCatalogBuilder'),str(ROOT/'db/tools')]
from stage_exported_catalog import stage
from mariadb_client import statements
from legacy_item_migration import file_hash
def main():
    ap=argparse.ArgumentParser()
    for name in ('candidate','baseline','helper','test-executable','work','mariadb-bin'):ap.add_argument('--'+name,required=True,type=Path)
    args=ap.parse_args();candidate=json.loads(args.candidate.read_text());old=json.loads(args.baseline.read_text());known={r['source_id'] for r in old['accepted']};added=sorted((r for r in candidate['accepted'] if r['source_id'] not in known),key=lambda r:r['source_id']);assert {r['source_id'] for r in added}=={71052}
    accepted={r['source_id']:r for r in candidate['accepted']};selected=[accepted[i] for i in (71052,30617,10,11200,14000)]
    p=candidate['packet'];rows=lambda t:[dict(zip(p['columns'][t],r)) for r in p['tables'][t]];key=lambda r:(int(r['category_id']),int(r['local_id']));bases={key(r):r for r in rows('item_base')};children={key(r):r for r in rows('item_consumable')};profiles={r['profile_id']:r for r in rows('attribute_service_profile')};profile=profiles[children[key(added[0]['item'])]['attribute_service_profile_id']]
    assert (profile['operation_kind'],profile['pool_kind'],profile['units_per_use'],profile['target_mask'],profile['minimum_count'],profile['maximum_count'])==('2','2','0','7','1','2');packets=[]
    def verify(sandbox,catalog):
        db=sandbox.client;db.query('CREATE DATABASE player CHARACTER SET utf8mb4 COLLATE utf8mb4_bin');db.query('CREATE TABLE player.player(id INT UNSIGNED PRIMARY KEY,exp INT UNSIGNED NOT NULL DEFAULT 0,skill_point SMALLINT NOT NULL DEFAULT 0,sub_skill_point SMALLINT NOT NULL DEFAULT 0,horse_skill_point SMALLINT NOT NULL DEFAULT 0,skill_level BLOB NULL) ENGINE=InnoDB');db.query('CREATE TABLE player.player_deleted LIKE player.player')
        paths=sorted((ROOT/'db/migrations/item_instance').glob('[0-9][0-9][0-9]_*.sql'));assert [int(x.name[:3]) for x in paths]==list(range(1,45))
        for path in paths:
            for statement in statements(path.read_text(encoding='utf-8-sig')):db.query(statement)
        db.query("INSERT INTO player.item_persistence_format(singleton,schema_version,catalog_revision,catalog_sha256) VALUES(1,1,1,UNHEX('"+catalog['hash']+"'))");db.query("CREATE USER 'game_reader'@'localhost' IDENTIFIED BY '"+sandbox.password+"'")
        for name in ('protos','player'):db.query("GRANT SELECT ON "+name+".* TO 'game_reader'@'localhost'")
        for table in ('item_owner_fence','item_wallet','item_operation_receipt','item_instance','item_instance_attribute','item_instance_socket','item_occupied_cell','item_ground_core','item_ground_entry','item_book_learning','item_recovery_schedule','item_soul_strike_event'):db.query("GRANT INSERT,UPDATE,DELETE ON player."+table+" TO 'game_reader'@'localhost'")
        db.query("GRANT UPDATE ON player.item_instance_id_allocator TO 'game_reader'@'localhost'");db.query("GRANT UPDATE ON player.player TO 'game_reader'@'localhost'");db.query("GRANT INSERT ON player.item_mission_event TO 'game_reader'@'localhost'")
        output=(args.work/'source-reusable-rare-runtime').resolve();stdin=[str(sandbox.port),sandbox.password,str(len(catalog['tables']['item_base'])),catalog['hash'],'0',str(output),str(len(selected))]
        for r in selected:stdin.extend(map(str,(*key(r['item']),r['refinement_level'],0)))
        result=subprocess.run([str(args.test_executable.resolve()),'--reusable-rare-catalog'],input='\n'.join(stdin)+'\n',text=True,capture_output=True,timeout=300,env=dict(os.environ,MARIADB_TLS_DISABLE_PEER_VERIFICATION='1'))
        log=(result.stdout+result.stderr).replace(sandbox.password,'[redacted]');(args.work/'source-reusable-rare-worker.log').write_text(log,encoding='utf8')
        if result.returncode or 'SOURCE_REUSABLE_RARE_WORKER=SUCCESS' not in log:raise RuntimeError('Reusable rare worker failed; see owned work log')
        for n,target in enumerate(selected[2:]):
            for phase in ('ready','used1','relogged1','ready2','used2','relogged2','stale'):
                files={kind:Path(str(output)+f'.{n}.{phase}.{kind}.hex') for kind in ('inventory','details','attributes')}
                for kind,path in files.items():
                    wire=bytes.fromhex(path.read_text());assert wire[0]==dict(inventory=225,details=227,attributes=236)[kind] and int.from_bytes(wire[1:5],'little')==len(wire)
                packets.append(dict(source=71052,target=target['source_id'],category=key(added[0]['item'])[0],local=key(added[0]['item'])[1],phase=phase,icon=bases[key(added[0]['item'])]['icon_asset'],**{k:str(v) for k,v in files.items()}))
        sandbox.verify()
    result=stage(args.candidate,args.helper,args.work,args.mariadb_bin,verify)
    evidence=dict(success=True,candidate_sha256=file_hash(args.candidate),catalog_hash=result['catalog_hash'],sources=added,packets=packets,actual_game_item_worker=True,full_actor_world_test=False,installed_to_game=False)
    (args.work/'source-reusable-rare-worker-result.json').write_text(json.dumps(evidence,indent=2)+'\n',encoding='utf8');print(json.dumps(dict(success=True,sources=1,targets=3,packets=len(packets),installed_to_game=False)))
if __name__=='__main__':main()
