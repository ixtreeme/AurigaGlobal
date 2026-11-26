#!/usr/bin/env python3
"""Explicit one-time historical construction-recipe import. Apply 058 first.
The running server never reads the old materials column. Stop game services
before applying; the receipt permits a safe no-op on repeated invocation.
"""
import argparse,hashlib,json,sys
from pathlib import Path
SOURCE="native-guild-building-materials-v1"
# Historical source decoding exists only in this offline importer.
MATERIALS={90010:(6,755),90011:(6,756),90012:(6,757)}
def build_plan(rows):
    recipes=[]
    for proto,text in rows:
        if int(proto)<=0:raise ValueError("Invalid building prototype")
        quantities={}
        for token in (text or "").strip().strip('"').split("/"):
            token=token.strip()
            if not token or token=="0":continue
            pair=[x.strip() for x in token.split(",")]
            if len(pair)!=2 or not all(x.isdigit() for x in pair):raise ValueError("Malformed source material")
            source,count=map(int,pair)
            if not source and not count:continue
            if source not in MATERIALS or not 1<=count<=0xffffffff:raise ValueError("Unapproved construction material")
            key=MATERIALS[source];quantities[key]=quantities.get(key,0)+count
            if quantities[key]>0xffffffff:raise ValueError("Material count overflow")
        native=[dict(category=c,local=l,count=n) for (c,l),n in sorted(quantities.items())]
        recipes.append((int(proto),native))
    if len({p for p,_ in recipes})!=len(recipes):raise ValueError("Duplicate building prototype")
    digest=hashlib.sha256(json.dumps(rows,separators=(",",":")).encode()).hexdigest()
    return dict(recipes=recipes,source_hash=digest)
def migrate(db,apply=False):
    if db.query("SELECT GET_LOCK('auriga.native-building-materials',10)")!=[["1"]]:raise RuntimeError("Another import is running")
    begun=False
    try:
        receipt=db.query("SELECT recipes,LOWER(HEX(source_hash)) FROM player.guild_building_material_import WHERE source='"+SOURCE+"'")
        if receipt:return dict(already_applied=True,recipes=int(receipt[0][0]),source_hash=receipt[0][1])
        db.query("START TRANSACTION");begun=True
        rows=db.query("SELECT vnum,materials FROM player.object_proto ORDER BY vnum FOR UPDATE")
        plan=build_plan(rows)
        if int(db.query("SELECT COUNT(*) FROM player.object_proto WHERE native_materials IS NOT NULL")[0][0]):
            raise RuntimeError("Native recipes already exist without an import receipt")
        if apply:
            for proto,native in plan["recipes"]:
                payload=json.dumps(native,separators=(",",":")).encode().hex()
                db.query("UPDATE player.object_proto SET native_materials=CONVERT(0x"+payload+" USING utf8mb4) WHERE vnum="+str(proto)+" AND native_materials IS NULL")
            db.query("INSERT INTO player.guild_building_material_import VALUES ('"+SOURCE+"',UNHEX('"+plan["source_hash"]+"'),"+str(len(rows))+",NOW())")
            db.query("COMMIT")
        else:db.query("ROLLBACK")
        begun=False
        return dict(already_applied=False,applied=apply,recipes=len(rows),source_hash=plan["source_hash"])
    finally:
        if begun:db.query("ROLLBACK")
        db.query("SELECT RELEASE_LOCK('auriga.native-building-materials')")
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--connection-file",type=Path,required=True)
    parser.add_argument("--apply",action="store_true")
    args=parser.parse_args()
    sys.path.insert(0,str(Path(__file__).resolve().parents[3]/"db/tools"))
    from mariadb_client import Client
    db=Client(**json.loads(args.connection_file.read_text(encoding="utf-8")))
    try:print(json.dumps(migrate(db,args.apply)))
    finally:db.close()
if __name__=="__main__":main()
