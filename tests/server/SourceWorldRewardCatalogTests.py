"""Source monster/group summon scrolls through SQL receipts and the shared post-write dispatcher."""
import argparse,json,os,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path[:0]=[str(ROOT/'SRC/Tools/ItemCatalogBuilder'),str(ROOT/'db/tools')]
from stage_exported_catalog import stage
from mariadb_client import statements
from legacy_item_migration import file_hash

def main():
    parser=argparse.ArgumentParser()
    for name in ('candidate','baseline','helper','test-executable','work','mariadb-bin','world-groups'):parser.add_argument('--'+name,required=True,type=Path)
    args=parser.parse_args();candidate=json.loads(args.candidate.read_text(encoding='utf8'));old=json.loads(args.baseline.read_text(encoding='utf8'));assert candidate['summary']['converted_source_rows']==old['summary']['converted_source_rows']
    pending={r['source_ids'][0] for r in candidate['world_rewards_runtime']['deferred']};assert pending=={71261,71262,71268,71273}
    p=candidate['packet'];rows=lambda t:[dict(zip(p['columns'][t],r)) for r in p['tables'][t]];bases={(int(r['category_id']),int(r['local_id'])):r for r in rows('item_base')};loots={}
    for r in rows('loot_entry'):loots.setdefault(r['profile_id'],[]).append(r)
    refs={(r['item']['category_id'],r['item']['local_id']):r for r in candidate['accepted']}
    cases=[]
    groups={r['vnum']:r for r in json.loads(args.world_groups.read_text(encoding='utf8'))['groups']}
    for box in rows('item_container'):
        if box['container_kind'] not in ('1','2'):continue
        rewards=loots[box['loot_profile_id']]
        if not any(r['reward_kind'] in ('4','5') for r in rewards):continue
        assert box['container_kind']=='1' and len(rewards)==1 and rewards[0]['reward_kind'] in ('4','5')
        reward=rewards[0];key=int(box['category_id']),int(box['local_id']);ref=refs[key];members=[]
        if reward['reward_kind']=='5':
            definition=groups[int(reward['amount'])];members=[definition['leader']['vnum']]+[m['vnum'] for m in definition.get('members',[])];assert members and all(int(n)>0 for n in members)
        cases.append(dict(ref,kind=int(reward['reward_kind']),definition=int(reward['amount']),group_members=members,deferred=ref['source_id'] in pending))
    cases.sort(key=lambda r:r['source_id']);assert len(cases)==17 and sum(r['kind']==5 for r in cases)==16
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
        output=(args.work/'source-world-reward-runtime').resolve();stdin=[str(sandbox.port),sandbox.password,str(len(catalog['tables']['item_base'])),catalog['hash'],'0',str(output),str(len(cases))]
        for r in cases:stdin.extend(map(str,(r['item']['category_id'],r['item']['local_id'],r['refinement_level'],0)))
        for r in cases:stdin.extend(map(str,(r['kind'],r['definition'])))
        result=subprocess.run([str(args.test_executable.resolve()),'--world-reward-catalog'],input='\n'.join(stdin)+'\n',text=True,capture_output=True,timeout=300,env=dict(os.environ,MARIADB_TLS_DISABLE_PEER_VERIFICATION='1'))
        log=(result.stdout+result.stderr).replace(sandbox.password,'[redacted]');(args.work/'source-world-reward-worker.log').write_text(log,encoding='utf8')
        if result.returncode or 'SOURCE_WORLD_REWARD_WORKER=SUCCESS' not in log:raise RuntimeError('Source world reward worker failed; see owned work log')
        for n,r in enumerate(cases):
            if r['deferred']:continue
            for phase in ('ready','opened','relogged'):
                files={kind:Path(str(output)+f'.{n}.{phase}.{kind}.hex') for kind in ('inventory','details','container')}
                for kind,path in files.items():
                    wire=bytes.fromhex(path.read_text());assert wire[0]=={'inventory':225,'details':227,'container':233}[kind] and int.from_bytes(wire[1:5],'little')==len(wire)
                key=r['item']['category_id'],r['item']['local_id'];packets.append(dict(source=r['source_id'],category=key[0],local=key[1],phase=phase,kind=r['kind'],definition=r['definition'],icon=bases[key]['icon_asset'],**{k:str(v) for k,v in files.items()}))
        sandbox.verify()
    result=stage(args.candidate,args.helper,args.work,args.mariadb_bin,verify);evidence=dict(success=True,candidate_sha256=file_hash(args.candidate),catalog_hash=result['catalog_hash'],sources=cases,packets=packets,groups_sha256=file_hash(args.world_groups),actual_game_item_worker=True,actual_shared_post_write_dispatcher=True,full_actor_world_test=False,installed_to_game=False)
    (args.work/'source-world-reward-worker-result.json').write_text(json.dumps(evidence,indent=2)+'\n',encoding='utf8');print(json.dumps(dict(success=True,sources=len(cases),packets=len(packets),installed_to_game=False)))
if __name__=='__main__':main()
