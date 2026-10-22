"""Build a console-free Qt6 Windows executable in a new output directory.

Install requirements.txt in a private virtual environment first. This builder
does not edit the original Server2 or a running server package.
"""
import argparse
import importlib.metadata
import os
from pathlib import Path
import shutil
import subprocess
import sys


def fix_windows_icu(bundle):
    # Qt's Windows build imports the OS ICU API. Dependency scanning can pick
    # the build Python's private, symbol-renamed ICU with the same DLL name.
    # Keep a compatible collected ICU; omit an incompatible shadow DLL so the
    # Windows loader uses System32, as it does in the working Qt environment.
    import pefile
    internal = bundle.resolve() / '_internal'
    core, collected = internal / 'PySide6/Qt6Core.dll', internal / 'icuuc.dll'
    if not core.is_file() or not collected.is_file():
        return
    with pefile.PE(str(core)) as module:
        icu = next((entry for entry in module.DIRECTORY_ENTRY_IMPORT if entry.dll.lower() == b'icuuc.dll'), None)
        required = {symbol.name for symbol in icu.imports if symbol.name} if icu else set()
    if not required:
        return
    with pefile.PE(str(collected)) as module:
        exported = {symbol.name for symbol in module.DIRECTORY_ENTRY_EXPORT.symbols}
    if required - exported:
        if not collected.resolve().is_relative_to(internal.resolve()):
            raise ValueError('Collected ICU must stay inside the new build output')
        collected.unlink()
        print('QT_ICU_FIXED: using the Windows system ICU API')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--work', type=Path, required=True)
    parser.add_argument('--validators', type=Path, required=True,
                        help='Directory containing ServerPackageCheck syntax-check binaries')
    args = parser.parse_args()
    source = Path(__file__).resolve().parent
    validators = args.validators.resolve()
    suffix = '.exe' if os.name == 'nt' else ''
    for name in ('ServerPackageCheck',):
        if not (validators / (name + suffix)).is_file():
            parser.error('Missing native syntax checker: ' + name)
    output, work = args.output.resolve(), args.work.resolve()
    if output.exists() or work.exists():
        parser.error('Output and work must be new directories')
    output.mkdir(parents=True)
    work.mkdir(parents=True)
    command = [sys.executable, '-m', 'PyInstaller', '--noconfirm', '--onedir', '--windowed',
               '--name', 'AurigaServerManager', '--distpath', str(output),
               '--workpath', str(work / 'cache'), '--specpath', str(work),
               '--paths', str(source), '--paths', str(source.parent),
               '--hidden-import', 'server_package.manage', '--hidden-import', 'run_server',
               '--hidden-import', 'ctypes.wintypes', '--exclude-module', 'PySide6.QtNetwork',
               '--add-data', str(source / 'remote_worker.py') + os.pathsep + '.',
               '--add-data', str(source / 'remote_deploy.py') + os.pathsep + '.',
               '--add-data', str(source / 'remote_packages.py') + os.pathsep + '.',
               '--add-data', str(source / 'server_target_worker.py') + os.pathsep + 'remote_support',
               '--add-data', str(source / 'backend.py') + os.pathsep + 'remote_support',
               '--add-data', str(source / 'editor_checks.py') + os.pathsep + 'remote_support',
               '--add-binary', str(validators / ('ServerPackageCheck' + suffix)) + os.pathsep + 'validators',
               '--exclude-module', 'PySide6.QtQml', '--exclude-module', 'PySide6.QtQuick',
               str(source / 'app.py')]
    code = subprocess.call(command, cwd=source)
    if code:
        return code
    fix_windows_icu(output / 'AurigaServerManager')
    licenses = output / 'AurigaServerManager/licenses'
    licenses.mkdir()
    shutil.copy2(source.parents[2] / 'SRC/3rd/LUA55/doc/readme.html', licenses / 'Lua-README.html')
    for name in ('PySide6-Essentials', 'shiboken6', 'pyinstaller'):
        distribution = importlib.metadata.distribution(name)
        destination = licenses / name
        destination.mkdir()
        (destination / 'METADATA.txt').write_text(distribution.read_text('METADATA') or '', encoding='utf-8')
        for relative in distribution.files or []:
            if '/licenses/' in relative.as_posix() or relative.name.upper().startswith(('LICENSE', 'COPYING')):
                original = Path(distribution.locate_file(relative))
                if original.is_file():
                    target = destination / relative.name
                    shutil.copy2(original, target)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
