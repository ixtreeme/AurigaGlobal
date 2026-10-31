"""Read-only syntax checks. Native parsers compile chunks, never execute them."""
from dataclasses import dataclass
import json
import os
from pathlib import Path
import re
import sys
import tempfile

import backend as api


@dataclass(frozen=True)
class Issue:
    message: str
    line: int = 0
    column: int = 0


def language_for(relative):
    path = Path(relative)
    rel = path.as_posix().lower()
    if path.suffix.lower() == '.json':
        return 'json'
    if path.suffix.lower() == '.quest' or (path.suffix.lower() == '.lua' and
            rel.startswith(('data/quest/quest/', 'data/quest/pre_qc/'))):
        return 'quest'
    if path.suffix.lower() in ('.lua', '.start', '.script', '.when') or rel.startswith('data/quest/object/state/'):
        return 'lua'
    return 'text'


def json_issues(text):
    try:
        json.loads(text)
        api.strict_json(text)
        return []
    except json.JSONDecodeError as error:
        return [Issue('JSON: ' + error.msg, error.lineno, error.colno)]
    except (api.EditorError, ValueError, RecursionError) as error:
        # The parser's object hook has no source offsets. Scan only already
        # valid JSON tokens to locate a duplicate in its actual object scope.
        stack = []
        for token in re.finditer(r'"(?:[^"\\]|\\.)*"|[{}\[\]]', text):
            value = token[0]
            if value in ('{', '['):
                stack.append(set() if value == '{' else None)
            elif value in ('}', ']'):
                if stack:
                    stack.pop()
            elif stack and stack[-1] is not None and text[token.end():].lstrip().startswith(':'):
                key = json.loads(value)
                if key in stack[-1]:
                    line = text.count('\n', 0, token.start()) + 1
                    column = token.start() - text.rfind('\n', 0, token.start())
                    return [Issue('Ismétlődő JSON-kulcs: ' + key, line, column)]
                stack[-1].add(key)
        return [Issue(str(error) or 'Túl mély JSON-szerkezet.')]


def native_program(name):
    suffix = '.exe' if os.name == 'nt' else ''
    base = Path(__file__).resolve().parent
    # Frozen apps resolve __file__ within _internal. Source builds use the
    # same binaries that are later included in the distributable bundle.
    choices = [base / 'validators' / (name + suffix)]
    repo = base.parents[2]
    choices += [repo / 'build/SRC/Server/GameServer' / config / (name + suffix)
                for config in ('RelWithDebInfo', 'Release', 'Debug', '')]
    return next((p for p in choices if p.is_file()), None)


class NativeRequest:
    def __init__(self, text, language, encoding):
        name = 'ServerPackageCheck'
        self.program = native_program(name)
        if not self.program:
            raise api.EditorError('A natív %s ellenőrző hiányzik; a szintaxis nem ellenőrizhető.' % name)
        self.temp = tempfile.TemporaryDirectory(prefix='auriga-syntax-')
        self.directory = Path(self.temp.name)
        try:
            self.path = self.directory / ('candidate.quest' if language == 'quest' else 'candidate.lua')
            self.path.write_bytes(text.encode(encoding))
            # Relative ASCII names work with the historical Lua file reader
            # even when the user's Windows profile contains accented letters.
            self.arguments = ['--check-quest' if language == 'quest' else '--check-lua', self.path.name]
            self.marker = 'QUEST_SYNTAX_OK' if language == 'quest' else 'LUA_SYNTAX_OK'
        except BaseException:
            self.close()
            raise

    def result(self, output, code):
        if code == 0 and self.marker in output.splitlines():
            return []
        # The source parser and Lua compiler both report the candidate line.
        match = re.search(r'candidate\.(?:quest|lua):(\d+):([^\n]*)', output)
        if match:
            return [Issue(match[2].strip() or 'Hibás szintaxis.', int(match[1]), 1)]
        lines = [line.strip() for line in output.splitlines() if line.strip()]
        return [Issue(('\n'.join(lines[-3:]) or 'Az ellenőrző nem adott érvényes eredményt.')[:800])]

    def close(self):
        self.temp.cleanup()
