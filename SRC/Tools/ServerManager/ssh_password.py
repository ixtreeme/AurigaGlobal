"""Memory/opt-in Windows credentials and a loopback-only askpass bridge.

OpenSSH keeps stdin for RPC/SFTP. Neither the password nor a password file is
put in its arguments, environment, configuration or saved SSH profile.
"""
import json
import base64
import contextlib
import hashlib
import os
from pathlib import Path
import secrets
import shlex
import socket
import sys
import threading
import subprocess
import tempfile
import re

from backend import EditorError
from credential_store import system_store
from ssh_trust import host_question

_passwords = {}
_launcher_directory = None
_launcher_lock = threading.Lock()


def native_askpass_launcher():
    """Windows source runs need an EXE: cmd.exe truncates multiline prompts."""
    global _launcher_directory
    with _launcher_lock:
        if _launcher_directory is not None:
            return Path(_launcher_directory.name) / 'AurigaSshAskpass.exe'
        framework = Path(os.environ.get('WINDIR', 'C:/Windows')) / 'Microsoft.NET'
        compiler = next((framework / name / 'v4.0.30319/csc.exe' for name in ('Framework64', 'Framework')
                         if (framework / name / 'v4.0.30319/csc.exe').is_file()), None)
        if compiler is None:
            raise EditorError('A forrásból futtatott SSH-segéd fordításához Windows .NET Framework szükséges. A kész szerverkezelő EXE ezt nem igényli.')
        temporary = tempfile.TemporaryDirectory(prefix='auriga-askpass-native-')
        program = Path(temporary.name) / 'AurigaSshAskpass.exe'
        try:
            result = subprocess.run([str(compiler), '/nologo', '/target:winexe', '/out:' + str(program),
                                     str(Path(__file__).with_name('ssh_askpass_launcher.cs'))],
                                    capture_output=True, timeout=30, creationflags=subprocess.CREATE_NO_WINDOW)
            if result.returncode:
                raise EditorError('A fejlesztői SSH-segéd fordítása nem sikerült. Használd a becsomagolt szerverkezelőt.')
        except Exception:
            temporary.cleanup()
            raise
        _launcher_directory = temporary
        return program


def credential_key(profile, role):
    endpoint = profile[role]
    return (profile['id'], role, endpoint['host'].lower(), endpoint['port'], endpoint['user'])


def remember(profile, role, password):
    if not isinstance(password, str) or any(c in password for c in '\0\r\n') or len(password.encode('utf-8')) > 1023:
        raise EditorError('Az SSH-jelszó legfeljebb 1023 bájt lehet, sortörés nélkül.')
    key = credential_key(profile, role)
    _passwords[key] = password  # Empty drafts must not revive an old saved password.


def recalled(profile, role):
    key = credential_key(profile, role)
    if key not in _passwords and profile[role].get('remember_password') and profile[role]['auth'] == 'password':
        password = system_store().read(credential_name(profile, role))
        remember(profile, role, password or '')
    return _passwords.get(key, '')


def credential_name(profile, role):
    key = credential_key(profile, role)
    identity = hashlib.sha256(json.dumps(key, ensure_ascii=True).encode()).hexdigest()
    return 'AurigaServerManager/SSH/' + profile['id'] + '/' + role + '/' + identity


@contextlib.contextmanager
def saved_preferences(profile=None, previous=None):
    """Commit secrets with the profile write, restoring them if that write fails."""
    changes = {}
    if previous:
        for role in ('target', 'jump'):
            if previous[role].get('remember_password'):
                changes[credential_name(previous, role)] = (previous[role]['user'], None)
    if profile:
        for role in ('target', 'jump'):
            endpoint = profile[role]
            if endpoint.get('remember_password') and endpoint['auth'] == 'password':
                password = recalled(profile, role)
                if not password:
                    raise EditorError('A jelszó megjegyzéséhez töltsd ki az SSH-jelszó mezőt, vagy vedd ki a jelölést.')
                changes[credential_name(profile, role)] = (endpoint['user'], password)
    if not changes:
        yield
        return
    store = system_store()
    originals = {name: store.read(name) for name in changes}
    completed = []
    try:
        for name, (user, password) in changes.items():
            if originals[name] == password: continue
            if password is None: store.delete(name)
            else: store.write(name, user, password)
            completed.append(name)
        yield
    except Exception:
        failed = False
        for name in reversed(completed):
            try:
                if originals[name] is None: store.delete(name)
                else: store.write(name, changes[name][0], originals[name])
            except (EditorError, OSError):
                failed = True
        if failed:
            raise EditorError('A mentés sikertelen, és a Windows jelszótár korábbi állapotát sem sikerült teljesen visszaállítani. Ellenőrizd a profil jelszavait.') from None
        raise


def forget(profile_id=None):
    for key in list(_passwords):
        if profile_id is None or key[0] == profile_id: del _passwords[key]


def read_line(connection):
    data = bytearray()
    while len(data) < 65536:
        chunk = connection.recv(1)
        if not chunk: raise ValueError('Closed askpass connection')
        if chunk == b'\n': return json.loads(data)
        data.extend(chunk)
    raise ValueError('Oversized askpass message')


class PasswordBroker:
    def __init__(self, profile, directory, approve_host=None):
        self.profile = profile
        self.approve_host = approve_host
        self.answers = {}
        self.attempts = {}
        for role in ('jump', 'target') if profile['mode'] == 'proxyjump' else ('target',):
            endpoint = profile[role]
            if endpoint['auth'] != 'password': continue
            password = recalled(profile, role)
            if not password:
                raise EditorError(('A célgép' if role == 'target' else 'Az ugrógép') +
                                  ' SSH-jelszava hiányzik. Add meg a Kapcsolatok és eszközök oldalon, majd mentsd a profilt.')
            host = endpoint['host_key_alias'] or endpoint['host'].lower()
            prompt = endpoint['user'] + '@' + host + "'s password:"
            if prompt in self.answers and self.answers[prompt] != password:
                raise EditorError('A két SSH-gép jelszókérése nem különböztethető meg. Adj meg külön hostkulcsnevet a profilban.')
            self.answers[prompt] = password
        self.token = secrets.token_hex(32)
        self.listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.listener.bind(('127.0.0.1', 0))
        self.listener.listen(8)
        self.listener.settimeout(.2)
        self.port = self.listener.getsockname()[1]
        self.stopping = threading.Event()
        if getattr(sys, 'frozen', False):
            program = Path(sys.executable)
            extra = {}
        else:
            # Only recognized endpoint-bound password/fingerprint prompts are
            # answered; OTP and password-change challenges receive no secret.
            # This launcher contains paths only. Passwords stay in the broker.
            script = Path(__file__).resolve()
            if os.name == 'nt':
                program = native_askpass_launcher()
                extra = dict(AURIGA_ASKPASS_PYTHON=sys.executable, AURIGA_ASKPASS_SCRIPT=str(script))
            else:
                program = Path(directory) / 'askpass'
                program.write_text('#!/bin/sh\nexec ' + shlex.join([sys.executable, '-B', str(script)]) + ' "$@"\n', encoding='utf-8')
                extra = {}
            program.chmod(0o700)
        self._environment = dict(extra, SSH_ASKPASS=str(program), SSH_ASKPASS_REQUIRE='force',
                                AURIGA_SSH_ASKPASS_PORT=str(self.port), AURIGA_SSH_ASKPASS_TOKEN=self.token,
                                PYINSTALLER_RESET_ENVIRONMENT='1')
        self.thread = threading.Thread(target=self.serve, name='AurigaSshAskpass', daemon=True)
        self.thread.start()

    @property
    def environment(self):
        # One password submission per endpoint per SSH process, including a
        # fallback between password and PAM. A later RPC/SFTP gets a fresh scope.
        scope = secrets.token_hex(16)
        self.attempts[scope] = set()
        return dict(self._environment, AURIGA_SSH_ASKPASS_SCOPE=scope)

    def password_identity(self, prompt):
        if prompt in self.answers: return prompt
        for identity in self.answers:
            userhost = identity[:-len("'s password:")]
            prefix = '(' + userhost + ') '
            if not prompt.startswith(prefix): continue
            challenge = prompt[len(prefix):].strip()
            user = userhost.split('@', 1)[0]
            # OpenSSH adds the verified connection's user@host prefix. OpenPAM
            # uses Password: or Password for user@server-hostname: on FreeBSD.
            if challenge.casefold() == 'password:' or re.fullmatch(
                    r'(?i:Password for )' + re.escape(user) + r'@[A-Za-z0-9_.:\[\]-]+:', challenge):
                return identity
        return None

    def serve(self):
        while not self.stopping.is_set():
            try:
                connection, _ = self.listener.accept()
            except socket.timeout:
                continue
            except OSError:
                break
            with connection:
                try:
                    connection.settimeout(1)
                    request = read_line(connection)
                    password = None
                    if isinstance(request, dict) and secrets.compare_digest(str(request.get('token', '')), self.token):
                        prompt = str(request.get('prompt', '')).strip()
                        identity = self.password_identity(prompt)
                        attempted = self.attempts.get(request.get('scope', ''))
                        if identity is not None and attempted is not None and identity not in attempted:
                            attempted.add(identity)
                            password = self.answers[identity]
                        if password is None and self.approve_host:
                            information = host_question(self.profile, prompt)
                            if information:
                                password = 'yes' if self.approve_host(information) else 'no'
                    connection.sendall((json.dumps(dict(password=password), ensure_ascii=True) + '\n').encode())
                except (OSError, ValueError, TypeError):
                    pass

    def close(self):
        self.stopping.set()
        self.listener.close()
        self.thread.join(timeout=1.5)
        self.answers.clear()
        self.attempts.clear()


def askpass_main(prompt):
    # Never log exceptions or prompts: only the requested password may reach
    # the private stdout pipe captured by OpenSSH.
    try:
        port = int(os.environ['AURIGA_SSH_ASKPASS_PORT'])
        token = os.environ['AURIGA_SSH_ASKPASS_TOKEN']
        scope = os.environ['AURIGA_SSH_ASKPASS_SCOPE']
        with socket.create_connection(('127.0.0.1', port), timeout=3) as connection:
            connection.sendall((json.dumps(dict(token=token, scope=scope, prompt=prompt), ensure_ascii=True) + '\n').encode())
            connection.settimeout(150)  # Host approval may require checking the fingerprint.
            response = read_line(connection)
        password = response.get('password')
        if not isinstance(password, str) or any(c in password for c in '\0\r\n'): return 1
        # Windows console encodings must not alter accented SSH passwords.
        sys.stdout.buffer.write(password.encode('utf-8') + b'\n')
        sys.stdout.buffer.flush()
        return 0
    except (OSError, ValueError, KeyError, TypeError):
        return 1


if __name__ == '__main__':
    try:
        prompt = base64.b64decode(sys.argv[2], validate=True).decode('utf-8') if len(sys.argv) == 3 and sys.argv[1] == '--prompt-base64' else sys.argv[1] if len(sys.argv) == 2 else ''
    except (ValueError, UnicodeError):
        raise SystemExit(1)
    raise SystemExit(askpass_main(prompt))
