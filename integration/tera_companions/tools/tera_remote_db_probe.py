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
out={}
for table in ('item_proto','mob_proto'):
 out[table+'_schema']=query('SHOW FULL COLUMNS FROM '+table)
 out[table+'_reserved']=query('SELECT vnum,name FROM '+table+' WHERE (vnum BETWEEN 620000 AND 620999) OR (vnum BETWEEN 32000 AND 32999)')
out['mount_item']=query('SELECT * FROM item_proto WHERE vnum=611500')
out['pet_items']=query('SELECT vnum,name,type,subtype,value0,value1,value2 FROM item_proto WHERE value1 BETWEEN 34000 AND 34999 LIMIT 12')
out['mount_mob']=query('SELECT * FROM mob_proto WHERE vnum=21147')
out['pet_mob']=query('SELECT * FROM mob_proto WHERE vnum=34003')
out['counts']=query('SELECT (SELECT COUNT(*) FROM item_proto) AS items,(SELECT COUNT(*) FROM mob_proto) AS mobs')
out['motion_schema']=query('SHOW FULL COLUMNS FROM mob_motion')
out['motion_existing']=query("SELECT * FROM mob_motion WHERE folder LIKE 'tera_%'")
out['pet_template']=query('SELECT * FROM item_proto WHERE vnum=48321')
out['all_ids']={t:query('SELECT vnum FROM '+t) for t in ('item_proto','mob_proto')}
out['quest_files']=[str(p.relative_to(r)) for p in (r/'data/quest').glob('*') if 'pet' in p.name.lower()]
out['quest_list_files']=[str(p.relative_to(r)) for p in (r/'data/quest').glob('*') if 'list' in p.name.lower()]
out['quest_compilers']=[str(p) for p in (r/'bin').glob('*')]
print(json.dumps(out,ensure_ascii=False))
