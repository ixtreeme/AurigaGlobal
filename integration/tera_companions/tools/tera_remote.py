import sys,json,os,subprocess
from pathlib import Path
SUPPORT=Path('C:/UnityProjects/AurigaUnity/build/SRC/SRC/Tools/ServerManager')
sys.path.insert(0,str(SUPPORT))
import ssh_connection,ssh_password
ssh_password.native_askpass_launcher=lambda:Path('C:/UnityProjects/AurigaUnity/build/ServerManager/AurigaServerManager.exe')
def main():
 profiles=json.loads(Path('C:/Users/richa/AppData/Local/Auriga/ServerManager/ssh-profiles.json').read_text())['profiles']
 profile=next(p for p in profiles if p['id']=='3a1fe592843f31843721b8fa')
 with_session=ssh_connection.Session(profile)
 try:
  env=dict(os.environ)
  if with_session.password_broker:env.update(with_session.password_broker.environment)
  program,args=with_session.ssh('python3 -B -u -')
  source=Path(sys.argv[1]).read_bytes()
  result=subprocess.run([program,*args],input=source,stdout=subprocess.PIPE,stderr=subprocess.PIPE,env=env,timeout=180,creationflags=subprocess.CREATE_NO_WINDOW)
  if len(sys.argv)>2:Path(sys.argv[2]).write_bytes(result.stdout)
  else:sys.stdout.buffer.write(result.stdout)
  if result.returncode:sys.stderr.buffer.write(result.stderr[-4000:]);raise SystemExit(result.returncode)
 finally:with_session.close()
if __name__=='__main__':main()
