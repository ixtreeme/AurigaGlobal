"""Three owned accounts; a PID-guarded temporary private-map NPC combat fixture."""
from pathlib import Path
import argparse,json,subprocess,sys
REPO=Path(__file__).resolve().parents[2]
sys.path[:0]=[str(REPO/'SRC/Tools/ServerManager'),str(REPO/'db/tools')]
import backend,item_bench
from mariadb_client import Client

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('action',choices=('prepare','reset','verify'));p.add_argument('--package',type=Path,required=True);p.add_argument('--fixture',type=Path,required=True);a=p.parse_args()
    path=a.fixture.resolve();assert path.is_relative_to(REPO/'build') and path.name.endswith('.private.json')
    root,manager,_,auth=item_bench.connection(a.package.resolve())
    if a.action=='prepare':
        assert not backend.describe(root)['running'] and not path.exists()
        subprocess.run([sys.executable,'-B',str(REPO/'tests/server/SocialHistoryUnityLiveFixture.py'),'prepare','--package',str(root),'--fixture',str(path)],check=True)
    c=json.loads(path.read_text());actors=c['Accounts'];assert len(actors)==3 and all(v['Login']=='codexsocial'+c['Token']+str(i) for i,v in enumerate(actors))
    db=Client(**auth,library=str(root/'bin/libmariadb.dll'))
    try:
        if a.action=='verify':
            ids=','.join(str(v['Pid']) for v in actors)
            assert db.query(f'SELECT owner_id,balance FROM player.item_wallet WHERE owner_kind=1 AND owner_id IN({ids}) AND currency=1 ORDER BY owner_id')==[[str(v['Pid']),'1000000'] for v in actors]
            assert int(db.query(f"SELECT hp FROM player.player WHERE id={actors[0]['Pid']}")[0][0])>0
            print('DEATH_RECAP_SQL=PASS isolated_player_alive native_wallets_unchanged=3');return
        assert not backend.describe(root)['running']
        for i,v in enumerate(actors):db.query(f"UPDATE player.player SET map_index=41,x={c['X']+i*100},y={c['Y']},hp=200,mp=200,random_hp=0,random_sp=0 WHERE id={v['Pid']} AND account_id={v['AccountId']}")
        if a.action=='reset':print('DEATH_RECAP_FIXTURE=RESET only_owned_positions_and_health=1');return
        folder=root/'run'/('dungeon-ui-live-'+c['Token']);folder.mkdir()
        questlist=root/'data/quest/quest_list';original=questlist.read_bytes();(folder/'quest_list.original').write_bytes(original)
        # Reuse the existing exact, audited dungeon-fixture restore/boot-restore path.
        relative='Quest/system/codex_dungeon_ui_'+c['Token']+'.lua';source=root/'data/quest'/relative;assert not source.exists()
        first=actors[0]['Pid'];guard=f'pc.get_player_id() == {first}'
        text=f'''quest codex_dungeon_ui_{c['Token']} begin
 state start begin
  when login with {guard} begin
   send_letter("Codex death recap local fixture")
   cmdchat("nx_dr_fixture 1 " .. q.getcurrentquestindex() .. " " .. pc.get_map_index())
   if pc.in_dungeon() and pc.get_map_index() >= 660000 and pc.get_map_index() < 670000 then
    local idx=pc.get_map_index()
    if d.getf(idx,"codex_death_owner") == 0 then
     clear_server_timer("deviltower_prepare",idx)
     clear_server_timer("deviltower_check",idx)
     clear_server_timer("deviltower_end",idx)
     d.clear_regen()
     d.kill_all(idx)
     d.setf(idx,"floor",13)
     d.setf(idx,"codex_death_owner",{first})
    end
   end
  end
  when button or info with {guard} begin
   if not pc.in_dungeon() then
    d.join_cords(66,3774,27040)
   else
    local idx=pc.get_map_index()
    if d.getf(idx,"codex_death_owner") ~= {first} then return end
    d.kill_all(idx)
    local vid=mob.spawn(691,pc.get_local_x()+2,pc.get_local_y(),0,true,1)
    cmdchat("nx_dr_mob 1 " .. vid)
   end
  end
  when die with {guard} and pc.in_dungeon() begin
   local idx=pc.get_map_index()
   if d.getf(idx,"codex_death_owner") == {first} then d.kill_all(idx) end
  end
 end
end
'''
        source.write_text(text,encoding='utf-8');questlist.write_bytes(original.rstrip()+b'\n'+relative.encode()+b'\n')
        c.update(QuestSource=str(source),QuestListBackup=str(folder/'quest_list.original'),QuestListInstalledHex=questlist.read_bytes().hex())
        path.write_text(json.dumps(c,indent=2)+'\n');print('DEATH_RECAP_FIXTURE=READY new_accounts=3 guarded_private_instance npc_ai_combat=1')
    finally:db.close()
if __name__=='__main__':main()
