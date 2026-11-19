import json,re,hashlib,shutil
from pathlib import Path
root=Path('C:/UnityProjects/AurigaUnity');folder=root/'Assets/TeraCompanionLab/Effects/MetricMeshes';backup=root/'Migration/TeraCompanions/MeshBefore';backup.mkdir(exist_ok=True)
results=[]
for p in folder.glob('*.asset'):
 s=p.read_text(encoding='utf-8');chunk=s.split('  m_SubMeshes:\n',1)[1].split('  m_Shapes:',1)[0]
 sections=re.findall(r'  - serializedVersion: 2\n(.*?)(?=  - serializedVersion: 2\n|\Z)',chunk,re.S)
 if len(sections)==1:continue
 assert len(sections)==2,p
 get=lambda x,k:int(re.search(r'    '+k+r': (\d+)',x)[1])
 assert all(get(x,'baseVertex')==0 and get(x,'topology')==0 for x in sections)
 assert re.search(r'  m_IndexFormat: (\d+)',s)[1]=='0'
 count=sum(get(x,'indexCount') for x in sections)
 assert get(sections[0],'firstByte')==0 and get(sections[1],'firstByte')==get(sections[0],'indexCount')*2
 indices=re.search(r'  m_IndexBuffer: ([0-9a-f]+)',s)[1];assert len(indices)==count*4
 vertexCount=int(re.search(r'    m_VertexCount: (\d+)',s)[1])
 bounds=re.search(r'  m_LocalAABB:\n(    m_Center: [^\n]+\n    m_Extent: [^\n]+)',s)[1]
 replacement='  - serializedVersion: 2\n    firstByte: 0\n    indexCount: '+str(count)+'\n    topology: 0\n    baseVertex: 0\n    firstVertex: 0\n    vertexCount: '+str(vertexCount)+'\n    localAABB:\n'+''.join('  '+line+'\n' for line in bounds.splitlines())
 out=s.replace('  m_SubMeshes:\n'+chunk,'  m_SubMeshes:\n'+replacement)
 assert re.search(r'  m_IndexBuffer: ([0-9a-f]+)',out)[1]==indices
 shutil.copy2(p,backup/p.name);p.write_text(out,encoding='utf-8')
 results.append(dict(mesh=p.name,triangles=count//3,vertices=vertexCount,indexBufferUnchanged=True,vertexDataUnchanged=s.split('  m_VertexData:',1)[1]==out.split('  m_VertexData:',1)[1]))
(root/'Migration/TeraCompanions/particle-mesh-fix.json').write_text(json.dumps(results,indent=2),encoding='utf-8');print('Particle meshes repaired',len(results))
