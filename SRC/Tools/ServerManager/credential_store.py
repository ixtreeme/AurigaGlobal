"""Current-user Windows Credential Manager. Never writes a password file."""
import ctypes
from ctypes import wintypes
import os

from backend import EditorError


class Credential(ctypes.Structure):
    _fields_ = [('Flags', wintypes.DWORD), ('Type', wintypes.DWORD),
                ('TargetName', wintypes.LPWSTR), ('Comment', wintypes.LPWSTR),
                ('LastWritten', wintypes.FILETIME), ('CredentialBlobSize', wintypes.DWORD),
                ('CredentialBlob', ctypes.POINTER(ctypes.c_ubyte)), ('Persist', wintypes.DWORD),
                ('AttributeCount', wintypes.DWORD), ('Attributes', ctypes.c_void_p),
                ('TargetAlias', wintypes.LPWSTR), ('UserName', wintypes.LPWSTR)]


class WindowsCredentialStore:
    def __init__(self):
        if os.name != 'nt':
            raise EditorError('A jelszó tartós megjegyzése a Windows Hitelesítőadat-kezelőjét igényli.')
        self.api = ctypes.WinDLL('Advapi32.dll', use_last_error=True)
        self.api.CredWriteW.argtypes = [ctypes.POINTER(Credential), wintypes.DWORD]
        self.api.CredWriteW.restype = wintypes.BOOL
        self.api.CredReadW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD,
                                      ctypes.POINTER(ctypes.POINTER(Credential))]
        self.api.CredReadW.restype = wintypes.BOOL
        self.api.CredDeleteW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD]
        self.api.CredDeleteW.restype = wintypes.BOOL
        self.api.CredFree.argtypes = [ctypes.c_void_p]
        self.api.CredFree.restype = None

    def error(self, action):
        return EditorError('A Windows jelszótár ' + action + ' sikertelen (hibakód: %d).' % ctypes.get_last_error())

    def read(self, name):
        pointer = ctypes.POINTER(Credential)()
        if not self.api.CredReadW(name, 1, 0, ctypes.byref(pointer)):
            if ctypes.get_last_error() == 1168: return None  # ERROR_NOT_FOUND
            raise self.error('olvasása')
        try:
            value = pointer.contents
            if value.CredentialBlobSize > 4096:
                raise EditorError('Érvénytelen SSH-bejegyzés a Windows jelszótárban.')
            return ctypes.string_at(value.CredentialBlob, value.CredentialBlobSize).decode('utf-8')
        except UnicodeError:
            raise EditorError('Sérült SSH-jelszó a Windows jelszótárban.') from None
        finally:
            self.api.CredFree(pointer)

    def write(self, name, user, password):
        raw = password.encode('utf-8')
        data = (ctypes.c_ubyte * len(raw)).from_buffer_copy(raw)
        value = Credential(Type=1, TargetName=name, Comment='Auriga Server Manager SSH',
                           CredentialBlobSize=len(raw), CredentialBlob=data,
                           Persist=2, UserName=user)  # CRED_PERSIST_LOCAL_MACHINE, current user only.
        try:
            if not self.api.CredWriteW(ctypes.byref(value), 0): raise self.error('mentése')
        finally:
            ctypes.memset(data, 0, len(raw))

    def delete(self, name):
        if not self.api.CredDeleteW(name, 1, 0) and ctypes.get_last_error() != 1168:
            raise self.error('törlése')


def system_store():
    return WindowsCredentialStore()
