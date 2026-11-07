"""Run real pet-passive combat in an owned Unity 6.3 validation snapshot."""
from pathlib import Path
import argparse, json, socket, subprocess, sys, time

repo = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--package', type=Path, required=True)
parser.add_argument('--unity-project', type=Path, required=True)
parser.add_argument('--fixture', type=Path, required=True)
parser.add_argument('--log', type=Path, required=True)
parser.add_argument('--timeout', type=int, default=2400)
args = parser.parse_args()
project = args.unity_project.resolve()
marker = json.loads((project/'validation-source-snapshot.json').read_text(encoding='utf-8'))
assert Path(marker['source']).resolve() != project, 'Use the isolated owned validation snapshot'
assert 'm_EditorVersion: 6000.3.27f1' in (project/'ProjectSettings/ProjectVersion.txt').read_text()
editor = Path('C:/Program Files/Unity/Hub/Editor/6000.3.27f1/Editor/Unity.exe')
assert editor.is_file()
config = json.loads(args.fixture.read_text(encoding='utf-8'))
assert config['Host']=='127.0.0.1' and len(config['Accounts'])==5 and len(config['Cases'])==18
assert all(a['Login']=='codexpcombat'+config['Token']+str(i) for i,a in enumerate(config['Accounts']))
sys.path.insert(0,str(repo/'SRC/Tools/ServerManager'))
import item_bench
item_bench.connection(args.package.resolve())
deadline = time.monotonic()+60
for port in (config['AuthPort'], config['GamePort']):
    while True:
        try:
            with socket.create_connection(('127.0.0.1',int(port)),timeout=2): break
        except OSError:
            if time.monotonic()>deadline: raise TimeoutError('Owned local endpoints not listening') from None
            time.sleep(.2)
request = project/'Temp/typed-shop-validation-request.json'
result = project/'validation-source-snapshot-result.json'
temporary = project/'Temp/typed-shop-validation-result.json'
status = project/'Temp/typed-shop-validation-status.txt'
assert not request.exists(), 'Another validator request is pending'
for path in (result,temporary,status): path.unlink(missing_ok=True)
payload = json.dumps(dict(Mode='pet-combat-live',ConfigPath=str(args.fixture.resolve())))


def write_request():
    request.parent.mkdir(exist_ok=True)
    staging = request.with_suffix('.tmp')
    staging.write_text(payload,encoding='utf-8'); staging.replace(request)


write_request()
startup = subprocess.STARTUPINFO(); startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW; startup.wShowWindow=0
process = subprocess.Popen([str(editor),'-batchmode','-projectPath',str(project),'-logFile',str(args.log.resolve())],startupinfo=startup)
print('Requested Unity pet passive combat matrix',flush=True)
deadline = time.monotonic()+args.timeout
try:
    while process.poll() is None:
        if time.monotonic()>deadline: raise TimeoutError('Owned Unity validation deadline')
        if not any(p.exists() for p in (request,result,temporary,status)): write_request()
        time.sleep(.2)
    assert result.exists(), 'Unity stopped without a validation result'
    outcome = json.loads(result.read_text(encoding='utf-8'))
    if not outcome['Success'] or process.returncode != 0: raise RuntimeError(outcome.get('Error','Combat validation failed'))
    print('PET_PASSIVE_UNITY_LIVE=SUCCESS',flush=True)
finally:
    if process.poll() is None: process.terminate(); process.wait(timeout=30)
