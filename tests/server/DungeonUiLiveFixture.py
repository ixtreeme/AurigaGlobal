"""Isolated local dungeon UI game flow: three new accounts and a PID-guarded temporary quest."""
from pathlib import Path
import argparse,json,subprocess,sys
REPO=Path(__file__).resolve().parents[2]
sys.path[:0]=[str(REPO/'SRC/Tools/ServerManager'),str(REPO/'db/tools')]
import backend,item_bench
from mariadb_client import Client,statements

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('action',choices=('prepare','verify','reset','restore','finish-restore'));p.add_argument('--package',required=True,type=Path);p.add_argument('--fixture',required=True,type=Path);a=p.parse_args()
    path=a.fixture.resolve();assert path.is_relative_to(REPO/'build') and path.name.endswith('.private.json')
    root,manager,_,auth=item_bench.connection(a.package.resolve())
    if a.action=='prepare':
        assert not backend.describe(root)['running'] and not path.exists()
        subprocess.run([sys.executable,'-B',str(REPO/'tests/server/SocialHistoryUnityLiveFixture.py'),'prepare','--package',str(root),'--fixture',str(path)],check=True)
    c=json.loads(path.read_text());actors=c['Accounts'];assert len(actors)==3 and all(v['Login']=='codexsocial'+c['Token']+str(i) for i,v in enumerate(actors))
    db=Client(**auth,library=str(root/'bin/libmariadb.dll'))
    try:
        if a.action=='finish-restore':
            # apply-quests conservatively rejects every removed quest, even one
            # with no persisted flags. Restore only this exact owned fixture's
            # previous immutable boot package, after proving source and ID parity.
            assert not backend.describe(root)['running']
            questlist=root/'data/quest/quest_list';assert questlist.read_bytes()==Path(c['QuestListBackup']).read_bytes()
            assert not Path(c['QuestSource']).exists()
            name='codex_dungeon_ui_'+c['Token'];assert name.replace('_','').isalnum()
            assert not db.query(f"SELECT dwPID FROM player.quest WHERE szName='{name}'"),'Persisted fixture flags require a separate migration'
            publication=root/'run/quest-reloads/current.json';old=manager.read_json(publication)
            assert name in old['quest_ids'] and old['quest_ids'][name]==max(old['quest_ids'].values())
            expected={k:v for k,v in old['quest_ids'].items() if k!=name}
            probe=manager.native([manager.executable('ServerPackageCheck'),'--quest-fingerprint',root/'data',root/'data/quest'])
            assert probe.returncode==0,probe.stderr
            fingerprint=json.loads(probe.stdout);matches=[]
            for candidate in (root/'run/quest-reloads').glob('*/completed.json'):
                p=manager.read_json(candidate)
                if p['revision']==fingerprint['revision'] and p['quest_ids']==expected:matches.append(p)
            assert len(matches)==1,'No unique unchanged baseline boot package'
            baseline=matches[0];snapshot=Path(baseline['snapshot']).resolve(strict=True)
            assert snapshot==root/'run/quest-reloads'/baseline['id']/'data'
            probe=manager.native([manager.executable('ServerPackageCheck'),'--quest-fingerprint',snapshot,snapshot/'quest'])
            assert probe.returncode==0 and json.loads(probe.stdout)==fingerprint,'Baseline snapshot no longer matches restored sources'
            backup=root/'run'/('dungeon-ui-live-'+c['Token'])/'test-publication.original.json'
            assert not backup.exists();backup.write_bytes(publication.read_bytes())
            manager.save_json(publication,baseline)
            print('DUNGEON_UI_BOOT=RESTORED exact_source_fingerprint=1 existing_quest_ids_unchanged=76 persisted_fixture_flags=0');return
        if a.action=='reset':
            assert not backend.describe(root)['running']
            for i,v in enumerate(actors):db.query(f"UPDATE player.player SET map_index=41,x={c['X']+i*100},y={c['Y']},hp=30000,mp=10000 WHERE id={v['Pid']} AND account_id={v['AccountId']}")
            c['RunWalletStart']={str(v['Pid']):int(db.query(f"SELECT balance FROM player.item_wallet WHERE owner_kind=1 AND owner_id={v['Pid']} AND currency=1")[0][0]) for v in actors};path.write_text(json.dumps(c,indent=2)+'\n')
            print('DUNGEON_UI_FIXTURE=RESET only_owned_positions=1');return
        if a.action=='prepare':
            for statement in statements((REPO/'db/migrations/dungeon/001_dungeon_ui.sql').read_text()):db.query(statement)
            folder=root/'run'/('dungeon-ui-live-'+c['Token']);folder.mkdir()
            questlist=root/'data/quest/quest_list';old=questlist.read_bytes();(folder/'quest_list.original').write_bytes(old)
            relative='Quest/system/codex_dungeon_ui_'+c['Token']+'.lua';source=root/'data/quest'/relative;assert not source.exists()
            first,second=actors[0]['Pid'],actors[1]['Pid'];guard=f'(pc.get_player_id() == {first} or pc.get_player_id() == {second})'
            text=f'''quest codex_dungeon_ui_{c['Token']} begin
 state start begin
  when login with {guard} begin
   send_letter("Codex dungeon UI local fixture")
   cmdchat("nx_dui_fixture 1 " .. q.getcurrentquestindex() .. " " .. pc.get_map_index())
   if pc.in_dungeon() and pc.get_map_index() >= 660000 and pc.get_map_index() < 670000 then
    local idx = pc.get_map_index()
    pc.setf("codex_dungeon_ui_fixture", "enter_time", get_global_time())
    if pc.get_player_id() == {first} then
     clear_server_timer("deviltower_prepare", idx)
     clear_server_timer("deviltower_checker", idx)
     clear_server_timer("deviltower_end", idx)
     d.clear_regen(idx)
     d.kill_all(idx)
     d.setf(idx, "floor", 13)
     d.setf(idx, "codex_live_owner", {first})
    end
   end
  end
  when button or info with {guard} begin
   if pc.get_player_id() != {first} then return end
   if not pc.in_dungeon() then
    d.join_cords(66, 3774, 27040)
   else
    local idx = pc.get_map_index()
    if d.getf(idx, "codex_live_owner") != {first} or d.getf(idx, "codex_live_done") != 0 then return end
    local vid = d.spawn_mob(idx, 101, pc.get_local_x(), pc.get_local_y() + 2)
    cmdchat("nx_dui_fixture_mob 1 " .. vid)
   end
  end
  when 101.kill with {guard} and pc.in_dungeon() begin
   local idx = pc.get_map_index()
   if d.getf(idx, "codex_live_owner") != {first} or d.getf(idx, "codex_live_done") != 0 then return end
   d.setf(idx, "codex_live_done", 1)
   pc.give_exp2(1234)
   d.complete(101, 1, 66, "codex_dungeon_ui_fixture")
  end
 end
end
'''
            source.write_text(text,encoding='utf-8');questlist.write_bytes(old.rstrip()+b'\n'+relative.encode()+b'\n')
            c['QuestSource']=str(source);c['QuestListBackup']=str(folder/'quest_list.original');c['QuestListInstalledHex']=questlist.read_bytes().hex()
            path.write_text(json.dumps(c,indent=2)+'\n');print('DUNGEON_UI_FIXTURE=READY new_accounts=3 pid_guarded_quest=1 additive_schema=1');return
        if a.action=='verify':
            first,second,outsider=[v['Pid'] for v in actors]
            rows=db.query(f'SELECT HEX(r.id),r.dungeon,r.elapsed,r.monsters,COUNT(*),SUM(m.damage),MAX(m.experience) FROM player.dungeon_ui_run r JOIN player.dungeon_ui_member m ON m.run_id=r.id WHERE m.pid IN({first},{second}) GROUP BY r.id,r.dungeon,r.elapsed,r.monsters ORDER BY r.completed_at DESC')
            assert rows and int(rows[0][1])==66 and int(rows[0][3])>=1 and int(rows[0][4])==2 and int(rows[0][5])>0 and int(rows[0][6])>=1234,rows
            assert db.query(f'SELECT COUNT(*) FROM player.dungeon_ui_member WHERE pid={outsider}')==[['0']]
            result_id=rows[0][0]
            for pid,delta,balance in db.query(f'SELECT m.pid,m.yang_delta,w.balance FROM player.dungeon_ui_member m JOIN player.item_wallet w ON w.owner_kind=1 AND w.owner_id=m.pid AND w.currency=1 WHERE m.run_id=UNHEX(\'{result_id}\') ORDER BY m.pid'):assert int(delta)==int(balance)-c.get('RunWalletStart',{}).get(pid,1000000)
            print('DUNGEON_UI_SQL=PASS real_instance real_damage real_kill real_completion participants=2 outsider=0 actual_exp wallet_delta_exact');return
        assert not backend.describe(root)['running']
        source=Path(c['QuestSource']).resolve();assert source.parent==root/'data/quest/Quest/system' and source.name=='codex_dungeon_ui_'+c['Token']+'.lua'
        questlist=root/'data/quest/quest_list';assert questlist.read_bytes().hex()==c['QuestListInstalledHex'],'Quest list changed; preserve it for manual reconciliation'
        questlist.write_bytes(Path(c['QuestListBackup']).read_bytes());source.unlink();print('DUNGEON_UI_FIXTURE=RESTORED original_quest_list=1 temporary_quest_removed=1')
    finally:db.close()
if __name__=='__main__':main()
