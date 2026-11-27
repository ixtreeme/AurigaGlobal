"""Bake already-imported portrait references into Unity artwork records without runtime numeric fallbacks."""
import argparse,re
from pathlib import Path
SHARED={"a0c0e517befe5f8469248e71afaf6291","4e9726aea1722a741a11ff54589955fa"}
def bake(text,resources):
    changes=[]
    def record(match):
        body=match[0];source=int(match[1])
        if not 630000<=source<632000:return body
        icon=re.search(r"(?m)^    icon: (.*)$",body)
        if not icon or ("fileID: 0" not in icon[1] and not any(g in icon[1] for g in SHARED)):return body
        asset=resources/"Metin2/ItemIcons/Tera"/(str(source)+".png")
        meta=asset.with_name(asset.name+".meta")
        if not asset.is_file() or not meta.is_file():return body
        guid=re.search(r"(?m)^guid: ([0-9a-f]{32})$",meta.read_text(encoding="utf-8-sig"))
        if not guid:raise ValueError("Missing portrait GUID: "+str(asset))
        body=re.sub(r"(?m)^    iconPath:.*$","    iconPath: Metin2/ItemIcons/Tera/"+str(source)+".png",body)
        body=re.sub(r"(?m)^    icon:.*$","    icon: {fileID: 21300000, guid: "+guid[1]+", type: 3}",body)
        changes.append(source);return body
    result=re.sub(r"(?m)^  - vnum: ([0-9]+)\n[\s\S]*?(?=^  - vnum: |\Z)",record,text)
    before=re.findall(r"(?m)^    (?:model|modelPath):.*$",text)
    after=re.findall(r"(?m)^    (?:model|modelPath):.*$",result)
    if before!=after:raise ValueError("Model reference changed during portrait bake")
    return result,changes
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument("--unity",type=Path,required=True);p.add_argument("--apply",action="store_true")
    a=p.parse_args();catalog=a.unity/"Assets/Metin2/GeneratedData/Metin2ItemCatalog.asset"
    text=catalog.read_text(encoding="utf-8-sig");result,changes=bake(text,a.unity/"Assets/Metin2/Resources")
    if a.apply and changes:catalog.write_text(result,encoding="utf-8",newline="\n")
    print("Baked portraits:" if a.apply else "Portraits to bake:",len(changes))
if __name__=="__main__":main()
