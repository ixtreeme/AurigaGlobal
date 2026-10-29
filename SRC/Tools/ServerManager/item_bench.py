"""Local item test bench ("Tárgyteszt").

Drives the game's real native planners and transactional persistence writer
(ItemBenchTool) for ONE dedicated test character in the selected local
package's own database. It never reads or writes player.item or the players.
"""
import configparser
import datetime
import json
import os
from pathlib import Path
import secrets
import subprocess
import sys

import backend as api

BENCH_OWNER = 990001
# Child tables first: this is also the drop order of a failed install.
TABLES = ('item_occupied_cell', 'item_instance_socket', 'item_instance_attribute', 'item_instance', 'item_operation_receipt',
          'item_wallet', 'item_storage', 'item_owner_fence', 'item_persistence_format')
NO_WINDOW = subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0


def _bundled(*parts):
    return Path(getattr(sys, '_MEIPASS', Path(__file__).resolve().parent)).joinpath(*parts)


def helper():
    suffix = '.exe' if os.name == 'nt' else ''
    choices = [_bundled('validators', 'ItemBenchTool' + suffix),
               Path(__file__).resolve().parents[3] / 'build/SRC/ItemDomain/RelWithDebInfo' / ('ItemBenchTool' + suffix)]
    path = next((path for path in choices if path.is_file()), None)
    if path is None: raise api.EditorError('Hiányzik a natív tárgyteszt-eszköz. A szerverkezelő frissítése szükséges.')
    return path


def migration():
    choices = [_bundled('migrations', '001_item_persistence.sql'),
               Path(__file__).resolve().parents[3] / 'db/migrations/item_instance/001_item_persistence.sql']
    path = next((path for path in choices if path.is_file()), None)
    if path is None: raise api.EditorError('Hiányzik a mentési táblák telepítőfájlja. A szerverkezelő frissítése szükséges.')
    return path


def run_tool(payload, executable=None):
    result = subprocess.run([str(executable or helper())], input=json.dumps(payload, ensure_ascii=True), capture_output=True,
                            text=True, encoding='utf-8', timeout=120, creationflags=NO_WINDOW)
    lines = [line.removeprefix('AURIGA_BENCH_RESULT ') for line in result.stdout.splitlines() if line.startswith('AURIGA_BENCH_RESULT ')]
    if len(lines) != 1: raise api.EditorError('A tárgyteszt-eszköz nem adott érvényes választ. Frissítsd az állapotot.')
    packet = api.strict_json(lines[0])
    if not packet.get('ok'): raise api.EditorError(packet.get('error') or 'A tárgyteszt-művelet sikertelen.')
    if result.returncode: raise api.EditorError('A tárgyteszt-művelet nem igazolt; frissítsd az állapotot.')
    return packet['result']


def connection(root):
    """Admin connection of the package's OWN local database, after the same
    ownership checks as the item editor."""
    root = api.package_root(root); manager = api.load_manager(root)
    owner = manager.read_json(root / 'run/mariadb/owner.json')
    if not manager.owned(manager.read_json(root / 'run/mariadb/process.json')): raise api.EditorError('A helyi tesztadatbázis nem fut.')
    try:
        manager.verify_datadir()
    except RuntimeError as error:
        raise api.EditorError('A helyi adatbázis ellenőrzése sikertelen: ' + str(error)) from None
    config = manager.config()['common']['sql']['player']
    if config['host'] != '127.0.0.1' or config['port'] != owner['port']: raise api.EditorError('Csak a kiválasztott csomag saját helyi adatbázisa kezelhető.')
    private = configparser.ConfigParser(interpolation=None); private.read(root / 'run/mariadb/admin.cnf', encoding='utf-8')
    return root, manager, owner, dict(port=owner['port'], user=private['client']['user'], password=private['client']['password'])


def call(root, action, **fields):
    auth = connection(root)[3]
    return run_tool(dict(fields, action=action, connection=auth))


def _client(owner, root, *extra, **options):
    binaries = Path(owner['binary_dir'])
    args = ['--defaults-extra-file=' + str(root / 'run/mariadb/admin.cnf'), '--protocol=tcp', '--host=127.0.0.1', '--port=' + str(owner['port'])]
    return subprocess.run([str(binaries / 'mariadb.exe' if os.name == 'nt' else binaries / 'mariadb'), *args, *extra],
                          capture_output=True, timeout=300, creationflags=NO_WINDOW, **options)


def install(root):
    """Create the nine additive item persistence tables in the local `player`
    database. The game server must be stopped; `player` is dumped first and a
    failed install removes exactly the tables it created."""
    root, manager, owner, _ = connection(root)
    with manager.supervisor_lock():
        if api.describe(root)['running']: raise api.EditorError('A mentési táblák telepítéséhez állítsd le a helyi játékszervert.')
        if call(root, 'status')['installed']: return dict(installed=True, already=True)
        names = ','.join("'" + name + "'" for name in TABLES)
        found = _client(owner, root, '-N', '-B', '-e', "SELECT TABLE_NAME FROM information_schema.TABLES WHERE TABLE_SCHEMA='player' AND TABLE_NAME IN (" + names + ")")
        if found.returncode: raise api.EditorError('A helyi adatbázis nem olvasható.')
        if found.stdout.strip(): raise api.EditorError('Részben telepített mentési táblák vannak a player adatbázisban; kézi ellenőrzés szükséges.')
        stamp = datetime.datetime.now().strftime('%Y%m%d-%H%M%S-') + secrets.token_hex(4)
        folder = root / 'run/item-bench' / stamp; folder.mkdir(parents=True)
        binaries = Path(owner['binary_dir'])
        dump = folder / 'player-before.sql'
        with dump.open('wb') as out:
            result = subprocess.run([str(binaries / ('mariadb-dump.exe' if os.name == 'nt' else 'mariadb-dump')),
                                     '--defaults-extra-file=' + str(root / 'run/mariadb/admin.cnf'), '--protocol=tcp', '--host=127.0.0.1',
                                     '--port=' + str(owner['port']), '--single-transaction', '--databases', 'player'],
                                    stdout=out, stderr=subprocess.PIPE, timeout=600, creationflags=NO_WINDOW)
        if result.returncode: raise api.EditorError('A player adatbázis mentése nem sikerült; semmi nem változott.')
        script = migration()
        with script.open('rb') as source:
            applied = _client(owner, root, stdin=source)
        if applied.returncode:
            _client(owner, root, '-e', 'DROP TABLE IF EXISTS ' + ','.join('player.' + name for name in TABLES))
            raise api.EditorError('A mentési táblák telepítése nem sikerült; a létrehozott táblák törölve. ' +
                                  applied.stderr.decode('utf-8', 'replace').strip()[:300])
        if not call(root, 'status')['installed']: raise api.EditorError('A telepítés után a mentési táblák nem teljesek.')
        (folder / 'receipt.json').write_text(json.dumps(dict(stamp=stamp, dump=dump.name, migration=script.name), indent=2), encoding='utf-8')
        return dict(installed=True, folder=str(folder))
