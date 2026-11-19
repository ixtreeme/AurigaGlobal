"""Source finite unique accessory lifetimes on the real SQL item worker."""
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
    assert not added
    source={r['vnum']:r for r in json.loads((ROOT/'build/full-proto-inputs.json').read_text(encoding='utf-8'))['tables']['player.item_proto']}
    packet=candidate['packet']
    def rows(table):return [dict(zip(packet['columns'][table],r)) for r in packet['tables'][table]]
    bases={(int(r['category_id']),int(r['local_id'])):r for r in rows('item_base')}
    appearances={r['profile_id']:r['model_asset'] for r in rows('item_appearance_profile')}
    rules={(int(r['category_id']),int(r['local_id'])):r for r in rows('item_definition_rules')}
    accessories={(int(r['category_id']),int(r['local_id'])):r for r in rows('item_accessory')}
    cases=sorted((r for r in candidate['accepted'] if r['item']['category_id']==3 and rules.get((3,r['item']['local_id']),{}).get('clock_kind','1')!='1'),key=lambda r:r['source_id'])
    assert len(cases)==93 and all(r['refinement_level']==0 for r in cases)
    permanent=next(r for r in candidate['accepted'] if r['item']['category_id']==3 and accessories[(3,r['item']['local_id'])]['equip_slot']=='23' and rules.get((3,r['item']['local_id']),{}).get('clock_kind','1')=='1' and rules.get((3,r['item']['local_id']),{}).get('allow_unequip','1')=='1' and source[r['source_id']]['antiflag']==0)
    for ref in cases:
        raw=source[ref['source_id']];clock=rules[(3,ref['item']['local_id'])]
        authored=[(raw['limittype'+str(i)],raw['limitvalue'+str(i)]) for i in range(2) if raw['limittype'+str(i)] in (7,8,9)]
        if authored:kind,value=authored[0];expected={7:2,8:3,9:4}[kind];duration=(value if value>0 else 36000 if kind==9 else 604800)*1000
        else:expected=2 if raw['value2'] else 4;duration=raw['value0']*(1000 if raw['value2'] else 60000)
        assert int(clock['clock_kind'])==expected and int(clock['duration_ms'])==duration and accessories[(3,ref['item']['local_id'])]['equip_slot']=='23'
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
        output=(args.work/'source-unique-clock-runtime').resolve();stdin=[str(sandbox.port),sandbox.password,str(len(catalog['tables']['item_base'])),catalog['hash'],'0',str(output),str(len(cases))]
        for ref in cases:stdin.extend(map(str,(ref['item']['category_id'],ref['item']['local_id'],ref['refinement_level'],0)))
        stdin.extend(map(str,(permanent['item']['category_id'],permanent['item']['local_id'])))
        result=subprocess.run([str(args.test_executable.resolve()),'--unique-clock-catalog'],input='\n'.join(stdin)+'\n',text=True,capture_output=True,timeout=600,env=dict(os.environ,MARIADB_TLS_DISABLE_PEER_VERIFICATION='1'))
        log=(result.stdout+result.stderr).replace(sandbox.password,'[redacted]');(args.work/'source-unique-clock-worker.log').write_text(log,encoding='utf-8')
        if result.returncode or 'SOURCE_UNIQUE_CLOCK_WORKER=SUCCESS' not in log:raise RuntimeError('Unique clock worker failed; see owned work log')
        order={name:i for i,name in enumerate(('born','relogged','equipped','paused','swapped','locked','wear_debit','expired','expired_relogged'))}
        for n,ref in enumerate(cases):
            rule=rules[(3,ref['item']['local_id'])]
            for path in sorted(args.work.glob('source-unique-clock-runtime.'+str(n)+'.*.inventory.hex'),key=lambda p:order[p.name.split('.')[-3]]):
                phase=path.name.split('.')[-3];files={kind:Path(str(output)+'.'+str(n)+'.'+phase+'.'+kind+'.hex') for kind in ('inventory','details')}
                for kind,path in files.items():
                    wire=bytes.fromhex(path.read_text());assert int.from_bytes(wire[1:5],'little')==len(wire) and wire[0]=={'inventory':225,'details':227}[kind]
                key=3,ref['item']['local_id'];packets.append(dict(source=ref['source_id'],category=3,local=key[1],clock=int(rule['clock_kind']),duration=int(rule['duration_ms']),allow_unequip=rule['allow_unequip']=='1',phase=phase,icon=bases[key]['icon_asset'],**{k:str(v) for k,v in files.items()},hashes={k:file_hash(v) for k,v in files.items()}))
        assert len(packets)==93*7
        sandbox.verify()
    result=stage(args.candidate,args.helper,args.work,args.mariadb_bin,verify)
    evidence=dict(success=True,candidate_sha256=file_hash(args.candidate),catalog_hash=result['catalog_hash'],sources=cases,source_states=len(cases),packets=packets,actual_game_item_worker=True,full_actor_world_test=False,installed_to_game=False)
    (args.work/'source-unique-clock-worker-result.json').write_text(json.dumps(evidence,indent=2)+'\n',encoding='utf-8');print(json.dumps(dict(success=True,sources=len(cases),packets=len(packets),installed_to_game=False)))
if __name__=='__main__':main()
