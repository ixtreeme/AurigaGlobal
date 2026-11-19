"""Render the supplied NPC shop, player trade and ESC menu, preserving Photoshop effects."""
from pathlib import Path
import argparse, hashlib, json, re, shutil, subprocess, tempfile, uuid
from contextlib import nullcontext
import NexoriaLiveText


def main():
    from psd_tools import PSDImage
    from PIL import Image
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--project', type=Path, default=Path('D:/Auriga-Unity6/SRC/AurigaUnity-20261103'))
    parser.add_argument('--design', type=Path, default=Path('D:/Nexoria UI'))
    parser.add_argument('--cache',type=Path,help='Keep completed native exports so an interrupted Photoshop job can resume.')
    args = parser.parse_args()
    out = args.project/'Assets/Metin2/Resources/Metin2/NexoriaUI'
    manifest = out/'item-windows-source.json'
    old = json.loads(manifest.read_text(encoding='utf-8')) if manifest.exists() else {'Windows': []}
    prior = {a['Asset']: a['Sha256'] for w in old['Windows'] for a in w['Assets']}
    sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
    template = (out/'nx_panel_sub.png.meta').read_text(encoding='utf-8')
    windows = []
    # Rectangles are the original authoring JSX coordinates, with no legacy-prefab geometry.
    menus = [('Segítség',404),('Rendszer opciók',484),('Játék opciók',556),('Karakter csere',636),('Kilépés',708),('Játék befejezése',780),('Vissza a játékba',872)]
    plans = [('Bolt','shop','nx_build_bolt.jsx',[("buy","Tárgy részletei","Vásárlás gomb",[904,744,412,54],True),("sell","Lábléc","Eladás gomb",[960,872,180,52],False),("close","Lábléc","Bezár gomb",[1156,872,164,52],False)]),
             ('Kereskedes','trade','nx_build_kereskedes.jsx',[("cancel","Lábléc","Mégse gomb",[920,780,190,52],False),("approve","Lábléc","Elfogadás gomb",[1126,780,194,52],True)]),
             ('Rendszer_Menu','menu','nx_build_rendszer_menu.jsx',[(str(i),'Menüpontok',name,[70,y,620,60],False) for i,(name,y) in enumerate(menus)])]
    with (nullcontext(args.cache.resolve()) if args.cache else tempfile.TemporaryDirectory(prefix='nexoria-authored-items-')) as scratch:
        scratch=Path(scratch)
        scratch.mkdir(parents=True,exist_ok=True)
        for folder,key,script_name,buttons in plans:
            source=args.design/folder/('Nexoria_'+folder+'.psd');script=args.design/folder/'scripts'/script_name
            psd=PSDImage.open(source)
            def walk(group,path=()):
                for layer in group:
                    p=path+(layer.name,)
                    yield layer,p
                    if layer.is_group(): yield from walk(layer,p)
            layers=list(walk(psd));specs=[];assets=[]
            def asset(name,predicate):
                name='nxi_'+key+'_'+name
                if (out/(name+'.png')).exists() and sha(out/(name+'.png'))!=prior.get(name):raise RuntimeError('Unrecorded export edits: '+name)
                specs.append(dict(Asset=name,Path=[],States=[dict(Path=list(p),Visible=bool(l.visible) and predicate(l,p)) for l,p in layers]))
                assets.append(dict(Asset=name))
                return name
            button_prefixes=[(parent,prefix) for _,parent,prefix,_,_ in buttons]
            def common(layer,path):
                name=layer.name
                if len(path)>1 and path[0]=='Ablakkeret és fejléc' and name=='Alcím':return False
                if any(path[0]==parent and name.startswith(prefix+' —') for parent,prefix in button_prefixes):return False
                if key=='shop':
                    if path[0]=='Oldalfülek':return False
                    if path[0]=='Kínálat' and (name.startswith('Tárgyhely ') or name.startswith('Görgető')):return False
                    if path[0]=='Tárgy részletei':
                        if name.startswith(('Kiválasztott tárgy','Megnevezés —','Érték —','Ár —')) or name in ('Név','Típus','Ár címke','Mennyiség szám'):return False
                    if path[0]=='Lábléc' and name in ('Saját Yang — összeg','Tipp'):return False
                elif key=='trade':
                    if path[0].startswith('Fél —') and (name.startswith(('Tárgyhely ','Állapot')) or name in ('Elfogadva keret','Portré — portré','Név','Adatok','Yang összeg','Tipp')):return False
                    if path[0]=='Lábléc' and name=='Megjegyzés':return False
                else:
                    if path[0]=='Karakter' and (name in ('Név','Adatok','Portré — portré') or name.startswith(('Online idő','Csatorna —'))):return False
                    if path[0]=='Menüpontok' and name=='Verzió':return False
                return True
            base=asset('base',common)
            controls=[]
            for control,parent,prefix,rect,dynamic in buttons:
                def selected(layer,path,parent=parent,prefix=prefix,dynamic=dynamic):
                    if path[0]!=parent:return False
                    if len(path)==1:return True
                    return path[1].startswith(prefix+' —') and not (dynamic and path[1].endswith((' — felirat',' — ikon')))
                art=asset('button_'+control,selected)
                item=dict(Key=control,Asset=art,Rect=rect,DynamicCaption=dynamic)
                if key=='shop' and control=='buy':item.update(Glyph='shop',GlyphRect=[144,27,20])
                if key=='trade' and control=='approve':item.update(Glyph='equip',GlyphRect=[35,26,18])
                controls.append(item)
            if key=='shop':
                for i,name in enumerate(('Fegyverek','Páncélok','Egyéb')):
                    for active in (False,True):
                        prefix='Fül — '+name
                        def tab(layer,path,prefix=prefix,active=active):
                            if path[0]!='Oldalfülek':return False
                            if len(path)==1:return True
                            if path[1]==prefix+' (aktív)':return active
                            return path[1]==prefix or path[1].startswith(prefix+' —')
                        art=asset('tab_'+str(i)+('_selected' if active else ''),tab)
                        # Active siblings are intentionally hidden in the document, make them visible in this export only.
                        if active:
                            for state in specs[-1]['States']:
                                if state['Path']==['Oldalfülek',prefix+' (aktív)']:state['Visible']=True
            NexoriaLiveText.prepare(layers,specs,assets,asset)
            config=dict(Source=source.as_posix(),Output=scratch.as_posix(),Specs=specs,LiveText=NexoriaLiveText.VERSION)
            cache_key=hashlib.sha256((json.dumps(config,sort_keys=True)+sha(source)+sha(script)).encode()).hexdigest()
            cached=scratch/(key+'.record.json')
            if cached.exists():
                saved=json.loads(cached.read_text(encoding='utf-8'))
                if saved['CacheKey']==cache_key and all((scratch/(a['Asset']+'.cropped.png')).exists() and sha(scratch/(a['Asset']+'.cropped.png'))==a['Sha256'] for a in saved['Window']['Assets']):
                    saved['Window']['Controls']=controls
                    NexoriaLiveText.refresh(saved['Window']['Assets'],assets)
                    cached.write_text(json.dumps(saved,ensure_ascii=False,indent=2),encoding='utf-8')
                    windows.append(saved['Window']);print('NEXORIA_AUTHORED_WINDOW=CACHED '+key,flush=True);continue
            code='var NX_EXPORT='+json.dumps(config,ensure_ascii=True)+';\n'+(Path(__file__).parent/'ExportNexoriaSettings.jsx').read_text(encoding='utf-8')
            jsx=scratch/(key+'.jsx');jsx.write_text(code,encoding='utf-8')
            subprocess.run(['powershell','-NoProfile','-STA','-ExecutionPolicy','Bypass','-File',str(Path(__file__).parent/'RunPhotoshopExport.ps1'),'-ScriptPath',str(jsx)],check=True)
            for record in assets:
                with Image.open(scratch/(record['Asset']+'.png')) as rendered:
                    target=scratch/(record['Asset']+'.cropped.png')
                    record.update(Bounds=NexoriaLiveText.crop(record,rendered.convert('RGBA'),target),Sha256=sha(target))
            windows.append(dict(Key=key,Width=psd.width,Height=psd.height,Source=source.as_posix(),SourceSha256=sha(source),GeometrySource=script.as_posix(),GeometrySha256=sha(script),NativePhotoshop=True,Base=base,CloseRect=[psd.width-110,82,44,44],Assets=assets,Controls=controls))
            cached.write_text(json.dumps(dict(CacheKey=cache_key,Window=windows[-1]),ensure_ascii=False,indent=2),encoding='utf-8')
            print('NEXORIA_AUTHORED_WINDOW=RENDERED '+key,flush=True)
        # All sources rendered before any existing resource is replaced.
        for window in windows:
            for record in window['Assets']:
                target=out/(record['Asset']+'.png')
                if target.exists() and sha(target)!=prior.get(record['Asset']):raise RuntimeError('Concurrent export edits: '+str(target))
                shutil.copy2(scratch/(record['Asset']+'.cropped.png'),target)
                meta=target.with_suffix('.png.meta')
                if not meta.exists():meta.write_text(re.sub(r'guid: [a-f0-9]+','guid: '+uuid.uuid4().hex,template,count=1),encoding='utf-8')
        keys={w['Key'] for w in windows}
        manifest.write_text(json.dumps(dict(Windows=[w for w in old['Windows'] if w['Key'] not in keys]+windows),indent=2,ensure_ascii=False)+'\n',encoding='utf-8')
    print('NEXORIA_AUTHORED_ITEM_WINDOWS=EXPORTED windows='+str(len(windows)),flush=True)


if __name__=='__main__':main()
