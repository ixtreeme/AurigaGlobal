import json,re,shutil,hashlib,collections
from pathlib import Path
r=Path('C:/UnityProjects/AurigaUnity');out=r/'Migration/TeraCompanions';out.mkdir(exist_ok=True)
settings=r/'Migration/TeraCompanionLab/settings.json';shutil.copy2(settings,out/'approved-settings.json')
saved={x['key']:x for x in json.loads(settings.read_text())['models']}
models={x['key']:x for x in json.loads((r/'Assets/TeraCompanionLab/catalog.json').read_text(encoding='utf-8'))['models']}
scene=(r/'Assets/TeraCompanionLab/CompanionLab.unity').read_text(encoding='utf-8')
blocks=re.findall(r'^  - name: (.*?)\n(.*?)(?=^  - name: |^  warriorPrefab:)',scene,re.M|re.S)
labels={'cz':('Jezdecké zvíře','Společník','Pečeť'), 'de':('Reittier','Begleiter','Siegel'),'en':('Mount','Pet','Seal'),'es':('Montura','Mascota','Sello'),'hu':('Hátas','Kísérő','Pecsét'),'it':('Cavalcatura','Compagno','Sigillo'),'pl':('Wierzchowiec','Towarzysz','Pieczęć'),'pt':('Montaria','Companheiro','Selo'),'ro':('Armăsar','Companion','Sigiliu'),'tr':('Binek','Yoldaş','Mühür')}
descs={'cz':('Vyvolá tuto jezdeckou bytost. Umožňuje jízdu a boj ze sedla.','Vyvolá tohoto společníka. Dalším použitím jej odvoláte.'),'de':('Beschwört dieses Reittier zum Reiten und berittenen Kampf.','Beschwört diesen Begleiter. Erneut verwenden, um ihn fortzuschicken.'),'en':('Summons this mount for riding and mounted combat.','Summons this companion. Use again to dismiss it.'),'es':('Invoca esta montura para cabalgar y combatir.','Invoca este compañero. Úsalo de nuevo para retirarlo.'),'hu':('Megidézi ezt a hátast lovagláshoz és nyeregből való harchoz.','Megidézi ezt a kísérőt. Újabb használattal visszahívható.'),'it':('Evoca questa cavalcatura per cavalcare e combattere.','Evoca questo compagno. Usa di nuovo per congedarlo.'),'pl':('Przywołuje tego wierzchowca do jazdy i walki.','Przywołuje tego towarzysza. Użyj ponownie, aby go odwołać.'),'pt':('Invoca esta montaria para cavalgar e combater.','Invoca este companheiro. Usa novamente para o dispensar.'),'ro':('Invocă această montură pentru călărie și luptă.','Invocă acest companion. Folosește din nou pentru a-l retrage.'),'tr':('Binmek ve atlı savaş için bu bineği çağırır.','Bu yoldaşı çağırır. Göndermek için tekrar kullan.')}
rows=[]
for name,b in blocks:
 kind=re.search(r'^    kind: (.*)$',b,re.M).group(1)
 key=re.search(r'prefabResource: Native/(\w+)',b).group(1);m=models[key]
 scale=float(re.search(r'^    scale: (.*)$',b,re.M).group(1));setting=saved.get(key)
 clips=[x.strip('"') for x in re.findall(r'^    - (.*)$',b.split('    clipNames:\n')[1].split('    scale:')[0],re.M)]
 resources=re.findall(r'^    - (.*)$',b.split('    clipResources:\n')[1].split('    clipNames:')[0],re.M)
 proper=re.sub(r'^(Vehicle_|Trans_|Pet_|BG_|FV_SUM_)','',name);proper=re.sub(r'([a-z])([A-Z])',r'\1 \2',proper).replace('_',' ')
 rows.append(dict(key=key,name=name,kind=kind,saddle=re.search(r'^    saddle:(.*)$',b,re.M).group(1).strip(),scale=setting['totalScale'] if setting else scale,sizeSaved=bool(setting),clips=clips,clipResources=resources,proper=proper,attachments=m.get('attachmentSpecs',[]),sounds=[dict(state=clip,**event) for clip,events in m.get('soundEvents',{}).items() for event in events if not re.search(r'spawn|summon|recall|extinction',clip+' '+event.get('cue',''),re.I)]))
db=json.loads(Path('work/tera-db-probe.json').read_text(encoding='utf-8'))
used={t:{int(x) for x in db['all_ids'][t].splitlines()[1:] if x} for t in ('item_proto','mob_proto')}
for kind,itembase,mobbase in (('mount',630000,32000),('pet',631000,32500)):
 group=sorted([x for x in rows if x['kind']==kind],key=lambda x:(x['name'].lower(),x['key']))
 counts=collections.Counter(x['proper'] for x in group)
 for i,row in enumerate(group):
  row['item']=itembase+i;row['mob']=mobbase+i
  assert row['item'] not in used['item_proto'] and row['mob'] not in used['mob_proto']
  p=row['proper']+((' '+row['key'][:4]) if counts[row['proper']]>1 else '')
  row['names']={l:('TERA '+label[0 if kind=='mount' else 1]+': '+p)[:49] for l,label in labels.items()}
  row['itemNames']={l:(label[2]+': '+p)[:49] for l,label in labels.items()}
  row['descriptions']={l:descs[l][0 if kind=='mount' else 1] for l in labels}
parts=[x for x in rows if x['kind'] not in ('mount','pet')]
rows=sorted([x for x in rows if x['kind'] in ('mount','pet')],key=lambda x:x['item'])
manifest={'version':1,'branch':'ECS_+_BGFX','settingsSha256':hashlib.sha256(settings.read_bytes()).hexdigest(),'entries':rows,'parts':parts}
(out/'release.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf-8')
# Stage original waveforms, deduplicated by their source content-hash filenames.
soundpaths={f for x in rows+parts for e in x['sounds'] for f in e['files']}
audio=r/'Assets/Metin2/Generated/TeraCompanions/Audio';audio.mkdir(parents=True,exist_ok=True)
for f in sorted(soundpaths):
 src=Path('C:/TeraOnline/CompanionResearch/Ready')/f
 if not src.is_file():raise FileNotFoundError(src)
 dst=audio/src.name
 if not dst.exists():shutil.copy2(src,dst)
print('Release',collections.Counter(x['kind'] for x in rows),'sounds',len(soundpaths),'sound events',sum(len(x['sounds']) for x in rows),'source-size defaults',[(x['name'],x['scale']) for x in rows if not x['sizeSaved']])
