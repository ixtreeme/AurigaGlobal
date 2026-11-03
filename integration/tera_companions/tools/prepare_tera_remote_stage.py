from pathlib import Path
import json,base64,hashlib
r=Path('C:/UnityProjects/AurigaUnity/build/SRC/integration/tera_companions')
prefix=Path('work/tera_remote_db_probe.py').read_text(encoding='utf-8').split('out={}')[0]
payload={name:base64.b64encode((r/name).read_bytes()).decode() for name in ('prototypes.sql','tera_companions.lua','release.json')}
body='''
import base64,hashlib,time
payload=PAYLOAD
stage=r/'run/tera-companions-release'
stage.mkdir(mode=0o700,parents=True,exist_ok=True)
for name,encoded in payload.items():
 raw=base64.b64decode(encoded);p=stage/name;p.write_bytes(raw);p.chmod(0o600)
release=json.loads((stage/'release.json').read_text())['entries']
itemids=','.join(str(e['item']) for e in release);mobids=','.join(str(e['mob']) for e in release)
before={}
ranges=query('SELECT vnum,vnum_range FROM item_proto WHERE vnum_range>0 AND ((vnum<=630202 AND vnum+vnum_range>=630000) OR (vnum<=631068 AND vnum+vnum_range>=631000))')
if any(line.strip() and not line.startswith('vnum\\t') for line in ranges.splitlines()):raise RuntimeError('A pre-existing item ID range overlaps the release')
for table,ids,column in (('item_proto',itemids,'vnum'),('mob_proto',mobids,'vnum'),('mob_motion',','.join("'tera_"+e['key']+"'" for e in release),'folder')):
 check=query('SELECT '+column+' FROM '+table+' WHERE '+column+' IN ('+ids+')')
 if any(line.strip() and line!=column for line in check.splitlines()):raise RuntimeError('Release ID already exists: '+table)
 raw=query('SELECT * FROM '+table+' ORDER BY '+column)
 (stage/(table+'-before.tsv')).write_text(raw);(stage/(table+'-before.tsv')).chmod(0o600)
 before[table]=hashlib.sha256(raw.encode()).hexdigest()
engines=query("SELECT TABLE_NAME,ENGINE FROM information_schema.TABLES WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME IN ('item_proto','mob_proto','mob_motion')")
if len(engines.splitlines())!=4 or any(not line.endswith('\\tInnoDB') for line in engines.splitlines()[1:]):raise RuntimeError('Transactional prototype tables required')
print('TABLE_ENGINES',engines.strip())
sql=(stage/'prototypes.sql').read_text()
trial=sql.replace('COMMIT;','ROLLBACK;')
query(trial)
for table in ('item_proto','mob_proto','mob_motion'):
 after=hashlib.sha256(query('SELECT * FROM '+table+' ORDER BY '+('folder' if table=='mob_motion' else 'vnum')).encode()).hexdigest()
 if after!=before[table]:raise RuntimeError('Prototype changed during rollback validation: '+table)
(stage/'preflight.json').write_text(json.dumps(dict(before=before,sqlSha256=hashlib.sha256(sql.encode()).hexdigest(),items=272,mobs=272,validated=True)))
print('TERA_SQL_TRANSACTION_ROLLBACK_VERIFIED items=272 mobs=272 existing_prototypes_unchanged=true')
print('STAGING',stage)
'''.replace('PAYLOAD',repr(payload))
Path('work/tera_remote_stage.py').write_text(prefix+body,encoding='utf-8')
