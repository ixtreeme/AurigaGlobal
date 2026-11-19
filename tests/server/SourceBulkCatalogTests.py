"""Three conversion groups together on the real item worker and owned SQL."""
import argparse,json,os,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];sys.path[:0]=[str(ROOT/'SRC/Tools/ItemCatalogBuilder'),str(ROOT/'db/tools')]
from stage_exported_catalog import stage
from mariadb_client import statements
from legacy_item_migration import file_hash
from convert_legacy_equipment import effects
from legacy_equipment_rules import bonus

def main():
 p=argparse.ArgumentParser()
 for name in ('candidate','baseline','helper','test-executable','work','mariadb-bin'):p.add_argument('--'+name,required=True,type=Path)
 args=p.parse_args();candidate=json.loads(args.candidate.read_text());old=json.loads(args.baseline.read_text());known={r['source_id'] for r in old['accepted']};added=sorted((r for r in candidate['accepted'] if r['source_id'] not in known),key=lambda r:r['source_id'])
 assert len(added)==161
 source={r['vnum']:r for r in json.loads((ROOT/'build/full-proto-inputs.json').read_text())['tables']['player.item_proto']};packet=candidate['packet'];base={tuple(map(int,r[:2])):dict(zip(packet['columns']['item_base'],r)) for r in packet['tables']['item_base']};packets=[]
 def verify(sandbox,catalog):
  db=sandbox.client;db.query('CREATE DATABASE player CHARACTER SET utf8mb4 COLLATE utf8mb4_bin');db.query('CREATE TABLE player.player(id INT UNSIGNED PRIMARY KEY) ENGINE=InnoDB');db.query('CREATE TABLE player.player_deleted LIKE player.player')
  paths=sorted((ROOT/'db/migrations/item_instance').glob('[0-9][0-9][0-9]_*.sql'));assert [int(f.name[:3]) for f in paths]==list(range(1,45))
  for path in paths:
   for sql in statements(path.read_text(encoding='utf-8-sig')):db.query(sql)
  db.query("INSERT INTO player.item_persistence_format(singleton,schema_version,catalog_revision,catalog_sha256) VALUES(1,1,1,UNHEX('"+catalog['hash']+"'))");db.query("CREATE USER 'game_reader'@'localhost' IDENTIFIED BY '"+sandbox.password+"'");db.query("GRANT SELECT ON protos.* TO 'game_reader'@'localhost'");db.query("GRANT SELECT ON player.* TO 'game_reader'@'localhost'")
  for table in ('item_owner_fence','item_wallet','item_operation_receipt','item_instance','item_instance_attribute','item_instance_socket','item_occupied_cell','item_ground_core','item_ground_entry','item_recovery_schedule'):
   db.query("GRANT INSERT,UPDATE,DELETE ON player."+table+" TO 'game_reader'@'localhost'")
  db.query("GRANT UPDATE ON player.item_instance_id_allocator TO 'game_reader'@'localhost'");db.query("GRANT INSERT ON player.item_mission_event TO 'game_reader'@'localhost'")
  output=(args.work/'source-bulk-runtime').resolve();stdin=[str(sandbox.port),sandbox.password,str(len(catalog['tables']['item_base'])),catalog['hash'],'0',str(output),str(len(added))]
  for ref in added:
   key=ref['item'];row=source[ref['source_id']];bonuses={}
   if key['category_id']==3:
    bonuses=effects(row)
    for i in range(2):
     extra=ref.get('source_extra') or {}
     if extra.get('bonus_type'+str(i)):
      b=bonus(extra['bonus_type'+str(i)],extra['bonus_value'+str(i)])
      if b:bonuses[int(b['bonus_id'])]+=int(b['value_scaled'])
   stdin += [str(key['category_id']),str(key['local_id']),str(ref['refinement_level']),str(len(bonuses))]+[str(v) for pair in sorted(bonuses.items()) for v in pair]
  result=subprocess.run([str(args.test_executable.resolve()),'--bulk-catalog'],input='\n'.join(stdin)+'\n',text=True,capture_output=True,timeout=240,env=dict(os.environ,MARIADB_TLS_DISABLE_PEER_VERIFICATION='1'))
  log=(result.stdout+result.stderr).replace(sandbox.password,'[redacted]');(args.work/'source-bulk-worker.log').write_text(log,encoding='utf-8')
  if result.returncode or 'SOURCE_BULK_WORKER=SUCCESS gear=126 passive=16 permanent=19' not in log:raise RuntimeError('Bulk source worker failed; see owned work log')
  for n,ref in enumerate(added):
   category=ref['item']['category_id'];key=ref['item'];item=base[key['category_id'],key['local_id']]
   for path in sorted(args.work.glob('source-bulk-runtime.'+str(n)+'.*.inventory.hex')):
    phase=path.name.split('.')[-3];paths={kind:Path(str(output)+'.'+str(n)+'.'+phase+'.'+kind+'.hex') for kind in ('inventory','details')+(('runtime',) if category==5 else ())}
    for kind,wire_path in paths.items():
     wire=bytes.fromhex(wire_path.read_text());assert wire[0]=={'inventory':225,'details':227,'runtime':243}[kind] and int.from_bytes(wire[1:5],'little')==len(wire)
    packets.append(dict(source=ref['source_id'],category=category,phase=phase,level=ref['refinement_level'],icon=item['icon_asset'],**{k:str(v) for k,v in paths.items()},hashes={k:file_hash(v) for k,v in paths.items()}))
  assert len(packets)==650
  sandbox.verify()
 result=stage(args.candidate,args.helper,args.work,args.mariadb_bin,verify)
 evidence=dict(success=True,candidate_sha256=file_hash(args.candidate),catalog_hash=result['catalog_hash'],sources=added,packets=packets,realms_modified=False,actual_game_item_worker=True,full_actor_world_test=False)
 (args.work/'source-bulk-worker-result.json').write_text(json.dumps(evidence,indent=2)+'\n');print(json.dumps(dict(success=True,source_rows=len(added),packets=len(packets),installed_to_game=False)))
if __name__=='__main__':main()
