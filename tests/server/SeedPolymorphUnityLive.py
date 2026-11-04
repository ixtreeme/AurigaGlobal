"""Deploy C24 and grant transformation probes on the verified, stopped local test PC."""
import argparse,datetime,hashlib,json,os,shutil,subprocess,sys
from pathlib import Path
repo=Path(__file__).resolve().parents[2];sys.path[:0]=[str(repo/'SRC/Tools/ServerManager'),str(repo/'db/tools')]
import item_bench,item_catalog
from mariadb_client import Client,statements
p=argparse.ArgumentParser(description=__doc__)
for n in ('package','source-fixture','fixture','catalog-tool','bench-tool','game','database','login'):p.add_argument('--'+n,type=Path,required=True)
p.add_argument('--reset-live',action='store_true',help='Reset only the newly created practice-book smoke fixture')
a=p.parse_args();root=a.package.resolve();_,manager,owner,auth=item_bench.connection(root);os.environ['PATH']=str(root/'bin')+os.pathsep+os.environ.get('PATH','')
supervisor=manager.read_json(root/'run/supervisor.json') if (root/'run/supervisor.json').exists() else {};assert not any(manager.owned(c['identity']) for c in supervisor.get('children',[])),'Stop owned stack first'
config=json.loads(a.source_fixture.read_text());account=config['Accounts'][0];pid=int(account['Pid']);token=config['Token'];assert config['Host']=='127.0.0.1' and len(token)==8 and all(c in '0123456789abcdef' for c in token) and account['Login']=='codexitem'+token+'0' and account['Name']=='CI'+token+'0'
db=Client(**auth,library=str(root/'bin/libmariadb.dll'));assert db.query('SELECT account_id,name FROM player.player WHERE id='+str(pid))==[[str(account['AccountId']),account['Name']]]
backup=root/'run/deployments'/datetime.datetime.now(datetime.timezone.utc).strftime('polymorph-%Y%m%dT%H%M%S');backup.mkdir(parents=True)
manifest_path=root/'package-manifest.json';manifest=json.loads(manifest_path.read_text());shutil.copy2(manifest_path,backup/manifest_path.name)
with (backup/'player-protos-before.sql').open('wb') as out:
 r=subprocess.run([str(Path(owner['binary_dir'])/'mariadb-dump.exe'),'--defaults-extra-file='+str(root/'run/mariadb/admin.cnf'),'--protocol=tcp','--host=127.0.0.1','--port='+str(owner['port']),'--single-transaction','--databases','player','protos'],stdout=out,stderr=subprocess.PIPE,creationflags=item_bench.NO_WINDOW,timeout=300);assert r.returncode==0,'SQL backup failed'
has_payload=db.query("SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA='player' AND TABLE_NAME='item_instance' AND COLUMN_NAME='polymorph_practice'")[0][0]=='1'
before=item_bench.run_tool(dict(action='load',owner=pid,connection=auth),a.bench_tool if has_payload else root/'manager/_internal/validators/ItemBenchTool.exe')['state'];(backup/'owner-before.json').write_text(json.dumps(before,indent=2),encoding='utf-8')
hashes=db.query('SELECT instance_id,HEX(state_sha256),state_revision FROM player.item_instance WHERE owner_kind=1 AND owner_id='+str(pid)+' ORDER BY instance_id');(backup/'instance-hashes-before.json').write_text(json.dumps(hashes),encoding='utf-8')
baseline_file=repo/'build/poly-original-baseline.json'
if not baseline_file.exists():
 previous=repo/'build/poly-seed-evidence.json';original=json.loads(previous.read_text())['backup'] if previous.exists() else str(backup)
 baseline_file.write_text(json.dumps(dict(backup=original)),encoding='utf-8')
baseline_backup=Path(json.loads(baseline_file.read_text())['backup']).resolve();assert baseline_backup.is_relative_to((root/'run/deployments').resolve())
preserved=json.loads((baseline_backup/'owner-before.json').read_text());preserved_hashes=json.loads((baseline_backup/'instance-hashes-before.json').read_text())
columns='job,level,exp,skill_point,sub_skill_point,horse_skill_point,HEX(skill_level),typed_book_revision,hp,mp';character=db.query('SELECT '+columns+' FROM player.player WHERE id='+str(pid))[0];(backup/'character-before.json').write_text(json.dumps(dict(columns=columns.split(','),values=character)),encoding='utf-8');config['PolyOriginalRace']=int(character[0]);assert int(character[1])>=50
learning=db.query('SELECT state_revision,HEX(state_blob),HEX(state_sha256),HEX(last_operation) FROM player.item_book_learning WHERE owner_kind=1 AND owner_id='+str(pid));(backup/'learning-before.json').write_text(json.dumps(learning),encoding='utf-8')
recovery=db.query('SELECT state_revision,HEX(state_blob),HEX(state_sha256) FROM player.item_recovery_schedule WHERE owner_kind=1 AND owner_id='+str(pid));(backup/'recovery-before.json').write_text(json.dumps(recovery),encoding='utf-8')
deployment=[('manager/_internal/validators/ItemBenchTool.exe',a.bench_tool),('manager/_internal/validators/CatalogEditorTool.exe',a.catalog_tool),('bin/GameServer.exe',a.game),('bin/Database.exe',a.database),('bin/LoginServer.exe',a.login),('manager/_internal/migrations/008_polymorph.sql',repo/'db/migrations/item_instance/008_polymorph.sql')]
baseline={}
for relative,source in deployment:
 assert source.is_file(),source
 target=root/relative
 if target.exists():baseline[relative]=dict(actual_sha256=hashlib.sha256(target.read_bytes()).hexdigest(),previous_manifest_sha256=manifest['files'].get(relative));shutil.copy2(target,backup/target.name)
(backup/'binary-baseline.json').write_text(json.dumps(baseline,indent=2))
for sql in statements((repo/'db/migrations/item_instance/008_polymorph.sql').read_text(encoding='utf-8')):db.query(sql)
def read():return item_catalog.run_tool(dict(action='read',connection=auth),executable=a.catalog_tool)
packet=read();(backup/'catalog-before.json').write_text(json.dumps(packet,indent=2),encoding='utf-8')
eligible=db.query('SELECT vnum FROM player.mob_proto WHERE type=0 AND vnum BETWEEN 8 AND 65535 AND level BETWEEN 1 AND 49 ORDER BY vnum LIMIT 1');assert eligible,'No local monster fixture available';probe_mob=int(eligible[0][0]);config['PolyMob']=probe_mob
profiles=[('PolyOrb',1,True,2000,0,probe_mob),('PolyBound',1,False,10000,0,probe_mob),('PolyBook',2,True,0,0,probe_mob),('PolyFail',2,True,0,0,probe_mob),('PolyLong',1,True,15000,0,probe_mob),('PolyHigh',1,True,2000,255,probe_mob),('PolyUnknown',1,True,2000,0,4294967295)]
for field,kind,fixed,duration,minimum,mob in profiles:
 editor=item_catalog.Catalog(packet);code='polymorph.local_probe_'+field.lower();old=next((r for r in editor.tables['item_base'] if r['content_code']==code),None);key=(13,int(old['local_id'])) if old else None;d=editor.item(key) if key else editor.new(13)
 form_code='form.local_poly_'+str(mob);form=next((r for r in editor.tables['polymorph_form'] if r['code']==form_code),None)
 d['base'].update(content_code=code,name_key='item.'+code+'.name',description_key='item.'+code+'.description',icon_asset='icon/item/70104.tga',max_stack='20' if kind==1 else '1',min_level=str(minimum));d['child']['fixed_form_id']=form['form_id'] if fixed and form else None
 d.update(new_poly_form=None if form else dict(code=form_code,mob_template_id=str(mob),book_bonus='4'),poly_target_fixed=fixed)
 d['poly']=dict(polymorph_kind=str(kind),mob_level_grace='20' if kind==1 else '0',grace_loss_numerator='3' if kind==1 else '0',grace_loss_denominator='10' if kind==1 else '1',max_book_grade='0' if kind==1 else '3',practice_per_grade='0' if kind==1 else '2',book_chance_base_percent='-10000' if field=='PolyFail' else '100' if kind==2 else '0')
 d['poly_skills']=[dict(skill_level=str(n),duration_ms=str(duration),attack_bonus_bp='2250' if kind==1 else '0',power_bp='3333') for n in range(41)];d['poly_masteries']=[] if kind==1 else [dict(mastery=str(n),duration_ms='8000',bonus_duration_ms='3000') for n in range(4)]
 config[field]=int(d['base']['local_id']);item_catalog.run_tool(dict(action='commit',connection=auth,tables=editor.apply(d,key),expected_hash=packet['hash']),packet['columns'],a.catalog_tool);packet=read()
wolf=next(r for r in item_catalog.Catalog(packet).tables['polymorph_form'] if r['code']=='form.local_poly_'+str(probe_mob));config['PolyForm']=int(wolf['form_id'])
# Correct only a newly seeded bound probe if a prior attempt selected a mob
# that this local database classifies differently. Keep its tombstone/receipt.
current=item_bench.run_tool(dict(action='load',owner=pid,connection=auth),a.bench_tool)['state'];original_ids={i['id'] for i in preserved['items']}
for i in current['items']:
 if i['category']==13 and i['local']==config['PolyBound'] and i['polymorph']['form']!=config['PolyForm']:
  assert i['id'] not in original_ids,'Never replace a preexisting item'
  item_bench.run_tool(dict(action='destroy',owner=pid,item=i['id'],connection=auth),a.bench_tool)
 if a.reset_live and i['category']==13 and i['local']==config['PolyBook']:
  assert i['id'] not in original_ids,'Never reset a preexisting book'
  item_bench.run_tool(dict(action='destroy',owner=pid,item=i['id'],connection=auth),a.bench_tool)
for field,kind,*_ in profiles:
 state=item_bench.run_tool(dict(action='load',owner=pid,connection=auth),a.bench_tool)['state'];count=sum(i['count'] for i in state['items'] if i['category']==13 and i['local']==config[field]);total=5 if kind==1 else 1;assert count<=total
 if count<total:
  request=dict(action='grant',owner=pid,category=13,local=config[field],count=total-count,connection=auth)
  if field=='PolyBound':request['polymorph_form']=config['PolyForm']
  if kind==2:request.update(polymorph_grade=1,polymorph_practice=1)
  item_bench.run_tool(request,a.bench_tool)
editor=item_catalog.Catalog(packet);code='weapon.local_probe_polymorph_weapon';old=next((r for r in editor.tables['item_base'] if r['content_code']==code),None);key=(1,int(old['local_id'])) if old else None;d=editor.item(key) if key else editor.new(1)
d['base'].update(content_code=code,name_key='item.'+code+'.name',description_key='item.'+code+'.description',icon_asset='icon/item/00010.tga',min_level='0');d['child'].update(physical_attack_base_raw='10000',magical_attack_base_raw='0');d['bonuses']=[dict(bonus_id='5',value_scaled='2500'),dict(bonus_id='26',value_scaled='500')];config['PolyWeapon']=int(d['base']['local_id'])
item_catalog.run_tool(dict(action='commit',connection=auth,tables=editor.apply(d,key),expected_hash=packet['hash']),packet['columns'],a.catalog_tool)
state=item_bench.run_tool(dict(action='load',owner=pid,connection=auth),a.bench_tool)['state']
if not any(i['category']==1 and i['local']==config['PolyWeapon'] for i in state['items']):item_bench.run_tool(dict(action='grant',owner=pid,category=1,local=config['PolyWeapon'],count=1,connection=auth),a.bench_tool)
after=item_bench.run_tool(dict(action='load',owner=pid,connection=auth),a.bench_tool)['state'];actual={i['id']:i for i in after['items']}
for old in preserved['items']:old.setdefault('polymorph',None);assert actual.get(old['id'])==old,'Prior item changed'
current_hashes={r[0]:r for r in db.query('SELECT instance_id,HEX(state_sha256),state_revision FROM player.item_instance WHERE owner_kind=1 AND owner_id='+str(pid)+' ORDER BY instance_id')};assert all(current_hashes.get(r[0])==r for r in preserved_hashes),'Prior digest changed'
assert db.query('SELECT '+columns+' FROM player.player WHERE id='+str(pid))[0]==character and db.query('SELECT state_revision,HEX(state_blob),HEX(state_sha256),HEX(last_operation) FROM player.item_book_learning WHERE owner_kind=1 AND owner_id='+str(pid))==learning
for relative,source in deployment:
 target=root/relative;target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source,target);manifest['files'][relative]=hashlib.sha256(target.read_bytes()).hexdigest()
manifest_path.write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8');db.close();a.fixture.write_text(json.dumps(config),encoding='utf-8');(repo/'build/poly-seed-evidence.json').write_text(json.dumps(dict(backup=str(backup),original_backup=str(baseline_backup),previous_items=len(preserved['items']),all_prior_items_and_hashes_exact=True,prior_learning_exact=True,verified_live_mob=probe_mob),indent=2));print('POLYMORPH_LOCAL_SETUP=READY; prior item states preserved:',len(preserved['items']),'; backup:',backup)
