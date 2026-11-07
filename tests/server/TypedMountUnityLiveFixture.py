"""Snapshot and verify dedicated localhost companion accounts around mount tests.

Reuses completed codexcompanion test accounts; no ordinary account is accepted.
Credentials and SQL snapshots remain in the caller's ignored build directory.
"""
from pathlib import Path
import argparse,json,sys

REPO=Path(__file__).resolve().parents[2]
sys.path[:0]=[str(REPO/'SRC/Tools/ServerManager'),str(REPO/'db/tools')]
import item_bench
from mariadb_client import Client

def read(path):return json.loads(path.read_text(encoding='utf-8-sig'))
def write(path,value):path.write_text(json.dumps(value,indent=2)+'\n',encoding='utf-8')

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('action',choices=('prepare','verify'))
    p.add_argument('--package',type=Path,required=True)
    p.add_argument('--template',type=Path)
    p.add_argument('--fixture',type=Path,required=True)
    p.add_argument('--log',type=Path)
    args=p.parse_args();fixture=args.fixture.resolve()
    assert fixture.is_relative_to(REPO/'build')
    root,_,_,auth=item_bench.connection(args.package.resolve())
    tool=root/'manager/_internal/validators/ItemBenchTool.exe'
    config=read(args.template if args.action=='prepare' else fixture)
    assert config['Host']=='127.0.0.1' and len(config['Accounts'])==2
    assert all(a['Login']=='codexcompanion'+config['Token']+str(i) for i,a in enumerate(config['Accounts']))
    owner=int(config['Accounts'][0]['Pid']);baseline=Path(str(fixture)+'.before.private.json')
    db=Client(**auth,library=str(root/'bin/libmariadb.dll'))
    def protected():
        where=f' WHERE NOT (owner_kind=1 AND owner_id={owner})'
        return {name:db.query(query+where+' ORDER BY '+order) for name,query,order in [
            ('items','SELECT owner_kind,owner_id,instance_id,state_revision,HEX(state_sha256),HEX(last_operation) FROM player.item_instance','instance_id'),
            ('wallets','SELECT owner_kind,owner_id,currency,state_revision,balance FROM player.item_wallet','owner_kind,owner_id,currency'),
            ('learning','SELECT owner_kind,owner_id,state_revision,HEX(state_blob),HEX(state_sha256),HEX(last_operation) FROM player.item_book_learning','owner_kind,owner_id')]}
    try:
        for account in config['Accounts']:
            assert db.query(f"SELECT account_id,name FROM player.player WHERE id={int(account['Pid'])}")==[[str(account['AccountId']),account['Name']]]
        state=item_bench.run_tool(dict(action='status',owner=owner,connection=auth),tool)['state']
        assert len(state['items'])==2 and {i['local'] for i in state['items']}=={config['PetLocal'],config['MountLocal']}
        assert all(i['category']==15 and i['count']==1 and i['companion']==dict(recall_requested=False,riding_requested=False,
            lifetime=dict(started=False,expired=False,deadline_ms=0,remaining_ms=0)) for i in state['items'])
        if args.action=='prepare':
            assert args.template and not fixture.exists() and not baseline.exists()
            config['RidingRace']=config['MountRace']
            write(baseline,dict(protected=protected(),items=state['items']));write(fixture,config)
            print('TYPED_MOUNT_FIXTURE=READY seals=2')
        else:
            before=read(baseline);assert protected()==before['protected']
            assert args.log
            log=args.log.read_text(encoding='utf-8',errors='replace')
            assert 'METIN2_TYPED_MOUNT_LIVE=SUCCESS' in log and 'METIN2_TYPED_COMPANION_LIVE=SUCCESS' in log
            assert all('PET_MOUNT_PHASE=SUCCESS phase='+v in log for v in ('mounted-relog','dismounted-relog','recalled-relog'))
            for current in state['items']:
                original=next(i for i in before['items'] if i['id']==current['id'])
                expected=dict(original);expected['revision']+=2 if current['local']==config['PetLocal'] else 7
                assert current==expected,'Only the nine accepted intent transitions may change the seals'
            print('TYPED_MOUNT_LOCAL_SQL=SUCCESS seals=2 accepted_transitions=9 preserved_items='+str(len(before['protected']['items'])))
    finally:db.close()

if __name__=='__main__':main()
