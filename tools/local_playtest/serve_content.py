"""Serve compiled Nexoria content on the owned server's LAN interface only."""
import argparse
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
import json
from pathlib import Path
import socket
import sys


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--package',type=Path,required=True)
    parser.add_argument('--content',type=Path,required=True)
    args=parser.parse_args()
    repo=Path(__file__).resolve().parents[2]
    sys.path.insert(0,str(repo/'SRC/Tools/ServerManager'))
    import backend
    package=backend.package_root(args.package.resolve())
    host=json.loads((package/'server.json').read_text(encoding='utf-8'))['game_defaults']['network']['bind_ip']
    local={r[4][0] for r in socket.getaddrinfo(socket.gethostname(),None,socket.AF_INET)}|{'127.0.0.1'}
    if host not in local:raise RuntimeError('The configured game host is not this computer')
    content=args.content.resolve(strict=True)
    if not any(content.glob('**/catalog*')):raise RuntimeError('No compiled Addressables catalog in the selected content directory')

    class Handler(SimpleHTTPRequestHandler):
        def translate_path(self,path):
            target=Path(super().translate_path(path)).resolve()
            if not target.is_relative_to(content):return str(content/'.denied')
            return str(target)
        def end_headers(self):
            self.send_header('Cache-Control','no-cache')
            super().end_headers()

    server=ThreadingHTTPServer((host,30090),partial(Handler,directory=str(content)))
    print(f'NEXORIA_LOCAL_CONTENT=http://{host}:30090/ directory={content}',flush=True)
    try:server.serve_forever()
    except KeyboardInterrupt:pass
    finally:server.server_close()


if __name__=='__main__':main()
