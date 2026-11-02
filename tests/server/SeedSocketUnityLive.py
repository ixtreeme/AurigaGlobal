"""Seed additive socket-maintenance probes in an owned, stopped local server package."""
import argparse
import datetime
import hashlib
import json
from pathlib import Path
import shutil
import sys

repo=Path(__file__).resolve().parents[2]
sys.path[:0]=[str(repo/'SRC/Tools/ServerManager'),str(repo/'db/tools')]
import item_bench,item_catalog
from mariadb_client import Client

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--package',type=Path,required=True)
parser.add_argument('--source-fixture',type=Path,required=True)
parser.add_argument('--fixture',type=Path,required=True)
parser.add_argument('--catalog-tool',type=Path,required=True)
parser.add_argument('--bench-tool',type=Path,required=True)
parser.add_argument('--game',type=Path,required=True)
args=parser.parse_args()
root=args.package.resolve();_,manager,_,auth=item_bench.connection(root)
supervisor=manager.read_json(root/'run/supervisor.json') if (root/'run/supervisor.json').exists() else {}
assert not any(manager.owned(c['identity']) for c in supervisor.get('children',[])),'Stop the owned local stack first'
config=json.loads(args.source_fixture.read_text(encoding='utf-8'))
assert config['Host']=='127.0.0.1' and config['Accounts'][0]['Login']=='codexitem'+config['Token']+'0'
token=config['Token'];account=config['Accounts'][0];pid=int(account['Pid'])
assert len(token)==8 and all(c in '0123456789abcdef' for c in token) and account['Name']=='CI'+token+'0'
db=Client(**auth,library=str(root/'bin/libmariadb.dll'))
try:assert db.query('SELECT account_id,name FROM player.player WHERE id='+str(pid))==[[str(account['AccountId']),account['Name']]]
finally:db.close()
backup=root/'run/deployments'/datetime.datetime.now(datetime.timezone.utc).strftime('socket-maintenance-%Y%m%dT%H%M%S')
backup.mkdir(parents=True)
def read():return item_catalog.run_tool(dict(action='read',connection=auth),executable=args.catalog_tool)
packet=read();(backup/'catalog-before.json').write_text(json.dumps(packet,indent=2),encoding='utf-8')
def save(category,field,code,edit):
    global packet
    editor=item_catalog.Catalog(packet)
    existing=next((row for row in editor.tables['item_base'] if row['content_code']==code),None)
    original=(category,int(existing['local_id'])) if existing else None
    document=editor.item(original) if original else editor.new(category)
    document['base'].update(content_code=code,name_key='item.'+code+'.name',description_key='item.'+code+'.description',icon_asset='icon/item/50011.tga' if category==8 else 'icon/item/50084.tga')
    edit(document)
    config[field]=int(document['base']['local_id'])
    item_catalog.run_tool(dict(action='commit',connection=auth,tables=editor.apply(document,original),expected_hash=packet['hash']),packet['columns'],args.catalog_tool)
    packet=read()
def service(kind,minimum=0,units=1):
    def edit(document):
        document['base']['min_level']=str(minimum)
        document.update(socket_service=dict(service_kind=str(kind),units_per_use=str(units)),recovery=None,effect=None,timed=None,toggle=None,cooldown=None)
    return edit
save(5,'CleanLocal','consumable.local_probe_socket_clean',service(1))
save(5,'ExtractAllLocal','consumable.local_probe_socket_all',service(2))
save(5,'ExtractLastLocal','consumable.local_probe_socket_last',service(3))
save(5,'HighSocketLocal','consumable.local_probe_socket_level',service(1,255))
save(5,'TwoUnitSocketLocal','consumable.local_probe_socket_two_units',service(2,units=2))
def refinement(document,maximum):
    import copy
    document['refinement']['max_level']=str(maximum)
    zero=document['levels'][0];document['levels']=[dict(copy.deepcopy(zero),level=str(i)) for i in range(maximum+1)]
def stone(group,retired=False):
    def edit(document):
        document['base']['retired']='1' if retired else '0'
        document['child'].update(allowed_targets='3',primary_group=group)
        refinement(document,3)
    return edit
save(7,'SocketStoneLocal','socket_stone.local_probe_return_one',stone('socket.local_probe_one'))
save(7,'SocketSecondStoneLocal','socket_stone.local_probe_return_two',stone('socket.local_probe_two'))
save(7,'RetiredSocketStoneLocal','socket_stone.local_probe_return_retired',stone('socket.local_probe_retired',True))
def weapon(document):
    document['base'].update(inventory_height='3',icon_asset='icon/item/00010.tga')
    document['child']['physical_attack_base_raw']='5000';refinement(document,1)
save(1,'SocketWeaponLocal','weapon.local_probe_socket_target',weapon)
(backup/'catalog-after.json').write_text(json.dumps(packet,indent=2),encoding='utf-8')
result=item_bench.run_tool(dict(action='setup',owner=config['Accounts'][0]['Pid'],connection=auth),args.bench_tool)
assert result['state']['owner']==config['Accounts'][0]['Pid']
shutil.copy2(root/'bin/GameServer.exe',backup/'GameServer-before.exe')
shutil.copy2(root/'package-manifest.json',backup/'package-manifest-before.json')
shutil.copy2(args.game,root/'bin/GameServer.exe')
manifest=json.loads((root/'package-manifest.json').read_text())
manifest['files']['bin/GameServer.exe']=hashlib.sha256((root/'bin/GameServer.exe').read_bytes()).hexdigest()
(root/'package-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
args.fixture.parent.mkdir(parents=True,exist_ok=True)
args.fixture.write_text(json.dumps(config),encoding='utf-8')
print('SOCKET_LOCAL_SETUP=READY; all stored owners validated; backup:',backup)
