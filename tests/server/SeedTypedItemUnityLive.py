"""Seed dedicated item smoke fixtures in an OWNED, stopped local package."""
import sys,json,datetime,argparse,secrets
from pathlib import Path
repo=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(repo/'SRC/Tools/ServerManager'))
import item_bench,item_catalog
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--package',type=Path,required=True)
parser.add_argument('--catalog-tool',type=Path,required=True)
parser.add_argument('--bench-tool',type=Path,required=True)
parser.add_argument('--fixture',type=Path,required=True)
parser.add_argument('--map-index',type=int,default=41)
parser.add_argument('--x',type=int,default=987300)
parser.add_argument('--y',type=int,default=268800)
parser.add_argument('--game-port',type=int,default=30078)
args=parser.parse_args()
root=args.package.resolve()
_,manager,_,auth=item_bench.connection(root)
state=manager.read_json(root/'run/supervisor.json') if (root/'run/supervisor.json').exists() else {}
assert not any(manager.owned(c['identity']) for c in state.get('children',[])), 'Stop the local stack first'
tool=args.catalog_tool.resolve()
bench=args.bench_tool.resolve()
config_path=args.fixture.resolve();config_path.parent.mkdir(parents=True,exist_ok=True)
config=json.loads(config_path.read_text(encoding='utf-8')) if config_path.exists() else {}
profile=json.loads((root/'client-connection.json').read_text(encoding='utf-8-sig'))
config.update(Host='127.0.0.1',ServerName=profile['Tls']['ServerName'],Pin=profile['Tls']['PinnedCertificateSha256'],Version=profile['ClientVersion'],AuthPort=profile['AuthPort'],MapIndex=args.map_index,X=args.x,Y=args.y)
config.setdefault('Token',secrets.token_hex(4))
def read():return item_catalog.run_tool(dict(action='read',connection=auth),executable=tool)
packet=read()
backup=root/'run/catalog-editor'/datetime.datetime.now().strftime('typed-items-demo-%Y%m%dT%H%M%S')
backup.mkdir(parents=True)
(backup/'before.json').write_text(json.dumps(packet,ensure_ascii=False,indent=2),encoding='utf-8')
keys={}
for category,name,icon,field in ((1,'sword','icon/item/00010.tga','WeaponLocal'),(5,'flask','icon/item/27001.tga','FlaskLocal'),(6,'ore','icon/item/30001.tga','MaterialLocal')):
    code={1:'weapon',5:'consumable',6:'material'}[category]+'.local_probe_'+name
    editor=item_catalog.Catalog(packet)
    found=next((r for r in editor.tables['item_base'] if r['content_code']==code),None)
    if found:keys[field]=int(found['local_id']);continue
    document=editor.new(category)
    document['base'].update(content_code=code,name_key=f'item.local_probe.{name}.name',description_key=f'item.local_probe.{name}.description',icon_asset=icon)
    keys[field]=int(document['base']['local_id'])
    tables=editor.apply(document)
    item_catalog.run_tool(dict(action='commit',connection=auth,tables=tables,expected_hash=packet['hash']),packet['columns'],tool)
    packet=read()
editor=item_catalog.Catalog(packet)
key=(1,keys['WeaponLocal'])
document=editor.item(key)
document['refinement']['max_level']='2'
document['levels']=[dict(document['levels'][0],level=str(level)) for level in range(3)]
document['recipes']=[dict(source_level='0',target_category_id=None,target_local_id=None,target_level='1',gold_cost='100',success_chance_bp='10000',failure_kind='1',failure_level=None,
    ingredients=[dict(category_id='6',local_id=str(keys['MaterialLocal']),refinement_level='0',amount='2')]),
    dict(source_level='1',target_category_id=None,target_local_id=None,target_level='2',gold_cost='50',success_chance_bp='0',failure_kind='1',failure_level=None,ingredients=[])]
tables=editor.apply(document,key)
item_catalog.run_tool(dict(action='commit',connection=auth,tables=tables,expected_hash=packet['hash']),packet['columns'],tool)
config.update(keys)
config['GamePort']=args.game_port
config_path.write_text(json.dumps(config),encoding='utf-8')
item_bench.run_tool(dict(action='setup',connection=auth),bench)
print('Owned demo keys:',keys)
print('Catalog snapshot backup:',backup)
