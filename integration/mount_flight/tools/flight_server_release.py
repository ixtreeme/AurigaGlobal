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
   files=['tests/server/MountLifecycleTests.cpp','SRC/Server/GameServer/network/input_login.cpp','SRC/Server/common/Headers/Header_CG.h','SRC/Server/common/Headers/Header_GC.h','SRC/Server/common/Headers/CG_Packet.h','SRC/Server/common/Headers/GC_Packet.h','SRC/Server/GameServer/network/packet_info.cpp','SRC/Server/GameServer/network/input.h','SRC/Server/GameServer/network/input_main.cpp','SRC/Server/GameServer/entity/movement_input.cpp','SRC/Server/GameServer/ecs/services/EntityNetworkDispatch.cpp','SRC/Server/GameServer/ecs/systems/MountSystem.cpp','SRC/Server/GameServer/ecs/systems/MountFlightSystem.hpp']
   digest=lambda b:hashlib.sha256(b.replace(b'\r\n',b'\n')).hexdigest()
   priorFile=Path('work/flight-upload-sha.json');prior=json.loads(priorFile.read_text()) if priorFile.exists() else {}
   code='from pathlib import Path\nimport hashlib,json\nr=Path("/Auriga/Source")\nfiles='+repr(files)+'\nprint(json.dumps({f:hashlib.sha256((r/f).read_bytes().replace(b"\\r\\n",b"\\n")).hexdigest() if (r/f).exists() else None for f in files}))'
   hashes=json.loads(remote(code));current={}
   for rel in files:
    local=(ROOT/rel).read_bytes();current[rel]=digest(local)
    baseline=subprocess.run(['git','show','HEAD:'+rel],cwd=ROOT,capture_output=True)
    allowed={current[rel],prior.get(rel)}
    if baseline.returncode==0:allowed.add(digest(baseline.stdout))
    if hashes[rel] not in allowed:raise RuntimeError('Independently changed remote source: '+rel)
   remote('from pathlib import Path\nimport shutil\nr=Path("/Auriga/Source");b=r/".mount-flight-before"\nfor f in '+repr(files)+':\n p=r/f;q=b/f\n if p.exists() and not q.exists():q.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,q)\n')
   snapshot=jobs.UploadSnapshot([ROOT/f for f in files],'/Auriga/Source')
   try:
    for row,rel in zip(snapshot.request['files'],files):row['path']=rel
    directories={str(parent).replace('\\','/') for f in files for parent in Path(f).parents if str(parent)!='.'};snapshot.request['directories']=sorted(directories,key=lambda x:(x.count('/'),x))
    req=dict(snapshot.request,overwrite=True);run(session.ssh('python3 -u -'),jobs.worker_input(req));run(session.sftp(snapshot.batch()));print(run(session.ssh('python3 -u -'),jobs.worker_input(dict(req,action='commit'))),flush=True)
    priorFile.write_text(json.dumps(current))
   finally:snapshot.close()
   request=jobs.build_request(settings);print('BUILD_ID',request['id'],flush=True)
   output=run(session.ssh('python3 -u -'),jobs.worker_input(request),True)
   if 'AURIGA_BUILD_SUCCESS ' not in output:raise RuntimeError('Build acknowledgement missing')
   remote('import subprocess\nsubprocess.run(["ctest","--test-dir",'+repr(settings['build_dir'])+',"-C",'+repr(settings['configuration'])+',"-R","^mount_lifecycle$","--output-on-failure"],check=True)\n',True)
   print('MOUNT_FLIGHT_SERVER_BUILD_VERIFIED',flush=True)
  elif phase=='status':
   state=rpc('status')['state'];print(json.dumps(dict(phase=state.get('phase'),rows=[{k:row.get(k) for k in ('name','running','pid')} for row in state['rows']]),ensure_ascii=False),flush=True)
  elif phase=='deploy':
   qa=Path('C:/UnityProjects/AurigaUnity/Migration/MountFlight/client-qa.txt')
   request=qa.parent/'run-qa.request'
   if not qa.exists() or not qa.read_text().startswith('PASS 20 checks') or qa.stat().st_mtime<request.stat().st_mtime:raise RuntimeError('Current client flight QA must pass first')
   if 'MOUNT_FLIGHT_SERVER_BUILD_VERIFIED' not in Path('work/flight-server-build3.log').read_text():raise RuntimeError('Current native flight build/tests must pass first')
   source=Path('work/tera_remote_db_probe.py').read_text().split('out={}')[0]
   print(remote(source+r'''
import hashlib
stage=r/'run/mount-flight-release';proof=json.loads((stage/'preflight.json').read_text());sql=(stage/'capability.sql').read_text()
assert proof['validated'] and hashlib.sha256(sql.encode()).hexdigest()==proof['sqlSha256']
assert hashlib.sha256(query('SELECT * FROM item_proto ORDER BY vnum').encode()).hexdigest()==proof['beforeSha256'],'Prototype table changed since preflight'
print('MOUNT_FLIGHT_LIVE_PREFLIGHT_PASS')
'''),flush=True)
   state=rpc('status')['state'];import re
   channels=sorted({int(re.fullmatch(r'ch(\d+)-core\d+',row['name'])[1]) for row in state['rows'] if row.get('running') and re.fullmatch(r'ch(\d+)-core\d+',row['name'])})
   if not channels:raise RuntimeError('No live channels to restore')
   stopped=False
   try:
    print('STOPPING_LIVE_CHANNELS',channels,flush=True);print(rpc('action',{'argv':['stop']})['output'],flush=True);stopped=True
    for attempt in range(40):
     state=rpc('status')['state']
     if not state['running'] and not state.get('starting'):break
     time.sleep(2)
    else:raise RuntimeError('Live stack did not stop')
    install=jobs.install_request(settings,profile['remote_root']);print('INSTALL_ID',install['id'],flush=True)
    output=run(session.ssh('python3 -u -'),jobs.worker_input(install),True)
    if 'AURIGA_INSTALL_SUCCESS ' not in output:raise RuntimeError('Native install acknowledgement missing')
    print(remote(source+r'''
import hashlib,re
stage=r/'run/mount-flight-release';sql=(stage/'capability.sql').read_text();proof=json.loads((stage/'preflight.json').read_text())
before=query('SELECT * FROM item_proto ORDER BY vnum');assert hashlib.sha256(before.encode()).hexdigest()==proof['beforeSha256']
ids=re.search(r'vnum IN \(([^)]+)\)',sql)[1];rows=query('SELECT vnum,value5 FROM item_proto WHERE vnum IN ('+ids+') ORDER BY vnum').splitlines()[1:];original=[list(map(int,line.split('\t'))) for line in rows];assert len(original)==84
(stage/'item_proto-deploy-before.tsv').write_text(before)
query(sql)
after=query('SELECT vnum,value5 FROM item_proto WHERE vnum IN ('+ids+') ORDER BY vnum');assert len(after.splitlines())==85 and all(int(line.split('\t')[1])==1 for line in after.splitlines()[1:])
fields=[line.split('\t')[0] for line in query('SHOW COLUMNS FROM item_proto').splitlines()[1:]]
restore='CASE vnum '+''.join('WHEN '+str(vnum)+' THEN '+str(value)+' ' for vnum,value in original)+'ELSE value5 END AS value5'
projection=','.join(restore if column=='value5' else '`'+column.replace('`','``')+'`' for column in fields)
assert query('SELECT '+projection+' FROM item_proto ORDER BY vnum')==before,'Unexpected prototype field change'
audit=dict(enabledFlyingMounts=84,otherPrototypeFieldsUnchanged=True,serverProtocol=225)
(stage/'installed.json').write_text(json.dumps(audit));print('MOUNT_FLIGHT_FLAGS_INSTALLED',json.dumps(audit))
'''),flush=True)
   finally:
    if stopped:
     argv=['start']
     for channel in channels:argv+=['--channel',str(channel)]
     print(rpc('action',{'argv':argv})['output'],flush=True)
   for attempt in range(60):
    state=rpc('status')['state'];required=[row for row in state['rows'] if row['name'] in ('database','login') or (re.fullmatch(r'ch(\d+)-core\d+',row['name']) and int(re.fullmatch(r'ch(\d+)-core\d+',row['name'])[1]) in channels)]
    if required and all(row['running'] for row in required):
     result=dict(running=len(required),channels=channels,backup='/Auriga/Server/run/deployments/'+install['id']+'/backup',flyingMounts=84)
     Path('work/flight-deployment-result.json').write_text(json.dumps(result,indent=2));print('MOUNT_FLIGHT_LIVE_DEPLOYMENT_SUCCESS',json.dumps(result),flush=True);break
    time.sleep(2)
   else:raise RuntimeError('Every required server process did not return')
  else:raise RuntimeError('Unknown phase')
 finally:session.close()
if __name__=='__main__':main()
