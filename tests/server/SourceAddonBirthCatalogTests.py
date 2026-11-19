"""Source paired addon births with an independent Python formula on the real SQL item worker."""
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
    rules={(int(r['category_id']),int(r['local_id']),int(r['refinement_level'])):r for r in rows('item_definition_rules')}
    cases=sorted((r for r in candidate['accepted'] if rules.get((r['item']['category_id'],r['item']['local_id'],r['refinement_level']),{}).get('birth_addon','0')=='1'),key=lambda r:r['source_id'])
    assert len(cases)==1557 and all(source[r['source_id']]['addon_type'] for r in cases)
    bindings={(int(r['category_id']),int(r['local_id'])):r['profile_id'] for r in rows('item_attribute_binding')}
    groups={r['profile_id']:r for r in rows('attribute_group') if r['pool_kind']=='1'}
    profiles={}
    for row in rows('bonus_profile_entry'):profiles.setdefault(row['profile_id'],set()).add(int(row['bonus_id']))
    expected=[]
    import math,hashlib
    for n,ref in enumerate(cases):
        key=ref['item']['category_id'],ref['item']['local_id'];level=ref['refinement_level'];data=bytearray(64);domain=b'auriga.item.addon.v1';data[:len(domain)]=domain;data[24:40]=bytes([0xA3,n&255,n>>8])+bytes(13);data[40:48]=(13000000+n).to_bytes(8,'little');counter=0;draws=[]
        for maximum in (4294967295,4294967295,17,17,20):
            limit=(1<<32)//maximum*maximum
            while True:
                data[48:56]=counter.to_bytes(8,'little');counter+=1;raw=int.from_bytes(hashlib.sha256(data).digest()[:4],'little')
                if raw<limit:draws.append(raw+1 if maximum==4294967295 else raw%maximum);break
        u=(draws[0]-.5)/4294967295;v=(draws[1]-.5)/4294967295;skill=max(-30,min(30,int(math.sqrt(-2*math.log(u))*math.cos(2*math.pi*v)*5+.5)))
        hit=-2*skill+(abs(draws[2]+draws[3]-16)+draws[4]%4+1 if abs(skill)<=20 else draws[4]%5+1)
        excluded=set()
        if groups[bindings[key]]['exclude_static']=='1':
            excluded.update(profiles.get(bases[key]['base_bonus_profile_id'],set()))
            base=bases[key];rl=next(r for r in rows('refinement_level') if r['profile_id']==base['refinement_profile_id'] and int(r['level'])==level)
            excluded.update(profiles.get(rl['extra_bonus_profile_id'],set()))
        expected.append([dict(bonus=bonus,value=value*100) for bonus,value in ((100,hit),(99,skill)) if value and bonus not in excluded])
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
        output=(args.work/'source-addon-birth-runtime').resolve();stdin=[str(sandbox.port),sandbox.password,str(len(catalog['tables']['item_base'])),catalog['hash'],'0',str(output),str(len(cases))]
        for n,ref in enumerate(cases):
            stdin.extend(map(str,(ref['item']['category_id'],ref['item']['local_id'],ref['refinement_level'],len(expected[n]))))
            for bonus in expected[n]:stdin.extend(map(str,(bonus['bonus'],bonus['value'])))
        result=subprocess.run([str(args.test_executable.resolve()),'--addon-birth-catalog'],input='\n'.join(stdin)+'\n',text=True,capture_output=True,timeout=600,env=dict(os.environ,MARIADB_TLS_DISABLE_PEER_VERIFICATION='1'))
        log=(result.stdout+result.stderr).replace(sandbox.password,'[redacted]');(args.work/'source-addon-birth-worker.log').write_text(log,encoding='utf-8')
        if result.returncode or 'SOURCE_ADDON_BIRTH_WORKER=SUCCESS' not in log:raise RuntimeError('Addon birth worker failed; see owned work log')
        order={name:i for i,name in enumerate(('probe','born','relogged','equipped','moved'))}
        for n,ref in enumerate(cases):
            for path in sorted(args.work.glob('source-addon-birth-runtime.'+str(n)+'.*.inventory.hex'),key=lambda p:order[p.name.split('.')[-3]]):
                phase=path.name.split('.')[-3];files={kind:Path(str(output)+'.'+str(n)+'.'+phase+'.'+kind+'.hex') for kind in ('inventory','details')}
                for kind,path in files.items():
                    wire=bytes.fromhex(path.read_text());assert int.from_bytes(wire[1:5],'little')==len(wire) and wire[0]=={'inventory':225,'details':227}[kind]
                key=ref['item']['category_id'],ref['item']['local_id'];packets.append(dict(source=ref['source_id'],category=key[0],local=key[1],level=ref['refinement_level'],expected=expected[n],phase=phase,icon=bases[key]['icon_asset'],**{k:str(v) for k,v in files.items()},hashes={k:file_hash(v) for k,v in files.items()}))
        assert len([r for r in packets if r['phase']=='probe'])==1557
        sandbox.verify()
    result=stage(args.candidate,args.helper,args.work,args.mariadb_bin,verify)
    evidence=dict(success=True,candidate_sha256=file_hash(args.candidate),catalog_hash=result['catalog_hash'],sources=cases,source_states=len(cases),packets=packets,actual_game_item_worker=True,full_actor_world_test=False,installed_to_game=False)
    (args.work/'source-addon-birth-worker-result.json').write_text(json.dumps(evidence,indent=2)+'\n',encoding='utf-8');print(json.dumps(dict(success=True,sources=len(cases),packets=len(packets),installed_to_game=False)))
if __name__=='__main__':main()
