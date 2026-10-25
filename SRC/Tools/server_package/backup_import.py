"""Restore a validated adapted backup into a pristine owned localhost package."""
import datetime
import hashlib
import json
import os
from pathlib import Path
import re
import secrets
import subprocess

ROLES = ('account', 'common', 'player', 'log')
SEED_TABLES = {'common.locale', 'player.mob_motion', 'player.mob_skill_splash'}


def digest(path):
    with Path(path).open('rb') as handle:
        return hashlib.file_digest(handle, 'sha256').hexdigest()


def qualified(name):
    if not re.fullmatch(r'(account|common|player|log)\.[A-Za-z0-9_]+', name):
        raise RuntimeError('Unexpected backup table identifier')
    return '.'.join('`' + part + '`' for part in name.split('.'))


def validate_backup(folder, schema):
    folder = Path(folder).resolve()
    manifest = json.loads((folder / 'manifest.json').read_text(encoding='utf-8'))
    report = json.loads((folder / 'report.json').read_text(encoding='utf-8'))
    if manifest.get('version') != 1 or report.get('version') != 1:
        raise RuntimeError('Unsupported adapted backup format')
    required = {'schema.sql', 'report.json', 'auto-increment.sql', *('data/' + role + '.sql' for role in ROLES)}
    if not required.issubset(manifest['files']):
        raise RuntimeError('Incomplete adapted backup')
    for relative, expected in manifest['files'].items():
        path = (folder / relative).resolve()
        if not path.is_relative_to(folder) or path.is_symlink() or not path.is_file() or digest(path) != expected:
            raise RuntimeError('Adapted backup content differs from its manifest')
    if digest(schema) != report['schema_sha256'] or digest(folder / 'schema.sql') != report['schema_sha256']:
        raise RuntimeError('Package schema differs from the validated adapted backup')
    counts, checksums = report.get('target_counts', {}), report.get('table_checksums', {})
    if len(counts) != 102 or set(counts) != set(checksums):
        raise RuntimeError('Expected verification results for all 102 tables')
    for name, count in counts.items():
        qualified(name)
        if type(count) is not int or count < 0 or not str(checksums[name]).isdigit():
            raise RuntimeError('Invalid table verification result')
    if any(row[1] != 'READY' for row in report['readiness']) or report['byte_checks']['active_character_slots'] != 4:
        raise RuntimeError('Backup did not pass readiness/four-slot verification')
    sequence = (folder / 'auto-increment.sql').read_text(encoding='utf-8')
    for line in sequence.splitlines():
        if line and not re.fullmatch(r'ALTER TABLE `(account|common|player|log)`\.`[A-Za-z0-9_]+` AUTO_INCREMENT=[1-9][0-9]*;', line):
            raise RuntimeError('Unexpected auto-increment restore statement')
    return folder, report


def restore_file(manager, path, log, database=None):
    folder = manager.db_folder()
    owner = manager.read_json(folder / 'owner.json')
    binary = Path(owner['binary_dir']) / ('mariadb.exe' if os.name == 'nt' else 'mariadb')
    argv = [str(binary), '--defaults-extra-file=' + str(folder / 'admin.cnf'), '--protocol=tcp',
            '--host=127.0.0.1', '--port=' + str(owner['port']), '--default-character-set=utf8mb4',
            '--binary-mode', '--max-allowed-packet=64M']
    if database:
        if database not in ROLES:
            raise RuntimeError('Unexpected restore database')
        argv += ['--database=' + database]
    manager.verify_datadir()
    with Path(path).open('rb') as source, Path(str(log) + '.out.log').open('xb') as out, Path(str(log) + '.error.log').open('xb') as error:
        result = subprocess.run(argv, stdin=source, stdout=out, stderr=error, timeout=600,
                                creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)
    if result.returncode:
        raise RuntimeError('Backup restore failed; SQL diagnostics remain in the private import log')


def import_backup(manager, folder):
    root, cfg = manager.ROOT, manager.config()
    folder, report = validate_backup(folder, root / 'db/schema.sql')
    if cfg['common'].get('locale') != 'utf8mb4' or cfg['common'].get('table_postfix', ''):
        raise RuntimeError('Adapted backup requires utf8mb4 connections and an empty table postfix')
    if any(cfg['common']['sql'][role]['database'] != role for role in ROLES):
        raise RuntimeError('Adapted backup database names differ from this localhost package')
    state = manager.db_folder()
    if not (state / 'initialized.json').is_file():
        raise RuntimeError('Run init-db before importing the adapted backup')
    if (state / 'import-in-progress.json').exists():
        raise RuntimeError('An earlier import was interrupted; inspect its import journal before continuing')
    with manager.supervisor_lock():
        stack = root / 'run/supervisor.json'
        if stack.is_file() and any(manager.owned(child['identity']) for child in manager.read_json(stack)['children']):
            raise RuntimeError('Stop the owned server stack before importing a backup')
        started = manager.start_db()
        mutated, before, journal = False, None, None
        try:
            manager.verify_datadir()
            mission = manager.mission_module()
            target, _ = mission.connection(cfg)
            name = target['database']
            present = manager.sql("SELECT COUNT(*) FROM information_schema.TABLES WHERE TABLE_SCHEMA='" + name +
                                  "' AND TABLE_NAME='mission_engine_characters';").strip()
            if present != '0' and int(manager.sql('SELECT COUNT(*) FROM ' + mission.identifier(name) +
                                                  '.mission_engine_characters;').strip()):
                raise RuntimeError('Mission progress exists; restore a coherent player and mission SQL backup together')
            actual = set(line.replace('\t', '.') for line in manager.sql("SELECT TABLE_SCHEMA,TABLE_NAME FROM information_schema.TABLES WHERE TABLE_SCHEMA IN ('account','common','player','log');").splitlines())
            if actual != set(report['target_counts']):
                raise RuntimeError('Local database tables differ from the adapted schema')
            for name in sorted(actual - SEED_TABLES):
                if int(manager.sql('SELECT COUNT(*) FROM ' + qualified(name) + ';').strip()):
                    raise RuntimeError('The local database contains data already; import into a new owned package')
            seed_keys = set(manager.sql('SELECT mKey FROM common.locale;').splitlines())
            if not seed_keys.issubset({'DB_NAME_COLUMN', 'SKILL_POWER_BY_LEVEL'}):
                raise RuntimeError('Local locale data differs from a fresh initialization')
            stamp = datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S-') + secrets.token_hex(4)
            logs = root / 'run/database-imports' / stamp
            logs.mkdir(parents=True)
            owner = manager.read_json(state / 'owner.json')
            binary = Path(owner['binary_dir']) / ('mariadb-dump.exe' if os.name == 'nt' else 'mariadb-dump')
            before = logs / 'before-import.sql'
            argv = [str(binary), '--defaults-extra-file=' + str(state / 'admin.cnf'), '--protocol=tcp', '--host=127.0.0.1',
                    '--port=' + str(owner['port']), '--single-transaction', '--skip-lock-tables', '--hex-blob',
                    '--default-character-set=utf8mb4', '--databases', *ROLES]
            with before.open('xb') as out, (logs / 'before-import.error.log').open('xb') as error:
                result = subprocess.run(argv, stdout=out, stderr=error, timeout=180,
                                        creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)
            if result.returncode:
                raise RuntimeError('Could not back up the pristine local database')
            journal = dict(version=1, backup=str(folder), log_directory=str(logs), before_sha256=digest(before), phase='IMPORTING')
            manager.save_json(state / 'import-in-progress.json', journal)
            mutated = True
            manager.sql('DELETE FROM common.locale;')
            for role in ROLES:
                restore_file(manager, folder / 'data' / (role + '.sql'), logs / role, role)
            restore_file(manager, folder / 'auto-increment.sql', logs / 'auto-increment')
            for name, expected in report['target_counts'].items():
                if name in ('player.mob_motion', 'player.mob_skill_splash') and expected == 0:
                    continue  # Keep the package's 1101 baked motion records.
                count = int(manager.sql('SELECT COUNT(*) FROM ' + qualified(name) + ';').strip())
                checksum = manager.sql('CHECKSUM TABLE ' + qualified(name) + ' EXTENDED;').strip().split('\t')[-1]
                if count != expected or checksum != report['table_checksums'][name]:
                    raise RuntimeError('Imported table verification differs: ' + name)
            if 'pid5' in manager.sql("SELECT COLUMN_NAME FROM information_schema.COLUMNS WHERE TABLE_SCHEMA='player' AND TABLE_NAME='player_index';").splitlines():
                raise RuntimeError('An unexpected fifth character slot remains')
            rows = manager.sql((root / 'db/check_ready.sql').read_text(encoding='utf-8'))
            if any(line and not line.endswith('\tREADY') for line in rows.splitlines()):
                raise RuntimeError('Imported reference data did not pass readiness checks')
            journal.update(phase='COMPLETE', verified_tables=102, report_sha256=digest(folder / 'report.json'))
            manager.save_json(logs / 'result.json', journal)
            manager.save_json(state / 'imported-backup.json', journal)
            (state / 'import-in-progress.json').unlink()
            print('LOCAL_BACKUP_IMPORTED tables=102 active_character_slots=4 verified_counts_and_checksums=yes')
        except BaseException:
            if mutated:
                try:
                    if digest(before) != journal['before_sha256']:
                        raise RuntimeError('Rollback snapshot hash differs')
                    restore_file(manager, before, logs / 'rollback')
                    journal['phase'] = 'ROLLED_BACK'
                    manager.save_json(logs / 'result.json', journal)
                    (state / 'import-in-progress.json').unlink()
                except BaseException:
                    journal['phase'] = 'ROLLBACK_FAILED'
                    manager.save_json(state / 'import-in-progress.json', journal)
            raise
        finally:
            if started:
                manager.stop_db()
