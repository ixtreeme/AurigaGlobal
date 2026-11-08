"""Owned localhost multi-stage Mission/Lua fixture; preserves existing owners."""
from pathlib import Path
from datetime import datetime,timezone
from collections import Counter
import argparse,hashlib,json,os,secrets,subprocess,sys
REPO=Path(__file__).resolve().parents[2]
sys.path[:0]=[str(REPO/'SRC/Tools/ServerManager'),str(REPO/'db/tools')]
import backend,item_bench,item_catalog
from mariadb_client import Client
from PetPassiveUnityLiveFixture import protected,read,write,literal

def other_missions(db,pids=()):
 where='' if not pids else ' WHERE character_id NOT IN ('+','.join(map(str,pids))+')'
 return db.query('SELECT character_id,owner_epoch,HEX(owner_token),storage_revision,runtime_revision,checkpoint_time,HEX(checkpoint_digest) FROM mission.mission_engine_characters'+where+' ORDER BY character_id')

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('action',choices=('seed','verify','restore'))
 for name in ('package','fixture'):p.add_argument('--'+name,type=Path,required=True)
 for name in ('template','log'):p.add_argument('--'+name,type=Path)
 a=p.parse_args();assert a.fixture.resolve().is_relative_to(REPO/'build');root,manager,owned,auth=item_bench.connection(a.package.resolve());db=Client(**auth,library=str(root/'bin/libmariadb.dll'));os.environ['PATH']=str(root/'bin')+os.pathsep+os.environ['PATH']
 try:
  if a.action=='restore':
   c=read(a.fixture);backup=Path(c['Backup'])
   with manager.supervisor_lock():
    assert not backend.describe(root)['running'];manifest=read(root/'package-manifest.json')
    for entry in c['InstalledFiles']:
     target=root/entry['path'];assert target.resolve().is_relative_to(root) and hashlib.sha256(target.read_bytes()).hexdigest()==entry['installed']
     if entry['original'] is None:target.unlink();manifest['files'].pop(entry['path'],None)
     else:target.write_bytes((backup/entry['original']).read_bytes());manifest['files'][entry['path']]=hashlib.sha256(target.read_bytes()).hexdigest()
    # The quest publisher retains every persisted quest/state identity. Keep a
    # handler-free tombstone instead of removing an already published quest.
    quest='Quest/system/codex_typed_'+c['Token']+'.lua';target=root/'data/quest'/quest
    retired=('quest codex_typed_'+c['Token']+' begin state start begin end end\n').encode()
    target.write_bytes(retired);manifest['files']['data/quest/'+quest]=hashlib.sha256(retired).hexdigest()
    listing=root/'data/quest/quest_list';listing.write_bytes(listing.read_bytes()+b'\n'+quest.encode()+b'\n');manifest['files']['data/quest/quest_list']=hashlib.sha256(listing.read_bytes()).hexdigest()
    write(root/'package-manifest.json',manifest)
   subprocess.run([sys.executable,str(root/'manage.py'),'apply-quests'],cwd=root,stdout=subprocess.PIPE,stderr=subprocess.PIPE,creationflags=item_bench.NO_WINDOW,check=True)
   print('TYPED_MISSION_FIXTURE_RESTORED');return
  if a.action=='verify':
   c=read(a.fixture);backup=Path(c['Backup']);pids=[int(v['Pid']) for v in c['Accounts']];assert protected(db,pids)==read(backup/'typed-before.json');
   if (backup/'mission-before.json').exists():assert other_missions(db,pids)==read(backup/'mission-before.json')
   assert 'METIN2_TYPED_MISSION_LIVE=SUCCESS' in a.log.read_text(encoding='utf-8',errors='replace')
   tool=root/'manager/_internal/validators/ItemBenchTool.exe';states=[item_bench.run_tool(dict(action='load',owner=pid,connection=auth),tool)['state'] for pid in pids]
   count=lambda state,local:sum(i['count'] for i in state['items'] if i['category']==6 and i['local']==local and not i.get('destroyed',False))
   assert count(states[0],c['MissionMaterialLocal'])==6 and count(states[1],c['MissionMaterialLocal'])==2
   assert count(states[0],c['MissionRewardLocal'])==4 and states[0]['wallets'][0]['balance']==1080
   pid=pids[0];assert db.query(f'SELECT COUNT(*) FROM mission.mission_engine_reward_ledger l JOIN mission.mission_engine_action_outbox o ON o.action_id=l.action_id WHERE o.character_id={pid}')==[['3']]
   assert db.query(f'SELECT event_kind,units,delivered FROM player.item_mission_event WHERE character_id={pid} ORDER BY event_id')==[['1','1','1'],['1','2','1']]
   for pid in pids:assert db.query(f'SELECT COUNT(*) FROM player.item WHERE owner_id={pid}')==[['0']]
   write(backup/'owners-after.json',states);print('TYPED_MISSION_LOCAL_SQL=SUCCESS protected_items='+str(len(read(backup/'typed-before.json')['items']))+' reward_actions=3 durable_collections=2');return
  assert a.template and not a.fixture.exists();template=read(a.template)
  with manager.supervisor_lock():
   assert not backend.describe(root)['running'];backup=root/'run/deployments'/datetime.now(timezone.utc).strftime('c34-fixture-%Y%m%dT%H%M%S%f');backup.mkdir();write(backup/'typed-before.json',protected(db));write(backup/'mission-before.json',other_missions(db))
   with (backup/'player-protos-mission-before.sql').open('wb') as out:
    result=subprocess.run([str(Path(owned['binary_dir'])/'mariadb-dump.exe'),'--defaults-extra-file='+str(root/'run/mariadb/admin.cnf'),'--protocol=tcp','--host=127.0.0.1','--port='+str(owned['port']),'--single-transaction','--databases','player','protos','mission'],stdout=out,stderr=subprocess.PIPE,creationflags=item_bench.NO_WINDOW,timeout=300);assert result.returncode==0 and out.tell()>0
   token=secrets.token_hex(4);c={k:template[k] for k in ('Host','ServerName','Pin','Version','AuthPort','GamePort','PetLocal','MountLocal','PetRace','MountRace')};c.update(Token=token,Accounts=[],Mission=True,Backup=str(backup),MissionNpcRace=9003,InstalledFiles=[])
   catalog_tool=root/'manager/_internal/validators/CatalogEditorTool.exe';bench=root/'manager/_internal/validators/ItemBenchTool.exe';initial=item_catalog.run_tool(dict(action='read',connection=auth),executable=catalog_tool);editor=item_catalog.Catalog(initial);published=initial
   for label in ('MissionMaterialLocal','MissionRewardLocal'):
    d=editor.new(6);local=int(d['base']['local_id']);code='mission.c34_'+token+'_'+label.lower();d['base'].update(content_code=code,name_key='item.'+code+'.name',description_key='item.'+code+'.description');tables=editor.apply(d);editor.packet['sequences']['6']=str(local+1);item_catalog.run_tool(dict(action='commit',connection=auth,tables=tables,expected_hash=published['hash']),initial['columns'],catalog_tool);published=item_catalog.run_tool(dict(action='read',connection=auth),executable=catalog_tool);c[label]=local
   for name,rows in initial['tables'].items():assert not (Counter(map(tuple,rows))-Counter(map(tuple,published['tables'][name]))),name
   columns=[row[0] for row in db.query('SHOW COLUMNS FROM player.player') if row[0]!='id' and 'GENERATED' not in row[5]]
   for index in range(2):
    login='codexcompanion'+token+str(index);password=secrets.token_hex(8);name='MQ'+token+str(index);db.query('INSERT INTO account.account(login,password,social_id,status,create_time) VALUES('+literal(login)+',PASSWORD('+literal(password)+"),'1234567','OK',NOW())");aid=int(db.query('SELECT LAST_INSERT_ID()')[0][0]);values=dict(account_id=str(aid),name=name,job='0',level='60',st='10',ht='10',dx='10',iq='10',random_hp='30000',random_sp='10000',skill_group='1',map_index='41',x=str(986600+index*100),y='269100',z='0',gold='0',hp='30000',mp='10000',typed_book_revision='0',horse_level='0',horse_riding='0');projection=['REPEAT(CHAR(0),OCTET_LENGTH(`'+k+'`))' if k in ('skill_level','quickslot') else literal(values[k]) if k in values else '`'+k+'`' for k in columns];db.query('INSERT INTO player.player('+','.join('`'+k+'`' for k in columns)+') SELECT '+','.join(projection)+f" FROM player.player WHERE id={int(template['Accounts'][0]['Pid'])}");pid=int(db.query('SELECT LAST_INSERT_ID()')[0][0]);db.query(f'INSERT INTO player.player_index(id,pid1,empire) VALUES({aid},{pid},1)');c['Accounts'].append(dict(Login=login,Password=password,Name=name,Pid=pid,AccountId=aid))
    def grant(action,**fields):return item_bench.run_tool(dict(action=action,owner=pid,connection=auth,**fields),bench)
    grant('setup',gold=1000 if index==0 else 0)
    for local in (c['PetLocal'],c['MountLocal']):grant('grant',category=15,local=local,count=1)
    material=grant('grant',category=6,local=c['MissionMaterialLocal'],count=10 if index==0 else 5)['granted'];c['MissionOwnerMaterialId' if index==0 else 'MissionSenderMaterialId']=material
   c['MissionStory']='c34.'+token+'.story';c['MissionLua']='c34.'+token+'.lua';c['MissionChoice']='c34.'+token+'.choice'
   def node(id,kind,**fields):return dict(id=id,kind=kind,**fields)
   def amount(local,count):return dict(category=6,local=local,level=0,count=count)
   def reward(count,cost=0,gold=0,gold_cost=0,requirements=0):return dict(schema=2,typed_items=[amount(c['MissionRewardLocal'],count)],typed_consume_items=[amount(c['MissionMaterialLocal'],cost)] if cost else [],typed_requirements=[amount(c['MissionMaterialLocal'],requirements)] if requirements else [],gold=gold,gold_cost=gold_cost)
   story=dict(schema=2,mission=c['MissionStory'],version=1,root='root',nodes=[node('root','sequence',children=['collect','talk','reward']),node('collect','objective',objective=dict(event='item_collected',subject=f"item.6.{c['MissionMaterialLocal']}.0",goal=3)),node('talk','objective',objective=dict(event='npc_interacted',subject='npc.9003',goal=1)),node('reward','reward',reward='bundle')],rewards=dict(bundle=reward(2,4,100,25,10)))
   lua=dict(schema=2,mission=c['MissionLua'],version=1,root='reward',nodes=[node('reward','reward',reward='bundle')],rewards=dict(bundle=reward(1,3)))
   choice=dict(schema=2,mission=c['MissionChoice'],version=1,root='root',choices={'c34.road':['forest','river']},nodes=[node('root','sequence',children=['choose','timer','branch']),node('choose','objective',objective=dict(event='dialogue_choice',subject='c34.road',goal=1)),node('timer','timer',duration=2000),node('branch','branch',branches=[dict(when=dict(op='choice_is',key='c34.road',value='forest'),child='forest')],fallback='river'),node('forest','reward',reward='forest_reward'),node('river','reward',reward='river_reward')],rewards=dict(forest_reward=reward(1,gold=5),river_reward=reward(9,gold=99)))
   source=dict(schema=1,package='c34.'+token,version=1,default_locale='hu',missions=[story,lua,choice],presentation=dict(locales={'hu':dict(title_story='Tartós tárgygyűjtés',title_lua='Lua típusos csere',title_choice='Elágazás és várakozás',summary='Helyi integrációs küldetés',collect='Gyűjts három anyagot',talk='Beszélj az NPC-vel',reward='Típusos jutalom',choose='Válassz útvonalat',timer='Várakozás')},missions={}))
   for code,label,nodes in [(c['MissionStory'],'title_story',['collect','talk','reward']),(c['MissionLua'],'title_lua',['reward']),(c['MissionChoice'],'title_choice',['choose','timer'])]:source['presentation']['missions'][code]=dict(title=label,summary='summary',objectives=[dict(node=n,title=n) for n in nodes])
   write(backup/'mission-source.json',source);compiler=REPO/'build/c26b-native/SRC/Server/GameServer/MissionCompiler.exe';result=subprocess.run([str(compiler),'compile',str(backup/'mission-source.json'),str(backup/'typed_missions.aurmission')],capture_output=True,text=True,creationflags=item_bench.NO_WINDOW);assert result.returncode==0,result.stdout+result.stderr
   manifest=read(root/'package-manifest.json')
   def install(relative,data):
    target=root/relative;assert target.resolve().is_relative_to(root);original=None
    if target.exists():assert hashlib.sha256(target.read_bytes()).hexdigest()==manifest['files'][relative];original=str(len(c['InstalledFiles']))+'.before';(backup/original).write_bytes(target.read_bytes())
    target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(data);digest=hashlib.sha256(data).hexdigest();manifest['files'][relative]=digest;c['InstalledFiles'].append(dict(path=relative,original=original,installed=digest))
   install('data/typed_missions.aurmission',(backup/'typed_missions.aurmission').read_bytes());install('data/typed_mission_runtime.json',(json.dumps(dict(version=1,package='typed_missions.aurmission',missions=[dict(code=code,version=1,npc_race=9003,min_level=1) for code in (c['MissionStory'],c['MissionLua'],c['MissionChoice'])]),indent=2)+'\n').encode())
   quest=f'''quest codex_typed_{token} begin
 state start begin
  when 9003.chat."C34 typed quest" with pc.get_name() == "{c['Accounts'][0]['Name']}" begin
   local state = pc.typed_mission_status("{c['MissionLua']}")
   if state == 0 then
    local ok, status = pc.typed_mission_action("{c['MissionLua']}", "start")
    say("C34_LUA_START " .. tostring(ok) .. " " .. status)
   elseif state == 2 then
    local before = pc.typed_item_count(6, {c['MissionMaterialLocal']}, 0)
    local ok, status = pc.typed_mission_action("{c['MissionLua']}", "claim")
    local after = pc.typed_item_count(6, {c['MissionMaterialLocal']}, 0)
    say("C34_LUA_CLAIM " .. tostring(ok) .. " " .. status .. " " .. tostring(before) .. ":" .. tostring(after))
   elseif state == 3 then
    say("C34_LUA_ALREADY " .. tostring(pc.typed_item_count(6, {c['MissionMaterialLocal']}, 0)))
   else say("C34_LUA_UNAVAILABLE") end
  end
 end
end
'''
   relative='Quest/system/codex_typed_'+token+'.lua';install('data/quest/'+relative,quest.encode('utf-8'));old=(root/'data/quest/quest_list').read_bytes();install('data/quest/quest_list',old+b'\n'+relative.encode()+b'\n')
   mapfile='data/map/metin2_map_main_razor93/regen.json';world=read(root/mapfile);world['regens'].append(dict(type='mob',x=652,y=643,range_x=0,range_y=0,z=0,dir=0,time=3600,percent=100,count=1,vnum=9003));install(mapfile,(json.dumps(world,indent=2)+'\n').encode());write(root/'package-manifest.json',manifest);write(a.fixture,c);assert protected(db,[v['Pid'] for v in c['Accounts']])==read(backup/'typed-before.json');print('TYPED_MISSION_FIXTURE=READY missions=3 characters=2 protected_items='+str(len(read(backup/'typed-before.json')['items'])))
  subprocess.run([sys.executable,str(root/'manage.py'),'apply-quests'],cwd=root,stdout=subprocess.PIPE,stderr=subprocess.PIPE,creationflags=item_bench.NO_WINDOW,check=True);
 finally:db.close()
if __name__=='__main__':main()
