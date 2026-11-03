import sys,json,os,subprocess,hashlib,base64,secrets,time
from pathlib import Path
ROOT=Path('C:/UnityProjects/AurigaUnity/build/SRC');SUPPORT=ROOT/'SRC/Tools/ServerManager'
sys.path.insert(0,str(SUPPORT));import remote_jobs as jobs,ssh_connection as ssh,ssh_password
sys.stdout.reconfigure(encoding='utf-8',errors='replace')
ssh_password.native_askpass_launcher=lambda:Path('C:/UnityProjects/AurigaUnity/build/ServerManager/AurigaServerManager.exe')
def main():
 assert subprocess.check_output(['git','branch','--show-current'],cwd=ROOT,text=True).strip()=='ECS_+_BGFX'
 profile=next(p for p in json.loads(Path('C:/Users/richa/AppData/Local/Auriga/ServerManager/ssh-profiles.json').read_text())['profiles'] if p['id']=='3a1fe592843f31843721b8fa')
 assert profile['work']['source_dir']=='/Auriga/Source' and profile['remote_root']=='/Auriga/Server'
 session=ssh.Session(profile)
 try:
  def run(command,data=None,stream=False):
   env=dict(os.environ)
   if session.password_broker:env.update(session.password_broker.environment)
   program,args=command
   if not stream:
    p=subprocess.run([program,*args],input=data,capture_output=True,env=env,timeout=180,creationflags=subprocess.CREATE_NO_WINDOW)
    if p.returncode:raise RuntimeError((p.stderr+p.stdout).decode(errors='replace')[-3000:])
    return p.stdout.decode('utf-8',errors='replace')
   p=subprocess.Popen([program,*args],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,env=env,creationflags=subprocess.CREATE_NO_WINDOW)
   p.stdin.write(data);p.stdin.close();tail=''
   for raw in p.stdout:
    line=raw.decode('utf-8',errors='replace');print(line,end='',flush=True);tail=(tail+line)[-12000:]
   if p.wait():raise RuntimeError('Remote job failed: '+tail[-2500:])
   return tail
  def remote(code,stream=False):return run(session.ssh('python3 -B -u -'),code.encode(),stream)
  def rpc(operation,args=None):
   req=dict(schema_version=1,id=secrets.token_hex(12),target=profile['id'],root=profile['remote_root'],operation=operation,arguments=args or {},support={n:(SUPPORT/(n+'.py')).read_text(encoding='utf-8') for n in ('crash_tools','backup_tools','backend','editor_checks')})
   code=(SUPPORT/'server_target_worker.py').read_text(encoding='utf-8')+'\nmain('+repr(base64.b64encode(json.dumps(req).encode()).decode())+')\n'
   output=remote(code);result=json.loads(next(l[len('AURIGA_TARGET_RESULT '):] for l in output.splitlines() if l.startswith('AURIGA_TARGET_RESULT ')))
   if not result['ok']:raise RuntimeError(result.get('error','manager failed'))
   return result['result']
  settings=dict(profile['work']);settings['targets']='GameServer Database LoginServer ServerPackageCheck MountLifecycleTests'
  phase=sys.argv[1]
  if phase=='build':
   print('LIVE_PREFLIGHT',json.dumps(rpc('status')['state'].get('phase')),flush=True)
   rel='SRC/Server/GameServer/pet/mount_inventory_helper.h';local=(ROOT/rel).read_bytes();digest=lambda b:hashlib.sha256(b.replace(b'\r\n',b'\n')).hexdigest()
   baseline=subprocess.check_output(['git','show','HEAD:'+rel],cwd=ROOT)
   result=json.loads(remote('from pathlib import Path\nimport hashlib,json\np=Path("/Auriga/Source/'+rel+'")\nprint(json.dumps(dict(hash=hashlib.sha256(p.read_bytes().replace(b"\\r\\n",b"\\n")).hexdigest())))'))
   if result['hash'] not in (digest(local),digest(baseline)):raise RuntimeError('Remote header independently changed')
   if result['hash']!=digest(local):
    remote('from pathlib import Path\nimport shutil\np=Path("/Auriga/Source/'+rel+'")\nb=Path("/Auriga/Source/.tera-companions-before-mount-helper.h")\nif not b.exists():shutil.copy2(p,b)\n')
    snapshot=jobs.UploadSnapshot([ROOT/rel],'/Auriga/Source')
    try:
     snapshot.request['files'][0]['path']=rel;snapshot.request['directories']=['SRC','SRC/Server','SRC/Server/GameServer','SRC/Server/GameServer/pet']
     req=dict(snapshot.request,overwrite=True);run(session.ssh('python3 -u -'),jobs.worker_input(req));run(session.sftp(snapshot.batch()));print(run(session.ssh('python3 -u -'),jobs.worker_input(dict(req,action='commit'))),flush=True)
    finally:snapshot.close()
   request=jobs.build_request(settings);print('BUILD_ID',request['id'],flush=True)
   output=run(session.ssh('python3 -u -'),jobs.worker_input(request),True)
   if 'AURIGA_BUILD_SUCCESS ' not in output:raise RuntimeError('Build acknowledgement missing')
   remote('import subprocess\nsubprocess.run(["ctest","--test-dir",'+repr(settings['build_dir'])+',"-C",'+repr(settings['configuration'])+',"-R","^mount_lifecycle$","--output-on-failure"],check=True)\n',True)
   print('TERA_SERVER_BUILD_VERIFIED',flush=True)
  elif phase=='status':
   state=rpc('status')['state'];print(json.dumps(dict(phase=state.get('phase'),rows=[{k:row.get(k) for k in ('name','running','pid')} for row in state['rows']]),ensure_ascii=False),flush=True)
  elif phase=='deploy':
   qa=Path('C:/UnityProjects/AurigaUnity/Migration/TeraCompanions/unity-qa.txt')
   if not qa.is_file() or 'PASS all 272 production Addressables acquisitions' not in qa.read_text():raise RuntimeError('Production Unity QA must pass before live publication')
   for report,marker in (('particle-mesh-qa.txt','PASS all 690 source particle meshes'),('play-qa.txt','PASS production SetMount')):
    proof=qa.parent/report
    if not proof.is_file() or marker not in proof.read_text() or 'FAIL ' in proof.read_text():raise RuntimeError('Client publication proof missing or failed: '+report)
   state=rpc('status')['state']
   import re
   channels=sorted({int(re.fullmatch(r'ch(\d+)-core\d+',row['name'])[1]) for row in state['rows'] if row.get('running') and re.fullmatch(r'ch(\d+)-core\d+',row['name'])})
   if not channels:raise RuntimeError('Cannot infer live channels for restart')
   source=Path('work/tera_remote_db_probe.py').read_text(encoding='utf-8').split('out={}')[0]
   validate=source+'''
import shutil,hashlib
stage=r/'run/tera-companions-release'
if not json.loads((stage/'preflight.json').read_text()).get('validated'):raise RuntimeError('SQL preflight missing')
check=stage/'check-data'
if check.exists():raise RuntimeError('A previous staged quest check exists; inspect it before retry')
check.mkdir(mode=0o700)
inventory=subprocess.run([str(r/'bin/ServerPackageCheck'),'--quest-fingerprint',str(r/'data'),str(r/'data/quest')],capture_output=True,text=True,check=True)
for relative in json.loads(inventory.stdout)['files']:
 p=pathlib.Path(relative)
 if p.is_absolute() or '..' in p.parts:raise RuntimeError('Unsafe quest inventory')
 src=r/'data'/p;dst=check/p;dst.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(src,dst)
quest=check/'quest/Quest/system/tera_companions.lua';shutil.copy2(stage/'tera_companions.lua',quest)
with (check/'quest/quest_list').open('a') as f:f.write('\\nQuest/system/tera_companions.lua\\n')
result=subprocess.run([str(r/'bin/ServerPackageCheck'),'--check-libraries',str(check)],capture_output=True,text=True)
if result.returncode:raise RuntimeError('Quest check failed: '+result.stderr[-2000:]+result.stdout[-1000:])
print('TERA_QUEST_STAGING_VALIDATED')
'''
   print(remote(validate),flush=True)
   stopped=False
   try:
    print('STOPPING_LIVE_CHANNELS',channels,flush=True);print(rpc('action',{'argv':['stop']})['output'],flush=True);stopped=True
    for attempt in range(40):
     state=rpc('status')['state']
     if not state['running'] and not state.get('starting'):break
     time.sleep(2)
    else:raise RuntimeError('Live stack did not stop')
    install=jobs.install_request(settings,profile['remote_root']);print('INSTALL_ID',install['id'],flush=True)
    result=run(session.ssh('python3 -u -'),jobs.worker_input(install),True)
    if 'AURIGA_INSTALL_SUCCESS ' not in result:raise RuntimeError('Installation acknowledgement missing')
    print(remote(source+'''
import shutil,hashlib
stage=r/'run/tera-companions-release';ql=r/'data/quest/quest_list'
shutil.copy2(ql,stage/'quest_list-before')
destination=r/'data/quest/Quest/system/tera_companions.lua'
if destination.exists():raise RuntimeError('Quest source unexpectedly already exists')
shutil.copy2(stage/'tera_companions.lua',destination)
text=ql.read_text()
if 'Quest/system/tera_companions.lua' not in text:ql.write_text(text.rstrip()+'\\nQuest/system/tera_companions.lua\\n')
print('TERA_QUEST_SOURCE_INSTALLED')
'''),flush=True)
    print(rpc('action',{'argv':['apply-quests']})['output'],flush=True)
    print(remote(source+'''
import hashlib
stage=r/'run/tera-companions-release';manifest=json.loads((stage/'release.json').read_text());entries=manifest['entries'];sql=(stage/'prototypes.sql').read_text()
if hashlib.sha256(sql.encode()).hexdigest()!=json.loads((stage/'preflight.json').read_text())['sqlSha256']:raise RuntimeError('SQL changed after preflight')
before={}
for table,field in (('item_proto','item'),('mob_proto','mob'),('mob_motion','key')):
 column='folder' if table=='mob_motion' else 'vnum';ids=','.join("'tera_"+e[field]+"'" if table=='mob_motion' else str(e[field]) for e in entries)
 if any(l.strip() and l!=column for l in query('SELECT '+column+' FROM '+table+' WHERE '+column+' IN ('+ids+')').splitlines()):raise RuntimeError('Release IDs already occupied')
 raw=query('SELECT * FROM '+table+' ORDER BY '+column);(stage/(table+'-deploy-before.tsv')).write_text(raw);before[table]=hashlib.sha256(raw.encode()).hexdigest()
query(sql)
audit={}
for table,field in (('item_proto','item'),('mob_proto','mob'),('mob_motion','key')):
 column='folder' if table=='mob_motion' else 'vnum';ids=','.join("'tera_"+e[field]+"'" if table=='mob_motion' else str(e[field]) for e in entries)
 records=query('SELECT * FROM '+table+' WHERE '+column+' IN ('+ids+') ORDER BY '+column)
 if len(records.splitlines())!=273:raise RuntimeError('Published record count mismatch '+table)
 previous=query('SELECT * FROM '+table+' WHERE '+column+' NOT IN ('+ids+') ORDER BY '+column)
 if hashlib.sha256(previous.encode()).hexdigest()!=before[table]:raise RuntimeError('Existing records changed '+table)
 (stage/(table+'-installed.tsv')).write_text(records);audit[table]=272
audit['existingRecordsUnchanged']=True;(stage/'installed.json').write_text(json.dumps(audit))
print('TERA_PROTOTYPES_INSTALLED',json.dumps(audit))
'''),flush=True)
   finally:
    if stopped:
     argv=['start']
     for channel in channels:argv+=['--channel',str(channel)]
     print(rpc('action',{'argv':argv})['output'],flush=True)
   for attempt in range(60):
    state=rpc('status')['state'];required=[row for row in state['rows'] if row['name'] in ('database','login') or (re.fullmatch(r'ch(\d+)-core\d+',row['name']) and int(re.fullmatch(r'ch(\d+)-core\d+',row['name'])[1]) in channels)]
    if required and all(row['running'] for row in required):
     print('TERA_LIVE_DEPLOYMENT_SUCCESS running='+str(len(required))+' channels='+str(channels)+' backup=/Auriga/Server/run/deployments/'+install['id']+'/backup',flush=True);break
    time.sleep(2)
   else:raise RuntimeError('Every required process did not return')
  else:raise RuntimeError('Unknown phase')
 finally:session.close()
if __name__=='__main__':main()
