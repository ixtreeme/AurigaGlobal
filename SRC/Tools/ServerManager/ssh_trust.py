"""First-contact approval of OpenSSH host fingerprints. No automatic trust."""
import re
import threading
import time


def host_question(profile, prompt):
    if not isinstance(prompt, str) or len(prompt) > 30000:
        return None
    match = re.match(r"\AThe authenticity of host '([^'\r\n]+)' can't be established", prompt)
    fingerprint = re.search(r'(?:\A|\n)([A-Z0-9_-]+) key fingerprint is:? (SHA256:[A-Za-z0-9+/]{43})\.?\s*(?:\n|\Z)', prompt)
    if not match or not fingerprint or not prompt.rstrip().endswith('Are you sure you want to continue connecting (yes/no/[fingerprint])?'):
        return None
    lookup = match[1].split(' (', 1)[0]
    roles = ('jump', 'target') if profile['mode'] == 'proxyjump' else ('target',)
    for role in roles:
        endpoint = profile[role]
        expected = endpoint['host_key_alias'] or (endpoint['host'].lower() if endpoint['port'] == 22 else
                                                  '[%s]:%d' % (endpoint['host'].lower(), endpoint['port']))
        if lookup != expected:
            continue
        return dict(role=role, host=endpoint['host'], port=endpoint['port'],
                    user=endpoint['user'], key_type=fingerprint[1], fingerprint=fingerprint[2])
    return None


def gui_approver(parent):
    # Import Qt only for a GUI-owned session; CLI/config tests remain stdlib-only.
    from PySide6.QtCore import QObject, Signal, Slot, Qt, QTimer
    from PySide6.QtWidgets import QWidget, QApplication, QMessageBox

    class Approver(QObject):
        requested = Signal(object)
        waiting = Signal(bool)

        def __init__(self, owner):
            super().__init__(owner)
            self.closed = False
            self.lock = threading.Lock()
            self.requests = []
            self.dialog = None
            self.requested.connect(self.show, Qt.ConnectionType.QueuedConnection)

        def approve(self, information):
            request = dict(information=information, finished=threading.Event(), accepted=False,
                           deadline=time.monotonic() + 120)
            with self.lock:
                if self.closed: return False
                self.requests.append(request)
            self.requested.emit(request)
            finished = request['finished'].wait(125)
            with self.lock:
                if request in self.requests: self.requests.remove(request)
                if not finished: request['finished'].set()
            return finished and request['accepted'] and not self.closed

        @Slot(object)
        def show(self, request):
            if self.closed or request['finished'].is_set(): return
            remaining = int((request['deadline'] - time.monotonic()) * 1000)
            if remaining <= 0:
                request['finished'].set()
                return
            info = request['information']
            owner = self.parent()
            while owner is not None and not isinstance(owner, QWidget): owner = owner.parent()
            dialog = QMessageBox(owner or QApplication.activeWindow())
            self.dialog = dialog
            dialog.setWindowTitle('Szerver azonosításának jóváhagyása')
            dialog.setTextFormat(Qt.TextFormat.PlainText)
            role = 'SSH-ugrógép' if info['role'] == 'jump' else 'Távoli szerver'
            dialog.setText('%s: %s:%s\n\nA szerver most bemutatott azonosítója még nincs jóváhagyva.\n'
                           'Ellenőrizd az ujjlenyomatot a szerver üzemeltetőjénél vagy konzolján.\n\n'
                           '%s ujjlenyomat:\n%s\n\nJóváhagyás után az app megjegyzi ezt a szervert.' %
                           (role, info['host'], info['port'], info['key_type'], info['fingerprint']))
            accept = dialog.addButton('Szerver jóváhagyása', QMessageBox.ButtonRole.AcceptRole)
            cancel = dialog.addButton('Mégse', QMessageBox.ButtonRole.RejectRole)
            dialog.setDefaultButton(cancel)
            expiry = QTimer(dialog)
            expiry.setSingleShot(True)
            expiry.timeout.connect(dialog.reject)
            expiry.start(remaining)
            self.waiting.emit(True)
            try:
                dialog.exec()
                request['accepted'] = dialog.clickedButton() is accept and not self.closed
            finally:
                request['finished'].set()
                self.dialog = None
                self.waiting.emit(False)
                dialog.deleteLater()

        def close(self):
            with self.lock:
                self.closed = True
                for request in self.requests: request['finished'].set()
            if self.dialog: self.dialog.reject()
            self.deleteLater()

    return Approver(parent)
