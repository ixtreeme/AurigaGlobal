"""Provision three TLS recovery cases for verified dedicated local PCs.

Restart the owned game stack before each run: the legacy DatabaseServer caches
player HP/MP, so direct setup SQL alone cannot reset a previously loaded PC.
"""
import argparse,datetime,json,socket,sys,time
from pathlib import Path
repo=Path(__file__).resolve().parents[2];sys.path[:0]=[str(repo/'SRC/Tools/ServerManager'),str(repo/'db/tools')]
import item_bench
from mariadb_client import Client
p=argparse.ArgumentParser(description=__doc__)
for name in ('package','unity-project','fixture','bench-tool'):p.add_argument('--'+name,type=Path,required=True)
p.add_argument('--timeout',type=int,default=1000);a=p.parse_args();root=a.package.resolve();project=a.unity_project.resolve();config=json.loads(a.fixture.read_text(encoding='utf-8'));token=config['Token'];account=config['Accounts'][0];pid=int(account['Pid'])
assert config['Host']=='127.0.0.1' and len(token)==8 and all(c in '0123456789abcdef' for c in token);assert account['Login']=='codexitem'+token+'0' and account['Name']=='CI'+token+'0'
_,_,_,auth=item_bench.connection(root);db=Client(**auth,library=str(root/'bin/libmariadb.dll'));identity='SELECT account_id,name FROM player.player WHERE id='+str(pid);assert db.query(identity)==[[str(account['AccountId']),account['Name']]]
deadline=time.monotonic()+60
while True:
 try:
  for port in (config['AuthPort'],config['GamePort']):
   with socket.create_connection(('127.0.0.1',int(port)),timeout=2):pass
  break
 except OSError:
  if time.monotonic()>deadline:raise TimeoutError('Owned local endpoints not listening') from None
  time.sleep(.2)
request=project/'Temp/typed-shop-validation-request.json';result=project/'Temp/typed-shop-validation-result.json';persisted=project/'validation-source-snapshot-result.json';assert not request.exists(),'Another request is pending'
request.parent.mkdir(parents=True,exist_ok=True);result.unlink(missing_ok=True);persisted.unlink(missing_ok=True)
for phase in range(3):
 for suffix in ('.json','.ready','.failed'):(project/f'Temp/typed-recovery-live-{token}-{phase}{suffix}').unlink(missing_ok=True)
backup=root/'run/recovery-live'/datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S');backup.mkdir(parents=True);payload=json.dumps(dict(Mode='recovery-live',ConfigPath=str(a.fixture.resolve())));request.write_text(payload,encoding='utf-8')
def bench(action,**values):return item_bench.run_tool(dict(action=action,owner=pid,connection=auth,**values),a.bench_tool)
def grant(category,field,count=1,level=0,**values):return bench('grant',category=category,local=config[field],count=count,level=level,**values)
def outcome():
 for path in (persisted,result):
  try:return json.loads(path.read_text(encoding='utf-8'))
  except (FileNotFoundError,json.JSONDecodeError):pass
 return None
normal=('AttrAddLocal','AttrRerollLocal','AttrLockLocal','AttrRelockLocal','AttrUnlockLocal','AttrRareAddLocal','AttrRareRerollLocal','AttrExtraAddLocal','AttrExtraRerollLocal')
done=set();started=time.monotonic();print('Requested real Unity TLS recovery validation',flush=True)
try:
 while (record:=outcome()) is None:
  if time.monotonic()-started>a.timeout:raise TimeoutError('Unity recovery deadline')
  if not request.exists() and not (project/'Temp/typed-shop-validation-status.txt').exists() and not done:request.parent.mkdir(parents=True,exist_ok=True);request.write_text(payload,encoding='utf-8')
  for phase in range(3):
   marker=project/f'Temp/typed-recovery-live-{token}-{phase}.json'
   if phase in done or not marker.exists():continue
   assert json.loads(marker.read_text())==dict(Token=token,Phase=phase,Pid=pid,PeerPid=int(config['Accounts'][1]['Pid']) if phase==2 else 0);assert db.query(identity)==[[str(account['AccountId']),account['Name']]]
   (backup/f'phase-{phase}-before.json').write_text(json.dumps(dict(typed=bench('load'),player=db.query('SELECT map_index,x,y,hp,mp FROM player.player WHERE id='+str(pid))),indent=2),encoding='utf-8');db.query('UPDATE player.player SET map_index=41,x=986600,y=269100,hp=8924 WHERE id='+str(pid)+' AND account_id='+str(int(account['AccountId'])))
   state=bench('setup',reset=True,gold=1000)['state']
   db.query('UPDATE player.player SET hp='+('1000' if phase==0 else '50000')+',mp='+('20' if phase==0 else '50000')+' WHERE id='+str(pid)+' AND account_id='+str(int(account['AccountId'])))
   for field in ('RecoveryReserveHp','RecoveryReserveSp','RecoveryReserveDual','RecoveryPeriodicHp','RecoveryPeriodicSp','RecoveryReplaceHp','RecoveryLongHp','RecoveryCooldownReserve','RecoveryCooldownInstant','RecoveryHigh','RecoveryContext'):grant(5,field,20)
   if phase==2:
    partner=config['Accounts'][1];partner_pid=int(partner['Pid']);assert partner['Login']=='codexitem'+token+'1' and partner['Name']=='CI'+token+'1';assert db.query('SELECT account_id,name FROM player.player WHERE id='+str(partner_pid))==[[str(partner['AccountId']),partner['Name']]]
    partner_position=db.query('SELECT map_index,x,y FROM player.player WHERE id='+str(partner_pid));(backup/'partner-position.json').write_text(json.dumps(partner_position),encoding='utf-8');(backup/'partner-before.json').write_text(json.dumps(item_bench.run_tool(dict(action='status',owner=partner_pid,connection=auth),a.bench_tool)),encoding='utf-8')
    db.query('UPDATE player.player SET map_index=41,x=986700,y=269100 WHERE id='+str(partner_pid)+' AND account_id='+str(int(partner['AccountId'])))
    item_bench.run_tool(dict(action='setup',owner=partner_pid,connection=auth),a.bench_tool);grant(1,'WeaponLocal',1,1)
   marker.with_suffix('.ready').write_text(token,encoding='ascii');done.add(phase);print('Provisioned recovery phase',phase,'pid',pid,flush=True)
  time.sleep(.2)
 if not record['Success']:raise RuntimeError(record.get('Error','Recovery validation failed'))
 assert done=={0,1,2};(backup/'after.json').write_text(json.dumps(bench('load'),indent=2),encoding='utf-8');bench('setup',reset=True,gold=1000)
 for category,field,count,level in ((1,'WeaponLocal',1,1),(2,'ArmorLocal',1,0),(8,'BoxLocal',3,0),(8,'ChestLocal',3,0),(18,'KeyLocal',3,0),(18,'AltKeyLocal',2,0),(18,'WrongKeyLocal',1,0),(8,'UnsupportedBoxLocal',1,0),(8,'EmptyBoxLocal',2,0),(5,'FlaskLocal',3,0),(6,'MaterialLocal',20,0),(16,'GoldTokenLocal',10,0),(16,'CoinTokenLocal',3,0),(16,'HugeTokenLocal',2,0),(5,'CleanLocal',3,0),(5,'ExtractAllLocal',3,0),(5,'ExtractLastLocal',3,0),(7,'InsertionStoneLocal',20,3),(7,'InsertionGoldLocal',5,3),(7,'InsertionArmorLocal',3,3)):grant(category,field,count,level)
 grant(1,'SocketWeaponLocal',1,1,stones=[dict(state=3,grade=1,category=7,local=config['SocketStoneLocal'],level=3),dict(state=4,grade=2),dict(state=3,grade=2,category=7,local=config['SocketSecondStoneLocal'],level=0)]);grant(1,'InsertionWeaponLocal',1,1,stones=[dict(state=1,grade=0),dict(state=2,grade=1),dict(state=2,grade=2)]);grant(3,'AccessoryManualLocal')
 for field in ('AccessoryDrillLocal','AccessoryJewelLocal','AccessoryPermanentLocal'):grant(6,field,60)
 for category,field in ((1,'AttrWeaponLocal'),(2,'AttrArmorLocal'),(3,'AttrAccessoryLocal')):grant(category,field,level=1)
 for field in normal:grant(5,field,60)
 for field in ('RecoveryReserveHp','RecoveryReserveSp','RecoveryReserveDual','RecoveryPeriodicHp','RecoveryPeriodicSp','RecoveryReplaceHp','RecoveryLongHp','RecoveryCooldownReserve','RecoveryCooldownInstant','RecoveryHigh','RecoveryContext'):grant(5,field,20)
 if 'partner_position' in locals():
  position=partner_position[0];db.query('UPDATE player.player SET map_index='+str(int(position[0]))+',x='+str(int(position[1]))+',y='+str(int(position[2]))+' WHERE id='+str(partner_pid)+' AND account_id='+str(int(partner['AccountId'])))
  (backup/'partner-after.json').write_text(json.dumps(item_bench.run_tool(dict(action='status',owner=partner_pid,connection=auth),a.bench_tool)),encoding='utf-8')
 print('RECOVERY_UNITY_LIVE=SUCCESS; all C4-C19 probes restored, including eleven recovery probes x20; backups:',backup,flush=True)
except BaseException:
 for phase in range(3):
  marker=project/f'Temp/typed-recovery-live-{token}-{phase}.json'
  if phase not in done and marker.exists():marker.with_suffix('.failed').write_text('fixture preparation failed',encoding='ascii')
 raise
finally:db.close()
