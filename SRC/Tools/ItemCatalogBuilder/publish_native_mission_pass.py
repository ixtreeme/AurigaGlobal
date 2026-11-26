#!/usr/bin/env python3
"""Publish an explicitly reviewed native Mission Pass contract; schema 059 required.
Does not create, reprice, substitute or convert any item. Existing progress
prevents contract edits; start a new season for changed rewards or objectives.
"""
import argparse,hashlib,json,sys
from pathlib import Path
def unsigned(value,maximum):
 if type(value) is not int or not 0<value<=maximum:raise ValueError("Invalid native season integer")
 return value
def plan(doc):
 if doc.get("schema_version")!=2 or doc.get("premium_purchase_enabled") is not False or type(doc.get("enabled")) is not bool:
  raise ValueError("Native Mission Pass schema 2 required")
 sid=unsigned(doc["season_id"],0xffffffff)
 start=unsigned(doc["starts_at_utc"],0x7fffffffffffffff);end=unsigned(doc["ends_at_utc"],0x7fffffffffffffff)
 if end<=start:raise ValueError("Invalid season interval")
 levels=unsigned(doc["levels"],100);step=unsigned(doc["xp_per_level"],0xffffffff)
 rows=[];seen=set()
 for row in doc["rewards"]:
  if set(row)!={"level","track","category","local","refinement","count"}:raise ValueError("Native reward key required")
  level=unsigned(row["level"],levels);track={"free":0,"premium":1}[row["track"]]
  category=unsigned(row["category"],0xffff);local=unsigned(row["local"],0xffffffff)
  if type(row["refinement"]) is not int or not 0<=row["refinement"]<=255:raise ValueError("Invalid refinement")
  count=unsigned(row["count"],200)
  if (level,track) in seen:raise ValueError("Duplicate reward slot")
  seen.add((level,track));rows.append([sid,level,track,category,local,row["refinement"],count])
 return dict(season=[sid,start,end,step,levels,int(doc["enabled"])],rewards=sorted(rows),
     source_hash=hashlib.sha256(json.dumps({k:v for k,v in doc.items() if k!="enabled"},sort_keys=True,separators=(",",":"),ensure_ascii=False).encode()).hexdigest())
def publish(db,doc,apply=False):
 p=plan(doc);sid=p["season"][0]
 if db.query("SELECT GET_LOCK('auriga.native-mission-pass-config',10)")!=[["1"]]:raise RuntimeError("Another publisher is running")
 begun=False
 try:
  db.query("START TRANSACTION");begun=True
  prior=db.query("SELECT season_id,starts_at,ends_at,xp_per_level,levels,enabled,LOWER(HEX(contract_sha256)) FROM player.native_mission_pass_season WHERE season_id="+str(sid)+" FOR UPDATE")
  rows=db.query("SELECT season_id,reward_level,track,category_id,local_id,refinement_level,item_count FROM player.native_mission_pass_reward WHERE season_id="+str(sid)+" ORDER BY reward_level,track FOR UPDATE")
  changed=([[*[str(v) for v in p["season"]],p["source_hash"]]]!=prior or [[str(v) for v in row] for row in p["rewards"]]!=rows)
  contract_changed=(not prior or prior[0][:5]!=[str(v) for v in p["season"][:5]] or
      prior[0][6]!=p["source_hash"] or rows!=[[str(v) for v in row] for row in p["rewards"]])
  if contract_changed and int(db.query("SELECT COUNT(*) FROM player.native_mission_pass_account WHERE season_id="+str(sid))[0][0]):
   raise RuntimeError("Published season has progress; use a new season ID")
  if apply and changed:
   db.query("INSERT INTO player.native_mission_pass_season VALUES("+",".join(map(str,p["season"]))+",UNHEX('"+p["source_hash"]+"')) ON DUPLICATE KEY UPDATE starts_at=VALUES(starts_at),ends_at=VALUES(ends_at),xp_per_level=VALUES(xp_per_level),levels=VALUES(levels),enabled=VALUES(enabled),contract_sha256=VALUES(contract_sha256)")
   db.query("DELETE FROM player.native_mission_pass_reward WHERE season_id="+str(sid))
   for row in p["rewards"]:db.query("INSERT INTO player.native_mission_pass_reward VALUES("+",".join(map(str,row))+")")
  db.query("COMMIT" if apply else "ROLLBACK");begun=False
  return dict(applied=apply,changed=changed,season=sid,rewards=len(p["rewards"]),source_hash=p["source_hash"])
 finally:
  if begun:db.query("ROLLBACK")
  db.query("SELECT RELEASE_LOCK('auriga.native-mission-pass-config')")
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument("--config",type=Path,required=True);p.add_argument("--connection-file",type=Path,required=True);p.add_argument("--apply",action="store_true");a=p.parse_args()
 sys.path.insert(0,str(Path(__file__).resolve().parents[3]/"db/tools"))
 from mariadb_client import Client
 db=Client(**json.loads(a.connection_file.read_text(encoding="utf-8")))
 try:print(json.dumps(publish(db,json.loads(a.config.read_text(encoding="utf-8")),a.apply)))
 finally:db.close()
if __name__=="__main__":main()
