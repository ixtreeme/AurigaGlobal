"""Three original utility items on actual SQL consumption, clocks and receipts."""
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
    args=ap.parse_args();candidate=json.loads(args.candidate.read_text());old=json.loads(args.baseline.read_text());known={r['source_id'] for r in old['accepted']};added=sorted((r for r in candidate['accepted'] if r['source_id'] not in known),key=lambda r:r['source_id']);assert {r['source_id'] for r in added}=={27610,71004,76008}
    p=candidate['packet'];rows=lambda t:[dict(zip(p['columns'][t],r)) for r in p['tables'][t]];key=lambda r:(int(r['category_id']),int(r['local_id']));bases={key(r):r for r in rows('item_base')};children={key(r):r for r in rows('item_consumable')};timed={r['profile_id']:r for r in rows('timed_effect_profile')};groups={r['group_id']:r for r in rows('timed_effect_group')};packets=[]
    for r in added:
        profile=timed[children[key(r['item'])]['timed_effect_profile_id']];fish=r['source_id']==27610;assert profile['bonus_profile_id'] is None and int(profile['utility_kind'])==(1 if fish else 2) and int(profile['duration_ms'])==(1200000 if fish else 536870911000);group=groups[profile['group_id']];assert (group['clock_kind'],group['reapply_kind'],group['remove_on_death'])==('1','1','1' if fish else '0')
    assert len({timed[children[key(r['item'])]['timed_effect_profile_id']]['group_id'] for r in added if r['source_id']!=27610})==1
    def verify(sandbox,catalog):
        db=sandbox.client;db.query('CREATE DATABASE player CHARACTER SET utf8mb4 COLLATE utf8mb4_bin');db.query('CREATE TABLE player.player(id INT UNSIGNED PRIMARY KEY,exp INT UNSIGNED NOT NULL DEFAULT 0,skill_point SMALLINT NOT NULL DEFAULT 0,sub_skill_point SMALLINT NOT NULL DEFAULT 0,horse_skill_point SMALLINT NOT NULL DEFAULT 0,skill_level BLOB NULL) ENGINE=InnoDB');db.query('CREATE TABLE player.player_deleted LIKE player.player')
        paths=sorted((ROOT/'db/migrations/item_instance').glob('[0-9][0-9][0-9]_*.sql'));assert [int(x.name[:3]) for x in paths]==list(range(1,45))
        for path in paths:
            for statement in statements(path.read_text(encoding='utf-8-sig')):db.query(statement)
        db.query("INSERT INTO player.item_persistence_format(singleton,schema_version,catalog_revision,catalog_sha256) VALUES(1,1,1,UNHEX('"+catalog['hash']+"'))");db.query("CREATE USER 'game_reader'@'localhost' IDENTIFIED BY '"+sandbox.password+"'")
        for name in ('protos','player'):db.query("GRANT SELECT ON "+name+".* TO 'game_reader'@'localhost'")
        for table in ('item_owner_fence','item_wallet','item_operation_receipt','item_instance','item_instance_attribute','item_instance_socket','item_occupied_cell','item_ground_core','item_ground_entry','item_book_learning','item_recovery_schedule','item_soul_strike_event'):db.query("GRANT INSERT,UPDATE,DELETE ON player."+table+" TO 'game_reader'@'localhost'")
        db.query("GRANT UPDATE ON player.item_instance_id_allocator TO 'game_reader'@'localhost'");db.query("GRANT UPDATE ON player.player TO 'game_reader'@'localhost'");db.query("GRANT INSERT ON player.item_mission_event TO 'game_reader'@'localhost'")
        output=(args.work/'source-timed-utility-runtime').resolve();stdin=[str(sandbox.port),sandbox.password,str(len(catalog['tables']['item_base'])),catalog['hash'],'0',str(output),str(len(added))]
        for r in added:stdin.extend(map(str,(*key(r['item']),r['refinement_level'],0)))
        result=subprocess.run([str(args.test_executable.resolve()),'--timed-utility-catalog'],input='\n'.join(stdin)+'\n',text=True,capture_output=True,timeout=300,env=dict(os.environ,MARIADB_TLS_DISABLE_PEER_VERIFICATION='1'))
        log=(result.stdout+result.stderr).replace(sandbox.password,'[redacted]');(args.work/'source-timed-utility-worker.log').write_text(log,encoding='utf8')
        if result.returncode or 'SOURCE_TIMED_UTILITY_WORKER=SUCCESS' not in log:raise RuntimeError('Timed utility worker failed; see owned work log')
        for n,r in enumerate(added):
            k=key(r['item']);profile=timed[children[k]['timed_effect_profile_id']]
            for phase in ('ready','active','relogged','elapsed','dead','spent','stale','last','expired'):
                files={kind:Path(str(output)+f'.{n}.{phase}.{kind}.hex') for kind in ('inventory','details','effects')}
                for kind,path in files.items():
                    wire=bytes.fromhex(path.read_text());assert wire[0]==dict(inventory=225,details=227,effects=243)[kind] and int.from_bytes(wire[1:5],'little')==len(wire)
                packets.append(dict(source=r['source_id'],category=k[0],local=k[1],utility=int(profile['utility_kind']),duration=int(profile['duration_ms']),phase=phase,icon=bases[k]['icon_asset'],**{k:str(v) for k,v in files.items()}))
        sandbox.verify()
    result=stage(args.candidate,args.helper,args.work,args.mariadb_bin,verify)
    evidence=dict(success=True,candidate_sha256=file_hash(args.candidate),catalog_hash=result['catalog_hash'],sources=added,packets=packets,actual_game_item_worker=True,full_actor_world_test=False,installed_to_game=False)
    (args.work/'source-timed-utility-worker-result.json').write_text(json.dumps(evidence,indent=2)+'\n',encoding='utf8');print(json.dumps(dict(success=True,sources=3,packets=len(packets),installed_to_game=False)))
if __name__=='__main__':main()
