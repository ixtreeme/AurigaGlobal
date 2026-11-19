import pathlib,json,subprocess,tempfile,os
r=pathlib.Path('/Auriga/Server');cfg=json.loads((r/'server.json').read_text())
c=dict(cfg['common']['sql']['player']);c.update(cfg.get('database',{}).get('sql',{}).get('player',{}))
def query(sql):
 with tempfile.TemporaryDirectory(prefix='tera-db-read-') as d:
  p=pathlib.Path(d)/'client.cnf'
  def q(v):return '"'+str(v).replace('\\','\\\\').replace('"','\\"')+'"'
  p.write_text('[client]\n'+''.join(k+'='+q(c[k])+'\n' for k in ('host','port','user','password'))+'default-character-set=utf8mb4\n');p.chmod(0o600)
  result=subprocess.run(['/usr/local/bin/mariadb','--defaults-extra-file='+str(p),'--batch','--raw',c['database']],input=sql,text=True,capture_output=True)
  if result.returncode:raise RuntimeError(result.stderr[-1200:])
  return result.stdout

import hashlib
stage=r/'run/mount-flight-release';stage.mkdir(mode=0o700,parents=True,exist_ok=True)
ids='630017,630018,630019,630021,630022,630030,630045,630046,630054,630055,630056,630057,630058,630059,630060,630061,630062,630063,630064,630065,630066,630067,630068,630073,630074,630075,630076,630077,630078,630079,630080,630081,630082,630083,630084,630085,630086,630088,630089,630090,630091,630100,630123,630127,630129,630130,630131,630132,630133,630134,630135,630136,630142,630143,630144,630145,630146,630147,630148,630149,630150,630151,630152,630153,630154,630162,630163,630164,630165,630166,630167,630168,630169,630170,630183,630184,630187,630188,630189,630190,630191,630192,630193,630194'
sql='START TRANSACTION;\nUPDATE item_proto SET value5=1 WHERE type=28 AND subtype=2 AND vnum IN (630017,630018,630019,630021,630022,630030,630045,630046,630054,630055,630056,630057,630058,630059,630060,630061,630062,630063,630064,630065,630066,630067,630068,630073,630074,630075,630076,630077,630078,630079,630080,630081,630082,630083,630084,630085,630086,630088,630089,630090,630091,630100,630123,630127,630129,630130,630131,630132,630133,630134,630135,630136,630142,630143,630144,630145,630146,630147,630148,630149,630150,630151,630152,630153,630154,630162,630163,630164,630165,630166,630167,630168,630169,630170,630183,630184,630187,630188,630189,630190,630191,630192,630193,630194);\nCOMMIT;\n'
before=query('SELECT * FROM item_proto ORDER BY vnum');(stage/'item_proto-before.tsv').write_text(before);(stage/'capability.sql').write_text(sql)
rows=query('SELECT vnum,value5 FROM item_proto WHERE type=28 AND subtype=2 AND vnum IN ('+ids+') ORDER BY vnum');assert len(rows.splitlines())==85
query(sql.replace('COMMIT;','ROLLBACK;'));assert query('SELECT * FROM item_proto ORDER BY vnum')==before
(stage/'preflight.json').write_text(json.dumps(dict(validated=True,items=84,sqlSha256=hashlib.sha256(sql.encode()).hexdigest(),beforeSha256=hashlib.sha256(before.encode()).hexdigest())))
print('MOUNT_FLIGHT_SQL_PREFLIGHT_PASS items=84 existing_rows_unchanged=True')
