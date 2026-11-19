"""Atomic live chest + monster rewards on an isolated owned MariaDB."""
import argparse,copy,json,os,sys,unittest,uuid
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path[:0]=[str(ROOT/'SRC/Tools/ServerManager'),str(ROOT/'db/tools')]
import item_catalog as catalog
from reward_catalog import Rewards
from backup_sandbox import Sandbox
from mariadb_client import statements
TOOL=MARIADB=WORK=None


class LiveRewardSql(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.db=Sandbox(WORK/uuid.uuid4().hex,MARIADB);cls.db.start();cls.db.verify()
        paths=[*sorted((ROOT/'db/migrations/item_combat').glob('[0-9][0-9][0-9]_*.sql')),
               ROOT/'db/migrations/item_instance/043_live_mob_drops.sql',ROOT/'db/migrations/item_instance/044_live_container_loot.sql']
        for path in paths:
            for statement in statements(path.read_text(encoding='utf-8-sig')):cls.db.client.query(statement)
        cls.auth=dict(port=cls.db.port,user='root',password=cls.db.password)
        packet=cls.call('read');document=catalog.Catalog(packet).new(6);document['base']['max_stack']='200'
        cls.item=(6,int(document['base']['local_id']));packet=cls.commit(packet,document)
        document=catalog.Catalog(packet).new(6);document['base']['max_stack']='200'
        cls.reward=(6,int(document['base']['local_id']));packet=cls.commit(packet,document)
        cls.boxes=[]
        for _ in range(3):
            document=catalog.Catalog(packet).new(8)
            document['loot']=dict(pick_mode='2',entries=[dict(entry_no='1',reward_kind='1',item_category_id='6',item_local_id=str(cls.item[1]),
                amount='1',weight='10000',rare_attribute_chance_bp='0',refinement_level='0')])
            cls.boxes.append((8,int(document['base']['local_id'])));packet=cls.commit(packet,document)
        model=catalog.Catalog(packet);shared=model.tables['item_container'][0]['loot_profile_id']
        for row in model.tables['item_container']:row['loot_profile_id']=shared
        cls.call('commit',tables=model.encode(),expected_hash=packet['hash'])

    @classmethod
    def tearDownClass(cls):cls.db.stop()

    @classmethod
    def call(cls,action,**fields):return catalog.run_tool(dict(action=action,connection=cls.auth,**fields),executable=TOOL)

    @classmethod
    def commit(cls,packet,document):return cls.call('commit',tables=catalog.Catalog(packet).apply(document),expected_hash=packet['hash'])

    def setUp(self):
        self.db.client.query('UPDATE protos.mob_drop_live_head SET release_id=NULL WHERE singleton=1')
        self.before=self.call('read')

    def publish(self,packet,model=None,tables=None):
        model=model or Rewards(packet);live=packet['live_drops']
        return self.call('commit_drops',tables=tables or model.model.encode(),expected_hash=packet['hash'],
            expected_drop_hash=live['hash'],expected_release=live['release'],release_id=uuid.uuid4().hex,**model.container_rows())

    def changed(self,packet,count=2,key=None):
        model=Rewards(packet);key=key or self.boxes[0];loot=model.loot(key);loot['entries'][0]['amount']=str(count);model.set_loot(key,loot);return model

    def scalar(self,text):return self.db.client.query(text)[0][0]

    def test_chests_copy_on_write_and_full_base_stays_unchanged(self):
        result=self.publish(self.before,self.changed(self.before));self.assertEqual(result['hash'],self.before['hash'])
        self.assertEqual(result['tables'],self.before['tables'])
        model=Rewards(result);self.assertEqual(model.loot(self.boxes[0])['entries'][0]['amount'],'2')
        for key in self.boxes[1:]:self.assertEqual(model.loot(key)['entries'][0]['amount'],'1')
        self.assertNotEqual(result['live_drops']['hash'],result['hash'])

    def test_one_release_contains_multiple_chests_and_mobs(self):
        model=Rewards(self.before)
        for key in self.boxes[:2]:
            loot=model.loot(key);loot['entries'][0]['amount']='3';model.set_loot(key,loot)
        drops=catalog.Catalog(self.before,live_drops=True)
        for mob in ('101','103'):
            document=drops.new_drop_group();document['group']['target_value']=mob
            document['entries']=[dict(category_id='6',local_id=str(self.item[1]),refinement_level='0',amount='3',chance_ppb='1000000000',
                weight='0',rare_attribute_chance_bp='0',killer_level_min='1',killer_level_max='65535')]
            tables=drops.apply_drops(document);packet=copy.deepcopy(self.before);packet['tables']=tables;drops=catalog.Catalog(packet)
        result=self.publish(self.before,model,tables=drops.encode());read=Rewards(result)
        self.assertEqual(len(read.model.drop_groups()),2);self.assertEqual(len(result['live_drops']['container_groups']),2)
        self.assertEqual(read.loot(self.boxes[1])['entries'][0]['amount'],'3');self.assertEqual(read.loot(self.boxes[2])['entries'][0]['amount'],'1')

    def test_pending_queue_collects_edits_before_one_verified_sql_release(self):
        import tempfile
        from unittest import mock
        os.environ.setdefault('QT_QPA_PLATFORM','offscreen')
        from PySide6.QtWidgets import QApplication,QMainWindow
        from pending_changes import PendingChanges
        import change_review as review
        app=QApplication.instance() or QApplication([])
        with tempfile.TemporaryDirectory(prefix='auriga-pending-sql-') as temporary:
            owner=QMainWindow();owner.root=Path(temporary);owner.change_review_root=Path(temporary)/'stages';owner.target_id=lambda:(1,'local',str(owner.root))
            queue=PendingChanges(owner);packet=self.before
            for key,count in zip(self.boxes[:2],('2','3')):
                loot=Rewards(packet).loot(key);loot['entries'][0]['amount']=count
                queue.enqueue('Láda','container',dict(before=packet,key=key,document=loot),[('Láda','Darab','1',count)])
                packet=queue.overlay_packet(packet)
            group=catalog.Catalog(packet,live_drops=True).new_drop_group();group['group']['target_value']='103'
            group['entries']=[dict(category_id='6',local_id=str(self.item[1]),refinement_level='0',amount='2',chance_ppb='1000000000',weight='0',rare_attribute_chance_bp='0',killer_level_min='1',killer_level_max='65535')]
            tables=catalog.Catalog(packet,live_drops=True).apply_drops(group)
            queue.enqueue('Mobdrop','mob_drop',dict(before=packet,document=group,original_id=None),review.reward_rows(packet,tables))
            self.assertEqual(self.call('read')['live_drops'],self.before['live_drops'],'staging must not write SQL')
            self.assertEqual(len(queue.entries()),3);calls=[]
            def commit(root,before,tables,record,*,containers):
                calls.append(1);live=before['live_drops']
                return self.call('commit_drops',tables=tables,expected_hash=before['hash'],expected_drop_hash=live['hash'],expected_release=live['release'],release_id=uuid.uuid4().hex,**containers)
            with mock.patch.object(catalog,'request',side_effect=lambda root:self.call('read')),mock.patch.object(catalog,'commit_drop_tables',side_effect=commit):result=queue.publish_catalog(queue.entries())
            self.assertEqual(len(calls),1);read=Rewards(result)
            self.assertEqual([read.loot(key)['entries'][0]['amount'] for key in self.boxes[:2]],['2','3']);self.assertEqual(len(read.model.drop_groups()),1)
            self.assertEqual(result['hash'],self.before['hash']);owner.deleteLater();app.processEvents()

    def test_combined_new_item_and_rewards_are_one_atomic_catalog_batch(self):
        model=catalog.Catalog(self.before);doc=model.new(6);doc['base']['max_stack']='200';key=(6,int(doc['base']['local_id']))
        tables=model.apply(doc);draft=dict(self.before,tables=tables);rewards=Rewards(draft)
        loot=rewards.loot(self.boxes[0]);loot['entries'].append(dict(entry_no='2',reward_kind='1',item_category_id='6',item_local_id=str(key[1]),amount='2',weight='2500',rare_attribute_chance_bp='0',refinement_level='0'))
        rewards.set_loot(self.boxes[0],loot);live=self.before['live_drops']
        args=dict(tables=tables,expected_hash=self.before['hash'],expected_release=live['release'],expected_drop_hash=live['hash'],release_id=uuid.uuid4().hex,**rewards.container_rows())
        invalid=copy.deepcopy(args);invalid['container_entries'][-1][6]='201'
        with self.assertRaises(catalog.api.EditorError):self.call('commit_catalog_batch',**invalid)
        unchanged=self.call('read');self.assertEqual(unchanged['hash'],self.before['hash']);self.assertEqual(unchanged['live_drops'],self.before['live_drops'])
        result=self.call('commit_catalog_batch',**args);self.assertIsNotNone(catalog.Catalog(result).item(key))
        self.assertEqual(Rewards(result).loot(self.boxes[0])['entries'][-1]['item_local_id'],str(key[1]))
        self.assertEqual(Rewards(result).loot(self.boxes[1]),Rewards(self.before).loot(self.boxes[1]))

    def test_combined_batch_rejects_stale_live_head_without_writing_item_changes(self):
        model=catalog.Catalog(self.before);doc=model.item(self.item);doc['base']['min_level']='7';tables=model.apply(doc,self.item)
        live=self.before['live_drops'];head=self.publish(self.before,self.changed(self.before))
        with self.assertRaises(catalog.api.EditorError):self.call('commit_catalog_batch',tables=tables,expected_hash=self.before['hash'],expected_release=live['release'],
            expected_drop_hash=live['hash'],release_id=uuid.uuid4().hex,**Rewards(head).container_rows())
        current=self.call('read');self.assertEqual(current['hash'],self.before['hash']);self.assertEqual(current['live_drops'],head['live_drops'])

    def test_actual_bulk_planner_publishes_all_targets_in_one_release(self):
        model=catalog.Catalog(self.before,live_drops=True);group=model.new_drop_group();group['group']['target_value']='103'
        group['entries']=[dict(category_id='6',local_id=str(self.item[1]),refinement_level='0',amount='1',chance_ppb='1000000000',
            weight='0',rare_attribute_chance_bp='0',killer_level_min='1',killer_level_max='65535')]
        first=self.publish(self.before,tables=model.apply_drops(group))
        rewards=Rewards(first);identity=rewards.model.drop_groups()[0]['group_id']
        plan=rewards.plan_add(self.reward,[('box',self.boxes[0]),('box',self.boxes[1]),('mob',identity)],amount=2,box_chance='25',drop_chance='5')
        self.assertEqual(plan['changed'],3)
        live=first['live_drops']
        saved=self.call('commit_drops',tables=plan['tables'],expected_hash=first['hash'],expected_drop_hash=live['hash'],
                        expected_release=live['release'],release_id=uuid.uuid4().hex,**plan['containers'])
        after=Rewards(saved)
        self.assertEqual([after.loot(key)['entries'][-1]['weight'] for key in self.boxes[:2]],['2500','2500'])
        self.assertEqual(after.model.drop_group(identity)['entries'][-1]['chance_ppb'],'50000000')
        repeat=after.plan_add(self.reward,[('box',self.boxes[0]),('box',self.boxes[1]),('mob',identity)])
        self.assertEqual(repeat['changed'],0,'retry does not append duplicate rewards')

    def test_bulk_removal_handles_last_rewards_atomically_and_preserves_shared_chest(self):
        model=catalog.Catalog(self.before,live_drops=True);group=model.new_drop_group();group['group']['target_value']='103'
        group['entries']=[dict(category_id='6',local_id=str(self.item[1]),refinement_level='0',amount='1',chance_ppb='1000000000',
            weight='0',rare_attribute_chance_bp='0',killer_level_min='1',killer_level_max='65535')]
        first=self.publish(self.before,tables=model.apply_drops(group));rewards=Rewards(first)
        identity=rewards.model.drop_groups()[0]['group_id'];plan=rewards.plan_remove(self.item,[('box',self.boxes[0]),('box',self.boxes[1]),('mob',identity)])
        live=first['live_drops'];saved=self.call('commit_drops',tables=plan['tables'],expected_hash=first['hash'],expected_drop_hash=live['hash'],
            expected_release=live['release'],release_id=uuid.uuid4().hex,**plan['containers'])
        read=Rewards(saved);self.assertEqual(saved['hash'],self.before['hash']);self.assertEqual(saved['tables'],self.before['tables'])
        for key in self.boxes[:2]:self.assertEqual(read.loot(key)['entries'][0]['reward_kind'],'9')
        self.assertEqual(read.loot(self.boxes[2]),Rewards(self.before).loot(self.boxes[2]));self.assertEqual(read.model.drop_groups(),[])
        self.assertEqual(read.plan_remove(self.item,[('box',self.boxes[0])])['changed'],0)

    def test_existing_count_and_chance_update_is_one_live_release_without_new_entries(self):
        model=catalog.Catalog(self.before,live_drops=True);group=model.new_drop_group();group['group']['target_value']='103'
        group['entries']=[dict(category_id='6',local_id=str(self.item[1]),refinement_level='0',amount='1',chance_ppb='1000000000',
            weight='0',rare_attribute_chance_bp='1250',killer_level_min='10',killer_level_max='90')]
        first=self.publish(self.before,tables=model.apply_drops(group));rewards=Rewards(first);identity=rewards.model.drop_groups()[0]['group_id']
        plan=rewards.plan_update(self.item,[('box',self.boxes[0]),('box',self.boxes[1]),('mob',identity)],amount=3,box_chance='50',drop_chance='75')
        live=first['live_drops'];saved=self.call('commit_drops',tables=plan['tables'],expected_hash=first['hash'],expected_drop_hash=live['hash'],
            expected_release=live['release'],release_id=uuid.uuid4().hex,**plan['containers'])
        read=Rewards(saved);self.assertEqual(saved['hash'],self.before['hash']);self.assertEqual(saved['tables'],self.before['tables'])
        for key in self.boxes[:2]:
            entries=read.loot(key)['entries'];self.assertEqual(len(entries),1);self.assertEqual((entries[0]['amount'],entries[0]['weight']),('3','5000'))
        self.assertEqual(read.loot(self.boxes[2]),Rewards(self.before).loot(self.boxes[2]))
        mob=read.model.drop_group(identity)['entries'];self.assertEqual(len(mob),1)
        self.assertEqual((mob[0]['amount'],mob[0]['chance_ppb'],mob[0]['rare_attribute_chance_bp'],mob[0]['killer_level_min'],mob[0]['killer_level_max']),('3','750000000','1250','10','90'))
        repeat=self.call('commit_drops',tables=plan['tables'],expected_hash=first['hash'],expected_drop_hash=live['hash'],expected_release=live['release'],release_id=uuid.uuid4().hex,**plan['containers'])
        self.assertEqual(repeat['live_drops'],saved['live_drops'],'an identical retry is harmless')
        stale=rewards.plan_update(self.item,[('box',self.boxes[0]),('mob',identity)],amount=4,drop_chance='60')
        with self.assertRaises(catalog.api.EditorError):
            self.call('commit_drops',tables=stale['tables'],expected_hash=first['hash'],expected_drop_hash=live['hash'],expected_release=live['release'],release_id=uuid.uuid4().hex,**stale['containers'])

    def test_stale_release_cannot_partially_overwrite_chests(self):
        first=self.publish(self.before,self.changed(self.before))
        with self.assertRaises(catalog.api.EditorError):self.publish(self.before,self.changed(self.before,4))
        self.assertEqual(self.call('read')['live_drops'],first['live_drops'])

    def test_sql_failure_rolls_back_entire_mixed_release(self):
        before_count=self.scalar('SELECT COUNT(*) FROM protos.mob_drop_live_release')
        self.db.client.query("CREATE TRIGGER protos.reject_live_box BEFORE INSERT ON protos.container_loot_live_entry FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='fixture failure'")
        try:
            with self.assertRaises(catalog.api.EditorError):self.publish(self.before,self.changed(self.before))
        finally:self.db.client.query('DROP TRIGGER protos.reject_live_box')
        self.assertEqual(self.scalar('SELECT COUNT(*) FROM protos.mob_drop_live_release'),before_count)
        self.assertEqual(self.call('read')['live_drops'],self.before['live_drops'])

    def test_invalid_stack_refinement_and_foreign_chest_reject_before_publication(self):
        for field,value in (('amount','201'),('refinement_level','1')):
            model=self.changed(self.before);model.overrides[self.boxes[0]]['entries'][0][field]=value
            with self.assertRaises(catalog.api.EditorError):self.publish(self.before,model)
        model=self.changed(self.before);model.overrides[(8,999999)]=model.overrides.pop(self.boxes[0])
        with self.assertRaises(catalog.api.EditorError):self.publish(self.before,model)
        self.assertEqual(self.call('read')['live_drops'],self.before['live_drops'])

    def test_legacy_drop_editor_preserves_live_chests_and_retry_is_idempotent(self):
        first=self.publish(self.before,self.changed(self.before))
        same=self.publish(first,self.changed(first))
        self.assertEqual(first['live_drops'],same['live_drops'])
        model=catalog.Catalog(first,live_drops=True);group=model.new_drop_group();group['group']['target_value']='103'
        group['entries']=[dict(category_id='6',local_id=str(self.item[1]),refinement_level='0',amount='1',chance_ppb='1000000000',
            weight='0',rare_attribute_chance_bp='0',killer_level_min='1',killer_level_max='65535')]
        result=self.call('commit_drops',tables=model.apply_drops(group),expected_hash=first['hash'],expected_drop_hash=first['live_drops']['hash'],
                         expected_release=first['live_drops']['release'],release_id=uuid.uuid4().hex)
        self.assertEqual(result['live_drops']['container_entries'],first['live_drops']['container_entries'])

    def test_non_item_rewards_nulls_and_restore_to_base_round_trip(self):
        model=self.changed(self.before);loot=model.loot(self.boxes[0]);loot['entries'].append(dict(entry_no='2',reward_kind='2',
            item_category_id=None,item_local_id=None,amount='50',weight='10000',rare_attribute_chance_bp='0',refinement_level='0'))
        model.set_loot(self.boxes[0],loot);first=self.publish(self.before,model)
        self.assertIsNone(first['live_drops']['container_entries'][-1][4])
        restored=self.publish(first,Rewards(self.before))
        self.assertEqual(restored['live_drops']['container_groups'],[]);self.assertEqual(restored['live_drops']['hash'],self.before['hash'])


if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--tool',type=Path,required=True);parser.add_argument('--mariadb-bin',type=Path,required=True);parser.add_argument('--work-dir',type=Path,required=True)
    args,rest=parser.parse_known_args();TOOL=args.tool.resolve();MARIADB=args.mariadb_bin;WORK=args.work_dir
    unittest.main(argv=[sys.argv[0],*rest],verbosity=2)
