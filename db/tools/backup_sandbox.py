"""Owned Windows MariaDB sandbox and narrowly scoped Navicat backup reader."""
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import secrets
import socket
import subprocess
import time

from mariadb_client import Client, statements

ROOT = Path(__file__).resolve().parents[2]
ROLES = ('account', 'common', 'player', 'log')


class BackupError(RuntimeError):
    pass


def digest(path):
    with Path(path).open('rb') as handle:
        return hashlib.file_digest(handle, 'sha256').hexdigest()


def quote(name):
    if not re.fullmatch(r'[A-Za-z0-9_]+', name):
        raise BackupError('Unsupported SQL identifier')
    return '`' + name + '`'


def write_json(path, value):
    Path(path).write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')


def navicat_tables(path, destination):
    """Keep table DDL and literal INSERTs; never execute dump routines/triggers.

    Sections and SQL tokens are checked independently. Unexpected statements
    inside a table/data section fail closed rather than being silently ignored.
    The untouched original is the archive for unsupported objects and columns.
    """
    text = Path(path).read_text(encoding='utf-8-sig')
    headers = list(re.finditer(r'^-- (Table structure for|Records of|Procedure structure for|Triggers structure for table) ([^\n]+)$', text, re.M))
    if not headers:
        raise BackupError('Expected a Navicat UTF-8 table/data backup: ' + Path(path).name)
    tables, rows, excluded = [], {}, []
    with Path(destination).open('x', encoding='utf-8', newline='\n') as output:
        output.write("SET NAMES utf8mb4;\nSET SESSION sql_mode='NO_ENGINE_SUBSTITUTION';\nSET FOREIGN_KEY_CHECKS=0;\nSET autocommit=0;\n")
        for index, header in enumerate(headers):
            kind, name = header[1], header[2].strip()
            if kind not in ('Table structure for', 'Records of'):
                excluded.append(dict(kind=kind, name=name))
                continue
            quote(name)
            body = text[header.end():headers[index + 1].start() if index + 1 < len(headers) else len(text)]
            for statement in statements(body):
                if re.fullmatch(r'SET FOREIGN_KEY_CHECKS\s*=\s*1', statement, re.I):
                    continue
                drop = re.fullmatch(r'DROP TABLE IF EXISTS `([^`]+)`', statement, re.I)
                create = re.match(r'CREATE TABLE `([^`]+)`\s*\(', statement, re.I)
                insert = re.match(r'INSERT INTO `([^`]+)`\s*(?:\([^)]*\)\s*)?VALUES\s*\(', statement, re.I)
                if kind == 'Table structure for' and drop and drop[1] == name:
                    continue
                if kind == 'Table structure for' and create and create[1] == name:
                    if name in tables:
                        raise BackupError('Duplicate table definition: ' + name)
                    tables.append(name)
                elif kind == 'Records of' and insert and insert[1] == name and name in tables:
                    rows[name] = rows.get(name, 0) + 1
                else:
                    raise BackupError('Unexpected statement in backup section: ' + kind + ' ' + name)
                output.write(statement + ';\n')
        output.write('COMMIT;\nSET autocommit=1;\nSET FOREIGN_KEY_CHECKS=1;\n')
    return dict(tables=tables, insert_statements=rows, excluded_objects=excluded)


class Sandbox:
    """New work folder, random loopback port, private admin file, exact PID owner."""
    def __init__(self, work, binary_dir):
        self.work, self.binary_dir = Path(work).resolve(), Path(binary_dir).resolve()
        self.process = self.client = None
        self.password = secrets.token_urlsafe(32)
        self.record = None
        spec = importlib.util.spec_from_file_location('_backup_process_identity', ROOT / 'SRC/Tools/server_package/manage.py')
        self.manager = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(self.manager)

    def start(self):
        if os.name != 'nt':
            raise BackupError('The isolated backup adapter currently targets Windows')
        self.work.mkdir(parents=True, exist_ok=False)
        for name in ('mariadbd', 'mariadb', 'mariadb-admin', 'mariadb-install-db', 'mariadb-dump'):
            if not (self.binary_dir / (name + '.exe')).is_file():
                raise BackupError('Missing MariaDB executable: ' + name)
        with socket.socket() as listener:
            listener.bind(('127.0.0.1', 0))
            self.port = listener.getsockname()[1]
        self.data = self.work / 'data'
        self.cnf = self.work / 'admin.cnf'
        write_json(self.work / 'owner.json', dict(version=1, work=str(self.work), data=str(self.data), binary_dir=str(self.binary_dir), port=self.port))
        self.run('mariadb-install-db', ['--datadir=' + str(self.data), '--port=' + str(self.port)], 'initialize')
        self._launch('')
        self.client.query("ALTER USER 'root'@'localhost' IDENTIFIED BY '" + self.password + "'")
        self.cnf.write_text('[client]\nuser=root\npassword=' + self.password + '\n', encoding='utf-8')
        self.cnf.chmod(0o600)
        self.client.close()
        self.client = Client(self.port, library=str(self.binary_dir.parent / 'lib/libmariadb.dll'), password=self.password)
        self.configure_client()
        return self

    def resume(self):
        owner = json.loads((self.work / 'owner.json').read_text(encoding='utf-8'))
        self.data, self.cnf = self.work / 'data', self.work / 'admin.cnf'
        if owner.get('work') != str(self.work) or owner.get('data') != str(self.data) or owner.get('binary_dir') != str(self.binary_dir):
            raise BackupError('Sandbox owner paths do not match')
        previous = json.loads((self.work / 'process.json').read_text(encoding='utf-8'))
        if self.manager.owned(previous):
            raise BackupError('The sandbox is already running')
        self.port = owner['port']
        with socket.socket() as listener:
            listener.bind(('127.0.0.1', self.port))
        self.password = re.search(r'^password=([^\r\n]+)$', self.cnf.read_text(encoding='utf-8'), re.M)[1]
        self._launch(self.password)
        self.configure_client()
        return self

    def configure_client(self):
        self.client.query('SET NAMES utf8mb4')
        self.client.query("SET SESSION sql_mode='STRICT_ALL_TABLES,NO_ZERO_DATE,NO_ZERO_IN_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION'")

    def _launch(self, password):
        with (self.work / 'server.log').open('ab') as log:
            self.process = subprocess.Popen([str(self.binary_dir / 'mariadbd.exe'), '--defaults-file=' + str(self.data / 'my.ini'),
                                            '--bind-address=127.0.0.1', '--max-allowed-packet=64M',
                                            '--innodb-flush-log-at-trx-commit=2', '--console'],
                                           stdout=log, stderr=subprocess.STDOUT, creationflags=subprocess.CREATE_NO_WINDOW)
        self.record = self.manager.identity(self.process.pid)
        if not self.record:
            raise BackupError('Cannot identify the new MariaDB process')
        write_json(self.work / 'process.json', self.record)
        library = self.binary_dir.parent / 'lib/libmariadb.dll'
        self.dll_directory = os.add_dll_directory(str(library.parent))
        deadline = time.monotonic() + 40
        while time.monotonic() < deadline:
            if self.process.poll() is not None:
                raise BackupError('Sandbox database exited; inspect its private server.log')
            try:
                self.client = Client(self.port, library=str(library), password=password)
                break
            except RuntimeError:
                time.sleep(.15)
        if self.client is None:
            raise BackupError('Sandbox database startup timed out')
        self.verify()

    def verify(self):
        if not self.manager.owned(self.record):
            raise BackupError('Sandbox process ownership no longer matches')
        actual = self.client.query('SELECT @@datadir')[0][0]
        if Path(actual).resolve() != self.data:
            raise BackupError('SQL endpoint does not belong to the owned sandbox directory')

    def run(self, name, arguments, log_name, input_file=None, output_file=None):
        source = Path(input_file).open('rb') if input_file else None
        target = Path(output_file or self.work / (log_name + '.out.log')).open('xb')
        try:
            with (self.work / (log_name + '.error.log')).open('xb') as error:
                result = subprocess.run([str(self.binary_dir / (name + '.exe')), *map(str, arguments)],
                                        stdin=source, stdout=target, stderr=error, timeout=600,
                                        creationflags=subprocess.CREATE_NO_WINDOW)
            if result.returncode:
                raise BackupError('MariaDB action failed: ' + log_name + '; details are in its private error log')
        finally:
            target.close()
            if source:
                source.close()

    def cli_args(self):
        return ['--defaults-extra-file=' + str(self.cnf), '--protocol=tcp', '--host=127.0.0.1', '--port=' + str(self.port)]

    def import_file(self, path, database, label):
        self.verify()
        self.run('mariadb', [*self.cli_args(), '--default-character-set=utf8mb4', '--binary-mode', '--database=' + database], label, input_file=path)

    def stop(self):
        if self.process is not None and self.process.poll() is None:
            if not self.manager.owned(self.record):
                raise BackupError('Refusing to stop a process which no longer belongs to this sandbox')
            try:
                if self.client:
                    self.verify()
                    self.client.query('SHUTDOWN')
                self.process.wait(timeout=15)
            except (RuntimeError, subprocess.TimeoutExpired):
                if self.manager.owned(self.record):
                    self.process.terminate()
                    self.process.wait(timeout=10)
        if self.client:
            self.client.close()
            self.client = None
        if hasattr(self, 'dll_directory'):
            self.dll_directory.close()
