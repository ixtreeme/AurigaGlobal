"""FreeBSD pkg operations, also embedded in the SSH worker (stdlib only)."""
import contextlib
import hashlib
import json
import os
from pathlib import Path
import platform
import queue
import re
import secrets
import shlex
import signal
import subprocess
import threading
import time

RESULT = 'AURIGA_PACKAGES_RESULT '
OPERATIONS = ('search', 'installed', 'plan', 'install', 'update')
NAME = r'[A-Za-z0-9][A-Za-z0-9_.+,-]{0,159}'


def validate(request):
    operation = request.get('operation')
    if operation not in OPERATIONS:
        raise ValueError('Ismeretlen csomagművelet.')
    if operation == 'search' and not re.fullmatch(NAME, request.get('query', '')):
        raise ValueError('A kereséshez adj meg egy csomagnevet vagy névrészletet (pl. ninja).')
    if operation in ('plan', 'install'):
        names = request.get('packages')
        if (not isinstance(names, list) or not 1 <= len(names) <= 32
                or any(not isinstance(name, str) or not re.fullmatch(NAME, name) for name in names)):
            raise ValueError('Adj meg legfeljebb 32 pontos csomagnevet, szóközzel elválasztva.')
        if operation == 'install' and not re.fullmatch(r'[0-9a-f]{64}', request.get('fingerprint', '')):
            raise ValueError('Telepítés előtt készíts telepítési tervet.')
    return request


class Pkg:
    def __init__(self, report):
        if platform.system() != 'FreeBSD':
            raise ValueError('Ez a csomagkezelő FreeBSD célgépen használható.')
        self.program = next((str(p) for p in (Path('/usr/local/sbin/pkg'), Path('/usr/local/bin/pkg'))
                             if p.is_file() and os.access(p, os.X_OK)), None)
        if not self.program:
            raise ValueError('A pkg nincs telepítve. A célgépen előbb futtasd: /usr/sbin/pkg bootstrap')
        self.report = report
        self.environment = dict(os.environ, PATH='/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin',
                                LC_ALL='C', LANG='C', TERM='dumb')

    def run(self, arguments, *, show=True, empty_ok=False, mutation=False, dry_run=False):
        argv = [self.program, '-o', 'REPO_AUTOUPDATE=false', *arguments]
        self.report('> ' + shlex.join(argv))
        process = subprocess.Popen(argv, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
            stderr=subprocess.PIPE, env=self.environment, start_new_session=os.name != 'nt')
        output, errors, size = [], [], 0
        lines = queue.Queue()

        def read(stream, error):
            try:
                for line in iter(stream.readline, b''):
                    lines.put((line, error))
            finally:
                stream.close()
                lines.put(None)

        readers = [threading.Thread(target=read, args=(stream, error), daemon=True)
                   for stream, error in ((process.stdout, False), (process.stderr, True))]
        for reader in readers:
            reader.start()
        deadline = time.monotonic() + 120
        finished = 0
        try:
            while True:
                try:
                    item = lines.get(timeout=.2)
                except queue.Empty:
                    if not mutation and time.monotonic() > deadline:
                        raise TimeoutError('A pkg lekérdezés időtúllépéssel leállt.')
                    continue
                if item is None:
                    finished += 1
                    if finished == len(readers):
                        break
                    continue
                line, error = item
                size += len(line)
                if size > 4 * 1024 * 1024 and not mutation:
                    raise ValueError('Túl nagy pkg válasz. Szűkítsd a keresést.')
                text = line.decode('utf-8', errors='replace')
                if size <= 4 * 1024 * 1024:
                    output.append(text)
                    if error:
                        errors.append(text)
                if show:
                    self.report(text.rstrip('\r\n'))
            code = process.wait()
        finally:
            if process.poll() is None:
                process.kill()
                process.wait()
            for reader in readers:
                reader.join(timeout=2)
        text = ''.join(output)
        # pkg install -n returns 1 when its summary is shown without answering
        # yes (src/install.c, rc == false). Do not use -y in a read-only plan:
        # some pkg versions would also try to change the automatic flag.
        planned = (dry_run and code == 1 and not ''.join(errors).strip()
                   and re.search(r'^The following [1-9][0-9]* package\(s\) will be affected ', text, re.M)
                   and re.search(r'^Number of packages to be (?:installed|upgraded|reinstalled|removed|downgraded): [1-9]', text, re.M))
        # EX_UNAVAILABLE (69) is pkg query/rquery's normal "no match" result.
        if code and not planned and not (empty_ok and code == 69 and not text.strip()):
            raise ValueError('A pkg parancs hibával leállt (%d). %s' % (code, text[-3000:].strip()))
        return text

    def plan(self, names):
        state = self.run(['query', '-a', '%n\t%v\t%a\t%k'], show=False, empty_ok=True)
        text = self.run(['install', '-U', '-n', '--', *names], dry_run=True)
        fingerprint = hashlib.sha256(json.dumps([names, sorted(state.splitlines()), text],
            ensure_ascii=True).encode()).hexdigest()
        return dict(packages=names, plan=text, fingerprint=fingerprint)


@contextlib.contextmanager
def mutation_log(request, emit):
    """Keep pkg transactions alive and logged if the SSH client disconnects."""
    if os.geteuid() != 0:
        raise ValueError('Telepítéshez és katalógusfrissítéshez root SSH-felhasználó szükséges.')
    lock = Path('/var/run/auriga-manager-pkg.lock')
    try:
        lock.mkdir(mode=0o700)
    except FileExistsError:
        raise ValueError('Már fut csomagművelet, vagy megszakadt korábbi munkamenet maradt. '
                         'Ellenőrizd: /var/run/auriga-manager-pkg.lock és /var/log/auriga-manager/packages.') from None
    handlers = {}
    try:
        (lock / 'owner.json').write_text(json.dumps(dict(id=request['id'], pid=os.getpid())), encoding='utf-8')
        folder = Path('/var/log/auriga-manager/packages')
        folder.mkdir(mode=0o700, parents=True, exist_ok=True)
        path = folder / (request['id'] + '.log')
        with path.open('x', encoding='utf-8') as log:
            def report(text):
                log.write(text + '\n');log.flush()
                try:
                    emit(text)
                except (BrokenPipeError, OSError):
                    pass
            for name in ('SIGHUP', 'SIGTERM', 'SIGINT'):
                sig = getattr(signal, name, None)
                if sig is not None and threading.current_thread() is threading.main_thread():
                    handlers[sig] = signal.signal(sig, signal.SIG_IGN)
            report('Távoli csomagnapló: ' + str(path))
            try:
                yield report
            except Exception as error:
                report('SIKERTELEN: ' + str(error))
                raise
    finally:
        for sig, handler in handlers.items():
            signal.signal(sig, handler)
        (lock / 'owner.json').unlink(missing_ok=True)
        lock.rmdir()


def execute(request, emit=print):
    validate(request)
    operation = request['operation']
    result = dict(operation=operation, id=request['id'])
    if operation in ('install', 'update'):
        # Check platform/pkg before creating any state on the target.
        Pkg(emit)
        with mutation_log(request, emit) as report:
            pkg = Pkg(report)
            if operation == 'install':
                plan = pkg.plan(request['packages'])
                if plan['fingerprint'] != request['fingerprint']:
                    raise ValueError('A telepítési terv megváltozott. Készíts új tervet, és ellenőrizd a változásokat.')
                pkg.run(['install', '-U', '-y', '--', *request['packages']], mutation=True)
                # Do not report success unless every explicitly requested name exists.
                for name in request['packages']:
                    pkg.run(['info', '-e', name], show=False)
            else:
                pkg.run(['update'], mutation=True)
            report('A csomagművelet sikeresen befejeződött.')
    else:
        pkg = Pkg(emit)
        if operation == 'plan':
            result.update(pkg.plan(request['packages']))
        else:
            if operation == 'search':
                arguments = ['rquery', '-U', '-g', '%n\t%v\t%c', '*' + request['query'] + '*']
            else:
                arguments = ['query', '-a', '%n\t%v\t%c']
            text = pkg.run(arguments, show=False, empty_ok=True)
            rows = [line.split('\t', 2) for line in text.splitlines() if '\t' in line]
            result.update(rows=rows[:500], total=len(rows))
            emit('%d csomag található; a táblázatban legfeljebb 500 jelenik meg.' % len(rows))
    emit(RESULT + json.dumps(result, ensure_ascii=True, separators=(',', ':')))


def request(operation, *, query='', packages='', fingerprint=''):
    value = dict(action='packages', id=secrets.token_hex(12), operation=operation)
    if operation == 'search':
        value['query'] = query.strip()
    if operation in ('plan', 'install'):
        value['packages'] = list(dict.fromkeys(packages.split()))
    if operation == 'install':
        value['fingerprint'] = fingerprint
    return validate(value)
