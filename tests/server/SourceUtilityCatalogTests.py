"""Actual original utility consumption on the SQL item worker and Unity use buttons."""
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
    parser.add_argument('--cohort',choices=('world','restoration','poison'),default='world')
    args=parser.parse_args();candidate=json.loads(args.candidate.read_text(encoding='utf8'));old=json.loads(args.baseline.read_text(encoding='utf8'));known={r['source_id'] for r in old['accepted']};added=sorted((r for r in candidate['accepted'] if r['source_id'] not in known),key=lambda r:r['source_id'])
    policies={27124:('cleanse',3,0,0,0),70008:('white_flag',5,0,0,0),70057:('courage_combined',6,0,0,0),71013:('firework_random',7,0,1,6),50103:('firework_4',11,0,4,4),50104:('firework_5',12,0,5,5),50105:('firework_6',13,0,6,6),50106:('firework_7',14,0,7,7),50107:('china_firework',15,0,0,0),50108:('spinning_top',16,0,0,0)}
    if args.cohort=='restoration':policies={50020:('restore_hp_full',17,0,0,0),50021:('restore_sp_full',18,0,0,0),50022:('restore_stamina_full',19,0,0,0)}
    if args.cohort=='poison':policies={27996:('self_poison',20,0,0,0)}
    assert {r['source_id'] for r in added}==set(policies)
    p=candidate['packet'];rows=lambda t:[dict(zip(p['columns'][t],r)) for r in p['tables'][t]];bases={(int(r['category_id']),int(r['local_id'])):r for r in rows('item_base')};quests={(int(r['category_id']),int(r['local_id'])):r for r in rows('item_quest')}
    for r in added:assert quests[r['item']['category_id'],r['item']['local_id']]['group_code']=='utility.'+policies[r['source_id']][0]
    packets=[]
    def verify(sandbox,catalog):
        db=sandbox.client;db.query('CREATE DATABASE player CHARACTER SET utf8mb4 COLLATE utf8mb4_bin')
        db.query('CREATE TABLE player.player(id INT UNSIGNED PRIMARY KEY,exp INT UNSIGNED NOT NULL DEFAULT 0,skill_point SMALLINT NOT NULL DEFAULT 0,sub_skill_point SMALLINT NOT NULL DEFAULT 0,horse_skill_point SMALLINT NOT NULL DEFAULT 0,skill_level BLOB NULL) ENGINE=InnoDB')
        db.query('CREATE TABLE player.player_deleted LIKE player.player')
        paths=sorted((ROOT/'db/migrations/item_instance').glob('[0-9][0-9][0-9]_*.sql'));assert [int(p.name[:3]) for p in paths]==list(range(1,45))
        for path in paths:
            for statement in statements(path.read_text(encoding='utf-8-sig')):db.query(statement)
        db.query("INSERT INTO player.item_persistence_format(singleton,schema_version,catalog_revision,catalog_sha256) VALUES(1,1,1,UNHEX('"+catalog['hash']+"'))")
        db.query("CREATE USER 'game_reader'@'localhost' IDENTIFIED BY '"+sandbox.password+"'")
        for name in ('protos','player'):db.query("GRANT SELECT ON "+name+".* TO 'game_reader'@'localhost'")
        for table in ('item_owner_fence','item_wallet','item_operation_receipt','item_instance','item_instance_attribute','item_instance_socket','item_occupied_cell','item_ground_core','item_ground_entry','item_book_learning','item_recovery_schedule','item_soul_strike_event'):
            db.query("GRANT INSERT,UPDATE,DELETE ON player."+table+" TO 'game_reader'@'localhost'")
        db.query("GRANT UPDATE ON player.item_instance_id_allocator TO 'game_reader'@'localhost'");db.query("GRANT UPDATE ON player.player TO 'game_reader'@'localhost'");db.query("GRANT INSERT ON player.item_mission_event TO 'game_reader'@'localhost'")
        output=(args.work/'source-utility-runtime').resolve();stdin=[str(sandbox.port),sandbox.password,str(len(catalog['tables']['item_base'])),catalog['hash'],'0',str(output),str(len(added))]
        for r in added:stdin.extend(map(str,(r['item']['category_id'],r['item']['local_id'],r['refinement_level'],0)))
        for r in added:stdin.extend(map(str,policies[r['source_id']][1:]))
        result=subprocess.run([str(args.test_executable.resolve()),'--utility-catalog'],input='\n'.join(stdin)+'\n',text=True,capture_output=True,timeout=300,env=dict(os.environ,MARIADB_TLS_DISABLE_PEER_VERIFICATION='1'))
        log=(result.stdout+result.stderr).replace(sandbox.password,'[redacted]');(args.work/'source-utility-worker.log').write_text(log,encoding='utf8')
        if result.returncode or 'SOURCE_UTILITY_WORKER=SUCCESS' not in log:raise RuntimeError('Source utility worker failed; see owned work log')
        for n,r in enumerate(added):
            for phase in ('bag','used','relogged','empty','empty_relogged'):
                files={kind:Path(str(output)+f'.{n}.{phase}.{kind}.hex') for kind in ('inventory','details')}
                for kind,path in files.items():
                    wire=bytes.fromhex(path.read_text());assert wire[0]=={'inventory':225,'details':227}[kind] and int.from_bytes(wire[1:5],'little')==len(wire)
                key=r['item']['category_id'],r['item']['local_id'];packets.append(dict(source=r['source_id'],category=key[0],local=key[1],level=r['refinement_level'],phase=phase,count=min(2,int(bases[key]['max_stack'])) if phase=='bag' else max(0,min(2,int(bases[key]['max_stack']))-1) if phase in ('used','relogged') else 0,handler=policies[r['source_id']][0],icon=bases[key]['icon_asset'],**{k:str(v) for k,v in files.items()}))
        sandbox.verify()
    result=stage(args.candidate,args.helper,args.work,args.mariadb_bin,verify);evidence=dict(success=True,candidate_sha256=file_hash(args.candidate),catalog_hash=result['catalog_hash'],sources=added,packets=packets,actual_game_item_worker=True,full_actor_world_test=False,installed_to_game=False)
    (args.work/'source-utility-worker-result.json').write_text(json.dumps(evidence,indent=2)+'\n',encoding='utf8');print(json.dumps(dict(success=True,sources=len(added),packets=len(packets),installed_to_game=False)))
if __name__=='__main__':main()
