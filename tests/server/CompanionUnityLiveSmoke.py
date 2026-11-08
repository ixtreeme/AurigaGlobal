"""Run companion/mount validators in the owned pinned Unity 6.3 snapshot."""
from pathlib import Path
import argparse,hashlib,json,socket,ssl,subprocess,sys,time

REPO=Path(__file__).resolve().parents[2]

def validate_local_host(package, host, nexoria=False):
    local_addresses={r[4][0] for r in socket.getaddrinfo(socket.gethostname(),None,socket.AF_INET)}
    owned_config=json.loads((package.resolve()/'server.json').read_text(encoding='utf-8'))
    assert host=='127.0.0.1' or nexoria and host in local_addresses and host==owned_config['game_defaults']['network']['bind_ip'], 'Only the owned local endpoint is allowed'
def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--mode',choices=('offline','live'),required=True)
    p.add_argument('--package',type=Path,required=True)
    p.add_argument('--unity-project',type=Path,required=True)
    p.add_argument('--fixture',type=Path)
    p.add_argument('--log',type=Path,required=True)
    p.add_argument('--timeout',type=int,default=900)
    args=p.parse_args();project=args.unity_project.resolve()
    marker=json.loads((project/'validation-source-snapshot.json').read_text(encoding='utf-8'))
    assert Path(marker['source']).resolve()!=project
    assert 'm_EditorVersion: 6000.3.27f1' in (project/'ProjectSettings/ProjectVersion.txt').read_text()
    editor=Path('C:/Program Files/Unity/Hub/Editor/6000.3.27f1/Editor/Unity.exe');assert editor.is_file()
    if args.mode=='live':
        assert args.fixture
        config=json.loads(args.fixture.read_text(encoding='utf-8-sig'))
        host=config['Host'];validate_local_host(args.package,host,config.get('Nexoria',False));assert len(config['Accounts'])==2
        assert all(a['Login']==('codexitem' if config.get('NexoriaBasic') else 'codexcompanion')+config['Token']+str(i) for i,a in enumerate(config['Accounts']))
        sys.path.insert(0,str(REPO/'SRC/Tools/ServerManager'))
        import item_bench
        item_bench.connection(args.package.resolve())
        # A bound game socket precedes FinalizeBoot. Wait for the actual pinned
        # TLS endpoint before launching Unity's shorter connection timeout.
        deadline=time.monotonic()+120
        readiness=ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
        readiness.check_hostname=False;readiness.verify_mode=ssl.CERT_NONE
        expected_pin=config['Pin'].replace(':','').replace(' ','').lower()
        assert len(expected_pin)==64 and all(c in '0123456789abcdef' for c in expected_pin)
        for port in (config['AuthPort'],config['GamePort']):
            while True:
                try:
                    with socket.create_connection((host,int(port)),timeout=2) as raw:
                        with readiness.wrap_socket(raw,server_hostname=config['ServerName']) as tls:
                            assert hashlib.sha256(tls.getpeercert(binary_form=True)).hexdigest()==expected_pin,'Owned TLS endpoint certificate mismatch'
                    break
                except OSError:
                    if time.monotonic()>deadline:raise TimeoutError('Owned local endpoints not listening') from None
                    time.sleep(.2)
    request=project/'Temp/typed-shop-validation-request.json';result=project/'validation-source-snapshot-result.json'
    temporary=project/'Temp/typed-shop-validation-result.json';status=project/'Temp/typed-shop-validation-status.txt'
    assert not request.exists(),'Another validator request is pending'
    for path in (result,temporary,status):path.unlink(missing_ok=True)
    payload=json.dumps(dict(Mode='companion-'+args.mode,ConfigPath=str(args.fixture.resolve()) if args.fixture else ''))
    def write_request():
        request.parent.mkdir(exist_ok=True);stage=request.with_suffix('.tmp');stage.write_text(payload,encoding='utf-8');stage.replace(request)
    write_request()
    startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
    process=subprocess.Popen([str(editor),'-batchmode','-projectPath',str(project),'-logFile',str(args.log.resolve())],startupinfo=startup)
    deadline=time.monotonic()+args.timeout
    try:
        while process.poll() is None:
            if time.monotonic()>deadline:raise TimeoutError('Owned Unity companion deadline')
            if not any(path.exists() for path in (request,result,temporary,status)):write_request()
            time.sleep(.2)
        assert result.exists(),'Unity stopped without a validation result'
        outcome=json.loads(result.read_text(encoding='utf-8'))
        if not outcome['Success'] or process.returncode!=0:raise RuntimeError(outcome.get('Error','Companion validation failed'))
        print('COMPANION_UNITY_VALIDATION=SUCCESS mode='+args.mode,flush=True)
    finally:
        if process.poll() is None:process.terminate();process.wait(timeout=30)
        if request.exists() and request.read_text(encoding='utf-8')==payload:
            request.unlink()

if __name__=='__main__':main()
