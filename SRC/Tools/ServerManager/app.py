"""Auriga Server Manager: Qt6 GUI and packaged management worker entry point."""
import os
from pathlib import Path
import runpy
import sys


def reconnect_streams():
    # The windowed PyInstaller executable has None streams. Popen's inherited
    # pipe handles still exist, including for run_server.py --check-config.
    if os.name == 'nt':
        import ctypes
        import msvcrt
        from ctypes import wintypes
        kernel = ctypes.WinDLL('kernel32', use_last_error=True)
        kernel.GetStdHandle.argtypes = [wintypes.DWORD]
        kernel.GetStdHandle.restype = wintypes.HANDLE
        kernel.GetCurrentProcess.restype = wintypes.HANDLE
        kernel.DuplicateHandle.argtypes = [wintypes.HANDLE, wintypes.HANDLE, wintypes.HANDLE,
                                         ctypes.POINTER(wintypes.HANDLE), wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
        current = kernel.GetCurrentProcess()
        for name, number in (('stdout', -11), ('stderr', -12)):
            if getattr(sys, name) is not None:
                continue
            original = kernel.GetStdHandle(number & 0xffffffff)
            duplicate = wintypes.HANDLE()
            if original and kernel.DuplicateHandle(current, original, current, ctypes.byref(duplicate), 0, False, 2):
                fd = msvcrt.open_osfhandle(duplicate.value, os.O_WRONLY)
                setattr(sys, name, os.fdopen(fd, 'w', encoding='utf-8', errors='replace', buffering=1))
    for name in ('stdout', 'stderr'):
        if getattr(sys, name) is None:
            setattr(sys, name, open(os.devnull, 'w'))


def main():
    arguments = sys.argv[1:]
    if os.environ.get('AURIGA_SSH_ASKPASS_TOKEN'):
        from ssh_password import askpass_main
        reconnect_streams()
        return askpass_main(arguments[0] if len(arguments) == 1 else '')
    if arguments[:1] == ['-B']:
        # The existing supervisor uses sys.executable to run this one helper.
        script = Path(arguments[1]).resolve()
        if script.name != 'run_server.py' or not (script.parent / '.auriga-package.json').is_file():
            return 2
        reconnect_streams()
        sys.dont_write_bytecode = True
        sys.argv = [str(script), *arguments[2:]]
        runpy.run_path(str(script), run_name='__main__')
        return 0
    if arguments[:1] == ['--worker']:
        from backend import worker
        return worker(arguments[1], arguments[2], arguments[3:])
    if arguments[:1] == ['--backup-scheduled'] and len(arguments)==2:
        reconnect_streams()
        import backend, backup_tools
        root=backend.package_root(arguments[1])
        try:
            result=backup_tools.run_schedule(root,backend)
            print('BACKUP_SCHEDULE_RESULT',result)
            return 0
        except Exception as error:
            print('BACKUP_SCHEDULE_FAILED',backend.redact(str(error),root))
            return 1
    from window import run
    return run(arguments)


if __name__ == '__main__':
    raise SystemExit(main())
