"""OpenSSH profiles shared by probes, SFTP and app-owned local forwards.

This module never contacts a host on import or while saving a profile. Generated
configuration is isolated from ~/.ssh/config, and both hops verify known keys.
"""
import copy
import contextlib
import ipaddress
import json
import os
from pathlib import Path, PurePosixPath
import re
import secrets
import shlex
import shutil
import tempfile

from backend import EditorError, sha, strict_json
import remote_jobs
import ssh_password


def new_profile():
    endpoint = dict(host='', port=22, user='', key_file='', host_key_alias='', auth='key', remember_password=False)
    return dict(id=secrets.token_hex(12), name='Éles FreeBSD', mode='proxyjump',
                target=copy.deepcopy(endpoint), jump=copy.deepcopy(endpoint),
                known_hosts=str(Path.home() / '.ssh/known_hosts'), remote_root='', forwards=[], work=remote_jobs.defaults())


def _text(value, label, maximum=1024):
    if not isinstance(value, str) or len(value) > maximum or any(ord(c) < 32 or ord(c) == 127 for c in value):
        raise EditorError('Érvénytelen mező: ' + label)
    return value


def _port(value):
    if type(value) is not int or not 1 <= value <= 65535:
        raise EditorError('A port 1 és 65535 közötti egész szám legyen.')
    return value


def _host(value):
    value = _text(value, 'szervercím', 253)
    if '%' in value:
        raise EditorError('A szervercím nem tartalmazhat OpenSSH-százaléktokent vagy IPv6-zónát.')
    try:
        return str(ipaddress.ip_address(value))
    except ValueError:
        if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9.-]*', value):
            raise EditorError('A szervercím DNS-név vagy IP-cím legyen, port és felhasználónév nélkül.')
        return value


def validate_profile(value, check_files=False, trust_new=False):
    template = new_profile()
    # Existing version-1 SSH profiles remain readable without a migration step.
    if isinstance(value, dict) and set(value) == set(template) - {'work'}:
        value = dict(value, work=remote_jobs.defaults())
    if not isinstance(value, dict) or set(value) != set(template):
        raise EditorError('Ismeretlen SSH-profil formátum.')
    profile = copy.deepcopy(value)
    profile['work'] = remote_jobs.validate_settings(profile['work'])
    if not re.fullmatch(r'[0-9a-f]{24}', _text(profile['id'], 'profilazonosító')):
        raise EditorError('Hibás SSH-profilazonosító.')
    if not _text(profile['name'], 'profil neve', 100).strip():
        raise EditorError('Adj nevet az SSH-profilnak.')
    if profile['mode'] not in ('proxyjump', 'direct'):
        raise EditorError('Ismeretlen SSH-kapcsolódási mód.')
    for role in ('target', 'jump'):
        endpoint = profile[role]
        if isinstance(endpoint, dict) and set(template[role]) - {'auth', 'remember_password'} <= set(endpoint) <= set(template[role]):
            endpoint.setdefault('auth', 'key')
            endpoint.setdefault('remember_password', False)
        if not isinstance(endpoint, dict) or set(endpoint) != set(template[role]):
            raise EditorError('Hibás SSH-végpont: ' + role)
        if endpoint['auth'] not in ('key', 'password'):
            raise EditorError('Ismeretlen SSH-hitelesítési mód.')
        if type(endpoint['remember_password']) is not bool:
            raise EditorError('A jelszó megjegyzésének értéke kapcsoló legyen.')
        for key in ('host', 'user', 'key_file', 'host_key_alias'):
            _text(endpoint[key], role + '.' + key)
        _port(endpoint['port'])
        if role == 'jump' and profile['mode'] != 'proxyjump':
            continue
        endpoint['host'] = _host(endpoint['host'])
        if not re.fullmatch(r'[A-Za-z0-9_][A-Za-z0-9_.-]*', endpoint['user']):
            raise EditorError('Add meg a célgép és az ugrógép SSH-felhasználóját.')
        if endpoint['host_key_alias'] and not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_.-]*', endpoint['host_key_alias']):
            raise EditorError('A hostkulcs neve betűt, számot, pontot, aláhúzást és kötőjelet tartalmazhat.')
        if endpoint['auth'] == 'key' and endpoint['key_file']:
            _local_path(endpoint['key_file'], check_files, 'SSH-privátkulcs')
    _local_path(profile['known_hosts'], check_files and not trust_new, 'szerverazonosító fájl (known_hosts)')
    root = _text(profile['remote_root'], 'távoli szervermappa')
    if root and (not root.startswith('/') or len(PurePosixPath(root).parts) <= 1 or '..' in PurePosixPath(root).parts):
        raise EditorError('A távoli szervermappa abszolút Unix-útvonal legyen, például /usr/home/auriga/server.')
    if not isinstance(profile['forwards'], list) or len(profile['forwards']) > 32:
        raise EditorError('Legfeljebb 32 porttovábbítás adható meg.')
    ports = set()
    for forward in profile['forwards']:
        if not isinstance(forward, dict) or set(forward) != {'local_port', 'remote_host', 'remote_port'}:
            raise EditorError('Hibás porttovábbítás.')
        local = _port(forward['local_port'])
        _port(forward['remote_port'])
        forward['remote_host'] = _host(forward['remote_host'])
        if local in ports:
            raise EditorError('Ugyanaz a helyi port csak egyszer adható meg.')
        ports.add(local)
    return profile


def _local_path(value, must_exist, label='SSH-fájl'):
    _text(value, 'helyi fájlútvonal')
    path = Path(value)
    # OpenSSH expands percent tokens and home shortcuts inside config values.
    # Require an absolute, literal path so no token can alter its meaning.
    if not value or not path.is_absolute() or any(char in value for char in '%~$*?'):
        raise EditorError('A(z) ' + label + ' teljes, helyi elérési útját add meg.')
    if must_exist and not path.is_file():
        raise EditorError('Nem található a(z) ' + label + ': ' + str(path))
    return path


def connection_route(profile):
    """Password retention is a local preference, not a new connection route."""
    route = {key: profile[key] for key in ('mode', 'known_hosts', 'remote_root')}
    for role in ('target', 'jump'):
        route[role] = {key: value for key, value in profile[role].items() if key != 'remember_password'}
        if route[role]['auth'] == 'password': route[role]['key_file'] = ''
    return route


def _quoted_path(value):
    return '"' + Path(value).as_posix().replace('\\', '\\\\').replace('"', '\\"') + '"'


def config_text(profile, trust_new=False):
    profile = validate_profile(profile)
    target, jump = 'auriga-' + profile['id'], 'jump-' + profile['id']
    lines = ['Host *', '    StrictHostKeyChecking ' + ('ask' if trust_new else 'yes'),
             '    UserKnownHostsFile ' + _quoted_path(profile['known_hosts']),
             '    GlobalKnownHostsFile none', '    UpdateHostKeys no', '    FingerprintHash sha256',
             '    ConnectTimeout 12', '    ConnectionAttempts 1',
             '    ServerAliveInterval 15', '    ServerAliveCountMax 3',
             '    ForwardAgent no', '    ForwardX11 no', '    RequestTTY no',
             '    PermitLocalCommand no', '    EscapeChar none',
             '    ControlMaster no', '    ControlPath none', '    LogLevel ERROR', '']
    for role, alias in (('target', target), ('jump', jump)):
        if role == 'jump' and profile['mode'] != 'proxyjump':
            continue
        endpoint = profile[role]
        lines += ['Host ' + alias, '    HostName ' + endpoint['host'],
                  '    User ' + endpoint['user'], '    Port ' + str(endpoint['port'])]
        if endpoint['auth'] == 'password':
            lines += ['    BatchMode no', '    PreferredAuthentications password,keyboard-interactive',
                      '    PasswordAuthentication yes', '    PubkeyAuthentication no',
                      '    KbdInteractiveAuthentication yes', '    NumberOfPasswordPrompts 1']
        else:
            lines += ['    BatchMode ' + ('no' if trust_new else 'yes'), '    PreferredAuthentications publickey',
                      '    PasswordAuthentication no', '    KbdInteractiveAuthentication no']
        if endpoint['auth'] == 'key' and endpoint['key_file']:
            lines += ['    IdentityFile ' + _quoted_path(endpoint['key_file']), '    IdentitiesOnly yes']
        if endpoint['host_key_alias']:
            lines += ['    HostKeyAlias ' + endpoint['host_key_alias']]
        if role == 'target' and profile['mode'] == 'proxyjump':
            lines += ['    ProxyJump ' + jump]
        lines += ['']
    return '\n'.join(lines)


def openssh(name):
    if os.name == 'nt':
        path = Path(os.environ.get('WINDIR', 'C:/Windows')) / 'System32/OpenSSH' / (name + '.exe')
        if path.is_file():
            return str(path)
    path = shutil.which(name)
    if not path:
        raise EditorError('Az OpenSSH kliens nem található: ' + name)
    return path


class Session:
    """Temporary config lives until its owning QProcess exits; no private keys copied."""
    def __init__(self, profile, *, parent=None, approve_host=None):
        self.trust_enabled = parent is not None or approve_host is not None
        self.profile = validate_profile(profile, check_files=True, trust_new=self.trust_enabled)
        self.trust_controller = None
        if self.trust_enabled:
            known = Path(self.profile['known_hosts'])
            known.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
            if not known.exists():
                try:
                    descriptor = os.open(known, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
                    os.close(descriptor)
                except FileExistsError: pass
        self.directory = tempfile.TemporaryDirectory(prefix='auriga-ssh-')
        self.config = Path(self.directory.name) / 'config'
        self.config.write_text(config_text(self.profile, self.trust_enabled), encoding='utf-8', newline='\n')
        self.config.chmod(0o600)
        self.alias = 'auriga-' + self.profile['id']
        self.password_broker = None
        try:
            if parent is not None:
                from ssh_trust import gui_approver
                self.trust_controller = gui_approver(parent)
                approve_host = self.trust_controller.approve
            roles = ('target', 'jump') if self.profile['mode'] == 'proxyjump' else ('target',)
            if self.trust_enabled or any(self.profile[role]['auth'] == 'password' for role in roles):
                self.password_broker = ssh_password.PasswordBroker(self.profile, self.directory.name, approve_host)
        except Exception:
            if self.trust_controller: self.trust_controller.close()
            self.directory.cleanup()
            raise

    def close(self):
        if self.trust_controller:
            self.trust_controller.close()
            self.trust_controller = None
        if self.password_broker:
            self.password_broker.close()
            self.password_broker = None
        self.directory.cleanup()

    def ssh(self, remote_command):
        return openssh('ssh'), ['-F', str(self.config), '-T', '-o', 'ClearAllForwardings=yes', self.alias, remote_command]

    def probe(self):
        root = self.profile['remote_root']
        script = ("printf 'AURIGA_OS='; /usr/bin/uname -s; "
                  "printf 'AURIGA_RELEASE='; /usr/bin/uname -r; "
                  "printf 'AURIGA_USER='; /usr/bin/id -un; "
                  "printf 'AURIGA_PYTHON='; (command -v python3 || true); ")
        if root:
            # The entire script is one sh -c argument. The SSH login shell may
            # be csh; it never interprets any user-supplied path as executable.
            script += ('if test -d ' + shlex.quote(root) + "; then printf 'AURIGA_DIRECTORY=yes\\n'; "
                       "else printf 'AURIGA_DIRECTORY=no\\n'; fi; "
                       'if test -f ' + shlex.quote(root + '/.auriga-package.json') + ' && test -f ' +
                       shlex.quote(root + '/manage.py') + ' && test -f ' + shlex.quote(root + '/server.json') +
                       "; then printf 'AURIGA_PACKAGE=yes\\n'; else printf 'AURIGA_PACKAGE=no\\n'; fi")
        else:
            script += "printf 'AURIGA_DIRECTORY=unset\\nAURIGA_PACKAGE=unset\\n'"
        return self.ssh(shlex.join(['/bin/sh', '-c', script]))

    def tunnel(self):
        if not self.profile['forwards']:
            raise EditorError('Adj meg legalább egy porttovábbítást az alagút indításához.')
        args = ['-F', str(self.config), '-N', '-T', '-o', 'ExitOnForwardFailure=yes']
        for row in self.profile['forwards']:
            host = row['remote_host']
            if ':' in host:
                host = '[' + host + ']'
            args += ['-L', '127.0.0.1:%d:%s:%d' % (row['local_port'], host, row['remote_port'])]
        return openssh('ssh'), [*args, self.alias]

    def sftp(self, batch):
        # SFTP uses exactly the same route, port, identity and trust settings.
        # -b forces BatchMode=yes internally; override it BEFORE -b so that
        # password auth keeps the checked batch file and its error semantics.
        options = ['-o', 'BatchMode=no'] if self.trust_enabled or self.profile['target']['auth'] == 'password' else []
        return openssh('sftp'), ['-F', str(self.config), *options, '-b', str(Path(batch)), self.alias]


def configure_process(process, session, timer=None):
    broker = getattr(session, 'password_broker', None)
    if broker:
        from PySide6.QtCore import QProcessEnvironment
        environment = QProcessEnvironment.systemEnvironment()
        for key, value in broker.environment.items(): environment.insert(key, value)
        process.setProcessEnvironment(environment)
    controller = getattr(session, 'trust_controller', None)
    if controller and timer is not None:
        paused = {}
        def waiting(active):
            if active and timer.isActive():
                paused['remaining'] = timer.remainingTime()
                timer.stop()
            elif not active and 'remaining' in paused:
                remaining = paused.pop('remaining')
                if process.state() != process.ProcessState.NotRunning: timer.start(max(remaining, 15000))
        controller.waiting.connect(waiting)


def probe_result(text):
    result = {}
    keys = {'OS', 'RELEASE', 'USER', 'PYTHON', 'DIRECTORY', 'PACKAGE'}
    for line in text.splitlines():
        if line.startswith('AURIGA_') and '=' in line:
            key, value = line[7:].split('=', 1)
            if key in keys:
                if key in result:
                    raise EditorError('Ismétlődő SSH-ellenőrzési eredmény.')
                result[key] = value
    if set(result) != keys or not all(result[key] for key in ('OS', 'RELEASE', 'USER')):
        raise EditorError('A szerver nem adott teljes kapcsolatellenőrzési választ.')
    return result


class ProfileStore:
    """Per-user settings; kept outside deployable packages and server.json."""
    def __init__(self, path):
        self.path = Path(path)
        self.snapshot = None
        self.loaded = False

    def load(self):
        data = self.path.read_bytes() if self.path.exists() else None
        if data is not None and len(data) > 1024 * 1024:
            raise EditorError('Túl nagy SSH-profilállomány.')
        value = strict_json(data.decode('utf-8-sig')) if data is not None else dict(version=1, profiles=[])
        if not isinstance(value, dict) or set(value) != {'version', 'profiles'} or type(value['version']) is not int or value['version'] != 1 or not isinstance(value['profiles'], list):
            raise EditorError('Ismeretlen SSH-profilállomány.')
        profiles = [validate_profile(profile) for profile in value['profiles']]
        if len({p['id'] for p in profiles}) != len(profiles):
            raise EditorError('Ismétlődő SSH-profilazonosító.')
        self.snapshot = sha(data) if data is not None else None
        self.loaded = True
        return profiles

    def save(self, profiles, *, transaction=None):
        profiles = [validate_profile(profile) for profile in profiles]
        if len({p['id'] for p in profiles}) != len(profiles):
            raise EditorError('Ismétlődő SSH-profilazonosító.')
        if not self.loaded:
            raise EditorError('Az SSH-profilállomány nem olvasható; a meglévő fájlt nem írjuk felül.')
        self.path.parent.mkdir(parents=True, exist_ok=True)
        temporary = None
        try:
            with self.lock():
                current = sha(self.path.read_bytes()) if self.path.exists() else None
                if current != self.snapshot:
                    raise EditorError('Az SSH-profilokat másik ablak módosította. Töltsd újra a listát.')
                data = (json.dumps(dict(version=1, profiles=profiles), ensure_ascii=False, indent=2) + '\n').encode('utf-8')
                if len(data) > 1024 * 1024:
                    raise EditorError('Túl sok SSH-profil.')
                # Hold the profile lock while changing its Windows secrets too.
                # A stale window must never overwrite/roll back another save.
                with transaction if transaction is not None else contextlib.nullcontext():
                    temporary = self.path.with_name(self.path.name + '.tmp-' + secrets.token_hex(6))
                    with temporary.open('xb') as output:
                        output.write(data)
                    temporary.chmod(0o600)
                    os.replace(temporary, self.path)
                    self.snapshot = sha(data)
        finally:
            if temporary is not None:
                temporary.unlink(missing_ok=True)

    @contextlib.contextmanager
    def lock(self):
        # OS locks are released on a crash. Keep the inode rather than unlinking
        # it underneath another process which may already be waiting on it.
        with self.path.with_name(self.path.name + '.lock').open('a+b') as handle:
            if os.name == 'nt':
                import msvcrt
                handle.seek(0, os.SEEK_END)
                if not handle.tell():
                    handle.write(b'\0')
                    handle.flush()
                handle.seek(0)
                lock, unlock = lambda: msvcrt.locking(handle.fileno(), msvcrt.LK_NBLCK, 1), lambda: msvcrt.locking(handle.fileno(), msvcrt.LK_UNLCK, 1)
            else:
                import fcntl
                lock, unlock = lambda: fcntl.flock(handle, fcntl.LOCK_EX | fcntl.LOCK_NB), lambda: fcntl.flock(handle, fcntl.LOCK_UN)
            try:
                lock()
            except OSError:
                raise EditorError('Másik ablak menti az SSH-profilokat. Próbáld újra később.') from None
            try:
                yield
            finally:
                handle.seek(0)
                unlock()
