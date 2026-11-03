import json,struct,collections,hashlib,shutil
from pathlib import Path
r=Path('C:/UnityProjects/AurigaUnity');ready=Path('C:/TeraOnline/CompanionResearch/Ready')
catalog=json.loads((r/'Assets/TeraCompanionLab/catalog.json').read_text(encoding='utf-8'))['models']
release=json.loads((r/'Migration/TeraCompanions/release.json').read_text(encoding='utf-8'))
sources={c['resource']:c for m in catalog for c in m.get('soundCues',[])}
required={s['cue'] for m in release['entries']+release['parts'] for s in m['sounds']}
programs=[];failures=[];classes=collections.Counter()
def floats(v):return [struct.unpack('<f',struct.pack('<I',i&0xffffffff))[0] for i in (v or {}).get('ints',[])]
for cue in sorted(required):
 c=sources.get(cue)
 if not c:failures.append((cue,'no source cue'));continue
 d=json.loads((ready/c['file']).read_text(encoding='utf-8'));objs=d['objects'];root=next(o for o in objs if o['class']=='Engine.SoundCue');p={x['name']:x['value'] for x in root['properties']};first=p['FirstNode']
 start=next(i for i,o in enumerate(objs) if o['path']==first['ref']);base=first['index']-start
 wavefiles={w['name']:w['file'] for w in c['waves']}
 nodes=[]
 for o in objs:
  kind=o['class'].split('SoundNode')[-1].lower();q={x['name']:x['value'] for x in o['properties']};classes[kind]+=1
  children=[i-base if i else -1 for i in q.get('ChildNodes',{}).get('ints',[])]
  if any(i>=len(objs) or i< -1 for i in children):failures.append((cue,'unresolved export index'))
  node=dict(kind=kind,children=children,weights=floats(q.get('Weights')),volumes=floats(q.get('InputVolume')),volumeMin=q.get('VolumeMin',.95),volumeMax=q.get('VolumeMax',1.05),pitchMin=q.get('PitchMin',.95),pitchMax=q.get('PitchMax',1.05),radiusMin=q.get('RadiusMin',400)*.01,radiusMax=q.get('RadiusMax',4000)*.01,lpfMin=q.get('LPFRadiusMin',3000)*.01,lpfMax=q.get('LPFRadiusMax',6000)*.01,lowpass=q.get('bAttenuateWithLPF',False),algorithm=str(q.get('DistanceAlgorithm','ATTENUATION_Linear')))
  if kind=='wave':
   node['file']=wavefiles.get(o['name'])
   if not node['file']:
    raw=ready.parent/'SoundExport'/Path(c['file']).stem/'SoundNodeWave'/(o['name']+'.ogg')
    if raw.is_file() and raw.read_bytes()[:4]==b'OggS':
     node['file']='Shared/Sounds/'+hashlib.sha256(raw.read_bytes()).hexdigest()+'.ogg'
     shutil.copy2(raw,r/'Assets/Metin2/Generated/TeraCompanions/Audio'/Path(node['file']).name)
    else:failures.append((cue,'missing wave '+o['name']))
  nodes.append(node)
 programs.append(dict(cue=cue,key=hashlib.sha256(cue.encode()).hexdigest()[:20],first=start,volume=p.get('VolumeMultiplier',.75),pitch=p.get('PitchMultiplier',1),maxConcurrent=p.get('MaxConcurrentPlayCount',16),nodes=nodes))
out=r/'Migration/TeraCompanions/audio-cues.json';out.write_text(json.dumps(dict(programs=programs),ensure_ascii=False,separators=(',',':')),encoding='utf-8')
print('CUE_PROGRAMS',len(programs),'node classes',classes,'failures',len(failures));print(failures[:12])
if failures:raise SystemExit(1)
