"""Build a new localhost distribution; never modify or replace the input server."""
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import zipfile
from server_package.data import DataBuilder, digest, validate_data, write_json
from server_package.legacy import PackageError, central_config

REPO = Path(__file__).resolve().parents[2]


def run(argv, cwd=None):
    result = subprocess.run([str(a) for a in argv], cwd=cwd, capture_output=True, text=True,
                            creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)
    if result.returncode:
        # Native preflight never prints SQL credentials. Keep detailed diagnostics in the package log.
        raise PackageError('Command failed: ' + Path(argv[0]).name + '\n' + result.stdout + result.stderr)
    return result.stdout + result.stderr


def binary(build, name):
    filename = name + ('.exe' if os.name == 'nt' else '')
    component = name if name in ('Database','LoginServer') else 'GameServer'
    for path in (build / filename, build / 'SRC/Server' / component / 'RelWithDebInfo' / filename,
                 build / 'SRC/Server' / component / filename):
        if path.is_file(): return path
    raise PackageError('Missing current build executable: ' + name)


def package_files(root):
    for path in sorted(root.rglob('*')):
        relative = path.relative_to(root)
        if relative.parts[0] == 'run' or path.name in ('package-manifest.json', 'server-files-ready.json'):
            continue
        if path.is_file(): yield relative.as_posix(), path


def create_archive(root, archive):
    root, archive = root.resolve(), archive.resolve()
    if not (root / '.auriga-package.json').is_file(): raise PackageError('Not an owned server package')
    source = Path(json.loads((root / '.auriga-package.json').read_text(encoding='utf-8'))['source']).resolve()
    if archive.is_relative_to(root) or archive.is_relative_to(source):
        raise PackageError('Archive must be outside the package and original server directories')
    sidecar = archive.with_name(archive.name + '.sha256')
    if archive.exists() or sidecar.exists(): raise FileExistsError('Archive or checksum already exists')
    manifest = json.loads((root / 'package-manifest.json').read_text(encoding='utf-8'))
    members = list(package_files(root))
    if set(manifest['files']) != {relative for relative, _ in members}:
        raise PackageError('Package files differ from the validated manifest')
    for relative, path in members:
        if path.is_symlink(): raise PackageError('Package archive cannot include linked files')
        if manifest['files'].get(relative) != digest(path):
            raise PackageError('Package changed after validation: ' + relative)
    with zipfile.ZipFile(archive, 'x', compression=zipfile.ZIP_DEFLATED, compresslevel=6) as output:
        for relative, path in members: output.write(path, root.name + '/' + relative)
        output.write(root / 'package-manifest.json', root.name + '/package-manifest.json')
    with zipfile.ZipFile(archive) as check:
        if check.testzip() is not None: raise PackageError('Archive CRC verification failed')
    checksum = digest(archive)
    # Exclusive sidecar creation avoids replacing a user's existing artifact.
    with sidecar.open('x', encoding='ascii') as handle:
        handle.write(checksum + '  ' + archive.name + '\n')
    print('ARCHIVE_READY:', archive)
    return checksum


def tls(output):
    openssl = shutil.which('openssl')
    if not openssl and Path('C:/Program Files/Git/usr/bin/openssl.exe').is_file():
        openssl = 'C:/Program Files/Git/usr/bin/openssl.exe'
    if not openssl: raise PackageError('OpenSSL executable is required to create a localhost TLS certificate')
    folder = output / 'tls'; folder.mkdir()
    run([openssl, 'req', '-x509', '-newkey', 'rsa:3072', '-sha256', '-nodes', '-days', '365',
         '-keyout', folder / 'server.key', '-out', folder / 'server.crt', '-subj', '/CN=localhost',
         '-addext', 'subjectAltName=DNS:localhost,IP:127.0.0.1', '-addext', 'basicConstraints=critical,CA:FALSE'])
    os.chmod(folder / 'server.key', 0o600)
    pem = (folder / 'server.crt').read_text(encoding='ascii')
    import ssl
    return hashlib.sha256(ssl.PEM_cert_to_DER_cert(pem)).hexdigest()


def build(source, output, build_dir, sql_port=3307):
    source, output, build_dir = source.resolve(), output.resolve(), build_dir.resolve()
    if not (source / 'db/conf.txt').is_file() and (source / 'srv1').is_dir(): source /= 'srv1'
    if not (source / 'db/conf.txt').is_file(): raise PackageError('Source needs srv1/db/conf.txt')
    if output == source or output in source.parents or source in output.parents:
        raise PackageError('Output must be a separate directory outside the original server')
    if not 1 <= sql_port <= 65535: raise PackageError('Invalid SQL port')
    output.mkdir(parents=False, exist_ok=False)
    write_json(output / '.auriga-package.json', dict(version=1, source=str(source), created=datetime.datetime.now(datetime.timezone.utc).isoformat()))
    print('Preparing new package:', output, flush=True)
    config, config_report = central_config(source, sql_port)
    write_json(output / 'server.json', config)
    os.chmod(output / 'server.json', 0o600)
    data = DataBuilder(source, output, binary(build_dir, 'ServerPackageCheck'))
    for path in [source / 'db/conf.txt', source / 'auth/CONFIG'] + sorted((source / 'chan').glob('ch*/core*/CONFIG')): data.track(path)
    print('Converting data and quest resources...', flush=True)
    paths = data.convert()
    checks = validate_data(output, config)
    bin_dir = output / 'bin'; bin_dir.mkdir()
    for name in ('Database','LoginServer','GameServer','ServerPackageCheck'):
        original = binary(build_dir, name)
        shutil.copy2(original, bin_dir / original.name)
        for dll in original.parent.glob('*.dll'):
            existing = bin_dir / dll.name
            if existing.exists() and digest(existing) != digest(dll):
                raise PackageError('Conflicting executable runtime DLL: ' + dll.name)
            shutil.copy2(dll, existing)
    print('Creating local TLS and management commands...', flush=True)
    fingerprint = tls(output)
    # Runtime tools are self-contained; no absolute source-tree path is needed.
    shutil.copy2(REPO / 'SRC/Tools/run_server.py', output / 'run_server.py')
    shutil.copy2(REPO / 'SRC/Tools/server_package/manage.py', output / 'manage.py')
    shutil.copy2(REPO / 'SRC/Tools/server_package/quest_reload.py', output / 'quest_reload.py')
    shutil.copy2(REPO / 'SRC/Tools/server_package/mission_database.py', output / 'mission_database.py')
    shutil.copy2(REPO / 'SRC/Tools/server_package/backup_import.py', output / 'db/backup_import.py')
    for name in ('schema.sql','seed_core.sql','check_ready.sql','mission_schema.sql'):
        shutil.copy2(REPO / 'db' / name, output / 'db' / name)
    shutil.copytree(REPO / 'db/migrations', output / 'db/migrations')
    # The live skill curve wins over the example seed's curve.
    skill_source = source / 'share/locale/germany/skill_power.txt'
    if skill_source.is_file():
        data.track(skill_source)
        for line in skill_source.read_text(encoding='latin1').splitlines():
            parts = line.split()
            if len(parts) > 1 and parts[1].lower() == 'ymir':
                numbers = [int(n) for n in parts[2:]]
                if len(numbers) == 40: numbers.insert(0, 0)
                if len(numbers) != 41: raise PackageError('Live skill-power curve must contain 41 numbers')
                (output / 'db/live_locale.sql').write_text("UPDATE `common`.`locale` SET `mValue`='" + ' '.join(map(str, numbers)) + "' WHERE `mKey`='SKILL_POWER_BY_LEVEL';\n", encoding='utf-8')
                break
    shutil.copy2(REPO / 'docs/server_configuration.md', output / 'server_configuration.md')
    shutil.copy2(REPO / 'docs/server2_package.md', output / 'server2_package.md')
    shutil.copy2(REPO / 'docs/lua55_quest_migration.md', output / 'lua55_quest_migration.md')
    (output / 'config').mkdir()
    (output / 'schemas').mkdir()
    shutil.copy2(REPO / 'config/server.json.example', output / 'config/server.json.example')
    shutil.copy2(REPO / 'docs/schemas/server.schema.json', output / 'schemas/server.schema.json')
    py = str(Path(sys.executable).resolve())
    py_literal = py.replace("'", "''")
    wrapper = ('$ErrorActionPreference = \'Stop\'\nSet-Location -LiteralPath $PSScriptRoot\n'
               '$python = if (Test-Path -LiteralPath \'%s\') { \'%s\' } else { \'python\' }\n'
               '& $python -B (Join-Path $PSScriptRoot \'manage.py\') @args\nexit $LASTEXITCODE\n') % (py_literal, py_literal)
    (output / 'server.ps1').write_text(wrapper, encoding='utf-8-sig')
    (output / 'server.cmd').write_text('@echo off\r\npowershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0server.ps1" %*\r\n', encoding='ascii')
    channel_rows = []
    for name, core in config['game_servers'].items():
        if core['server']['channel'] == 99 or any(row['Id'] == core['server']['channel'] for row in channel_rows): continue
        channel_rows.append(dict(Id=core['server']['channel'], Name='Channel ' + str(core['server']['channel']), Port=core['network']['port']))
    write_json(output / 'client-connection.json', dict(Host='127.0.0.1', AuthPort=config['login']['port'], GamePort=channel_rows[0]['Port'],
               MarkPort=channel_rows[0]['Port'], ClientVersion='20251204', Channels=channel_rows,
               Tls=dict(ServerName='localhost', PinnedCertificateSha256=fingerprint)))
    print('Checking all native instances and data...', flush=True)
    native = run([sys.executable, '-B', output / 'manage.py', 'prepare'], cwd=output.parent)
    (output / 'preflight.log').write_text(native, encoding='utf-8')
    data.verify_originals()
    report = dict(status='SERVER_FILES_READY', database_stage='DEFERRED', source=str(source), output=str(output),
                  configuration=config_report, data=checks, paths=paths, notes=data.notes,
                  input_files=data.inputs, operations=data.operations,
                  certificate_sha256=fingerprint,
                  database_dump_found=False, database_import_required=True,
                  excluded=['old executables/core dumps/logs/PIDs','unused locale backup/map_0 archives',
                            'raw client graphics and MSA (motions baked into db/motion.sql)'])
    write_json(output / 'conversion-report.json', report)
    (output / 'README.md').write_text(readme(output, config_report, checks, paths), encoding='utf-8')
    files = {}
    for relative, path in package_files(output): files[relative] = digest(path)
    write_json(output / 'package-manifest.json', dict(version=1, files=files))
    print(native, end='')
    print('SERVER_PACKAGE_READY:', output)
    return report


def readme(output, configuration, checks, paths):
    return r'''# Auriga Server2 – localhost csomag

A Server2 eredeti példánya változatlan. Ez a csomag a módosított C++ szerverhez,
Windowsos localhost futtatáshoz készült, új TLS-kulccsal és külön helyi SQL-jelszóval.
Az összes szerverbeállítás a **server.json** fájlban található.

Az összeállított szerverfájlok előkészítése és ellenőrzése:

```powershell
.\server.ps1 prepare
.\server.ps1 check
.\server.ps1 status
```

A `prepare` mind a 15 szerverpéldány futási mappáját, alapállományait és guild
képmappáit előkészíti, majd `SERVER_FILES_READY` eredményt ad. Meglévő futási
állományt nem ír felül. Ez a lépés nem nyit SQL-kapcsolatot és nem indít szolgáltatásokat.
Új helyre kicsomagolás után is ugyanez a parancs készíti elő a futási mappákat.

Később, az adatbázis illesztésekor használható parancsok:

```powershell
.\server.ps1 init-db
.\server.ps1 database-check
.\server.ps1 start --channel 1
.\server.ps1 stop
.\server.ps1 stop-db
```

A `server.cmd` ugyanezeket az argumentumokat fogadja el. A Pythonos megfelelő:
`python -B manage.py <parancs>`.

Az `init-db` kizárólag a csomag saját, új MariaDB-adatkönyvtárát hozza létre.
Az adatbázis alap sémáját, a live skillgörbét és a mozgástáblákat automatikusan
betölti. Létező adatkönyvtárat nem inicializál újra, Windows-szolgáltatást nem módosít.
Az `init-db` után illesztett referencia- és játékosadatok szükségesek.
Ha a csomag `db/backup` alatt ellenőrzött mentést is tartalmaz, a `setup-db`
az inicializálást és az importot egyben végzi el. A `database-check` az SQL-adatok
készültségét ellenőrzi. Az alap fájlcsomag önmagában nem tartalmaz játékosadatokat.

A `start` csak sikeres fájl-/TLS-/adatbázis-ellenőrzés után indít: Database,
kijelölt game core-ok, majd LoginServer. Konzolos felügyelet alatt fut; Ctrl+C
vagy másik konzolból `stop` kéri a szerverek leállását. Hiba esetén a saját
elindított gyerekfolyamatokat leállítja. A naplók core-onként a `run` alatt vannak.

A `client-connection.json` a Unity-profilhoz szükséges nyilvános végpontokat és
a localhost tanúsítvány SHA256 pinjét tartalmazza; SQL-jelszó nincs benne.
Az Editorban ezekhez igazítandó a Metin2ServerConnection profil, a TLS pin formátumát
a kliens `Metin2TlsOptions` határozza meg. Az új kulcs önaláírt localhost tanúsítványhoz tartozik.

A `conversion-report.json` az eredeti fájlok SHA256 értékeit, a konverziókat és a
feloldatlan questadat-hivatkozásokat tartalmazza. A `preflight.log` a natív
konfiguráció-/JSON-/Lua-szintaktikai ellenőrzés eredménye. Ez még nem teljes játékpróba.
A `data.unindexed_core_maps` jelzi a mentés régi MAP_ALLOW bejegyzéseit, amelyekhez
nincs mapindex-bejegyzés. Ezeket a listákban megőriztük; mapadatot nem találtunk ki.
A CH2–CH4 core1-ben a 11-es map, a CH5–CH6-ban további régi mapok érintettek.
A CH1 és a közös CH99 minden engedélyezett mapja megtalálható.

Az előkészítő új kimeneti mappával ismételhető; a forrásmentés és ez a csomag
nem kerül automatikusan felülírásra. A FreeBSD-n futó régi binárisok helyett itt
Windowsos programok vannak; más rendszerre saját build kell.

Összesítés: %d core, %d indexelt map, %d JSON.
Feloldatlan Lua-erőforrások: %d (részletek a jelentésben).
''' % (configuration['core_count'], checks['indexed_maps'], checks['json_files'], len(paths['unresolved_lua_resources']))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--build-dir', type=Path, default=REPO / 'build')
    parser.add_argument('--sql-port', type=int, default=3307)
    parser.add_argument('--archive', type=Path, help='Optional new ZIP output path')
    parser.add_argument('--manager-dir', type=Path, help='Optional built Qt6 manager bundle to include')
    args = parser.parse_args()
    try:
        build(args.source, args.output, args.build_dir, args.sql_port)
        if args.manager_dir:
            from ServerManager.install_manager import install
            install(args.output, args.manager_dir)
        if args.archive: create_archive(args.output, args.archive)
    except PackageError as error:
        print('PACKAGE_FAILED:', error, file=sys.stderr); return 1
    except (OSError, ValueError, KeyError, TypeError):
        print('PACKAGE_FAILED: invalid input/layout or unavailable output; source values are not printed. The original server was not modified.', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
