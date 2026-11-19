"""Original three-item costume transfer and its dependent source chest: actual SQL and native wire."""
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
    known={r['source_id'] for r in old['accepted']};added=[r for r in candidate['accepted'] if r['source_id'] not in known];assert {r['source_id'] for r in added}=={70065,50066}
    packet=candidate['packet'];rows=lambda table:[dict(zip(packet['columns'][table],r)) for r in packet['tables'][table]]
    bases={(int(r['category_id']),int(r['local_id'])):r for r in rows('item_base')}
    packets=[]
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
        for table in ('item_owner_fence','item_wallet','item_operation_receipt','item_instance','item_instance_attribute','item_instance_socket','item_occupied_cell','item_ground_core','item_ground_entry','item_book_learning','item_recovery_schedule','item_soul_strike_event'):
            db.query("GRANT INSERT,UPDATE,DELETE ON player."+table+" TO 'game_reader'@'localhost'")
        db.query("GRANT UPDATE ON player.item_instance_id_allocator TO 'game_reader'@'localhost'");db.query("GRANT UPDATE ON player.player TO 'game_reader'@'localhost'");db.query("GRANT INSERT ON player.item_mission_event TO 'game_reader'@'localhost'")
        output=(args.work/'source-costume-transfer-runtime').resolve()
        stdin=[sandbox.port,sandbox.password,len(catalog['tables']['item_base']),catalog['hash'],0,output,2]
        for r in added:stdin.extend((r['item']['category_id'],r['item']['local_id'],0,0))
        result=subprocess.run([str(args.test_executable.resolve()),'--costume-transfer-catalog'],input='\n'.join(map(str,stdin))+'\n',text=True,capture_output=True,timeout=300,env=dict(os.environ,MARIADB_TLS_DISABLE_PEER_VERIFICATION='1'))
        log=(result.stdout+result.stderr).replace(sandbox.password,'[redacted]');(args.work/'source-costume-transfer-worker.log').write_text(log)
        if result.returncode or 'SOURCE_COSTUME_TRANSFER_WORKER=SUCCESS sources=2' not in log:raise RuntimeError('Original costume transfer worker failed; see owned worker log')
        for n,r in enumerate(added):
            key=r['item'];phases=([name+'_'+step for name in ('body','hair','weapon','stole') for step in (('ready','denied','preview','stale','repreview','applied','replayed','relogged') if name=='body' else ('ready','preview','applied','replayed','relogged'))] if r['source_id']==70065 else ['ready']+['opened'+str(n) for n in range(17)])
            for phase in phases:
                files={kind:Path(str(output)+f'.{n}.{phase}.{kind}.hex') for kind in ('inventory','details',('transfer' if r['source_id']==70065 else 'container'))}
                for kind,path in files.items():
                    raw=bytes.fromhex(path.read_text());assert raw[0]==dict(inventory=225,details=227,transfer=220,container=233)[kind] and int.from_bytes(raw[1:5],'little')==len(raw)
                extra={}
                if r['source_id']==70065:
                    frame=bytes.fromhex(files['transfer'].read_text());extra=dict(case_kind={'body':1,'hair':2,'weapon':3,'stole':4}[phase.split('_')[0]],scroll=int.from_bytes(frame[15:23],'little'),receiver=int.from_bytes(frame[23:31],'little'),donor=int.from_bytes(frame[31:39],'little'),nonce=int.from_bytes(frame[7:15],'little'),expected_scroll_count=1 if phase.endswith(('applied','replayed','relogged')) else 2)
                packets.append(dict(source=r['source_id'],category=key['category_id'],local=key['local_id'],phase=phase,icon=bases[key['category_id'],key['local_id']]['icon_asset'],**extra,**{k:str(v) for k,v in files.items()}))
        assert len(packets)==41;sandbox.verify()
    result=stage(args.candidate,args.helper,args.work,args.mariadb_bin,verify)
    evidence=dict(success=True,candidate_sha256=file_hash(args.candidate),catalog_hash=result['catalog_hash'],new_sources=added,packets=packets,actual_game_item_worker=True,full_actor_world_test=False,installed_to_game=False)
    (args.work/'source-costume-transfer-worker-result.json').write_text(json.dumps(evidence,indent=2)+'\n');print(json.dumps(dict(success=True,sources=2,packets=len(packets),installed_to_game=False)))
if __name__=='__main__':main()
