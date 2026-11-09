"""Export the authored settings PSD and its JSX row geometry, without legacy UI layouts."""
from pathlib import Path
import argparse, ast, hashlib, json, re, sys, uuid, subprocess, tempfile, shutil, time
from contextlib import nullcontext


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--project', type=Path, default=Path('D:/Auriga-Unity6/SRC/AurigaUnity-20261103'))
    parser.add_argument('--design', type=Path, default=Path('D:/Nexoria UI/Beallitasok'))
    parser.add_argument('--raster-preview-only', action='store_true', help='Diagnostic fallback; native Photoshop rendering is the default.')
    parser.add_argument('--script-job', type=Path, help='Create a Photoshop Scripts/Browse job when the COM server is unavailable.')
    args = parser.parse_args()
    from psd_tools import PSDImage
    design = args.design.resolve()
    script = design/'scripts/nx_build_beallitasok.jsx'
    literal = '[' + script.read_text(encoding='utf-8').split('var TABS = [', 1)[1].split('\n];', 1)[0] + ']'
    literal = re.sub(r"'(?:[^'\\]|\\.)*'", lambda m: json.dumps(ast.literal_eval(m[0]), ensure_ascii=False), literal)
    literal = re.sub(r'([{,]\s*)([A-Za-z_]\w*)\s*:', r'\1"\2":', literal)
    tabs = json.loads(literal)
    psd = PSDImage.open(design/'Nexoria_Beallitasok.psd')
    def child(group, name): return next(layer for layer in group if layer.name == name)
    def at(*names):
        layer = psd
        for name in names: layer = child(layer, name)
        return layer
    out = args.project/'Assets/Metin2/Resources/Metin2/NexoriaUI'
    old_path = out/'settings-source.json'
    previous = json.loads(old_path.read_text(encoding='utf-8')) if old_path.exists() else {'Assets': []}
    previous = {row['Asset']: row['Sha256'] for row in previous['Assets']}
    sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
    template = (out/'nx_panel_sub.png.meta').read_text(encoding='utf-8')
    assets, native_specs = [], []
    stage=tempfile.TemporaryDirectory(prefix='nexoria-settings-staging-')
    def path_of(layer):
        parts=[]
        while layer is not psd:
            parts.append(layer.name);layer=layer.parent
        return list(reversed(parts))
    def tree(layer):
        yield layer
        if layer.is_group():
            for item in layer:
                yield from tree(item)
    def export(name, layer, predicate=None):
        target = Path(stage.name)/(name+'.png')
        existing=out/target.name
        if existing.exists() and sha(existing) != previous.get(name): raise RuntimeError('Unrecorded export edits: '+str(existing))
        bounds = [0,0,psd.width,psd.height] if layer is psd else list(layer.bbox)
        if args.raster_preview_only:
            image = layer.composite(layer_filter=predicate)
            if image is None: raise RuntimeError('Empty PSD group: '+layer.name)
            crop = image.getbbox()
            if crop:
                image = image.crop(crop)
                bounds = [bounds[0]+crop[0],bounds[1]+crop[1],bounds[0]+crop[2],bounds[1]+crop[3]]
            image.save(target)
        assets.append(dict(Asset=name, Layer=layer.name, Path=path_of(layer), Bounds=bounds, Sha256=sha(target) if args.raster_preview_only else None))
        native_specs.append(dict(Asset=name,Path=path_of(layer),States=[dict(Path=path_of(item),Visible=bool(item.visible) and (predicate(item) if predicate else True)) for item in tree(layer) if item is not psd]))
        return name, bounds
    content = at('Tartalom')
    page_layers = [child(content, 'Fül tartalma — '+tab['name']) for tab in tabs]
    for page in page_layers: page.visible = True
    def common(layer):
        if layer.name.startswith(('Fül tartalma —', 'Fül —')) or layer.name in ('Aktív állapot', 'Bezárás gomb', 'Helykitöltő'): return False
        if layer.name == 'Név' and layer.parent.name == 'Profil': return False
        return layer.is_visible()
    export('nxs_common', psd, common)
    export('nxs_close', at('Ablakkeret és fejléc','Bezárás gomb'))
    pages, rows, samples = [], [], {}
    controls = ('Kapcsoló —', 'Csúszka —', 'Legördülő —', 'Választó —')
    for ti, (tab, page) in enumerate(zip(tabs, page_layers)):
        nav = at('Fülek', 'Fül — '+tab['name']); active = child(nav, 'Aktív állapot')
        active.visible = False
        normal, normal_bounds = export('nxs_nav_'+str(ti), nav)
        active.visible = True
        selected, selected_bounds = export('nxs_nav_'+str(ti)+'_selected', nav)
        active.visible = False
        base, bounds = export('nxs_page_'+str(ti), page, lambda layer: not layer.name.startswith(controls) and layer.is_visible())
        pages.append(dict(Name=tab['name'], Asset=base, Bounds=bounds, Normal=normal, NormalBounds=normal_bounds, Selected=selected, SelectedBounds=selected_bounds))
        for ci, sections in enumerate(tab['cols']):
            x1, x2, y = (372,880,280) if ci==0 else (912,1420,280)
            for si, section in enumerate(sections):
                y += 40
                for ri, row in enumerate(section['rows']):
                    cy = y+24; kind=row['type']; key=f't{ti}c{ci}s{si}r{ri}'
                    group = child(child(page,'Szakasz — '+section['title']), 'Beállítás — '+row['label'])
                    control = next(layer for layer in group if layer.name.startswith(controls))
                    samples.setdefault(kind,control)
                    if kind=='toggle': samples.setdefault('on' if row['on'] else 'off',control)
                    width = 300 if kind=='slider' else 96 if kind=='toggle' else row.get('w',240)
                    rows.append(dict(Key=key,Tab=ti,Label=row['label'],Type=kind,Rect=[x2-width,cy-24,width,48],
                                     LabelRect=[x1+4,cy-18,x2-width-x1-12,36],Options=row.get('options',[]),ControlPath=path_of(control),Selected=row.get('sel',0),Ratio=row.get('v',0)))
                    y += 48
                y += 10
    for state in ('on','off'): export('nxs_toggle_'+state,samples[state])
    dropdown=samples['dropdown'];export('nxs_dropdown',dropdown,lambda layer:layer.name!='Érték' and layer.is_visible())
    slider=samples['slider']
    for name, layer in [('track','Sín'),('fill','Kitöltés'),('value','Érték háttér')]:export('nxs_slider_'+name,child(slider,layer))
    export('nxs_slider_knob',slider,lambda layer:layer is slider or layer.name in ('Fogantyú','Fogantyú kristály'))
    segment=samples['segment'];export('nxs_segment',child(segment,'Keret'))
    selected=next(layer for group in segment if group.is_group() for layer in group if layer.name=='Kijelölés')
    export('nxs_segment_selected',selected)
    for record in assets:
        reference=next((row for row in rows if record['Path'][:len(row['ControlPath'])]==row['ControlPath']),None)
        if reference:record['ReferenceRow']=reference['Key']
    if not args.raster_preview_only:
        from PIL import Image
        native_context=nullcontext(args.script_job.resolve()) if args.script_job else tempfile.TemporaryDirectory(prefix='nexoria-settings-native-')
        with native_context as scratch:
            scratch=Path(scratch)
            scratch.mkdir(parents=True,exist_ok=True)
            config=dict(Source=(design/'Nexoria_Beallitasok.psd').as_posix(),Output=scratch.as_posix(),Specs=native_specs)
            if args.script_job:
                assert not (scratch/'done.txt').exists() and not (scratch/'error.txt').exists(),'Use a fresh native export job'
                config.update(Done=(scratch/'done.txt').as_posix(),Error=(scratch/'error.txt').as_posix())
            code='var NX_EXPORT='+json.dumps(config,ensure_ascii=True)+';\n'+(Path(__file__).parent/'ExportNexoriaSettings.jsx').read_text(encoding='utf-8')
            jsx=scratch/'export.jsx';jsx.write_text(code,encoding='utf-8')
            if args.script_job:
                (scratch/'ready.txt').write_text(str(jsx),encoding='utf-8');print('NEXORIA_PHOTOSHOP_SCRIPT_READY='+str(jsx),flush=True)
                deadline=time.monotonic()+900
                while not (scratch/'done.txt').exists():
                    if (scratch/'error.txt').exists():raise RuntimeError((scratch/'error.txt').read_text())
                    if time.monotonic()>deadline:raise TimeoutError('Photoshop script export')
                    time.sleep(1)
            else:subprocess.run(['powershell','-NoProfile','-STA','-ExecutionPolicy','Bypass','-File',str(Path(__file__).parent/'RunPhotoshopExport.ps1'),'-ScriptPath',str(jsx)],check=True)
            for record in assets:
                with Image.open(scratch/(record['Asset']+'.png')) as rendered:
                    image=rendered.convert('RGBA');bounds=image.getbbox()
                    if bounds is None:raise RuntimeError('Empty native Photoshop export: '+record['Asset'])
                    target=Path(stage.name)/(record['Asset']+'.png');image.crop(bounds).save(target)
                    record['Bounds']=list(bounds);record['Sha256']=sha(target)
            by_name={a['Asset']:a for a in assets}
            for page in pages:
                for asset_key,bounds_key in [('Asset','Bounds'),('Normal','NormalBounds'),('Selected','SelectedBounds')]:page[bounds_key]=by_name[page[asset_key]]['Bounds']
    data=dict(Width=1500,Height=940,Source=(design/'Nexoria_Beallitasok.psd').as_posix(),SourceSha256=sha(design/'Nexoria_Beallitasok.psd'),
              GeometrySource=script.as_posix(),GeometrySha256=sha(script),NativePhotoshop=not args.raster_preview_only,Assets=assets,Pages=pages,Rows=rows)
    for record in assets:
        target=out/(record['Asset']+'.png')
        if target.exists() and sha(target)!=previous.get(record['Asset']):raise RuntimeError('Concurrent export edits: '+str(target))
        shutil.copy2(Path(stage.name)/target.name,target)
        meta=target.with_suffix('.png.meta')
        if not meta.exists():meta.write_text(re.sub(r'guid: [a-f0-9]+','guid: '+uuid.uuid4().hex,template,count=1),encoding='utf-8')
    old_path.write_text(json.dumps(data,indent=2,ensure_ascii=False)+'\n',encoding='utf-8')
    stage.cleanup()
    print('NEXORIA_AUTHORED_SETTINGS=EXPORTED pages='+str(len(pages))+' rows='+str(len(rows))+' assets='+str(len(assets)))


if __name__=='__main__': main()
