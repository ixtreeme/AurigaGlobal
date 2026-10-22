"""Bounded crash inventory and read-only debugger reports; no Qt dependency."""
import datetime
import json
import os
from pathlib import Path
import shutil
import signal
import struct
import subprocess
import tempfile
import time

MAX_REPORT = 512000


def checked_path(root, value):
    if not isinstance(value, str) or not value or any(ord(c) < 32 for c in value):
        raise ValueError('Adj meg core- vagy programfájl-útvonalat.')
    path = Path(value)
    if '..' in path.parts:
        raise ValueError('Az útvonal nem tartalmazhat .. elemet.')
    if not path.is_absolute(): path = Path(root) / path
    for parent in (path, *path.parents):
        if parent.is_symlink() or (hasattr(parent, 'is_junction') and parent.is_junction()):
            raise ValueError('Linkelt core- vagy programútvonal nem elemezhető.')
    if not path.is_file(): raise ValueError('A fájl nem található: ' + str(path))
    return path


def header(path):
    with path.open('rb') as stream: raw = stream.read(64)
    if raw[:4] == b'MDMP': return dict(format='minidump')
    if len(raw) < 52 or raw[:4] != b'\x7fELF' or raw[4] not in (1, 2) or raw[5] not in (1, 2):
        raise ValueError('A fájl nem ELF core/program vagy Windows minidump.')
    endian = '<' if raw[5] == 1 else '>'
    kind, machine = struct.unpack(endian + 'HH', raw[16:20])
    return dict(format='ELF', bits=32 if raw[4] == 1 else 64, endian=endian, kind=kind, machine=machine)


def debuggers():
    environment = dict(os.environ)
    if os.name != 'nt': environment['PATH'] = '/usr/local/bin:/usr/bin:/bin:/usr/local/sbin:/usr/sbin:/sbin'
    return {name: shutil.which(name, path=environment.get('PATH')) for name in ('gdb', 'lldb', 'cdb')}


def events(root):
    result = []
    directory = Path(root) / 'run/crashes'
    if directory.is_symlink(): return result
    for folder in sorted(directory.glob('*'), reverse=True):
        if folder.is_symlink() or not folder.is_dir() or folder.name == 'binaries': continue
        try:
            path = checked_path(root, str(folder / 'event.json'))
            if path.stat().st_size > 64000: continue
            value = json.loads(path.read_text(encoding='utf-8'))
            if value.get('schema_version') != 1 or value.get('id') != folder.name: continue
            if any(not isinstance(value.get(key),str) for key in ('id','instance','reason','timestamp')): continue
            if type(value.get('pid')) is not int:continue
            if not isinstance(value.get('cores',[]),list) or any(not isinstance(name,str) for name in value.get('cores',[])):continue
            result.append(value)
            if len(result) >= 200: break
        except (OSError, ValueError, TypeError): continue
    return result


def inventory(root):
    root = Path(root)
    history = events(root)
    by_core = {name: event for event in history for name in event.get('cores', [])}
    cores, visited, limited = [], 0, False
    candidates = list(root.glob('*.core')) + list(root.glob('core.*')) + list(root.glob('*.dmp'))
    if (root/'core').is_file(): candidates.append(root/'core')
    for directory, folders, filenames in os.walk(root/'run', followlinks=False):
        base = Path(directory)
        folders[:] = [name for name in folders if name not in
            ('mariadb','editor-backups','manager-updates','deployments','binaries','quest-generations','mark')
            and not (base/name).is_symlink() and not (hasattr(base/name, 'is_junction') and (base/name).is_junction())]
        visited += len(filenames) + 1
        if visited > 10000:
            limited = True;break
        candidates.extend(base/name for name in filenames if name == 'core' or '.core' in name or
                          Path(name).suffix.lower() in ('.dmp', '.mdmp'))
    for path in candidates:
        try:
            checked_path(root, str(path))
            info = header(path)
            if info['format'] == 'ELF' and info['kind'] != 4: continue
            stat = path.stat()
            relative = path.relative_to(root).as_posix()
            event = by_core.get(relative)
            name = next((name for name in ('Database','LoginServer','GameServer') if path.name.startswith(name)), 'GameServer')
            binary = event.get('binary') if event else None
            cores.append(dict(path=relative, size=stat.st_size, modified=stat.st_mtime, mtime_ns=stat.st_mtime_ns, format=info['format'],
                event=event.get('id') if event else None, binary=binary or 'bin/'+name+('.exe' if os.name == 'nt' else '')))
        except (OSError, ValueError): continue
        if len(cores) >= 500: limited = True;break
    latest = history[0] if history else None
    return dict(events=history, cores=sorted(cores, key=lambda row: row['modified'], reverse=True),
                debuggers=debuggers(), limited=limited, latest=latest and
                dict(id=latest['id'], instance=latest['instance'], reason=latest['reason'], timestamp=latest['timestamp']))


def summary(root):
    history = events(root)
    latest = history[0] if history else None
    return dict(count=len(history), latest=latest and
        {key: latest[key] for key in ('id','instance','reason','timestamp')})


def debugger_command(core, binary, kind, available):
    if kind == 'minidump':
        if not available.get('cdb'):
            raise ValueError('Windows minidumphoz CDB (Debugging Tools for Windows) szükséges a célgépen.')
        return [available['cdb'], '-sins', '-y', str(binary.parent), '-i', str(binary.parent),
                '-z', str(core), '-c', '.ecxr;~*kb;q']
    if available.get('gdb'):
        return [available['gdb'], '-nx', '--batch', '-iex', 'set auto-load off',
            '-iex', 'set debuginfod enabled off', '-iex', 'set pagination off',
            '-iex', 'set print frame-arguments none', '--se', str(binary), '--core', str(core),
            '-ex', 'info threads', '-ex', 'thread apply all bt 80', '-ex', 'info sharedlibrary']
    if available.get('lldb'):
        return [available['lldb'], '--no-lldbinit', '--batch',
            '-O', 'settings set target.load-script-from-symbol-file false',
            '-O', 'settings set target.load-cwd-lldbinit false',
            '--file', str(binary), '--core', str(core), '-o', 'process status',
            '-o', 'thread list', '-o', 'thread backtrace all -c 80', '-o', 'image list']
    raise ValueError('A célgépen nincs GDB vagy LLDB. FreeBSD-n a Csomagkezelőben telepíthető: gdb.')


def analyze(root, core_name, binary_name, expected=None):
    core, binary = checked_path(root, core_name), checked_path(root, binary_name)
    before = core.stat()
    if expected and (before.st_size != expected.get('size') or before.st_mtime_ns != expected.get('mtime_ns')):
        raise ValueError('A core fájl módosult. Frissítsd a listát, és válaszd ki újra.')
    info = header(core)
    if info['format'] == 'ELF':
        executable = header(binary)
        if info['kind'] != 4: raise ValueError('A kiválasztott ELF fájl nem core dump.')
        if executable.get('kind') not in (2,3) or any(executable.get(key) != info[key] for key in ('bits','machine','endian')):
            raise ValueError('A bináris és a core formátuma vagy architektúrája eltér.')
    environment = dict(os.environ, LC_ALL='C', LANG='C', TERM='dumb', DEBUGINFOD_URLS='')
    if os.name != 'nt': environment['PATH'] = '/usr/local/bin:/usr/bin:/bin:/usr/local/sbin:/usr/sbin:/sbin'
    warnings = []
    relative = core.relative_to(Path(root)).as_posix() if core.is_relative_to(Path(root)) else str(core)
    matching = next((event for event in events(root) if relative in event.get('cores', [])), None)
    if matching:
        import hashlib
        with binary.open('rb') as stream: digest = hashlib.file_digest(stream, 'sha256').hexdigest()
        if digest != matching.get('binary_sha256'):
            raise ValueError('A bináris ellenőrzőösszege eltér az összeomláskor rögzítettől. Válaszd az archivált binárist.')
    else:
        warnings.append('Ehhez a core fájlhoz nincs rögzített bináris-ellenőrzőösszeg. A kiválasztott programnak ugyanabból a buildből kell származnia.')
    command = debugger_command(core, binary, info['format'], debuggers())
    with tempfile.TemporaryFile() as output:
        process = subprocess.Popen(command, cwd=binary.parent, stdin=subprocess.DEVNULL, stdout=output,
            stderr=subprocess.STDOUT, env=environment, start_new_session=os.name != 'nt',
            creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)
        deadline = time.monotonic() + 90
        try:
            while process.poll() is None:
                if time.monotonic() > deadline: raise TimeoutError('A core-elemzés időkorlátja lejárt.')
                if os.fstat(output.fileno()).st_size > MAX_REPORT: raise ValueError('Túl nagy debuggerkimenet; az elemzés leállt.')
                time.sleep(.05)
        finally:
            if process.poll() is None:
                if os.name != 'nt': os.killpg(process.pid, signal.SIGKILL)
                else: process.kill()
                process.wait()
        if os.fstat(output.fileno()).st_size > MAX_REPORT:
            raise ValueError('Túl nagy debuggerkimenet; szűkebb elemzés szükséges.')
        output.seek(0); text = output.read(MAX_REPORT).decode('utf-8', errors='replace')
    if process.returncode:
        raise ValueError('A debugger nem tudta elemezni a core fájlt.\n' + text[-12000:])
    after = core.stat()
    if (before.st_size,before.st_mtime_ns) != (after.st_size,after.st_mtime_ns):
        raise ValueError('Elemzés közben módosult a core fájl. Próbáld újra, amikor elkészült az írása.')
    return dict(core=str(core), binary=str(binary), debugger=Path(command[0]).name,
                report='\n'.join(warnings) + ('\n\n' if warnings else '') + text,
                checked_at=datetime.datetime.now(datetime.timezone.utc).isoformat())
