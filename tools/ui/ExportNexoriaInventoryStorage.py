"""Export the supplied inventory/storage artwork with native Photoshop effects."""
from pathlib import Path
import argparse, hashlib, json, re, shutil, subprocess, uuid
import NexoriaLiveText


def main():
    from psd_tools import PSDImage
    from PIL import Image
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--project', type=Path, default=Path('D:/Auriga-Unity6/SRC/AurigaUnity-20261103'))
    parser.add_argument('--design', type=Path, default=Path('D:/Nexoria UI'))
    parser.add_argument('--cache', type=Path, default=Path('build/c41-native-cache'))
    args = parser.parse_args()
    output = args.project / 'Assets/Metin2/Resources/Metin2/NexoriaUI'
    manifest = output / 'item-windows-source.json'
    old = json.loads(manifest.read_text(encoding='utf-8'))
    prior = {a['Asset']: a['Sha256'] for w in old['Windows'] for a in w['Assets']}
    sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
    template = (output/'nx_panel_sub.png.meta').read_text(encoding='utf-8')
    scratch = args.cache.resolve(); scratch.mkdir(parents=True, exist_ok=True)
    windows = []
    for folder, key in [('Leltar', 'inventory'), ('Raktar', 'storage')]:
        source = args.design/folder/('Nexoria_'+folder+'.psd')
        script = args.design/folder/'scripts'/('nx_build_'+folder.lower()+'.jsx')
        psd = PSDImage.open(source)
        def walk(group, path=()):
            for layer in group:
                p = path+(layer.name,); yield layer, p
                if layer.is_group(): yield from walk(layer, p)
        layers = list(walk(psd)); specs = []; assets = []; controls = []
        def asset(name, predicate):
            name = 'nxi_'+key+'_'+name
            target = output/(name+'.png')
            if target.exists() and sha(target) != prior.get(name): raise RuntimeError('Unrecorded export edit: '+name)
            specs.append(dict(Asset=name, Path=[], States=[dict(Path=list(p), Visible=bool(predicate(l,p))) for l,p in layers]))
            assets.append(dict(Asset=name)); return name
        def only(*paths):
            paths = [tuple(p) for p in paths]
            return lambda layer,p: any(p[:len(q)] == q or q[:len(p)] == p for q in paths) and not ('rejtett' in layer.name.lower())
        if key == 'inventory':
            def base_filter(layer,p):
                if not layer.visible: return False
                if p[0].startswith('Eredeti '): return False
                if p[:2] == ('Ablakkeret és fejléc','Kategória fülek'): return False
                if p[:2] == ('Karakter és felszerelés','Felszerelés helyek'): return False
                if p[:2] in [('Karakter és felszerelés','Részletek gomb'),('Karakter és felszerelés','Szett gomb')]: return False
                if p[0] == 'Tárgyak' and len(p)>1:
                    if p[1].startswith('Hely '): return False
                    if p[1:3] in [('Lapozó','Oldalszám'),('Súly','Felirat'),('Súly','Töltés'),('Pénz','Összeg'),('Pénz','Bolt gomb')]: return False
                return True
            base = asset('base', base_filter)
            tab_parent = ('Ablakkeret és fejléc','Kategória fülek')
            tab_names = ['Összes (aktív)','Fegyverek','Páncélok','Ékszerek','Italok','Alapanyagok','Ládák','Kedvencek']
            for i,n in enumerate(tab_names): asset('tab_'+str(i)+'_icon', only(tab_parent+('Fül — '+n,'Ikon')))
            asset('tab_active',only(tab_parent+('Fül — Összes (aktív)','Háttér')))
            asset('tab_idle',only(tab_parent+('Fül — Fegyverek','Háttér')))
            asset('cell',only(('Tárgyak','Hely 5-1 — üres','Hely')))
            asset('capacity_fill',only(('Tárgyak','Súly','Töltés')))
            for name, path, rect in [('details',('Karakter és felszerelés','Részletek gomb'),[112,1034,214,66]),('set',('Karakter és felszerelés','Szett gomb'),[348,1034,216,66]),('shop',('Tárgyak','Pénz','Bolt gomb'),[990,1094,54,52])]:
                controls.append(dict(Key=name,Asset=asset('button_'+name,only(path)),Rect=rect,DynamicCaption=False))
            close = [956,82,68,64]
        else:
            def base_filter(layer,p):
                if not layer.visible: return False
                if p[0] in ('Fülek','Oldal — Raktár','Oldal — Tárgypiac raktár'): return False
                if p[:2] == ('Ablakkeret és fejléc','Alcím'): return False
                if p[0]=='Közös háttér' and len(p)>1 and p[1].startswith('Bezár gomb'): return False
                return True
            base = asset('base',base_filter)
            for i,n in enumerate(('Raktár','Tárgypiac raktár')):
                for active in (False,True):
                    prefix='Fül — '+n
                    def tab(layer,p,prefix=prefix,active=active):
                        return p == ('Fülek',) or (len(p)>1 and p[0]=='Fülek' and ((p[1]==prefix+' (aktív)' if active else p[1]==prefix) or p[1] in (prefix+' — felirat',prefix+' — ikon')))
                    asset('tab_'+str(i)+('_active' if active else ''),tab)
            asset('cell',only(('Oldal — Raktár','Tárgyhelyek','Tárgyhely 45')))
            asset('page_strip',only(('Oldal — Raktár','Oldalválasztó','Oldal — keret')))
            asset('page_active',only(('Oldal — Raktár','Oldalválasztó','Oldal — kijelölés')))
            for name,path,prefix,rect in [('close',('Közös háttér',),'Bezár gomb',[900,920,120,52]),('password',('Oldal — Raktár',),'Jelszó változtatás',[620,920,264,52]),('all',('Oldal — Tárgypiac raktár',),'Összes kivétele',[620,920,264,52]),('refresh',('Oldal — Tárgypiac raktár','Fejléc'),'Frissítés',[992,248,48,48]),('search',('Oldal — Raktár','Oldalválasztó'),'Keresés',[992,248,48,48]),('sort',('Oldal — Raktár','Oldalválasztó'),'Rendezés',[932,248,48,48])]:
                def button(layer,p,path=path,prefix=prefix):
                    return path[:len(p)]==p or (p[:len(path)]==path and len(p)>len(path) and p[len(path)].startswith(prefix) and 'rejtett' not in p[len(path)])
                controls.append(dict(Key=name,Asset=asset('button_'+name,button),Rect=rect,DynamicCaption=False))
            close=[990,82,44,44]
        NexoriaLiveText.prepare(layers,specs,assets,asset)
        config=dict(Source=source.as_posix(),Output=scratch.as_posix(),Specs=specs,LiveText=NexoriaLiveText.VERSION)
        cache_key=hashlib.sha256((json.dumps(config,sort_keys=True)+sha(source)+sha(script)).encode()).hexdigest()
        cached=scratch/(key+'.record.json')
        if cached.exists():
            saved=json.loads(cached.read_text(encoding='utf-8'))
            if saved['CacheKey']==cache_key and all((scratch/(a['Asset']+'.cropped.png')).exists() and sha(scratch/(a['Asset']+'.cropped.png'))==a['Sha256'] for a in saved['Window']['Assets']):
                NexoriaLiveText.refresh(saved['Window']['Assets'],assets)
                windows.append(saved['Window']);print('NEXORIA_INVENTORY_STORAGE=CACHED '+key,flush=True);continue
        code='var NX_EXPORT='+json.dumps(config,ensure_ascii=True)+';\n'+(Path(__file__).parent/'ExportNexoriaSettings.jsx').read_text(encoding='utf-8')
        jsx=scratch/(key+'.jsx');jsx.write_text(code,encoding='utf-8')
        subprocess.run(['powershell','-NoProfile','-STA','-ExecutionPolicy','Bypass','-File',str(Path(__file__).parent/'RunPhotoshopExport.ps1'),'-ScriptPath',str(jsx)],check=True)
        for record in assets:
            with Image.open(scratch/(record['Asset']+'.png')) as rendered:
                target=scratch/(record['Asset']+'.cropped.png')
                record.update(Bounds=NexoriaLiveText.crop(record,rendered.convert('RGBA'),target),Sha256=sha(target))
        window=dict(Key=key,Width=psd.width,Height=psd.height,Source=source.as_posix(),SourceSha256=sha(source),GeometrySource=script.as_posix(),GeometrySha256=sha(script),NativePhotoshop=True,Base=base,CloseRect=close,Assets=assets,Controls=controls)
        windows.append(window);cached.write_text(json.dumps(dict(CacheKey=cache_key,Window=window),ensure_ascii=False,indent=2),encoding='utf-8')
        print('NEXORIA_INVENTORY_STORAGE=RENDERED '+key,flush=True)
    for window in windows:
        for record in window['Assets']:
            target=output/(record['Asset']+'.png')
            if target.exists() and sha(target)!=prior.get(record['Asset']):raise RuntimeError('Concurrent export edit: '+str(target))
            shutil.copy2(scratch/(record['Asset']+'.cropped.png'),target)
            meta=target.with_suffix('.png.meta')
            if not meta.exists():meta.write_text(re.sub(r'guid: [a-f0-9]+','guid: '+uuid.uuid4().hex,template,count=1),encoding='utf-8')
    old['Windows']=[w for w in old['Windows'] if w['Key'] not in ('inventory','storage')]+windows
    manifest.write_text(json.dumps(old,indent=2,ensure_ascii=False)+'\n',encoding='utf-8')
    print('NEXORIA_INVENTORY_STORAGE=EXPORTED documents=2 assets='+str(sum(len(w['Assets']) for w in windows)),flush=True)


if __name__=='__main__':main()
