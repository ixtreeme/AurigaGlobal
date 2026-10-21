"""Convert serving data, retain an input/output audit, and rewrite known Lua paths."""
import contextlib
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import re
import shutil
import tempfile
from .legacy import PackageError

TOOLS = Path(__file__).resolve().parents[1]


def digest(path):
    result = hashlib.sha256()
    with path.open('rb') as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b''):
            result.update(chunk)
    return result.hexdigest()


def write_json(path, document):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(document, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')


def module(name):
    path = TOOLS / ('motion_bake.py' if name == 'motion_bake' else 'txt2json/' + name + '.py')
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


class DataBuilder:
    def __init__(self, source, output, checker=None):
        self.source, self.output = source.resolve(), output.resolve()
        self.checker = checker
        self.inputs, self.operations, self.notes = {}, [], []
        self.converters = {name: module('txt2json_' + name) for name in ('group','typed','tables','regen','map','quest')}

    def track(self, source):
        key = source.relative_to(self.source).as_posix()
        self.inputs.setdefault(key, dict(sha256=digest(source), size=source.stat().st_size))

    def record(self, source, target, operation):
        self.track(source)
        self.operations.append(dict(source=source.relative_to(self.source).as_posix(),
                                    target=target.relative_to(self.output).as_posix(), operation=operation))

    def copy(self, source, target):
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)
        self.record(source, target, 'copy')

    def invoke(self, source, target, callback):
        target.parent.mkdir(parents=True, exist_ok=True)
        log = io.StringIO()
        with contextlib.redirect_stdout(log):
            result = callback()
        if result not in (0, None):
            raise PackageError('Converter rejected: ' + source.relative_to(self.source).as_posix())
        for line in log.getvalue().splitlines():
            if line.startswith(('note:', 'warning:', 'WARN')):
                self.notes.append(line.replace(str(self.source), '<source>').replace(str(self.output), '<output>'))
        self.record(source, target, 'convert')

    def group(self, source, target, mode, structural='tokens', reference=None):
        with tempfile.TemporaryDirectory(prefix='auriga-convert-') as directory:
            intermediate = Path(directory) / 'tree.json'
            def convert():
                self.converters['group'].convert(structural, str(source), str(intermediate))
                converter = self.converters['typed'].CONVERTERS[mode]
                if reference is not None:
                    return converter(str(intermediate), str(target), str(reference))
                return converter(str(intermediate), str(target))
            self.invoke(source, target, convert)

    def regen(self, source, target):
        with tempfile.TemporaryDirectory(prefix='auriga-regen-') as directory:
            intermediate = Path(directory) / 'regen.json'
            def convert():
                self.converters['regen'].convert(str(source), str(intermediate))
                return self.converters['typed'].convert_regen(str(intermediate), str(target))
            self.invoke(source, target, convert)

    def convert(self):
        locale = self.source / 'share/locale/germany'
        data = self.output / 'data'
        modes = dict(group='group', group_group='group_group', mob_drop_item='mob_drop',
                     drop_item_group='drop_item_group', special_item_group='special_item_group',
                     common_drop_item='common_drop_item')
        for filename, mode in modes.items():
            self.group(locale / (filename + '.txt'), data / (filename + '.json'), mode,
                       reference=data / 'group.json' if filename == 'group_group' else None)
        for filename in ('battle_pass','dragon_soul_table'):
            self.group(locale / (filename + '.txt'), data / (filename + '.json'), 'named_rows', 'rows')
        for mode, filename in (('blend','blend'),('cube','cube'),('fishing','fishing'),('gaya','gaya'),('guild','guild_renewal_levelup')):
            source, target = locale / (filename + '.txt'), data / (filename + '.json')
            converter = self.converters['tables'].CONVERTERS[mode]
            self.invoke(source, target, lambda s=source, t=target, c=converter: c(str(s), str(t)))
        for filename, columns, root_key in (('ori_to_new_table', ('ori','new'), 'pairs'), ('etc_drop_item', ('item','prob'), None)):
            source, target = locale / (filename + '.txt'), data / (filename + '.json')
            rows = []
            for line in source.read_text(encoding='latin1').splitlines():
                line = line.split('//', 1)[0].strip()
                if not line or line.startswith('#'): continue
                values = line.split()
                if len(values) != 2: raise PackageError('Invalid two-column table: ' + filename)
                rows.append(dict(zip(columns, (int(values[0]), int(values[1])) if root_key else (values[0], float(values[1])))))
            write_json(target, {root_key: rows} if root_key else rows)
            self.record(source, target, 'convert')
        for name in ('settings.lua','translate.lua','restart_city_pos.lua','BlueDragon.lua','monkey_dungeon.lua','oxquiz.lua','whisper.txt'):
            source = locale / name
            if source.is_file(): self.copy(source, data / name)
        for mode, filename in (('npc','questnpc'),('category','questcategory')):
            source, target = locale / 'quest' / (filename + '.txt'), data / 'quest' / (filename + '.json')
            write_json(target, self.converters['quest'].convert(mode, source.read_text(encoding='utf-8-sig'), str(source)))
            self.record(source, target, 'convert')
        quest = locale / 'quest'
        for folder in ('libraries', 'Quest'):
            for source in sorted((quest / folder).rglob('*')):
                if source.is_file() and source.suffix not in ('.rar','.core','.bak'):
                    self.copy(source, data / 'quest' / source.relative_to(quest))
        for name in ('questlib.lua','locale.lua','quest_list','quest_functions'):
            self.copy(quest / name, data / 'quest' / name)
        if self.checker is None:
            raise PackageError('Current ServerPackageCheck is required to validate quest sources')
        from prepare_quests import reconcile
        self.quest_report = reconcile(data / 'quest', self.checker, quest, duplicate_policy='last')
        # Mutable Gaya state is not static shop data. Preserve its bytes.
        for source in sorted((locale / 'gaya').rglob('*')):
            if source.is_file(): self.copy(source, data / source.relative_to(locale))
        maps = locale / 'map'
        self.invoke(maps / 'index', data / 'map/index.json', lambda: self.converters['map'].convert_index(str(maps / 'index'), str(data / 'map/index.json')))
        for folder in sorted(p for p in maps.iterdir() if p.is_dir()):
            for source in sorted(folder.iterdir()):
                if not source.is_file(): continue
                lower = source.name.lower()
                target = data / 'map' / folder.name / source.name
                if lower == 'server_attr':
                    self.copy(source, target)
                elif lower in ('setting.txt','town.txt','dungeon.txt'):
                    mode = lower[:-4]; target = target.with_name(mode + '.json')
                    c = self.converters['map'].CONVERTERS[mode]
                    self.invoke(source, target, lambda s=source, t=target, c=c: c(str(s), str(t)))
                elif lower.endswith('.txt') and lower not in ('mapproperty.txt','metin.txt'):
                    self.regen(source, target.with_suffix('.json'))
        for source in sorted((self.source / 'share/data/dungeon').rglob('*.txt')):
            if source.name.lower() == 'motlist.txt': continue
            target = data / source.relative_to(self.source / 'share/data')
            self.regen(source, target.with_suffix('.json'))
        for name in ('CMD','BANIP','CRC','VERSION'):
            source = self.source / 'share/conf' / name
            if source.is_file():
                self.copy(source, self.output / 'runtime_defaults' / name)
        for source in sorted((self.source / 'chan/mark').glob('*')):
            if source.is_file(): self.copy(source, self.output / 'runtime_defaults/mark' / source.name)
        for config in sorted((self.source / 'chan').glob('ch*/core*/CONFIG')):
            symbol = config.parent / 'guild_symbol.tga'
            if symbol.is_file():
                name = config.parent.parent.name + '-' + config.parent.name
                self.copy(symbol, self.output / 'runtime_defaults/instances' / name / 'guild_symbol.tga')
        # Build motions from original MSA once; runtime needs only SQL tables.
        motion = module('motion_bake')
        target = self.output / 'db/motion.sql'; target.parent.mkdir(parents=True, exist_ok=True)
        count = 0
        with target.open('w', encoding='utf-8', newline='\n') as out:
            out.write('SET NAMES utf8mb4;\n')
            for folder in sorted((self.source / 'share/data/monster').iterdir()):
                if not folder.is_dir(): continue
                motlist = folder / 'motlist.txt'
                if not motlist.is_file(): continue
                self.track(motlist)
                for filename in motion.parse_motlist(str(motlist)).values():
                    path = folder / filename
                    if path.is_file(): self.track(path)
                count += motion.bake_folder(str(folder), out)
        self.notes.append('motion.sql: %d generated motion/splash rows' % count)
        result = self.patch_lua_paths(data)
        result['quest_migration'] = self.quest_report
        return result

    def patch_lua_paths(self, data):
        # Rewrite literal resource paths in runtime objects AND editable quest sources.
        pattern = re.compile(rb'(["\'])((?:data/|locale/germany/|/[^"\'\r\n]*/share/(?:data/|locale/germany/))[^"\'\r\n]+)\1')
        changes, missing = [], set()
        for path in sorted(data.rglob('*')):
            if not path.is_file() or path.suffix not in ('.lua','.quest','.start','.script','.when','.arg'): continue
            raw = path.read_bytes()
            if raw.startswith(b'\x1bLua'): raise PackageError('Lua bytecode needs recompilation: ' + str(path.relative_to(data)))
            changed = []
            def replace(match):
                original = match[2].decode('latin1')
                relative = re.sub(r'^.*?/share/(?:data/|locale/germany/)', '', original)
                relative = re.sub(r'^(?:data/|locale/germany/)', '', relative)
                candidate = Path(relative)
                if '..' in candidate.parts: raise PackageError('Resource path escapes data directory')
                if candidate.suffix.lower() == '.txt' and (data / candidate.with_suffix('.json')).is_file():
                    relative = candidate.with_suffix('.json').as_posix()
                if not (data / relative).exists():
                    missing.add(relative)
                    return match[0]
                replacement = 'data/' + relative
                # data/... JSON paths are resolved by the production JsonFile loader.
                # Other Lua resources need the configured absolute base at execution.
                if replacement.endswith('.json'):
                    value = match[1] + replacement.encode('latin1') + match[1]
                else:
                    value = b'(get_locale_base_path()..' + match[1] + ('/' + relative).encode('latin1') + match[1] + b')'
                if value != match[0]: changed.append(dict(before=original, after=replacement))
                return value
            updated = pattern.sub(replace, raw)
            if changed:
                path.write_bytes(updated)
                changes.append(dict(file=path.relative_to(self.output).as_posix(), replacements=changed))
        return dict(lua_path_changes=changes, unresolved_lua_resources=sorted(missing))

    def verify_originals(self):
        for relative, metadata in self.inputs.items():
            if digest(self.source / relative) != metadata['sha256']:
                raise PackageError('Source changed during conversion: ' + relative)


def validate_data(root, config):
    data = root / 'data'
    index = json.loads((data / 'map/index.json').read_text(encoding='utf-8'))['maps']
    ids = [row['index'] for row in index]
    if len(ids) != len(set(ids)): raise PackageError('Duplicate map IDs')
    missing = []
    for row in index:
        base = data / 'map' / row['name']
        for name in ('setting.json','town.json','server_attr'):
            if not (base / name).is_file(): missing.append('map/' + row['name'] + '/' + name)
        if (base / 'setting.json').exists():
            setting = json.loads((base / 'setting.json').read_text(encoding='utf-8'))
            if len(setting.get('map_size', [])) != 2 or len(setting.get('base_position', [])) != 2:
                raise PackageError('Incomplete map setting: ' + row['name'])
    unindexed = {}
    for name, core in config['game_servers'].items():
        unknown = set(core.get('map_allow', [])) - set(ids)
        if unknown: unindexed[name] = sorted(unknown)
    if missing: raise PackageError('Missing indexed map files: ' + repr(missing))
    groups = json.loads((data / 'group.json').read_text(encoding='utf-8'))['groups']
    group_ids = {g['vnum'] for g in groups}
    group_groups = json.loads((data / 'group_group.json').read_text(encoding='utf-8'))['groups']
    unresolved = sorted({m['vnum'] for g in group_groups for m in g.get('members', []) if m['vnum'] not in group_ids})
    return dict(indexed_maps=len(index), groups=len(groups), group_groups=len(group_groups),
                unresolved_group_ids=unresolved, unindexed_core_maps=unindexed,
                json_files=sum(1 for _ in data.rglob('*.json')))
