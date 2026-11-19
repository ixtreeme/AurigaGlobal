"""Package the active client's authored icons without changing catalog data."""
import argparse,hashlib,io,json,sys
from pathlib import Path
from zipfile import ZipFile,ZIP_DEFLATED
from PySide6.QtCore import QBuffer,QByteArray,QIODevice
from PySide6.QtGui import QImage
from PIL import Image


def export(catalog,unity,output):
    packet=json.loads(catalog.read_text(encoding='utf-8-sig'))
    column=packet['columns']['item_base'].index('icon_asset')
    assets=sorted({row[column].replace('\\','/').casefold() for row in packet['tables']['item_base']})
    root=unity/'Assets/Metin2/Generated/ItemIcons'
    files={p.relative_to(root).as_posix().casefold():p for p in root.rglob('*') if p.is_file() and p.suffix.casefold()!='.meta'}
    # This authored quest icon is converted to PNG by the Unity importer.
    files['ui/game/quest/questicon/level_07.tga']=unity/'Assets/Metin2/Resources/Metin2/QuestIcons/level_07.png'
    icons={};missing=[];written=set();output.parent.mkdir(parents=True,exist_ok=True)
    with ZipFile(output,'w',ZIP_DEFLATED,compresslevel=6) as archive:
        for asset in assets:
            path=files.get(asset)
            if path is None or not path.is_file():missing.append(asset);continue
            image=QImage(str(path))
            if image.isNull():
                # Qt's TGA reader does not support the original RLE variant.
                with Image.open(path) as original:
                    png=io.BytesIO();original.convert('RGBA').save(png,format='PNG');image=QImage.fromData(png.getvalue())
            if image.isNull():raise ValueError('Nem olvasható tárgyikon: '+str(path))
            data=QByteArray();buffer=QBuffer(data);buffer.open(QIODevice.OpenModeFlag.WriteOnly)
            if not image.save(buffer,'PNG'):raise ValueError('Nem alakítható át a tárgyikon: '+asset)
            raw=bytes(data);member='images/'+hashlib.sha256(raw).hexdigest()+'.png'
            if member not in written:archive.writestr(member,raw);written.add(member)
            icons[asset]=member
        manifest=dict(version=1,icons=icons,missing=missing,source='active Unity item and quest icons')
        archive.writestr('icons.json',json.dumps(manifest,ensure_ascii=False,separators=(',',':')))
    print('ITEM_ICONS_EXPORTED=%d unique_images=%d missing=%d bytes=%d'%(len(icons),len(written),len(missing),output.stat().st_size))


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--catalog',required=True,type=Path);p.add_argument('--unity',required=True,type=Path);p.add_argument('--output',required=True,type=Path)
    a=p.parse_args();export(a.catalog,a.unity,a.output)
