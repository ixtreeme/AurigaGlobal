"""All original fish admissions and seasonal bundles on actual item worker, clocks and native stats."""
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
    assert {r['source_id'] for r in added}=={50183,50216,61400}
    selected=sorted((r for r in candidate['accepted'] if r.get('semantics') in ('cooked_fish_effect','original_food_bonus_bundle')),key=lambda r:r['source_id']);assert len(selected)==34
    packet=candidate['packet']
    def rows(table):return [dict(zip(packet['columns'][table],r)) for r in packet['tables'][table]]
    bases={(int(r['category_id']),int(r['local_id'])):r for r in rows('item_base')}
    appearances={r['profile_id']:r['model_asset'] for r in rows('item_appearance_profile')}
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
        output=(args.work/'source-multi-effect-runtime').resolve();stdin=[str(sandbox.port),sandbox.password,str(len(catalog['tables']['item_base'])),catalog['hash'],'0',str(output),str(len(selected))]
        for ref in selected:stdin.extend(map(str,(ref['item']['category_id'],ref['item']['local_id'],ref['refinement_level'],0)))
        result=subprocess.run([str(args.test_executable.resolve()),'--multi-effect-catalog'],input='\n'.join(stdin)+'\n',text=True,capture_output=True,timeout=300,env=dict(os.environ,MARIADB_TLS_DISABLE_PEER_VERIFICATION='1'))
        log=(result.stdout+result.stderr).replace(sandbox.password,'[redacted]');(args.work/'source-multi-effect-worker.log').write_text(log,encoding='utf-8')
        if result.returncode or 'SOURCE_MULTI_EFFECT_WORKER=SUCCESS sources=34' not in log:raise RuntimeError('Multi effect worker failed; see owned work log')
        timed={r['profile_id']:r for r in rows('timed_effect_profile')};groups={r['group_id']:r for r in rows('timed_effect_group')};consumables={(int(r['category_id']),int(r['local_id'])):r for r in rows('item_consumable')};by_key={(r['item']['category_id'],r['item']['local_id']):r for r in selected}
        entries=rows('bonus_profile_entry');order={name:i for i,name in enumerate(('ready','active','relogged','denied','elapsed','dead','mixed_ready','mixed_active','human_denied','reverse_denied','food_ready','food_active','food_relogged'))}
        for n in range(len(selected)+3):
            for path in sorted(args.work.glob('source-multi-effect-runtime.'+str(n)+'.*.inventory.hex'),key=lambda p:order[p.name.split('.')[-3]]):
                phase=path.name.split('.')[-3];files={kind:Path(str(output)+'.'+str(n)+'.'+phase+'.'+kind+'.hex') for kind in ('inventory','details','effects')}
                wire=bytes.fromhex(files['effects'].read_text());key=int.from_bytes(wire[56:58],'little'),int.from_bytes(wire[58:62],'little');ref=by_key[key];profile=timed[consumables[key]['timed_effect_profile_id']];group=groups[profile['group_id']]
                for kind,path in files.items():
                    raw=bytes.fromhex(path.read_text());assert raw[0]==dict(inventory=225,details=227,effects=243)[kind] and int.from_bytes(raw[1:5],'little')==len(raw)
                packets.append(dict(source=ref['source_id'],category=key[0],local=key[1],phase=phase,icon=bases[key]['icon_asset'],family=int(group['family_kind']),conflict=int(profile['conflict_bonus_id'] or 0),duration=int(profile['duration_ms']),bonuses=[dict(id=int(r['bonus_id']),value=int(r['value_scaled'])) for r in entries if r['profile_id']==profile['bonus_profile_id']],**{k:str(v) for k,v in files.items()}))
        assert {r['source'] for r in packets}=={r['source_id'] for r in selected};sandbox.verify()
    result=stage(args.candidate,args.helper,args.work,args.mariadb_bin,verify)
    evidence=dict(success=True,candidate_sha256=file_hash(args.candidate),catalog_hash=result['catalog_hash'],sources=selected,new_sources=added,packets=packets,actual_game_item_worker=True,full_actor_world_test=False,installed_to_game=False)
    (args.work/'source-multi-effect-worker-result.json').write_text(json.dumps(evidence,indent=2)+'\n',encoding='utf-8');print(json.dumps(dict(success=True,sources=len(selected),new_sources=len(added),packets=len(packets),installed_to_game=False)))
if __name__=='__main__':main()
