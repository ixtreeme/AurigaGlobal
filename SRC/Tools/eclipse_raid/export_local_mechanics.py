"""Export numeric mechanics from cached build-12340 DBC + local upstream SQL.

No models, textures, sound or other art are read/exported. SQL-created spells
and creature template spell slots are part of the server logic dependency graph.
"""
import argparse
import csv
import hashlib
import json
import re
import struct
import zipfile
from pathlib import Path


class Table:
    def __init__(self, folder, schemas, name):
        self.name = name
        data = (folder / (name.lower()+'.dbc')).read_bytes()
        magic, count, fields, size, _ = struct.unpack_from('<4s4I', data)
        assert magic == b'WDBC'
        text = (schemas/(name+'.dbd')).read_text(encoding='utf-8-sig')
        types = {}
        for line in text.splitlines()[1:]:
            if not line.strip() or line.startswith(('BUILD', 'LAYOUT')):
                break
            parts = line.split()
            if len(parts)>1:
                types[parts[1].rstrip('?')] = parts[0].split('<')[0]
        candidates = []
        for block in re.split(r'\n\s*\n', text):
            if not any('12340' in line for line in block.splitlines() if line.startswith('BUILD')):
                continue
            columns, offset = [], 0
            for line in block.splitlines():
                if line.startswith(('BUILD', 'LAYOUT', 'COMMENT')):
                    continue
                line = re.sub(r'\$[^$]+\$', '', line.split('//')[0].strip())
                m = re.fullmatch(r'(\w+)(?:<([^>]+)>)?(?:\[(\d+)\])?', line)
                if not m:
                    continue
                key, bits, length = m.groups()
                kind, length = types.get(key, 'int'), int(length or 1)
                if kind == 'locstring':
                    length *= 17
                width = int((bits or '32').lstrip('u'))//8 if kind == 'int' else 4
                columns.append((key, offset, length, kind, bool(bits and bits.startswith('u')), width))
                offset += length*width
            if offset == size:
                candidates.append(columns)
        if len(candidates) != 1:
            raise ValueError(f'{name}: unverified/ambiguous build layout ({fields} fields)')
        self.columns = candidates[0]
        self.rows = {}
        for n in range(count):
            at, row = 20+n*size, {}
            for key, offset, length, kind, unsigned, width in self.columns:
                if kind in ('string','locstring'):
                    continue
                fmt = 'f' if kind=='float' else ({1:'B',2:'H',4:'I'} if unsigned else {1:'b',2:'h',4:'i'})[width]
                values = list(struct.unpack_from('<'+fmt*length, data, at+offset))
                row[key] = values[0] if length==1 else values
            self.rows[row['ID']] = row
        self.sha = hashlib.sha256(data).hexdigest()


def sql_table(archive, name):
    path = next(p for p in archive.namelist() if p.endswith('/db_world/'+name+'.sql'))
    raw = archive.read(path)
    text = raw.decode('utf-8')
    columns = re.findall(r'^\s*`([^`]+)`', text, re.M)
    rows = []
    for line in text.splitlines():
        if not re.match(r'^\(\d+,',line):
            continue
        assert line.endswith(('),',');'))
        values = next(csv.reader([line[1:-2]],quotechar="'",escapechar='\\'))
        if len(values)!=len(columns):
            raise ValueError(f'{name}: SQL field count mismatch')
        rows.append(dict(zip(columns, values)))
    return rows, hashlib.sha256(raw).hexdigest()


ALIASES = dict(AttributesExB='AttributesEx2', AttributesExC='AttributesEx3',
               AttributesExD='AttributesEx4', AttributesExE='AttributesEx5',
               AttributesExF='AttributesEx6', AttributesExG='AttributesEx7',
               EffectAmplitude='EffectMultipleValue', EffectBonusCoefficient='EffectBonusMultiplier',
               )


def merge_sql(table, rows):
    for raw in rows:
        row = {}
        for key, _, count, kind, _, _ in table.columns:
            if kind in ('string','locstring'):
                continue
            source = ALIASES.get(key,key)
            names = [source] if count==1 else [source+'_'+str(n+1) for n in range(count)]
            values = []
            for name in names:
                if name not in raw:
                    # A mechanical field missing from a schema is not fabricated.
                    raise ValueError(f'{table.name}: SQL mapping missing {key} ({name})')
                values.append(float(raw[name]) if kind=='float' else int(raw[name]))
            row[key] = values[0] if count==1 else values
        table.rows[row['ID']] = row


def export(args):
    prior = json.loads(args.output.read_text(encoding='utf-8-sig'))
    tables = {name:Table(args.dbc,args.schemas,name) for name in
              ['Spell','SpellCastTimes','SpellDuration','SpellRadius','SpellRange','SpellDifficulty']}
    # Preserve the established numerical-mechanics schema; do not export
    # unused shapeshift/equipment/text columns through an unverified SQL alias.
    wanted = (set(prior['spells'][0])-{'related'}) | {'CastingTimeIndex','DurationIndex','RangeIndex','EffectRadiusIndex'}
    tables['Spell'].columns = [c for c in tables['Spell'].columns if c[0] in wanted]
    tables['Spell'].rows = {i:{k:v for k,v in r.items() if k in wanted} for i,r in tables['Spell'].rows.items()}
    sql_hashes = {}
    with zipfile.ZipFile(args.upstream_zip) as archive:
        for name,table in tables.items():
            rows,digest = sql_table(archive,name.lower()+'_dbc')
            sql_hashes[name] = digest
            merge_sql(table,rows)
        creature_rows,sql_hashes['creature_template'] = sql_table(archive,'creature_template')
        slots,sql_hashes['creature_template_spell'] = sql_table(archive,'creature_template_spell')
        levels,sql_hashes['creature_classlevelstats'] = sql_table(archive,'creature_classlevelstats')
    spell_table = tables['Spell'].rows
    text = '\n'.join(p.read_text(encoding='utf-8-sig') for p in args.source.glob('*') if p.suffix in ('.cpp','.h'))
    spell_roots = {int(m) for m in re.findall(r'\b\d{4,5}\b',text) if int(m) in spell_table}
    spell_roots.update(s['ID'] for s in prior['spells'])
    spell_roots.update(v.get('id',v.get('ID')) if isinstance(v,dict) else v for v in prior['missing_records'])
    npcs = {int(m) for m in re.findall(r'NPC_\w+\s*=\s*(\d+)',text)}
    creatures = {int(row['entry']):row for row in creature_rows}
    npc_pending = list(npcs)
    while npc_pending:
        row = creatures.get(npc_pending.pop())
        if row:
            for field in ('difficulty_entry_1','difficulty_entry_2','difficulty_entry_3'):
                entry = int(row[field])
                if entry and entry not in npcs:
                    npcs.add(entry);npc_pending.append(entry)
    npc_spells = {}
    for row in slots:
        if int(row['CreatureID']) in npcs and row['Spell']!='NULL':
            sid = int(row['Spell']);spell_roots.add(sid)
            npc_spells.setdefault(int(row['CreatureID']),[]).append(sid)
    variants = {}
    for row in sorted(tables['SpellDifficulty'].rows.values(),key=lambda r:r['ID']):
        for sid in row['DifficultySpellID']:
            if sid>0:
                variants[sid] = row
    pending, included, missing = list(spell_roots), {}, set()
    while pending:
        sid = pending.pop()
        if sid in included or sid in missing:
            continue
        row = spell_table.get(sid)
        if not row:
            missing.add(sid);continue
        row = dict(row)
        related = {}
        for key,name in [('CastingTimeIndex','SpellCastTimes'),('DurationIndex','SpellDuration'),
                         ('RangeIndex','SpellRange'),('EffectRadiusIndex','SpellRadius')]:
            ids = row.get(key,[])
            ids = ids if isinstance(ids,list) else [ids]
            related[name] = [tables[name].rows[i] for i in ids if i in tables[name].rows and i>0]
        related['SpellDifficulty'] = [variants[sid]] if sid in variants else []
        row['related'] = related;included[sid] = row
        pending.extend(i for i in row['EffectTriggerSpell'] if i>0)
        for kind,bp,dice in zip(row['Effect'],row['EffectBasePoints'],row['EffectDieSides']):
            value=bp+(1 if dice==1 else 0)
            if kind in (64,77,140) and value>999 and value in spell_table:
                pending.append(value)
        if sid in variants:
            pending.extend(i for i in variants[sid]['DifficultySpellID'] if i>0)
        for kind,entry in zip(row['Effect'],row['EffectMiscValue']):
            if kind==28 and entry in creatures:
                npcs.add(entry)
                for slot in slots:
                    if int(slot['CreatureID'])==entry and slot['Spell']!='NULL':
                        pending.append(int(slot['Spell']))
    payload = dict(schema_version=1,client_build=12340,
                   source_sha256=hashlib.sha256(text.encode()).hexdigest(),
                   tables_sha256={k:t.sha for k,t in tables.items()},sql_sha256=sql_hashes,
                   spells=[included[i] for i in sorted(included)],missing_records=sorted(missing),
                   notice='Numeric mechanics only. Source literals, SQL-created spells, difficulty variants and triggers are included; data presence is not implemented behavior.')
    args.output.write_text(json.dumps(payload,indent=2)+'\n',encoding='utf-8')
    chosen=[]
    base_stats={(int(r['level']),int(r['class'])):r for r in levels}
    for entry in sorted(npcs):
        row=creatures.get(entry)
        if row:
            selected={key:row[key] for key in row if key not in ('name','subname','IconName','AIName','ScriptName')}
            stats=base_stats.get((int(row['minlevel']),int(row['unit_class'])))
            if not stats:
                raise ValueError(f'Creature {entry}: missing verified class/level base stats')
            selected['base_statistics']={k:v for k,v in stats.items() if k!='comment'}
            chosen.append(selected)
    args.creatures.write_text(json.dumps(dict(creatures=chosen,spells=npc_spells),indent=2)+'\n')
    print('ICC_NUMERIC_EXPORT',len(included),'spells',len(missing),'unresolved',len(chosen),'creature templates')


if __name__=='__main__':
    parser=argparse.ArgumentParser()
    for option in ('dbc','schemas','upstream-zip','source','output','creatures'):
        parser.add_argument('--'+option,type=Path,required=True)
    export(parser.parse_args())
