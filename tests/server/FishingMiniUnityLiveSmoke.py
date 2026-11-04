"""Run C25c-2 real TLS/UI tests in a marked source snapshot, never the user's Editor."""
import argparse,json,socket,subprocess,sys,time
from pathlib import Path
repo=Path(__file__).resolve().parents[2];sys.path.insert(0,str(repo/'SRC/Tools/ServerManager'))
import item_bench
p=argparse.ArgumentParser(description=__doc__)
for name in ('package','unity-project','fixture'):p.add_argument('--'+name,type=Path,required=True)
p.add_argument('--unity-editor',type=Path,default=Path('C:/Program Files/Unity/Hub/Editor/6000.3.27f1/Editor/Unity.exe'))
p.add_argument('--log',type=Path,required=True);p.add_argument('--timeout',type=int,default=600);a=p.parse_args()
item_bench.connection(a.package) # verifies the owned local MariaDB and package
project=a.unity_project.resolve();fixture=a.fixture.resolve();config=json.loads(fixture.read_text(encoding='utf-8'))
marker=project/'validation-source-snapshot.json';assert marker.is_file() and Path(json.loads(marker.read_text())['source']).resolve()!=project
assert config['Host']=='127.0.0.1' and config['Accounts'][0]['Login']=='codexitem'+config['Token']+'0'
assert '6000.3.27f1' in (project/'ProjectSettings/ProjectVersion.txt').read_text()
deadline=time.monotonic()+60
for port in (config['AuthPort'],config['GamePort']):
 while True:
  try:
   with socket.create_connection(('127.0.0.1',int(port)),timeout=2):pass
   break
  except OSError:
   if time.monotonic()>deadline:raise TimeoutError('Owned local endpoints not listening') from None
   time.sleep(.2)
request=project/'Temp/typed-shop-validation-request.json';result=project/'validation-source-snapshot-result.json';temporary=project/'Temp/typed-shop-validation-result.json';status=project/'Temp/typed-shop-validation-status.txt'
assert not request.exists(),'Another validator request is pending'
for path in (result,temporary,status):path.unlink(missing_ok=True)
payload=json.dumps(dict(Mode='fishing-mini-live',ConfigPath=str(fixture)));request.parent.mkdir(exist_ok=True);request.write_text(payload,encoding='utf-8')
startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
process=subprocess.Popen([str(a.unity_editor),'-batchmode','-projectPath',str(project),'-logFile',str(a.log.resolve())],startupinfo=startup)
print('Requested real Unity TLS tool validation',flush=True);deadline=time.monotonic()+a.timeout
try:
 while process.poll() is None:
  if time.monotonic()>deadline:raise TimeoutError('Owned validation editor deadline')
  # Unity may recreate Temp during its first import.
  if not request.exists() and not status.exists() and not result.exists() and not temporary.exists():request.parent.mkdir(exist_ok=True);request.write_text(payload,encoding='utf-8')
  time.sleep(.2)
 if not result.exists():raise RuntimeError('Unity stopped without a result; inspect the selected log')
 record=json.loads(result.read_text(encoding='utf-8'))
 if not record['Success']:raise RuntimeError(record.get('Error','Gathering validation failed'))
 print('FISHING_MINI_UNITY=SUCCESS',flush=True)
finally:
 if process.poll() is None:process.terminate();process.wait(timeout=30)
