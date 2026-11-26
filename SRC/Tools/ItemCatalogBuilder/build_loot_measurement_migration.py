"""Build an offline, measurement-only migration manifest. Never used for gameplay item identity."""
import argparse, ast, hashlib, json, re
from pathlib import Path

def build(catalog, aliases, artwork):
    rows=catalog["tables"]["item_base"]
    cols=catalog["columns"]["item_base"]
    rows=[dict(zip(cols,row)) if isinstance(row,list) else row for row in rows]
    active={(int(r["category_id"]),int(r["local_id"])):r for r in rows if str(r["retired"])=="0"}
    mapping={}
    for line in aliases.splitlines():
        if not line.strip() or line.startswith("#"):continue
        old,category,local=map(int,line.split(","))
        if old in mapping and mapping[old]!=(category,local):raise ValueError("Ambiguous alias")
        mapping[old]=(category,local)
    entries=[]
    for block in re.split(r"(?m)^  - vnum: ",artwork)[1:]:
        old=int(block.splitlines()[0])
        def field(name):
            m=re.search(r"(?m)^    "+name+r": (.*(?:\n      [^\n]*)*)",block)
            if not m:return ""
            value=" ".join(line.strip() for line in m[1].splitlines()).strip()
            return ast.literal_eval(value) if value.startswith('"') else value
        native=active.get(mapping.get(old))
        row=dict(prototype=old,category=0,local=0,name=field("name"),icon=field("iconPath"))
        if native:
            row.update(category=int(native["category_id"]),local=int(native["local_id"]),name=native["name_key"],icon=native["icon_asset"])
        entries.append(row)
    if len({e["prototype"] for e in entries})!=len(entries):raise ValueError("Duplicate historical identity")
    return dict(version=1,catalog_sha256=catalog["hash"],entries=entries)

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--catalog",type=Path,required=True);p.add_argument("--aliases",type=Path,required=True)
    p.add_argument("--artwork",type=Path,required=True);p.add_argument("--output",type=Path,required=True)
    a=p.parse_args()
    result=build(json.loads(a.catalog.read_bytes()),a.aliases.read_text(encoding="utf-8-sig"),a.artwork.read_text(encoding="utf-8-sig"))
    a.output.write_text(json.dumps(result,ensure_ascii=False,separators=(",",":"))+"\n",encoding="utf-8")
    print("Measurement migration entries:",len(result["entries"]))
if __name__=="__main__":main()
