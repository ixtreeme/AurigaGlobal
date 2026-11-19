"""Converted book aids, instant/timed potions, costume reset and stone draws on the real SQL item worker."""
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
    args=parser.parse_args();candidate=json.loads(args.candidate.read_text(encoding='utf-8'));old=json.loads(args.baseline.read_text(encoding='utf-8'))
    known={r['source_id'] for r in old['accepted']};added=sorted((r for r in candidate['accepted'] if r['source_id'] not in known),key=lambda x:x['source_id'])
    assert len(added)==11
    packet=candidate['packet']
    def rows(table):return [dict(zip(packet['columns'][table],r)) for r in packet['tables'][table]]
    bases={(int(r['category_id']),int(r['local_id'])):r for r in rows('item_base')}
    bindings={(int(r['category_id']),int(r['local_id'])):r['profile_id'] for r in rows('item_attribute_binding')}
    policies={r['policy_id']:r for r in rows('item_use_policy')}
    source_refs={(r['item']['category_id'],r['item']['local_id']):r for r in candidate['accepted'] if r['refinement_level']==0}
    def target(category):
        for key,profile in bindings.items():
            if key[0]!=category or key not in source_refs:continue
            use=policies[bases[key]['use_policy_id']]
            if int(use['allowed_classes'])&1 and int(use['allowed_sexes'])&1 and any(r['profile_id']==profile and r['pool_kind']=='1' and int(r['capacity'])>=2 for r in rows('attribute_group')):return source_refs[key]
        raise AssertionError('No captured eligible target')
    costume=target(4);refs={r['source_id']:r for r in candidate['accepted']}
    services={r['profile_id']:r for r in rows('attribute_service_profile')}
    seed=next((5,int(r['local_id'])) for r in rows('item_consumable') if r['attribute_service_profile_id'] and (p:=services[r['attribute_service_profile_id']])['operation_kind']=='1' and p['pool_kind']=='1' and int(p['target_mask'])&8 and p['success_bp']=='10000' and p['units_per_use']=='1' and p['minimum_count']=='0')
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
        output=(args.work/'source-context-runtime').resolve();stdin=[str(sandbox.port),sandbox.password,str(len(catalog['tables']['item_base'])),catalog['hash'],'0',str(output),str(len(added))]
        for ref in added:stdin.extend(map(str,(ref['item']['category_id'],ref['item']['local_id'],ref['refinement_level'],0)))
        stdin.extend(map(str,(costume['item']['category_id'],costume['item']['local_id'],costume['refinement_level'])))
        for key in (seed,(20,refs[70402]['item']['local_id']),(9,refs[50305]['item']['local_id'])):stdin.extend(map(str,key))
        result=subprocess.run([str(args.test_executable.resolve()),'--context-catalog'],input='\n'.join(stdin)+'\n',text=True,capture_output=True,timeout=300,env=dict(os.environ,MARIADB_TLS_DISABLE_PEER_VERIFICATION='1'))
        log=(result.stdout+result.stderr).replace(sandbox.password,'[redacted]');(args.work/'source-context-worker.log').write_text(log,encoding='utf-8')
        if result.returncode or 'SOURCE_CONTEXT_WORKER=SUCCESS aids=3 potions=2 effects=4 resets=1 stone_draws=1' not in log:raise RuntimeError('Source context worker failed; see owned work log')
        for n,ref in enumerate(added):
            order=dict(bag=0,ready=1,applied=2,relogged=99 if ref['item']['category_id']==8 else 3,book_ready=4,used=5,expired=6);order.update({'draw'+str(i):6+i for i in range(14)})
            for path in sorted(args.work.glob('source-context-runtime.'+str(n)+'.*.inventory.hex'),key=lambda p:order[p.name.split('.')[-3]]):
                phase=path.name.split('.')[-3];files={kind:Path(str(output)+'.'+str(n)+'.'+phase+'.'+kind+'.hex') for kind in ('inventory','details','service')};files={k:v for k,v in files.items() if v.exists()}
                for kind,wire_path in files.items():
                    wire=bytes.fromhex(wire_path.read_text());assert int.from_bytes(wire[1:5],'little')==len(wire)
                    assert wire[0] in ({'inventory':(225,),'details':(227,),'service':(243,220,244,233)}[kind])
                key=ref['item']['category_id'],ref['item']['local_id'];packets.append(dict(source=ref['source_id'],category=key[0],local=key[1],level=ref['refinement_level'],phase=phase,icon=bases[key]['icon_asset'],**{k:str(v) for k,v in files.items()},hashes={k:file_hash(v) for k,v in files.items()}))
        assert {r['source'] for r in packets}=={r['source_id'] for r in added};sandbox.verify()
    result=stage(args.candidate,args.helper,args.work,args.mariadb_bin,verify)
    evidence=dict(success=True,candidate_sha256=file_hash(args.candidate),catalog_hash=result['catalog_hash'],sources=added,packets=packets,actual_game_item_worker=True,full_actor_world_test=False,installed_to_game=False)
    (args.work/'source-context-worker-result.json').write_text(json.dumps(evidence,indent=2)+'\n',encoding='utf-8');print(json.dumps(dict(success=True,sources=len(added),packets=len(packets),installed_to_game=False)))
if __name__=='__main__':main()
