"""Inventory C++ SQL literals for the checked-in server feature profile.

This is a lexical inventory, not a C++ compiler. Assembled builders and format
arguments still need explicit contracts; no unhandled template is silently
counted as a successful database test.
"""
import ast
import hashlib
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TOKEN = re.compile(r'R"(?P<delim>[^()\s]{0,16})\([\s\S]*?\)(?P=delim)"|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|//[^\n]*|/\*[\s\S]*?\*/')
SQL = re.compile(r'^\s*(SELECT\s+|INSERT\s+(?:IGNORE\s+)?INTO\s+|REPLACE\s+INTO\s+|UPDATE\s+[^\s]+\s+SET\s+|DELETE\s+FROM\s+|CREATE\s+TABLE\s+|ALTER\s+TABLE\s+|DROP\s+TABLE\s+|TRUNCATE\s+TABLE\s+|SHOW\s+(?:FULL\s+)?TABLES\b|SET\s+(?:@|NAMES\b))', re.I)


def source_digest(path):
    """Ignore checkout line endings while pinning reviewed query builders."""
    return hashlib.sha256(Path(path).read_bytes().replace(b'\r\n', b'\n')).hexdigest()


def without_comments(text):
    return TOKEN.sub(lambda m: re.sub(r'[^\n]', ' ', m[0]) if m[0].startswith(('/',)) else m[0], text)


def condition(expression, macros):
    expression = re.sub(r'defined\s*\(\s*(\w+)\s*\)|defined\s+(\w+)',
                        lambda m: '1' if (m[1] or m[2]) in macros else '0', expression)
    expression = re.sub(r'\b[A-Za-z_]\w*\b', lambda m: str(macros.get(m[0], 0)), expression)
    expression = re.sub(r'(\d+)[uUlL]+\b', r'\1', expression)
    expression = expression.replace('&&', ' and ').replace('||', ' or ')
    expression = re.sub(r'!(?!=)', ' not ', expression)
    if re.search(r'[^\d\s()<>!=&|+*/%\-a-z]', expression):
        raise ValueError('unsupported preprocessor condition: ' + expression)
    return bool(eval(expression, {'__builtins__': {}}, {}))


def active_text(text, macros):
    result, stack = [], []
    enabled = True
    for line in text.splitlines(keepends=True):
        directive = re.match(r'\s*#\s*(\w+)\s*(.*)', line)
        if not directive:
            result.append(line if enabled else re.sub(r'[^\n]', ' ', line))
            continue
        kind, value = directive.groups()
        value = value.strip()
        if kind in ('if', 'ifdef', 'ifndef'):
            test = enabled and ((value in macros) if kind == 'ifdef' else (value not in macros) if kind == 'ifndef' else condition(value, macros))
            stack.append([enabled, test])
            enabled = enabled and test
        elif kind == 'elif':
            parent, taken = stack[-1]
            test = parent and not taken and condition(value, macros)
            stack[-1][1] |= test
            enabled = parent and test
        elif kind == 'else':
            parent, taken = stack[-1]
            enabled = parent and not taken
            stack[-1][1] = True
        elif kind == 'endif':
            enabled = stack.pop()[0]
        elif enabled and kind == 'define':
            name, _, definition = value.partition(' ')
            name = name.split('\t')[0]
            if '(' not in name:
                try: macros[name] = int(definition.strip().rstrip('uUlL'), 0)
                except ValueError: macros[name] = 1
        elif enabled and kind == 'undef':
            macros.pop(value, None)
        result.append(re.sub(r'[^\n]', ' ', line))
    if stack: raise ValueError('unbalanced preprocessor directives')
    return ''.join(result)


def feature_macros():
    macros = {'_WIN32': 1, 'WIN32': 1, '_MSC_VER': 1951, 'NDEBUG': 1, '__cplusplus': 202302}
    for name in ('service.h', 'CommonDefines.h'):
        active_text(without_comments((ROOT / 'SRC/Server/common' / name).read_text(encoding='utf-8')), macros)
    return macros


def enum_values(path, name):
    text = active_text(without_comments((ROOT / path).read_text(encoding='utf-8')), feature_macros())
    body = re.search(r'\benum\s+' + re.escape(name) + r'\s*\{(.*?)\};', text, re.S)[1]
    result, value = {}, -1
    for item in body.split(','):
        item = item.strip()
        if not item: continue
        symbol, separator, expression = item.partition('=')
        value = int(expression.strip(), 0) if separator else value + 1
        result[symbol.strip()] = value
    return result


def literals(path):
    path = Path(path)
    if not path.is_absolute(): path = ROOT / path
    raw = path.read_bytes().decode('utf-8', errors='replace')
    local = feature_macros()
    if '/Database/' in path.as_posix(): local['ENABLE_PROTO_FROM_DB'] = 1
    text = active_text(without_comments(raw), local)
    strings = [m for m in TOKEN.finditer(text) if m[0].startswith(('"', 'R"'))]
    i = 0
    while i < len(strings):
        token = strings[i]
        start, end = token.span()
        decode = lambda value: value[value.index('(') + 1:value.rindex(')')] if value.startswith('R"') else ast.literal_eval(value)
        parts = [decode(token[0])]
        while i + 1 < len(strings) and not text[end:strings[i + 1].start()].strip():
            i += 1
            token = strings[i]
            parts.append(decode(token[0]))
            end = token.end()
        yield text, start, end, ''.join(parts)
        i += 1


def inventory():
    records = []
    for path in sorted((ROOT / 'SRC/Server').rglob('*')):
        if path.suffix not in ('.cpp', '.h', '.hpp'): continue
        raw = path.read_bytes().decode('utf-8', errors='replace')
        stripped = without_comments(raw)
        if not re.search(r'"\s*(SELECT|INSERT|REPLACE|UPDATE|DELETE|CREATE|ALTER|DROP|TRUNCATE|SHOW|SET)\b', stripped, re.I): continue
        for text, start, end, value in literals(path):
            logging = re.search(r'(?:sys_err|LOG_\w+)\s*\(\s*$', text[max(0, start - 150):start])
            if SQL.match(value) and not logging:
                relative = path.relative_to(ROOT).as_posix()
                context = text[max(0, start - 200):min(len(text), end + 220)]
                database = 'account' if '/LoginServer/' in relative or 'SQL_ACCOUNT' in context else 'log' if relative.endswith('/core/log.cpp') else 'common' if 'AccountDB::instance()' in context or 'SQL_COMMON' in context else 'player'
                records.append({'id': hashlib.sha256((relative + '\n' + value).encode()).hexdigest()[:16],
                                'file': relative, 'line': text.count('\n', 0, start) + 1,
                                'database': database, 'sql': value})
    # Repeated literals in one file represent the same schema contract.
    return list({row['id']: row for row in records}.values())


def formatted_sql(sql):
    sql = sql.replace('%%', '\x01')
    sql = re.sub(r'locale_name_%s', 'locale_name_en', sql)
    sql = re.sub(r'ORDER BY (\w+\.\w+) %s', r'ORDER BY \1 DESC', sql, flags=re.I)
    sql = re.sub(r'ORDER BY d\.%s', 'ORDER BY d.completed DESC', sql, flags=re.I)
    sql = re.sub(r'(\(vnum, (?:type, subtype, )?name, )%s', r'\1locale_name', sql, flags=re.I)
    sql = re.sub(r'([A-Za-z_]\w*)%s', r'\1', sql)
    def replace(match):
        if match[0].endswith('s'):
            prefix = sql[:match.start()]
            if re.search(r'\b(ASC|DESC)\s*$', prefix, re.I): return ''
            return 'test' if prefix.count("'") % 2 else '1'
        return '1'
    sql = re.sub(r'%(?:[-+ #0]*\d*(?:\.\d+)?(?:ll|I64|l|h|z|t)?[diuoxXfFeEgGcs])', replace, sql)
    sql = sql.replace('\x01', '%')
    if re.match(r'REPLACE INTO item\b', sql, re.I): sql = sql.replace("'test'", "'INVENTORY'")
    sql = sql.replace("inet_aton('test')", "inet_aton('127.0.0.1')")
    return sql


if __name__ == '__main__':
    import json
    print(json.dumps(inventory(), indent=2, ensure_ascii=True))
