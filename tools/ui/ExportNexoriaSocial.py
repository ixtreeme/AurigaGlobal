"""Import both original Social PSD views, their controls and reusable conversation artwork."""
from pathlib import Path
import argparse,hashlib,json,re,shutil,subprocess,uuid
import NexoriaLiveText

def main():
    from psd_tools import PSDImage
    from PIL import Image
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--project',type=Path,default=Path('D:/Auriga-Unity6/SRC/AurigaUnity-20261103'))
    p.add_argument('--design',type=Path,default=Path('D:/Nexoria UI/Social'))
    p.add_argument('--cache',type=Path,default=Path('build/nexoria-social-export'))
    args=p.parse_args();out=args.project/'Assets/Metin2/Resources/Metin2/NexoriaUI';manifest=out/'item-windows-source.json'
    original=manifest.read_bytes();data=json.loads(original);prior={a['Asset']:a['Sha256'] for w in data['Windows'] for a in w['Assets']}
    sha=lambda path:hashlib.sha256(path.read_bytes()).hexdigest();windows=[]
    for view,suffix in [('friends','Baratok'),('chat','Cseveges')]:
        key='social_'+view;prefix='nxi_'+key+'_';source=args.design/('Nexoria_Social_'+suffix+'.psd');script=args.design/'scripts/nx_build_social.jsx'
        source_hash=sha(source);psd=PSDImage.open(source);layers=[]
        def walk(group,path=()):
            for layer in group:
                q=path+(layer.name,);layers.append((layer,q))
                if layer.is_group():walk(layer,q)
        walk(psd);specs=[];assets=[];controls=[];cache=(args.cache/view).resolve();cache.mkdir(parents=True,exist_ok=True)
        def only(*paths):
            wanted=[tuple(path.split('/')) for path in paths]
            for path in wanted:
                assert sum(q==path for _,q in layers)==1,path
            return lambda l,q:l.visible and any(q[:len(path)]==path or path[:len(q)]==q for path in wanted)
        def asset(name,keep):
            name=prefix+name;target=out/(name+'.png')
            if target.exists() and sha(target)!=prior.get(name):raise RuntimeError('Unrecorded artwork edit: '+name)
            specs.append(dict(Asset=name,Path=[],States=[dict(Path=list(q),Visible=bool(keep(l,q))) for l,q in layers]));assets.append(dict(Asset=name));return name
        def control(key,path,rect):
            controls.append(dict(Key=key,Asset=asset(key,only(path)),Rect=rect,DynamicCaption=False))
        def shape_asset(name,path):
            result=asset(name,only(path));assets[-1]['BodyRect']=list(next(l.bbox for l,q in layers if q==tuple(path.split('/'))));return result
        is_friend=view=='friends';chat='Privát beszélgetés — Luna' if is_friend else 'Céhchat';listing='Barátlista' if is_friend else 'Középső lista'
        def base_keep(l,q):
            if not l.visible:return False
            if q[0]=='Ablakkeret':return True
            if q[0]=='Bal menü':return len(q)==1 or q[1] in ('Menü panel','Vízjel embléma')
            if q[0]==listing:
                if len(q)==1:return True
                if q[1]=='Keresés':return q[-1]!='Helykitöltő'
                if q[1]=='Görgetősáv':return q[-1]!='Fogantyú'
                return q[1] in ('Lista panel','Cím — Barátok')
            if q[0]==chat:
                if len(q)==1:return True
                if q[1] in ('Chat panel','Üzenetterület'):return True
                if q[1]=='Fejléc':return q[-1]=='Fejléc alsó vonal' or len(q)==2
                if q[1]=='Üzenetírás':return len(q)==2 or q[-1]=='Beviteli mező'
            return False
        base=asset('base',base_keep)
        nav=[('friends','Barátok',163,56),('guild','Céh',229,56),('conversations','Beszélgetések',291,56),('requests','Kérések',354,56)] if is_friend else [('conversations','Csevegés',185,65),('friends','Barátok',265,65),('guild','Céh',343,63),('requests','Kérések',421,66),('blocked','Tiltólista',497,64)]
        for code,label,y,h in nav:
            name='Fül — '+label+(' (aktív)' if code==('friends' if is_friend else 'conversations') else '')
            paths=['Bal menü/'+name+'/Ikon','Bal menü/'+name+'/Felirat']
            controls.append(dict(Key='nav_'+code,Asset=asset('nav_'+code,only(*paths)),Rect=[52 if is_friend else 45,y,201 if is_friend else 177,h],DynamicCaption=False))
        active='Bal menü/Fül — '+('Barátok' if is_friend else 'Csevegés')+' (aktív)'
        asset('nav_active',only(active+'/Háttér',*([active+'/Jelölő gyémánt'] if is_friend else [])))
        if not is_friend:asset('nav_inactive',only('Bal menü/Fül — Barátok/Háttér'))
        if is_friend:
            control('add_friend',listing+'/Barát hozzáadása gomb',[598,211,48,38])
            control('invite',chat+'/Fejléc/Barát hozzáadása gomb',[1145,172,53,46])
            control('gift',chat+'/Fejléc/Ajándék gomb',[1212,172,53,46])
            control('more',chat+'/Fejléc/Továbbiak gomb',[1279,172,53,46])
            control('emoji',chat+'/Üzenetírás/Emoji gomb',[686,646,57,46])
            control('send',chat+'/Üzenetírás/Küldés gomb',[1272,646,60,46])
            control('attach',chat+'/Üzenetírás/Csatolás ikon',[1217,646,43,46])
            shape_asset('row_selected','Barátlista/Barát — Luna (kijelölt)/Kijelölés')
            asset('class_mark',only('Barátlista/Barát — Luna (kijelölt)/Kaszt jel'))
            for name,who in [('warrior','ShadowKnight'),('mage','Luna'),('archer','Artemis'),('rogue','Nexor'),('shaman','Elara')]:
                path='Barátlista/Barát — '+who+(' (kijelölt)' if who=='Luna' else '')+'/Avatar — '+who
                asset('avatar_'+name,only(path))
            path='Barátlista/Barát — ShadowKnight/Avatar — ShadowKnight'
            asset('avatar_neutral',only(path+'/Avatar háttér',path+'/Avatar keret'))
            asset('status_online',only('Barátlista/Barát — Luna (kijelölt)/Állapot pont'))
            asset('badge',only('Bal menü/Fül — Kérések/Jelvény — 2/Kör'))
        else:
            control('filter',listing+'/Szűrő gomb',[548,190,44,40])
            control('invite',chat+'/Fejléc/Tag meghívása gomb',[1182,195,50,48])
            control('settings',chat+'/Fejléc/Csatorna beállítások gomb',[1250,195,50,48])
            control('pin',chat+'/Fejléc/Rögzítés gomb',[1318,195,50,48])
            control('attach',chat+'/Üzenetírás/Melléklet gomb',[636,695,46,45])
            control('emoji',chat+'/Üzenetírás/Emoji ikon',[1260,695,45,45])
            control('send',chat+'/Üzenetírás/Küldés gomb',[1313,695,63,45])
            shape_asset('row_selected','Középső lista/Céhchat — Phoenix Legion (kijelölt)/Háttér')
            shape_asset('row_hover','Középső lista/Beszélgetés — Luna/Kiemelés')
            asset('guild_emblem',only('Céhchat/Fejléc/Céhjelvény'))
            asset('section_caret',only('Középső lista/Szakasz — Céhcsatornák (1)/Nyíl'))
            asset('day_badge',only('Céhchat/Dátum — Ma/Címke'))
        messages=[q for l,q in layers if len(q)==3 and q[:2]==(chat,'Üzenetek')]
        for name,path in [('incoming',next(q for q in messages if ' — Te ' not in q[-1])),('outgoing',next(q for q in messages if ' — Te ' in q[-1]))]:shape_asset('bubble_'+name,'/'.join(path+('Buborék',)))
        # The small MMORPG lettering belongs to the decorative footer wordmark.
        # Keep the original brand graphic rasterized instead of treating it as an interactive label.
        text_layers=[(l,q) for l,q in layers if q[:3]!=('Ablakkeret','Alsó címer','Logó')]
        NexoriaLiveText.prepare(text_layers,specs,assets,asset)
        config=dict(Source=source.as_posix(),Output=cache.as_posix(),Specs=specs,LiveText=NexoriaLiveText.VERSION)
        cache_key=hashlib.sha256((json.dumps(config,sort_keys=True)+source_hash+sha(script)).encode()).hexdigest();record=cache/'record.json'
        saved=json.loads(record.read_text(encoding='utf-8')) if record.exists() else None
        if saved and saved['CacheKey']==cache_key and all((cache/(a['Asset']+'.cropped.png')).exists() and sha(cache/(a['Asset']+'.cropped.png'))==a['Sha256'] for a in saved['Window']['Assets']):
            window=saved['Window'];NexoriaLiveText.refresh(window['Assets'],assets)
        else:
            jsx=cache/'export.jsx';jsx.write_text('var NX_EXPORT='+json.dumps(config,ensure_ascii=True)+';\n'+(Path(__file__).parent/'ExportNexoriaSettings.jsx').read_text(encoding='utf-8'),encoding='utf-8')
            subprocess.run(['powershell','-NoProfile','-STA','-ExecutionPolicy','Bypass','-File',str(Path(__file__).parent/'RunPhotoshopExport.ps1'),'-ScriptPath',str(jsx)],check=True)
            for a in assets:
                target=cache/(a['Asset']+'.cropped.png')
                with Image.open(cache/(a['Asset']+'.png')) as im:a.update(Bounds=NexoriaLiveText.crop(a,im.convert('RGBA'),target),Sha256=sha(target))
            window=dict(Key=key,Width=psd.width,Height=psd.height,Source=source.as_posix(),SourceSha256=source_hash,GeometrySource=script.as_posix(),GeometrySha256=sha(script),NativePhotoshop=True,Base=base,CloseRect=[1270,77,46,44] if is_friend else [1350,106,40,40],Assets=assets,Controls=controls)
            record.write_text(json.dumps(dict(CacheKey=cache_key,Window=window),ensure_ascii=False,indent=2),encoding='utf-8')
        for a in window['Assets']:
            current=next(v for v in assets if v['Asset']==a['Asset'])
            if 'BodyRect' in current:a['BodyRect']=current['BodyRect']
        assert sha(source)==source_hash and manifest.read_bytes()==original
        windows.append((window,cache))
    template=(out/'nx_panel_sub.png.meta').read_text(encoding='utf-8')
    for window,cache in windows:
        for a in window['Assets']:
            target=out/(a['Asset']+'.png');shutil.copy2(cache/(a['Asset']+'.cropped.png'),target);meta=target.with_suffix('.png.meta')
            if not meta.exists():meta.write_text(re.sub(r'guid: [a-f0-9]+','guid: '+uuid.uuid4().hex,template,count=1),encoding='utf-8')
    data['Windows']=[w for w in data['Windows'] if w['Key'] not in {v['Key'] for v,_ in windows}]+[w for w,_ in windows]
    manifest.write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n',encoding='utf-8');print('NEXORIA_SOCIAL=EXPORTED views=2 assets='+str(sum(len(w['Assets']) for w,_ in windows)),flush=True)
if __name__=='__main__':main()
