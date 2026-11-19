"""Export the original death recap PSD, excluding demonstration game data."""
from pathlib import Path
import argparse,hashlib,json,re,shutil,subprocess,uuid
import NexoriaLiveText

def main():
    from psd_tools import PSDImage
    from PIL import Image
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--project',type=Path,default=Path('D:/Auriga-Unity6/SRC/AurigaUnity-20261103'))
    p.add_argument('--design',type=Path,default=Path('D:/Nexoria UI'))
    p.add_argument('--cache',type=Path,default=Path('build/nexoria-death-export'))
    a=p.parse_args();out=a.project/'Assets/Metin2/Resources/Metin2/NexoriaUI';manifest=out/'item-windows-source.json'
    original=manifest.read_bytes();data=json.loads(original);prior={x['Asset']:x['Sha256'] for w in data['Windows'] for x in w['Assets']}
    sha=lambda path:hashlib.sha256(path.read_bytes()).hexdigest()
    windows=[]
    for folder,key in [('Halal_Osszesito','death_recap')]:
        source=next((a.design/folder).glob('*.psd'));geometry=next((a.design/folder/'scripts').glob('nx_build_*.jsx'))
        source_hash=sha(source);psd=PSDImage.open(source);layers=[];specs=[];assets=[];controls=[]
        cache=(a.cache/key).resolve();cache.mkdir(parents=True,exist_ok=True);prefix='nxi_'+key+'_'
        def walk(group,path=()):
            for layer in group:
                q=path+(layer.name,);layers.append((layer,q))
                if layer.is_group():walk(layer,q)
        walk(psd)
        def only(*paths):
            wanted=[tuple(path.split('/')) for path in paths]
            for path in wanted:assert sum(q==path for _,q in layers)==1,path
            return lambda l,q:l.visible and any(q[:len(path)]==path or path[:len(q)]==q for path in wanted)
        def asset(name,keep):
            name=prefix+name;target=out/(name+'.png')
            if target.exists() and sha(target)!=prior.get(name):raise RuntimeError('Unrecorded artwork edit: '+name)
            specs.append(dict(Asset=name,Path=[],States=[dict(Path=list(q),Visible=bool(keep(l,q))) for l,q in layers]));assets.append(dict(Asset=name));return name
        def keep(l,q):
            if not l.visible:return False
            if q[0]=='Fejléc':return len(q)==1 or q[1]=='Cím'
            if q[0] not in ('Utolsó 10 mp','Források és tanács'):return False
            if len(q)==1:return True
            return q[1]=='Panel' or q[1].startswith('Szakasz') or q[1] in ('Életerő lefutás — sín','Életerő lefutás — címke')
        base=asset('base',keep)
        for name,path in [('row','Utolsó 10 mp/Sebzés 1'),('row_selected','Utolsó 10 mp/Sebzés 6'),('critical','Utolsó 10 mp/Sebzés 4 — kritikus'),('track','Források és tanács/Forrás — Orkfőnök — sín'),('bar_hit','Források és tanács/Forrás — Orkharcos — kitöltés'),('bar_skill','Források és tanács/Forrás — Orkfőnök — kitöltés'),('bar_dot','Források és tanács/Forrás — Méreg — kitöltés'),('tip_row','Források és tanács/Tanács 1')]:asset(name,only(path))
        for kind,index in [('hit',2),('skill',1),('dot',3)]:asset('icon_'+kind,only(f'Utolsó 10 mp/Sebzés {index} — ikon kör',f'Utolsó 10 mp/Sebzés {index} — ikon'))
        for i in range(1,4):asset('tip_icon_'+str(i),only(f'Források és tanács/Tanács {i} — ikon'))
        for name,label,x,w in [('town','Újraéledés a városban',300,400),('here','Újraéledés helyben',720,480)]:
            paths=[q for l,q in layers if len(q)==2 and q[0]=='Újraéledés' and q[1].startswith(label+' —') and l.visible]
            controls.append(dict(Key=name,Asset=asset(name,only(*['/'.join(q) for q in paths])),Rect=[x,790,w,62],DynamicCaption=False))
        text_layers=[(l,q) for l,q in layers]
        NexoriaLiveText.prepare(text_layers,specs,assets,asset)
        config=dict(Source=source.as_posix(),Output=cache.as_posix(),Specs=specs,LiveText=NexoriaLiveText.VERSION)
        cache_key=hashlib.sha256((json.dumps(config,sort_keys=True)+source_hash+sha(geometry)).encode()).hexdigest();record=cache/'record.json'
        saved=json.loads(record.read_text(encoding='utf-8')) if record.exists() else None
        if saved and saved['CacheKey']==cache_key and all((cache/(x['Asset']+'.cropped.png')).exists() and sha(cache/(x['Asset']+'.cropped.png'))==x['Sha256'] for x in saved['Window']['Assets']):window=saved['Window'];NexoriaLiveText.refresh(window['Assets'],assets)
        else:
            jsx=cache/'export.jsx';jsx.write_text('var NX_EXPORT='+json.dumps(config,ensure_ascii=True)+';\n'+(Path(__file__).parent/'ExportNexoriaSettings.jsx').read_text(encoding='utf-8'),encoding='utf-8')
            subprocess.run(['powershell','-NoProfile','-STA','-ExecutionPolicy','Bypass','-File',str(Path(__file__).parent/'RunPhotoshopExport.ps1'),'-ScriptPath',str(jsx)],check=True)
            for x in assets:
                target=cache/(x['Asset']+'.cropped.png')
                with Image.open(cache/(x['Asset']+'.png')) as im:x.update(Bounds=NexoriaLiveText.crop(x,im.convert('RGBA'),target),Sha256=sha(target))
            window=dict(Key=key,Width=psd.width,Height=psd.height,Source=source.as_posix(),SourceSha256=source_hash,GeometrySource=geometry.as_posix(),GeometrySha256=sha(geometry),NativePhotoshop=True,Base=base,CloseRect=[psd.width-80,40,40,40],Assets=assets,Controls=controls)
            record.write_text(json.dumps(dict(CacheKey=cache_key,Window=window),ensure_ascii=False,indent=2),encoding='utf-8')
        assert sha(source)==source_hash
        windows.append(window)
    assert manifest.read_bytes()==original,'Shared manifest changed during export'
    template=(out/'nx_panel_sub.png.meta').read_text(encoding='utf-8')
    for window in windows:
        for x in window['Assets']:
            target=out/(x['Asset']+'.png');shutil.copy2(a.cache/window['Key']/(x['Asset']+'.cropped.png'),target)
            meta=target.with_suffix('.png.meta')
            if not meta.exists():meta.write_text(re.sub(r'guid: [a-f0-9]+','guid: '+uuid.uuid4().hex,template,count=1),encoding='utf-8')
    keys={w['Key'] for w in windows};data['Windows']=[w for w in data['Windows'] if w['Key'] not in keys]+windows
    manifest.write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print('NEXORIA_DEATH_RECAP=EXPORTED windows=1 assets='+str(sum(len(w['Assets']) for w in windows)),flush=True)

if __name__=='__main__':main()
