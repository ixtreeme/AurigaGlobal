"""Deploy C22 Game/Database together and provision books on the verified test PC."""
import argparse,datetime,hashlib,json,shutil,sys
from pathlib import Path
repo=Path(__file__).resolve().parents[2];sys.path[:0]=[str(repo/'SRC/Tools/ServerManager'),str(repo/'db/tools')]
import item_bench,item_catalog
from mariadb_client import Client
p=argparse.ArgumentParser(description=__doc__)
for n in ('package','source-fixture','fixture','catalog-tool','bench-tool','game','database','login'):p.add_argument('--'+n,type=Path,required=True)
a=p.parse_args();root=a.package.resolve();_,manager,_,auth=item_bench.connection(root)
supervisor=manager.read_json(root/'run/supervisor.json') if (root/'run/supervisor.json').exists() else {};assert not any(manager.owned(c['identity']) for c in supervisor.get('children',[])),'Stop owned stack first'
config=json.loads(a.source_fixture.read_text());token=config['Token'];account=config['Accounts'][0];pid=int(account['Pid']);assert config['Host']=='127.0.0.1' and len(token)==8 and all(c in '0123456789abcdef' for c in token) and account['Login']=='codexitem'+token+'0' and account['Name']=='CI'+token+'0'
db=Client(**auth,library=str(root/'bin/libmariadb.dll'));assert db.query('SELECT account_id,name FROM player.player WHERE id='+str(pid))==[[str(account['AccountId']),account['Name']]]
backup=root/'run/deployments'/datetime.datetime.now(datetime.timezone.utc).strftime('books-%Y%m%dT%H%M%S');backup.mkdir(parents=True)
manifest=json.loads((root/'package-manifest.json').read_text());shutil.copy2(root/'package-manifest.json',backup/'package-manifest.json')
deployment=[('manager/_internal/validators/ItemBenchTool.exe',a.bench_tool),('bin/GameServer.exe',a.game),('bin/LoginServer.exe',a.login),('bin/Database.exe',a.database),('manager/_internal/migrations/007_book_learning.sql',repo/'db/migrations/item_instance/007_book_learning.sql')]
baseline={}
for relative,source in deployment:
 assert source.is_file(),source
 target=root/relative
 if target.exists():
  actual=hashlib.sha256(target.read_bytes()).hexdigest();baseline[relative]=dict(actual_sha256=actual,previous_manifest_sha256=manifest['files'].get(relative));shutil.copy2(target,backup/target.name)
(backup/'binary-baseline.json').write_text(json.dumps(baseline,indent=2))
for relative,source in deployment:
 target=root/relative
 target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source,target);manifest['files'][relative]=hashlib.sha256(target.read_bytes()).hexdigest()
(root/'package-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8');item_bench.helper=lambda:a.bench_tool;item_bench.install(root)
columns='job,level,exp,skill_group,skill_point,sub_skill_point,horse_skill_point,HEX(skill_level),typed_book_revision';row=db.query('SELECT '+columns+' FROM player.player WHERE id='+str(pid))[0];(backup/'character-before.json').write_text(json.dumps(dict(columns=columns.split(','),values=row)),encoding='utf-8')
job=int(row[0])%4;group=int(row[3]) or 1;master=1+30*job+15*(group-1);empire=int(db.query('SELECT empire FROM player.player_index WHERE id='+str(account['AccountId']))[0][0]);config.update(BookMainSkill=121,BookOtherSkill=122,BookMasterSkill=master)
def read():return item_catalog.run_tool(dict(action='read',connection=auth),executable=a.catalog_tool)
packet=read();(backup/'catalog-before.json').write_text(json.dumps(packet,indent=2),encoding='utf-8')
# field, operation, target mode, skill, required, chance, XP, delay, min level,
# forbidden empire. The mastery book uses its own seeded real class skill.
profiles=[('BookLearn',1,1,121,2,10000,100,2000,0,0),('BookAlternative',1,1,121,2,10000,100,2000,0,0),('BookFail',1,1,122,1,0,0,1200,0,0),('BookBound',1,2,121,1,10000,0,0,0,0),('BookForget',2,1,121,1,10000,0,0,0,0),('BookHorse',3,3,0,1,10000,0,1000,50,0),('BookMaster',1,1,master,1,10000,0,0,0,0),('BookHigh',1,1,121,1,10000,0,0,255,0),('BookNative',1,1,126+empire-1,1,10000,0,0,0,empire),('BookLong',1,1,121,2,10000,100,10000,0,0)]
for field,operation,mode,skill,required,chance,xp,delay,minimum,forbidden in profiles:
 editor=item_catalog.Catalog(packet);code='skill_book.local_probe_'+field.lower();old=next((r for r in editor.tables['item_base'] if r['content_code']==code),None);key=(9,int(old['local_id'])) if old else None;d=editor.item(key) if key else editor.new(9)
 d['base'].update(content_code=code,name_key='item.'+code+'.name',description_key='item.'+code+'.description',icon_asset='icon/item/50300.tga',max_stack='20',min_level=str(minimum));d['book']=dict(operation_kind=str(operation),target_mode=str(mode),units_per_use='1',allowed_masteries='0' if operation==3 else '2' if field=='BookMaster' else '1',allow_delay_bypass='0',bonus_policy='1')
 d['book_targets']=[] if operation==3 else [dict(skill_id=str(target),refund_pool='2' if operation==2 else '0',forbidden_empire=str(forbidden)) for target in ([121,122] if mode==2 else [skill])]
 levels=range(20,25) if field=='BookMaster' else range(1,6) if operation==2 else range(0,6)
 d['book_steps']=[dict(current_level=str(level),next_level=str(level-1 if operation==2 else level+1),min_character_level=str(minimum),required_successes=str(required),success_chance_bp=str(chance),boosted_chance_bp=str(chance),experience_cost=str(xp),delay_minimum_ms=str(delay),delay_maximum_ms=str(delay),delay_on_failure='1' if delay else '0') for level in levels]
 config[field]=int(d['base']['local_id']);item_catalog.run_tool(dict(action='commit',connection=auth,tables=editor.apply(d,key),expected_hash=packet['hash']),packet['columns'],a.catalog_tool);packet=read()
(backup/'catalog-after.json').write_text(json.dumps(packet,indent=2),encoding='utf-8');before=item_bench.run_tool(dict(action='setup',owner=pid,connection=auth),a.bench_tool);(backup/'owner-before.json').write_text(json.dumps(before,indent=2),encoding='utf-8')
for field,*_ in profiles:
 state=item_bench.run_tool(dict(action='load',owner=pid,connection=auth),a.bench_tool);items=state.get('items',state.get('state',{}).get('items',[]));count=sum(i['count'] for i in items if i['category']==9 and i['local']==config[field]);request=dict(action='grant',owner=pid,category=9,local=config[field],count=5-count,connection=auth)
 if field=='BookBound':request['book_skill']=121
 if count<5:item_bench.run_tool(request,a.bench_tool)
# Reset ONLY the dedicated PC's learning fixture, with the stack stopped. All
# prior item states and owner receipts are preserved, and backed up above.
skills=bytearray.fromhex(row[7] or '00'*2550);skills=skills[:2550]+bytearray(max(0,2550-len(skills)))
for skill in (121,122,126+empire-1):skills[skill*10:skill*10+10]=bytes(10)
skills[master*10:master*10+10]=bytes([1,20])+bytes(8)
db.query('START TRANSACTION');db.query('DELETE FROM player.item_book_learning WHERE owner_kind=1 AND owner_id='+str(pid));db.query("UPDATE player.player SET typed_book_revision=0,exp=1000,level=GREATEST(level,50),skill_group="+str(group)+",horse_skill_point=0,skill_level=UNHEX('"+skills.hex()+"') WHERE id="+str(pid));db.query('COMMIT');db.close()
a.fixture.write_text(json.dumps(config),encoding='utf-8');print('BOOKS_LOCAL_SETUP=READY; prior item inventory preserved; backup:',backup)
