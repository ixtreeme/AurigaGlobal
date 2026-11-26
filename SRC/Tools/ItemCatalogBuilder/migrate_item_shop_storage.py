#!/usr/bin/env python3
"""One-time, explicit legacy shop import into native storage.

Apply migration 056 first. The runtime never imports, falls back to, or joins
legacy tables. The immutable receipt makes a repeated import a no-op, including
after the source tables have been archived. No inventory/catalog rows change.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path

SOURCE = "legacy-item-shop-v1"
TABLES = {
    "offers": ("player.ishop_offer", ("offer_id","display_category","display_subcategory","unit_price","discount_percent",
        "offer_until","added_at","baseline_sales","weekly_limit","monthly_limit","history_scope_id","package_title","enabled")),
    "members": ("player.ishop_offer_member", ("offer_id","slot","category_id","local_id","refinement_level","quantity")),
    "history": ("player.ishop_purchase_archive", ("account_id","bought_at","units","paid","history_scope_id","category_id","local_id","refinement_level")),
}
def _uint(value, bits=32):
    n=int(value or 0)
    if n<0 or n >= (1<<bits): raise ValueError("Unsigned field out of range")
    return n
def _key(row):
    key=(_uint(row[0],16),_uint(row[1]),_uint(row[2],8))
    if not key[0] or not key[1]: raise ValueError("Native member has an empty key")
    return key
def _date(value):
    return None if not value or str(value).startswith("0000-") else str(value)
def build_plan(data):
    mappings={int(r[0]):r for r in data["mapping"]}
    by_source={}
    for r in data["mapping"]: by_source.setdefault(int(r[1]),set()).add(_key(r[2:5]))
    # Native quota cohorts are densely allocated independently of item identities.
    scopes={code:i+1 for i,code in enumerate(sorted(c for c in by_source if c>0))}
    packages={int(r[0]):str(r[1]) for r in data["packages"]}
    package_members={}
    for r in data["package_members"]: package_members.setdefault(int(r[0]),[]).append(r)
    offers=[];members=[];history=[]
    for row in data["offers"]:
        offer=_uint(row[0]);code=int(row[3]);title=packages.get(offer)
        if not offer: raise ValueError("Offer ID must be positive")
        if title is not None:
            group=sorted(package_members.get(offer,[]),key=lambda r:int(r[1]))
            if not title or len(title.encode("utf-8"))>512 or [int(r[1]) for r in group]!=list(range(1,7)):
                raise ValueError("Native package must have six ordered members and a title")
            group=[(offer,int(r[1]),*_key(r[2:5]),_uint(r[5],16)) for r in group]
            scope=0
        else:
            mapping=mappings.get(offer)
            if mapping is None or int(mapping[1])!=code: continue
            group=[(offer,1,*_key(mapping[2:5]),1)]
            scope=scopes.get(code,0)
        if any(not 1<=r[-1]<=128 for r in group) or len({r[2:5] for r in group})!=len(group):
            raise ValueError("Invalid native member count or duplicate key")
        offers.append((offer,_uint(row[1],8),_uint(row[2],8),_uint(row[4]),min(100,_uint(row[5],8)),
            _date(row[6]),_date(row[7]),_uint(row[8],64),int(row[9] or 0)&0xffffffff,
            int(row[10] or 0)&0xffffffff,scope,title,1))
        members.extend(group)
    for row in data["history"]:
        code=int(row[2]);keys=by_source.get(code,set())
        key=next(iter(keys)) if len(keys)==1 else (0,0,0)
        units=int(row[3])
        if units<-(1<<31) or units>=(1<<31): raise ValueError("Historical quantity is out of range")
        history.append((_uint(row[0]),str(row[1]),units,_uint(row[4],64),scopes.get(code,0),*key))
    digest=hashlib.sha256(json.dumps(data,ensure_ascii=False,separators=(",",":")).encode("utf-8")).hexdigest()
    return dict(offers=offers,members=members,history=history,source_hash=digest,skipped=len(data["offers"])-len(offers))
def read_source(db):
    return {
      "offers": db.query("SELECT id,categoryType,categorySubType,itemVnum,itemPrice,discount,offerTime,addedTime,sellCount,week_limit,month_limit FROM player.ishop_data ORDER BY id"),
      "mapping": db.query("SELECT shop_id,legacy_vnum,category_id,local_id,refinement_level FROM player.ishop_native_item ORDER BY shop_id"),
      "packages": db.query("SELECT shop_id,title FROM player.ishop_native_package ORDER BY shop_id"),
      "package_members": db.query("SELECT shop_id,slot,category_id,local_id,refinement_level,quantity FROM player.ishop_native_package_member ORDER BY shop_id,slot"),
      "history": db.query("SELECT accountID,buyDate,itemVnum,itemCount,itemPrice FROM player.ishop_log ORDER BY accountID,buyDate,itemVnum,itemCount,itemPrice"),
    }
def _literal(value):
    if value is None: return "NULL"
    if isinstance(value,int): return str(value)
    raw=str(value).encode("utf-8")
    return "CONVERT(0x"+raw.hex()+" USING utf8mb4)" if raw else "''"
def _insert(db,table,columns,rows):
    for start in range(0,len(rows),250):
        values=",".join("("+",".join(map(_literal,r))+")" for r in rows[start:start+250])
        db.query("INSERT INTO "+table+" ("+",".join(columns)+") VALUES "+values)
def migrate(db,apply=False):
    lock=db.query("SELECT GET_LOCK('auriga.native-item-shop-cutover',10)")
    if lock!=[["1"]]: raise RuntimeError("Another item-shop import is running")
    begun=False
    try:
        receipt=db.query("SELECT offers,members,history_rows,HEX(source_hash) FROM player.ishop_offer_import WHERE source='"+SOURCE+"'")
        if receipt: return dict(already_applied=True,offers=int(receipt[0][0]),members=int(receipt[0][1]),history=int(receipt[0][2]),source_hash=receipt[0][3].lower())
        db.query("START TRANSACTION WITH CONSISTENT SNAPSHOT");begun=True
        if any(int(db.query("SELECT COUNT(*) FROM "+table)[0][0]) for table,_ in TABLES.values()):
            raise RuntimeError("Native shop storage is not empty; refusing to overwrite offers or history")
        plan=build_plan(read_source(db))
        result=dict(already_applied=False,offers=len(plan["offers"]),members=len(plan["members"]),history=len(plan["history"]),
            source_hash=plan["source_hash"],skipped=plan["skipped"],applied=bool(apply))
        if apply:
            for key,(table,columns) in TABLES.items(): _insert(db,table,columns,plan[key])
            db.query("INSERT INTO player.ishop_offer_import(source,completed_at,source_hash,offers,members,history_rows) VALUES ('"+
                SOURCE+"',NOW(),UNHEX('"+plan["source_hash"]+"'),"+",".join(str(result[k]) for k in ("offers","members","history"))+")")
            db.query("COMMIT")
        else: db.query("ROLLBACK")
        begun=False
        return result
    finally:
        if begun: db.query("ROLLBACK")
        db.query("SELECT RELEASE_LOCK('auriga.native-item-shop-cutover')")
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--connection-file",type=Path,required=True,help="Private JSON connection parameters for db/tools/mariadb_client.py; never commit credentials")
    parser.add_argument("--apply",action="store_true",help="Atomically apply the reviewed import; otherwise preview counts")
    args=parser.parse_args()
    import sys
    sys.path.insert(0,str(Path(__file__).resolve().parents[3]/"db/tools"))
    from mariadb_client import Client
    db=Client(**json.loads(args.connection_file.read_text(encoding="utf-8")))
    try: print(json.dumps(migrate(db,args.apply),ensure_ascii=False))
    finally: db.close()
if __name__=="__main__": main()
