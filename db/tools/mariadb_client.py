"""Small stdlib-only MariaDB C client for isolated schema tests (no credentials in logs)."""
import ctypes as c
import ctypes.util
import os
import re


def statements(text):
    token = re.compile(r"'(?:''|\\.|[^'\\])*'|`(?:``|[^`])*`|\"(?:\\.|[^\"\\])*\"|--[^\n]*|/\*[\s\S]*?\*/|;")
    start = 0
    parts = []
    for match in token.finditer(text):
        if match[0] == ';':
            parts.append(text[start:match.start()])
            start = match.end()
    parts.append(text[start:])
    for part in parts:
        cleaned = token.sub(lambda m: '' if m[0].startswith(('--', '/*')) else m[0], part).strip()
        if cleaned: yield cleaned


class Client:
    def __init__(self, port, database=None, library=None, user='root', password=''):
        library = library or os.environ.get('AURIGA_MARIADB_LIBRARY') or c.util.find_library('mariadb')
        if not library: raise RuntimeError('Set AURIGA_MARIADB_LIBRARY to libmariadb.dll/libmariadb.so')
        self.lib = c.CDLL(library)
        functions = {
            'mysql_init': (c.c_void_p, [c.c_void_p]),
            'mysql_real_connect': (c.c_void_p, [c.c_void_p, c.c_char_p, c.c_char_p, c.c_char_p, c.c_char_p, c.c_uint, c.c_char_p, c.c_ulong]),
            'mysql_query': (c.c_int, [c.c_void_p, c.c_char_p]),
            'mysql_error': (c.c_char_p, [c.c_void_p]),
            'mysql_errno': (c.c_uint, [c.c_void_p]),
            'mysql_store_result': (c.c_void_p, [c.c_void_p]),
            'mysql_num_fields': (c.c_uint, [c.c_void_p]),
            'mysql_fetch_row': (c.POINTER(c.c_void_p), [c.c_void_p]),
            'mysql_fetch_lengths': (c.POINTER(c.c_ulong), [c.c_void_p]),
            'mysql_free_result': (None, [c.c_void_p]),
            'mysql_close': (None, [c.c_void_p]),
            'mysql_select_db': (c.c_int, [c.c_void_p, c.c_char_p]),
            'mysql_stmt_init': (c.c_void_p, [c.c_void_p]),
            'mysql_stmt_prepare': (c.c_int, [c.c_void_p, c.c_char_p, c.c_ulong]),
            'mysql_stmt_error': (c.c_char_p, [c.c_void_p]),
            'mysql_stmt_close': (c.c_int, [c.c_void_p]),
            'mysql_stmt_field_count': (c.c_uint, [c.c_void_p]),
            'mysql_stmt_param_count': (c.c_ulong, [c.c_void_p]),
        }
        for name, (result, args) in functions.items():
            fn = getattr(self.lib, name)
            fn.restype, fn.argtypes = result, args
        self.handle = self.lib.mysql_init(None)
        if not self.handle: raise RuntimeError('mysql_init failed')
        if not self.lib.mysql_real_connect(self.handle, b'127.0.0.1', user.encode('utf-8'), password.encode('utf-8'), database.encode() if database else None, port, None, 0):
            error = self.lib.mysql_error(self.handle).decode()
            self.close()
            raise RuntimeError(error)

    def query(self, sql):
        if self.lib.mysql_query(self.handle, sql.encode('utf-8')):
            raise RuntimeError(str(self.lib.mysql_errno(self.handle)) + ': ' + self.lib.mysql_error(self.handle).decode())
        result = self.lib.mysql_store_result(self.handle)
        rows = []
        if result:
            try:
                count = self.lib.mysql_num_fields(result)
                while True:
                    row = self.lib.mysql_fetch_row(result)
                    if not row: break
                    lengths = self.lib.mysql_fetch_lengths(result)
                    rows.append([c.string_at(row[i], lengths[i]).decode('utf-8', errors='replace') if row[i] else None for i in range(count)])
            finally: self.lib.mysql_free_result(result)
        return rows

    def prepare(self, database, sql):
        if self.lib.mysql_select_db(self.handle, database.encode()): raise RuntimeError('Cannot select ' + database)
        statement = self.lib.mysql_stmt_init(self.handle)
        try:
            raw = sql.encode('utf-8')
            if self.lib.mysql_stmt_prepare(statement, raw, len(raw)):
                raise RuntimeError(self.lib.mysql_stmt_error(statement).decode())
            return self.lib.mysql_stmt_field_count(statement), self.lib.mysql_stmt_param_count(statement)
        finally: self.lib.mysql_stmt_close(statement)

    def close(self):
        if self.handle: self.lib.mysql_close(self.handle); self.handle = None
