"""Deploy C23 and add probes to the verified local PC, preserving existing items and learning."""
import argparse,datetime,hashlib,json,os,shutil,subprocess,sys
from pathlib import Path
repo=Path(__file__).resolve().parents[2];sys.path[:0]=[str(repo/'SRC/Tools/ServerManager'),str(repo/'db/tools')]
import item_bench,item_catalog
from mariadb_client import Client,statements
p=argparse.ArgumentParser(description=__doc__)
for n in ('package','source-fixture','fixture','catalog-tool','bench-tool','game','database','login'):p.add_argument('--'+n,type=Path,required=True)
a=p.parse_args();root=a.package.resolve();_,manager,owner,auth=item_bench.connection(root)
os.environ['PATH']=str(root/'bin')+os.pathsep+os.environ.get('PATH','')
supervisor=manager.read_json(root/'run/supervisor.json') if (root/'run/supervisor.json').exists() else {};assert not any(manager.owned(c['identity']) for c in supervisor.get('children',[])),'Stop owned stack first'
config=json.loads(a.source_fixture.read_text());token=config['Token'];account=config['Accounts'][0];pid=int(account['Pid']);assert config['Host']=='127.0.0.1' and len(token)==8 and all(c in '0123456789abcdef' for c in token) and account['Login']=='codexitem'+token+'0' and account['Name']=='CI'+token+'0'
db=Client(**auth,library=str(root/'bin/libmariadb.dll'));assert db.query('SELECT account_id,name FROM player.player WHERE id='+str(pid))==[[str(account['AccountId']),account['Name']]]
backup=root/'run/deployments'/datetime.datetime.now(datetime.timezone.utc).strftime('book-aids-%Y%m%dT%H%M%S');backup.mkdir(parents=True)
manifest=json.loads((root/'package-manifest.json').read_text());shutil.copy2(root/'package-manifest.json',backup/'package-manifest.json')
# A complete SQL backup precedes the catalog DDL; credentials stay in the owned cnf.
with (backup/'protos-before.sql').open('wb') as out:
 r=subprocess.run([str(Path(owner['binary_dir'])/'mariadb-dump.exe'),'--defaults-extra-file='+str(root/'run/mariadb/admin.cnf'),'--protocol=tcp','--host=127.0.0.1','--port='+str(owner['port']),'--single-transaction','--databases','protos'],stdout=out,stderr=subprocess.PIPE,creationflags=item_bench.NO_WINDOW,timeout=300)
 assert r.returncode==0,'Catalog backup failed'
version=db.query('SELECT schema_version FROM protos.catalog_format WHERE singleton=1')[0][0];assert version in ('25','26')
oldbench=root/'manager/_internal/validators/ItemBenchTool.exe' if version=='25' else a.bench_tool
def load(tool):return item_bench.run_tool(dict(action='load',owner=pid,connection=auth),tool)['state']
before=load(oldbench);(backup/'owner-before.json').write_text(json.dumps(before,indent=2),encoding='utf-8')
columns='job,level,exp,skill_group,skill_point,sub_skill_point,horse_skill_point,HEX(skill_level),typed_book_revision,hp,mp';row=db.query('SELECT '+columns+' FROM player.player WHERE id='+str(pid))[0];(backup/'character-before.json').write_text(json.dumps(dict(columns=columns.split(','),values=row)),encoding='utf-8')
learning=db.query('SELECT owner_kind,owner_id,state_revision,HEX(state_blob),HEX(state_sha256),HEX(last_operation) FROM player.item_book_learning WHERE owner_kind=1 AND owner_id='+str(pid));(backup/'learning-before.json').write_text(json.dumps(learning),encoding='utf-8')
assert learning and int(row[8])==int(learning[0][2]),'Learning checkpoint mismatch'
blob=bytes.fromhex(learning[0][3]);assert blob[:4]==b'BOK1' and int.from_bytes(blob[2572:2576],'little')==0 and int.from_bytes(blob[2576:2580],'little')==0,'Fixture needs empty aid balances'
skills=bytes.fromhex(row[7]);assert len(skills)==2550 and skills[122*10+1]==0,'Dedicated C23 skill must start at zero'
config.update(AidTarget=122,AidExperience=int(row[2]))
deployment=[('manager/_internal/validators/ItemBenchTool.exe',a.bench_tool),('manager/_internal/validators/CatalogEditorTool.exe',a.catalog_tool),('bin/GameServer.exe',a.game),('bin/LoginServer.exe',a.login),('bin/Database.exe',a.database)]
baseline={}
for relative,source in deployment:
 assert source.is_file(),source
 target=root/relative;assert target.exists(),target
 baseline[relative]=dict(actual_sha256=hashlib.sha256(target.read_bytes()).hexdigest(),previous_manifest_sha256=manifest['files'].get(relative));shutil.copy2(target,backup/target.name)
(backup/'binary-baseline.json').write_text(json.dumps(baseline,indent=2))
version=db.query('SELECT schema_version FROM protos.catalog_format WHERE singleton=1')[0][0];assert version in ('25','26')
if version=='25':
 for sql in statements((repo/'db/migrations/item_combat/032_book_aids.sql').read_text(encoding='utf-8')):db.query(sql)
def read():return item_catalog.run_tool(dict(action='read',connection=auth),executable=a.catalog_tool)
packet=read();(backup/'catalog-before-probes.json').write_text(json.dumps(packet,indent=2),encoding='utf-8')
for field,kind,charges,maximum,minimum in [('AidChance',1,1,1,0),('AidBypass',2,1,1,0),('AidPack',1,2,3,0),('AidHigh',1,1,1,255)]:
 editor=item_catalog.Catalog(packet);code='consumable.local_probe_'+field.lower();old=next((r for r in editor.tables['item_base'] if r['content_code']==code),None);key=(5,int(old['local_id'])) if old else None;d=editor.item(key) if key else editor.new(5)
 d.update(book_aid=dict(aid_kind=str(kind),units_per_use='1',charges_per_use=str(charges),maximum_charges=str(maximum)),recovery=None,effect=None,timed=None,toggle=None,socket_service=None,attribute_service=None,cooldown=None)
 d['base'].update(content_code=code,name_key='item.'+code+'.name',description_key='item.'+code+'.description',icon_asset='icon/item/71001.tga',max_stack='20',min_level=str(minimum));config[field]=int(d['base']['local_id'])
 item_catalog.run_tool(dict(action='commit',connection=auth,tables=editor.apply(d,key),expected_hash=packet['hash']),packet['columns'],a.catalog_tool);packet=read()
editor=item_catalog.Catalog(packet);code='skill_book.local_probe_aidbook';old=next((r for r in editor.tables['item_base'] if r['content_code']==code),None);key=(9,int(old['local_id'])) if old else None;d=editor.item(key) if key else editor.new(9)
d['base'].update(content_code=code,name_key='item.'+code+'.name',description_key='item.'+code+'.description',icon_asset='icon/item/50300.tga',max_stack='20',min_level='0');d['book']=dict(operation_kind='1',target_mode='1',units_per_use='1',allowed_masteries='1',allow_delay_bypass='1',bonus_policy='2');d['book_targets']=[dict(skill_id='122',refund_pool='0',forbidden_empire='0')];d['book_steps']=[dict(current_level=str(level),next_level=str(level+1),min_character_level='0',required_successes='1',success_chance_bp='0',boosted_chance_bp='10000',experience_cost='25',delay_minimum_ms='600000',delay_maximum_ms='600000',delay_on_failure='1') for level in (0,1)];config['AidBook']=int(d['base']['local_id'])
item_catalog.run_tool(dict(action='commit',connection=auth,tables=editor.apply(d,key),expected_hash=packet['hash']),packet['columns'],a.catalog_tool)
for field,category,total in [('AidChance',5,5),('AidBypass',5,5),('AidPack',5,1),('AidHigh',5,1),('AidBook',9,5)]:
 state=load(a.bench_tool);count=sum(i['count'] for i in state['items'] if i['category']==category and i['local']==config[field]);assert count<=total
 if count<total:item_bench.run_tool(dict(action='grant',owner=pid,category=category,local=config[field],count=total-count,connection=auth),a.bench_tool)
after=load(a.bench_tool);actual={i['id']:i for i in after['items']};assert all(actual.get(i['id'])==i for i in before['items']),'Prior item changed'
assert db.query('SELECT '+columns+' FROM player.player WHERE id='+str(pid))[0]==row and db.query('SELECT owner_kind,owner_id,state_revision,HEX(state_blob),HEX(state_sha256),HEX(last_operation) FROM player.item_book_learning WHERE owner_kind=1 AND owner_id='+str(pid))==learning,'Prior learning changed'
for relative,source in deployment:
 target=root/relative;shutil.copy2(source,target);manifest['files'][relative]=hashlib.sha256(target.read_bytes()).hexdigest()
(root/'package-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8');db.close();a.fixture.write_text(json.dumps(config),encoding='utf-8');(repo/'build/aids-seed-evidence.json').write_text(json.dumps(dict(backup=str(backup),previous_items=len(before['items']),all_previous_items_exact=True,previous_learning_exact=True),indent=2));print('BOOK_AIDS_LOCAL_SETUP=READY; existing item states:',len(before['items']),'; backup:',backup)
