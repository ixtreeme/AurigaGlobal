"""Target-bound SSH RPC. No remote files are mirrored into a local package."""
import base64
import copy
import hashlib
import json
from pathlib import Path
import secrets

from PySide6.QtCore import QObject, QProcess, QTimer, Signal
import backend as api
import ssh_connection as ssh

MAX_OUTPUT = 16 * 1024 * 1024


def support_file(name):
    base = Path(__file__).resolve().parent
    choices = [base / 'remote_support' / name, base / name]
    path = next((p for p in choices if p.is_file()), None)
    if path is None: raise api.EditorError('Hiányzó távoli kezelőmodul: ' + name)
    return path.read_text(encoding='utf-8')


def worker_input(request):
    payload = dict(request, support={name: support_file(name + '.py') for name in ('crash_tools', 'backend', 'editor_checks')})
    encoded = base64.b64encode(json.dumps(payload, ensure_ascii=True).encode()).decode('ascii')
    return (support_file('server_target_worker.py') + '\nif __name__ == "__main__":\n    main(' + repr(encoded) + ')\n').encode('utf-8')


def document(packet):
    raw = base64.b64decode(packet['data'], validate=True)
    if hashlib.sha256(raw).hexdigest() != packet['sha256']:
        raise api.EditorError('A távoli dokumentum ellenőrzőösszege hibás.')
    return api.Document.from_bytes(packet['relative'], raw, allow_invalid_json=True)


class RemoteClient(QObject):
    changed = Signal()

    def __init__(self, profile, parent=None):
        super().__init__(parent)
        self.profile = copy.deepcopy(profile)
        self.process = self.session = self.timer = None
        self.operation = None
        self.mutating = self.polling = False
        self.output = bytearray()
        self.failure = ''
        self.request_id = None
        self.success = self.error = None
        self.last_transport_error = False

    def busy(self):
        return self.process is not None and not self.polling

    def request(self, operation, arguments, success, error, *, poll=False):
        if self.process is not None:
            if self.polling and not poll: self.cancel()
            else: return False
        root = self.profile.get('remote_root', '')
        if not root:
            error('Add meg a távoli szervermappát, majd mentsd az SSH-profilt.')
            return False
        session = None
        self.last_transport_error = True
        try:
            identity = secrets.token_hex(12)
            request = dict(schema_version=1, id=identity, target=self.profile['id'], root=root,
                           operation=operation, arguments=arguments)
            payload = worker_input(request)
            session = ssh.Session(self.profile, parent=self)
            command = session.ssh('python3 -B -u -')
            process = QProcess(self)
            process.setProcessChannelMode(QProcess.ProcessChannelMode.SeparateChannels)
            self.process, self.session = process, session
            self.operation, self.polling = operation, poll
            self.mutating = operation in ('save', 'action')
            self.request_id, self.success, self.error = identity, success, error
            self.output, self.failure = bytearray(), ''
            self.timer = QTimer(self)
            self.timer.setSingleShot(True)
            ssh.configure_process(process, session, self.timer)
            self.timer.timeout.connect(lambda: self.timeout(process))
            process.started.connect(lambda: self.feed(process, payload))
            process.readyReadStandardOutput.connect(lambda: self.read(process))
            process.readyReadStandardError.connect(lambda: self.read_error(process))
            process.finished.connect(lambda code, status: self.finished(process, request, code, status))
            process.errorOccurred.connect(lambda reason: self.failed(process, request, reason))
            self.timer.start(600000 if operation == 'action' else 120000 if operation in ('save','crash-analyze') else 45000)
            self.changed.emit()
            process.start(*command)
            return True
        except (api.EditorError, OSError, ValueError) as failure:
            if session: session.close()
            error(str(failure))
            return False

    def feed(self, process, payload):
        if self.process is process:
            process.write(payload)
            process.closeWriteChannel()

    def read(self, process):
        data = bytes(process.readAllStandardOutput())
        if self.process is not process: return
        self.output.extend(data)
        if len(self.output) > MAX_OUTPUT:
            self.failure = 'Túl nagy távoli válasz; a művelet állapotát újra kell ellenőrizni.'
            process.kill()

    def read_error(self, process):
        data = bytes(process.readAllStandardError()).decode('utf-8', errors='replace')
        if self.process is process:
            self.failure = (self.failure + data)[-8000:]

    def timeout(self, process):
        if self.process is process:
            self.failure = 'A távoli kérés időkorlátja lejárt. Ellenőrizd újra a célgép állapotát; egy már elküldött szerverművelet tovább futhat.'
            process.kill()

    def failed(self, process, request, reason):
        if reason == QProcess.ProcessError.FailedToStart and self.process is process:
            self.failure = 'Az SSH kliens nem indítható: ' + process.errorString()
            self.finished(process, request, 1, QProcess.ExitStatus.CrashExit)

    def finished(self, process, request, code, status):
        if self.process is not process: return
        self.read(process); self.read_error(process)
        success, error = self.success, self.error
        try:
            lines = [line[len('AURIGA_TARGET_RESULT '):] for line in bytes(self.output).decode('utf-8').splitlines()
                     if line.startswith('AURIGA_TARGET_RESULT ')]
            if code or status != QProcess.ExitStatus.NormalExit or len(lines) != 1:
                raise api.EditorError(self.failure.strip() or 'A távoli szerver nem adott érvényes választ.')
            response = api.strict_json(lines[0])
            if any(response.get(key) != request[key] for key in ('schema_version', 'id', 'root', 'target')):
                raise api.EditorError('A távoli válasz másik szerverhez vagy kéréshez tartozik.')
            self.last_transport_error = False
            if response.get('ok') is not True:
                raise api.EditorError(response.get('error') or 'A távoli művelet sikertelen.')
            result, failure = response['result'], None
        except (api.EditorError, UnicodeError, ValueError, KeyError, TypeError) as issue:
            result, failure = None, str(issue)
        self.cleanup(process)
        if failure: error(failure)
        else: success(result)

    def cleanup(self, process):
        if self.timer:
            self.timer.stop(); self.timer.deleteLater()
        if self.session: self.session.close()
        self.process = self.session = self.timer = None
        self.operation = None
        self.mutating = self.polling = False
        self.success = self.error = None
        self.output = bytearray()
        process.deleteLater()
        self.changed.emit()

    def cancel(self):
        process = self.process
        if process is None: return
        self.process = None  # Old callbacks cannot apply to another target.
        process.kill()
        process.waitForFinished(3000)
        self.cleanup(process)
