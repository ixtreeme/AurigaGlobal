"""Export the three original dungeon PSDs, excluding demonstration game data."""
from pathlib import Path
import argparse,hashlib,json,re,shutil,subprocess,uuid
import NexoriaLiveText

def main():
    from psd_tools import PSDImage
    from PIL import Image
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--project',type=Path,default=Path('D:/Auriga-Unity6/SRC/AurigaUnity-20261103'))
    p.add_argument('--design',type=Path,default=Path('D:/Nexoria UI'))
    p.add_argument('--cache',type=Path,default=Path('build/nexoria-dungeon-export'))
    a=p.parse_args();out=a.project/'Assets/Metin2/Resources/Metin2/NexoriaUI';manifest=out/'item-windows-source.json'
    original=manifest.read_bytes();data=json.loads(original);prior={x['Asset']:x['Sha256'] for w in data['Windows'] for x in w['Assets']}
    sha=lambda path:hashlib.sha256(path.read_bytes()).hexdigest()
    windows=[]
    for folder,key in [('Kazamata','dungeon_info'),('Kazamata_Eredmeny','dungeon_result'),('Kazamata_Ranglista','dungeon_rank')]:
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
        def control(name,parent,label,rect):
            paths=[q for l,q in layers if len(q)==2 and q[0]==parent and q[-1].startswith(label+' —') and l.visible]
            assert paths,label
            controls.append(dict(Key=name,Asset=asset(name,only(*['/'.join(q) for q in paths])),Rect=rect,DynamicCaption=False))
        if key=='dungeon_info':
            def keep(l,q):
                if not l.visible:return False
                if q[0]=='Ablakkeret és fejléc':return q[-1]!='Alcím'
                if q[0]=='Kazamata lista':return len(q)==1 or q[1] in ('Panel','Szűrő — keret','Szűrő — Mind','Szűrő — Elérhető')
                if q[0]!='Részletek':return False
                if len(q)==1:return True
                n=q[1]
                return n in ('Panel','Jel','Jel — ikon','Belépő tárgy') or n.startswith(('Szakasz','Statisztika címke','Statisztika —')) or (n.startswith('Adat —') and not n.endswith('érték'))
            base=asset('base',keep)
            row='Kazamata lista/Kazamata — Démontorony';selected='Kazamata lista/Kazamata — Sárkány barlang'
            for name,path in [('row',row+'/Sor'),('row_selected',selected+'/Sor'),('icon',row+'/Jel'),('icon_glyph',row+'/Jel — ikon'),('filter_selected','Kazamata lista/Szűrő — kijelölés'),('badge_ok',row+'/Állapot'),('badge_wait','Kazamata lista/Kazamata — Vörös erdő/Állapot'),('badge_locked','Kazamata lista/Kazamata — Jégkirálynő terme/Állapot')]:asset(name,only(path))
            for name,label,x,w in [('rank_completed','Teljesítés',908,130),('rank_time','Idő',1047,130),('rank_damage','Sebzés',1186,130)]:control(name,'Részletek','Ranglista — '+label,[x,676,w,48])
            control('find_team','Részletek','Csapat keresése',[514,800,386,56]);control('teleport','Részletek','Teleportálás',[916,800,400,56])
        elif key=='dungeon_result':
            def keep(l,q):
                if not l.visible:return False
                if q[0]=='Ablakkeret és fejléc':return q[-1]!='Alcím'
                if q[0]=='Értékelés':return len(q)==1 or q[1] in ('Panel','Fény') or q[1].endswith(('címke','egység'))
                if q[0]=='Sebzésmérő':return len(q)==1 or q[1] in ('Panel','Nézet — keret','Nézet — Sebzés','Nézet — Gyógyítás','Nézet — Kapott')
                if q[0]=='Jutalmak':return len(q)==1 or q[1]=='Panel' or q[1].startswith(('Szakasz','Jutalom —','Jutalom címke')) or re.fullmatch(r'Zsákmány [1-6]',q[1]) is not None
                return False
            base=asset('base',keep)
            asset('stars',only('Értékelés/Csillagok'))
            for name,path in [('row','Sebzésmérő/1. Kira'),('row_selected','Sebzésmérő/4. ShadowKnight'),('bar_background','Sebzésmérő/1. Kira — sáv háttér'),('bar_damage','Sebzésmérő/1. Kira — sáv'),('bar_taken','Sebzésmérő/4. ShadowKnight — sáv'),('bar_healing','Sebzésmérő/5. Luna — sáv'),('mvp','Sebzésmérő/1. Kira — MVP'),('record','Értékelés/Statisztika — Idő — rekord'),('segment','Sebzésmérő/Nézet — kijelölés')]:asset(name,only(path))
            for name,label,x,w in [('exit','Kilépés',60,240),('share','Megosztás',316,240),('retry','Újra',1100,340)]:control(name,'Műveletek',label,[x,868,w,56])
        else:
            page='Oldal — Kazamata'
            def keep(l,q):
                if not l.visible:return False
                if q[0]=='Ablakkeret és fejléc':return q[-1]!='Alcím'
                if q[0]=='Fülek':return len(q)==1 or 'Sárkányfészek' not in q[1]
                if q[0]!=page:return False
                if len(q)==1:return True
                if q[1]=='Kazamata választó — keret':return True
                if q[1]=='Dobogó':return len(q)==2 or q[2]=='Panel' or len(q)==3 or q[-1] in ('Fény','Portré — háttér','Portré — gyűrű','Portré — gyémánt','Talapzat','Helyezés')
                if q[1]=='Lista':return len(q)==2 or q[2]=='Panel' or q[2]=='Fejléc sáv' or q[2].startswith('Oszlop —')
                return False
            base=asset('base',keep)
            asset('segment',only(page+'/Kazamata választó — kijelölés'))
            # Rows, podium shapes and column captions retain their exact source artwork.
            asset('row_selected',only(page+'/Lista/Saját sor'))
            asset('row',only(page+'/Lista/Sor 4'))
            # Column captions are rebound to the actual selected server metric by the client.
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
            window=dict(Key=key,Width=psd.width,Height=psd.height,Source=source.as_posix(),SourceSha256=source_hash,GeometrySource=geometry.as_posix(),GeometrySha256=sha(geometry),NativePhotoshop=True,Base=base,CloseRect=[psd.width-111,81,46,46],Assets=assets,Controls=controls)
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
    print('NEXORIA_DUNGEONS=EXPORTED windows=3 assets='+str(sum(len(w['Assets']) for w in windows)),flush=True)

if __name__=='__main__':main()
