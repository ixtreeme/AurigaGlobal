"""Captured sash definitions on the real asynchronous SQL item worker."""
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
    args=ap.parse_args();candidate=json.loads(args.candidate.read_text(encoding='utf8'));baseline=json.loads(args.baseline.read_text(encoding='utf8'));known={r['source_id'] for r in baseline['accepted']};added=sorted((r for r in candidate['accepted'] if r['source_id'] not in known),key=lambda r:r['source_id'])
    expected=set(range(85001,85009))|set(range(85011,85019))|set(range(85071,85083))|{86080};assert {r['source_id'] for r in added}==expected
    p=candidate['packet'];rows=lambda t:[dict(zip(p['columns'][t],r)) for r in p['tables'][t]]
    key=lambda r:(int(r['category_id']),int(r['local_id']))
    bases={key(r):r for r in rows('item_base')};regalia={key(r):r for r in rows('item_regalia')};profiles={key(r):r for r in rows('absorption_sash_profile')};appearances={int(r['profile_id']):r for r in rows('item_appearance_profile')}
    sashes=[r for r in added if r['source_id']!=86080];clear=next(r for r in added if r['source_id']==86080);clear_key=clear['item']['category_id'],clear['item']['local_id']
    assert len(profiles)==28 and len(rows('sash_donor_profile'))==2940
    timed={key(r) for r in rows('item_definition_rules') if r.get('clock_kind') is not None and int(r['clock_kind'])>1}
    permanent=next(k for k,r in regalia.items() if int(r['regalia_kind'])==1 and k not in timed)
    # Real birth-magic profiles supply persisted attributes; choose source equipment,
    # not a synthetic donor or hand-written instance payload.
    bindings={key(r) for r in rows('item_definition_rules') if r.get('birth_attribute_profile_id') is not None}
    donor_rows=rows('sash_donor_profile');donors=[]
    for category in (1,2):
        d=next(r for r in donor_rows if key(r)[0]==category and key(r) in bindings and int(r['refinement_level'])==0)
        donors.append((*key(d),int(d['refinement_level'])))
    packets=[]
    def verify(sandbox,catalog):
        db=sandbox.client;db.query('CREATE DATABASE player CHARACTER SET utf8mb4 COLLATE utf8mb4_bin')
        db.query('CREATE TABLE player.player(id INT UNSIGNED PRIMARY KEY,exp INT UNSIGNED NOT NULL DEFAULT 0,skill_point SMALLINT NOT NULL DEFAULT 0,sub_skill_point SMALLINT NOT NULL DEFAULT 0,horse_skill_point SMALLINT NOT NULL DEFAULT 0,skill_level BLOB NULL) ENGINE=InnoDB');db.query('CREATE TABLE player.player_deleted LIKE player.player')
        paths=sorted((ROOT/'db/migrations/item_instance').glob('[0-9][0-9][0-9]_*.sql'));assert [int(x.name[:3]) for x in paths]==list(range(1,45))
        for path in paths:
            for statement in statements(path.read_text(encoding='utf-8-sig')):db.query(statement)
        db.query("INSERT INTO player.item_persistence_format(singleton,schema_version,catalog_revision,catalog_sha256) VALUES(1,1,1,UNHEX('"+catalog['hash']+"'))")
        db.query("CREATE USER 'game_reader'@'localhost' IDENTIFIED BY '"+sandbox.password+"'")
        for name in ('protos','player'):db.query("GRANT SELECT ON "+name+".* TO 'game_reader'@'localhost'")
        for table in ('item_owner_fence','item_wallet','item_operation_receipt','item_instance','item_instance_attribute','item_instance_socket','item_occupied_cell','item_ground_core','item_ground_entry','item_book_learning','item_recovery_schedule','item_soul_strike_event'):db.query("GRANT INSERT,UPDATE,DELETE ON player."+table+" TO 'game_reader'@'localhost'")
        db.query("GRANT UPDATE ON player.item_instance_id_allocator TO 'game_reader'@'localhost'");db.query("GRANT UPDATE ON player.player TO 'game_reader'@'localhost'");db.query("GRANT INSERT ON player.item_mission_event TO 'game_reader'@'localhost'")
        output=(args.work/'source-sash-runtime').resolve();stdin=[str(sandbox.port),sandbox.password,str(len(catalog['tables']['item_base'])),catalog['hash'],'0',str(output),str(len(sashes))]
        for r in sashes:stdin.extend(map(str,(*key(r['item']),r['refinement_level'],0)))
        stdin.extend(map(str,(*clear_key,*permanent,*donors[0],*donors[1])))
        result=subprocess.run([str(args.test_executable.resolve()),'--absorption-sash-catalog'],input='\n'.join(stdin)+'\n',text=True,capture_output=True,timeout=600,env=dict(os.environ,MARIADB_TLS_DISABLE_PEER_VERIFICATION='1'))
        log=(result.stdout+result.stderr).replace(sandbox.password,'[redacted]');(args.work/'source-sash-worker.log').write_text(log,encoding='utf8')
        if result.returncode or 'SOURCE_ABSORPTION_SASH_WORKER=SUCCESS' not in log:raise RuntimeError('Source absorption sash worker failed; see owned work log')
        for n,r in enumerate(sashes):
            k=key(r['item']);grade=int(profiles[k]['grade']);model=appearances[int(bases[k]['appearance_profile_id'])]['model_asset']
            for phase in ('absorb_ready','absorbed','equipped','clear_ready','cleared','combine_ready','combined',*(['failed'] if grade>1 else [])):
                files={kind:Path(str(output)+f'.{n}.{phase}.{kind}.hex') for kind in ('inventory','details','appearance','reply')};files={k:v for k,v in files.items() if v.exists()}
                assert {'inventory','details','appearance'}<=files.keys()
                for kind,path in files.items():
                    wire=bytes.fromhex(path.read_text());assert wire[0]==dict(inventory=225,details=227,appearance=241,reply=220)[kind] and int.from_bytes(wire[1:5],'little')==len(wire)
                packets.append(dict(source=r['source_id'],category=k[0],local=k[1],grade=grade,model=model,index=n,phase=phase,**{k:str(v) for k,v in files.items()}))
        sandbox.verify()
    result=stage(args.candidate,args.helper,args.work,args.mariadb_bin,verify)
    evidence=dict(success=True,candidate_sha256=file_hash(args.candidate),catalog_hash=result['catalog_hash'],sources=added,packets=packets,donors=donors,stole=permanent,clear=clear_key,actual_game_item_worker=True,full_actor_world_test=False,installed_to_game=False)
    (args.work/'source-sash-worker-result.json').write_text(json.dumps(evidence,indent=2)+'\n',encoding='utf8');print(json.dumps(dict(success=True,sources=len(added),packets=len(packets),installed_to_game=False)))
if __name__=='__main__':main()
