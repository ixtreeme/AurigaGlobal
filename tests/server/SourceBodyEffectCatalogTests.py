"""Actual converted body effects through an owned SQL DB and the game item worker."""
import argparse,json,os,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'SRC/Tools/ItemCatalogBuilder'));sys.path.insert(0,str(ROOT/'db/tools'))
from stage_exported_catalog import stage
from mariadb_client import statements
from legacy_item_migration import file_hash

def main():
    p=argparse.ArgumentParser();p.add_argument('--candidate',type=Path,required=True);p.add_argument('--helper',type=Path,required=True)
    p.add_argument('--test-executable',type=Path,required=True);p.add_argument('--work',type=Path,required=True);p.add_argument('--mariadb-bin',type=Path,required=True);args=p.parse_args()
    document=json.loads(args.candidate.read_text(encoding='utf-8'));refs={r['source_id']:r for r in document['accepted']}
    source=(78002,78006,88212);keys=[refs[n]['item'] for n in source];armor=refs[11200]['item'];assert all(k['category_id']==4 for k in keys) and armor['category_id']==2
    tables=document['packet'];costumes={(r['category_id'],r['local_id']):r for r in [dict(zip(tables['columns']['item_costume'],row)) for row in tables['tables']['item_costume']]}
    bases={(r['category_id'],r['local_id']):r for r in [dict(zip(tables['columns']['item_base'],row)) for row in tables['tables']['item_base']]}
    effects=[];mapping=json.loads(Path('D:/Auriga-Unity6/SRC/AurigaUnity-20261103/Assets/Metin2/Resources/Metin2CostumeEffectCatalog.json').read_text(encoding='utf-8-sig'));palette={e['id']:e for e in mapping['entries']}
    for ref in document['accepted']:
        key=ref['item'];row=costumes.get((str(key['category_id']),str(key['local_id'])))
        if row and row['costume_kind']=='6':
            identity=int(row['body_effect_id']);entry=palette[identity];effects.append(dict(source=ref['source_id'],id=identity,path=entry['path'],bone=entry['bone'],icon=bases[(str(key['category_id']),str(key['local_id']))]['icon_asset']))
    packets=[]
    def verify(sandbox,candidate):
        db=sandbox.client;db.query('CREATE DATABASE player CHARACTER SET utf8mb4 COLLATE utf8mb4_bin')
        db.query('CREATE TABLE player.player(id INT UNSIGNED PRIMARY KEY) ENGINE=InnoDB');db.query('CREATE TABLE player.player_deleted LIKE player.player')
        migrations=sorted((ROOT/'db/migrations/item_instance').glob('[0-9][0-9][0-9]_*.sql'));assert [int(p.name[:3]) for p in migrations]==list(range(1,45))
        for path in migrations:
            if path.name.startswith('028_'):
                db.query('INSERT INTO player.item_owner_fence VALUES(1,7050,1,7)');db.query('INSERT INTO player.item_storage VALUES(1,7050,2,1,26,1,1)')
            for sql in statements(path.read_text(encoding='utf-8-sig')):db.query(sql)
        assert db.query('SELECT grid_columns,state_revision FROM player.item_storage JOIN player.item_owner_fence USING(owner_kind,owner_id) WHERE owner_kind=1 AND owner_id=7050')==[['28','9']]
        db.query("INSERT INTO player.item_persistence_format(singleton,schema_version,catalog_revision,catalog_sha256) VALUES(1,1,1,UNHEX('"+candidate['hash']+"'))")
        db.query("CREATE USER 'game_reader'@'localhost' IDENTIFIED BY '"+sandbox.password+"'")
        db.query("GRANT SELECT ON protos.* TO 'game_reader'@'localhost'");db.query("GRANT SELECT ON player.* TO 'game_reader'@'localhost'")
        for table in ('item_owner_fence','item_wallet','item_operation_receipt','item_instance','item_instance_attribute','item_instance_socket','item_occupied_cell','item_ground_core','item_ground_entry'):
            db.query("GRANT INSERT,UPDATE,DELETE ON player."+table+" TO 'game_reader'@'localhost'")
        db.query("GRANT UPDATE ON player.item_instance_id_allocator TO 'game_reader'@'localhost'");db.query("GRANT INSERT ON player.item_mission_event TO 'game_reader'@'localhost'")
        output=(args.work/'source-body-effect-runtime').resolve()
        stdin='\n'.join([str(sandbox.port),sandbox.password,str(len(candidate['tables']['item_base'])),candidate['hash'],'0',str(output)]+[str(k['local_id']) for k in keys]+[str(armor['local_id'])])+'\n'
        result=subprocess.run([str(args.test_executable.resolve()),'--body-effect-catalog'],input=stdin,text=True,capture_output=True,timeout=120,env=dict(os.environ,MARIADB_TLS_DISABLE_PEER_VERIFICATION='1'))
        log=(result.stdout+result.stderr).replace(sandbox.password,'[redacted]');(args.work/'source-body-effect-worker.log').write_text(log,encoding='utf-8')
        if result.returncode or 'SOURCE_BODY_EFFECT_WORKER=SUCCESS' not in log:raise RuntimeError('Source effect worker failed; see owned work log')
        for owner in (7041,7042):
            assert db.query('SELECT destroyed,stack_count,storage_kind,LEFT(HEX(costume_lifetime_blob),12) FROM player.item_instance WHERE category_id=4 AND owner_kind=1 AND owner_id='+str(owner))==[['1','0',None,'43544C010101']]
        for n,key in enumerate(keys):
            identity=int(costumes[(str(key['category_id']),str(key['local_id']))]['body_effect_id'])
            for phase in ('bag','unarmored','worn','armor_off','unequipped','reequipped')+(() if n==0 else ('expired',)):
                paths={kind:Path(str(output)+'.'+str(n)+'.'+phase+'.'+kind+'.hex') for kind in ('inventory','details','appearance')}
                for kind,path in paths.items():
                    wire=bytes.fromhex(path.read_text(encoding='ascii'));assert wire[0]=={'inventory':225,'details':227,'appearance':241}[kind] and int.from_bytes(wire[1:5],'little')==len(wire)
                packets.append(dict(variant=n,source=source[n],phase=phase,expected_effect=identity if phase in ('worn','reequipped') else 0,**{kind:str(path) for kind,path in paths.items()},hashes={kind:file_hash(path) for kind,path in paths.items()}))
        sandbox.verify()
    result=stage(args.candidate,args.helper,args.work,args.mariadb_bin,verify)
    evidence=dict(success=True,validation_kind='body_effects',candidate_sha256=file_hash(args.candidate),catalog_hash=result['catalog_hash'],native_keys=keys,packets=packets,effects=effects,realms_modified=False,actual_game_item_worker=True,full_actor_world_test=False)
    (args.work/'source-body-effect-worker-result.json').write_text(json.dumps(evidence,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(dict(success=True,catalog_hash=result['catalog_hash'],variants=3,packets=len(packets),installed_to_game=False)))
if __name__=='__main__':main()
