"""Recovered wearable and rare-service prototypes on the real worker, in owned SQL."""
import argparse,json,os,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];sys.path[:0]=[str(ROOT/'SRC/Tools/ItemCatalogBuilder'),str(ROOT/'db/tools')]
from stage_exported_catalog import stage
from mariadb_client import statements
from legacy_item_migration import file_hash
from convert_legacy_equipment import effects
from legacy_equipment_rules import bonus

def main():
    parser=argparse.ArgumentParser()
    for name in ('candidate','baseline','helper','test-executable','work','mariadb-bin'):parser.add_argument('--'+name,required=True,type=Path)
    args=parser.parse_args();candidate=json.loads(args.candidate.read_text(encoding='utf-8'));old=json.loads(args.baseline.read_text(encoding='utf-8'))
    known={x['source_id'] for x in old['accepted']};added=sorted((x for x in candidate['accepted'] if x['source_id'] not in known),key=lambda x:x['source_id'])
    assert sum(x['item']['category_id']==4 for x in added)>=100 and sum(x['item']['category_id']==5 for x in added)==4
    source={x['vnum']:x for x in json.loads((ROOT/'build/full-proto-inputs.json').read_text(encoding='utf-8'))['tables']['player.item_proto']}
    packet=candidate['packet']
    def rows(table):return [dict(zip(packet['columns'][table],x)) for x in packet['tables'][table]]
    bases={(int(x['category_id']),int(x['local_id'])):x for x in rows('item_base')};profiles={x['profile_id']:x for x in rows('attribute_service_profile')}
    attrs={x['profile_id'] for x in rows('attribute_candidate') if x['pool_kind']=='3'} & {x['profile_id'] for x in rows('attribute_candidate') if x['pool_kind']=='2'}
    use={x['policy_id']:x for x in rows('item_use_policy')}
    target_keys={(int(x['category_id']),int(x['local_id'])) for x in rows('item_attribute_binding') if x['category_id']=='1' and x['profile_id'] in attrs and int(use[bases[1,int(x['local_id'])]['use_policy_id']]['allowed_classes'])&1 and int(use[bases[1,int(x['local_id'])]['use_policy_id']]['allowed_sexes'])&1}
    target=next(x for x in candidate['accepted'] if (x['item']['category_id'],x['item']['local_id']) in target_keys and x['refinement_level']==0)
    target_key=target['item']['category_id'],target['item']['local_id']
    source_keys={(x['item']['category_id'],x['item']['local_id']) for x in candidate['accepted']}
    normal=next((5,int(x['local_id'])) for x in rows('item_consumable') if (5,int(x['local_id'])) in source_keys and x['attribute_service_profile_id'] and (p:=profiles[x['attribute_service_profile_id']])['pool_kind']=='1' and p['operation_kind']=='1' and int(p['target_mask'])&1 and p['minimum_count']=='0' and p['success_bp']=='10000' and p['units_per_use']=='1' and p['maximum_target_level']=='0')
    refs={x['source_id']:x for x in candidate['accepted']};tools=[(5,refs[n]['item']['local_id']) for n in (30617,30618,30619,30620)]+[normal]
    packets=[]
    def verify(sandbox,catalog):
        db=sandbox.client;db.query('CREATE DATABASE player CHARACTER SET utf8mb4 COLLATE utf8mb4_bin');db.query('CREATE TABLE player.player(id INT UNSIGNED PRIMARY KEY) ENGINE=InnoDB');db.query('CREATE TABLE player.player_deleted LIKE player.player')
        paths=sorted((ROOT/'db/migrations/item_instance').glob('[0-9][0-9][0-9]_*.sql'));assert [int(f.name[:3]) for f in paths]==list(range(1,45))
        for path in paths:
            for sql in statements(path.read_text(encoding='utf-8-sig')):db.query(sql)
        db.query("INSERT INTO player.item_persistence_format(singleton,schema_version,catalog_revision,catalog_sha256) VALUES(1,1,1,UNHEX('"+catalog['hash']+"'))");db.query("CREATE USER 'game_reader'@'localhost' IDENTIFIED BY '"+sandbox.password+"'");db.query("GRANT SELECT ON protos.* TO 'game_reader'@'localhost'");db.query("GRANT SELECT ON player.* TO 'game_reader'@'localhost'")
        for table in ('item_owner_fence','item_wallet','item_operation_receipt','item_instance','item_instance_attribute','item_instance_socket','item_occupied_cell','item_ground_core','item_ground_entry','item_recovery_schedule'):
            db.query("GRANT INSERT,UPDATE,DELETE ON player."+table+" TO 'game_reader'@'localhost'")
        db.query("GRANT UPDATE ON player.item_instance_id_allocator TO 'game_reader'@'localhost'");db.query("GRANT INSERT ON player.item_mission_event TO 'game_reader'@'localhost'")
        output=(args.work/'source-wearable-runtime').resolve();stdin=[str(sandbox.port),sandbox.password,str(len(catalog['tables']['item_base'])),catalog['hash'],'0',str(output),str(len(added))]
        for ref in added:
            key=ref['item'];bonuses={}
            if key['category_id'] in (2,4):
                bonuses=effects(source[ref['source_id']])
                for i in range(2):
                    extra=ref.get('source_extra') or {}
                    if extra.get('bonus_type'+str(i)):
                        b=bonus(extra['bonus_type'+str(i)],extra['bonus_value'+str(i)])
                        if b:bonuses[int(b['bonus_id'])]+=int(b['value_scaled'])
            stdin += [str(key['category_id']),str(key['local_id']),str(ref['refinement_level']),str(len(bonuses))]+[str(v) for pair in sorted(bonuses.items()) for v in pair]
        stdin += [str(target_key[0]),str(target_key[1]),str(target['refinement_level'])]+[str(v) for pair in tools for v in pair]
        result=subprocess.run([str(args.test_executable.resolve()),'--wearables-catalog'],input='\n'.join(stdin)+'\n',text=True,capture_output=True,timeout=300,env=dict(os.environ,MARIADB_TLS_DISABLE_PEER_VERIFICATION='1'))
        log=(result.stdout+result.stderr).replace(sandbox.password,'[redacted]');(args.work/'source-wearable-worker.log').write_text(log,encoding='utf-8')
        if result.returncode or 'SOURCE_WEARABLE_WORKER=SUCCESS' not in log:raise RuntimeError('Source wearable worker failed; see owned work log')
        for n,ref in enumerate(added):
            key=ref['item'];base=bases[key['category_id'],key['local_id']]
            visual=[dict(characterClass=int(x['character_class']),sex=int(x['sex']),address=x['model_asset']) for x in rows('item_costume_visual') if (int(x['category_id']),int(x['local_id']))==(key['category_id'],key['local_id'])]
            for path in sorted(args.work.glob('source-wearable-runtime.'+str(n)+'.*.inventory.hex')):
                phase=path.name.split('.')[-3];files={kind:Path(str(output)+'.'+str(n)+'.'+phase+'.'+kind+'.hex') for kind in ('inventory','details','service','appearance')}
                files={k:v for k,v in files.items() if v.exists()}
                for kind,wire_path in files.items():
                    wire=bytes.fromhex(wire_path.read_text());assert int.from_bytes(wire[1:5],'little')==len(wire) and (kind=='service' or wire[0]=={'inventory':225,'details':227,'appearance':241}[kind])
                packets.append(dict(source=ref['source_id'],category=key['category_id'],local=key['local_id'],phase=phase,level=ref['refinement_level'],icon=base['icon_asset'],visuals=visual,**{k:str(v) for k,v in files.items()},hashes={k:file_hash(v) for k,v in files.items()}))
        assert set(x['source'] for x in packets)=={x['source_id'] for x in added}
        sandbox.verify()
    result=stage(args.candidate,args.helper,args.work,args.mariadb_bin,verify)
    evidence=dict(success=True,candidate_sha256=file_hash(args.candidate),catalog_hash=result['catalog_hash'],sources=added,packets=packets,realms_modified=False,actual_game_item_worker=True,full_actor_world_test=False)
    (args.work/'source-wearable-worker-result.json').write_text(json.dumps(evidence,indent=2)+'\n',encoding='utf-8');print(json.dumps(dict(success=True,sources=len(added),packets=len(packets),installed_to_game=False)))
if __name__=='__main__':main()
