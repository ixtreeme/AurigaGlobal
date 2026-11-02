"""Seed additive currency-token probes in an owned, stopped local server package."""
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
backup=root/'run/deployments'/datetime.datetime.now(datetime.timezone.utc).strftime('token-redemption-%Y%m%dT%H%M%S')
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
def token(currency,amount,minimum=0):
    def edit(document):
        document['base']['min_level']=str(minimum)
        document['child'].update(wallet_currency=str(currency),amount_per_unit=str(amount))
    return edit
save(16,'GoldTokenLocal','currency_token.local_probe_gold',token(1,25))
save(16,'CoinTokenLocal','currency_token.local_probe_account',token(2,100))
save(16,'HighTokenLocal','currency_token.local_probe_level',token(1,25,255))
save(16,'HugeTokenLocal','currency_token.local_probe_huge',token(1,9223372036854775807))
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
print('TOKEN_LOCAL_SETUP=READY; all stored owners validated; backup:',backup)
