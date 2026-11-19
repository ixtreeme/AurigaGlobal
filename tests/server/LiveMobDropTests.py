"""Real immutable drop publication/CAS/rollback on a fresh owned MariaDB."""
from pathlib import Path
import argparse,copy,json,os,sys,unittest,uuid
ROOT=Path(__file__).resolve().parents[2]
sys.path[:0]=[str(ROOT/'SRC/Tools/ServerManager'),str(ROOT/'db/tools')]
import item_catalog as catalog
from backup_sandbox import Sandbox
from mariadb_client import statements
TOOL=MARIADB=WORK=None


class LiveDropSql(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.db=Sandbox(WORK/uuid.uuid4().hex,MARIADB);cls.db.start();cls.db.verify()
        for path in [*sorted((ROOT/'db/migrations/item_combat').glob('[0-9][0-9][0-9]_*.sql')),ROOT/'db/migrations/item_instance/043_live_mob_drops.sql']:
            for s in statements(path.read_text(encoding='utf-8-sig')):cls.db.client.query(s)
        cls.auth=dict(port=cls.db.port,user='root',password=cls.db.password)
        packet=cls.call('read');doc=catalog.Catalog(packet).new(6);doc['base']['max_stack']='200'
        cls.key=(int(doc['base']['category_id']),int(doc['base']['local_id']))
        cls.call('commit',tables=catalog.Catalog(packet).apply(doc),expected_hash=packet['hash'])

    @classmethod
    def tearDownClass(cls):cls.db.stop()

    @classmethod
    def call(cls,action,**fields):return catalog.run_tool(dict(action=action,connection=cls.auth,**fields),executable=TOOL)

    def setUp(self):
        self.db.client.query('UPDATE protos.mob_drop_live_head SET release_id=NULL WHERE singleton=1')
        self.before=self.call('read')

    def tables(self,packet,count=3):
        model=catalog.Catalog(packet,live_drops=True);doc=model.new_drop_group()
        doc['group'].update(code='drop.live_sql',target_value='101')
        doc['entries']=[dict(category_id=self.key[0],local_id=self.key[1],refinement_level='0',amount=str(count),
            chance_ppb='1000000000',weight='0',rare_attribute_chance_bp='0',killer_level_min='1',killer_level_max='65535')]
        return model.apply_drops(doc)

    def publish(self,packet,tables=None,release=None):
        live=packet['live_drops']
        return self.call('commit_drops',tables=tables or self.tables(packet),expected_hash=packet['hash'],
            expected_drop_hash=live['hash'],expected_release=live['release'],release_id=release or uuid.uuid4().hex)

    def scalar(self,q):return self.db.client.query(q)[0][0]

    def test_publish_keeps_item_catalog_and_static_drop_rows_unchanged(self):
        result=self.publish(self.before)
        self.assertEqual(result['hash'],self.before['hash'])
        self.assertEqual(result['tables'],self.before['tables'])
        self.assertNotEqual(result['live_drops']['hash'],result['hash'])
        self.assertEqual(catalog.Catalog(result,live_drops=True).drop_groups()[0]['target_value'],'101')
        self.assertEqual(self.call('read')['live_drops'],result['live_drops'])

    def test_two_editors_cannot_overwrite_each_other(self):
        result=self.publish(self.before)
        with self.assertRaises(catalog.api.EditorError):self.publish(self.before,self.tables(self.before,4))
        self.assertEqual(self.call('read')['live_drops'],result['live_drops'])

    def test_write_failure_rolls_back_release_rows_and_head(self):
        before=self.scalar('SELECT COUNT(*) FROM protos.mob_drop_live_release')
        self.db.client.query("CREATE TRIGGER protos.live_drop_fail BEFORE INSERT ON protos.mob_drop_live_entry FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='owned rollback test'")
        try:
            with self.assertRaises(catalog.api.EditorError):self.publish(self.before)
        finally:self.db.client.query('DROP TRIGGER protos.live_drop_fail')
        self.assertEqual(self.scalar('SELECT COUNT(*) FROM protos.mob_drop_live_release'),before)
        self.assertEqual(self.call('read')['live_drops']['release'],'')

    def test_duplicate_request_does_not_publish_twice(self):
        release=uuid.uuid4().hex;tables=self.tables(self.before)
        first=self.publish(self.before,tables,release);again=self.publish(self.before,tables,release)
        self.assertEqual(first['live_drops'],again['live_drops'])
        self.assertEqual(self.scalar("SELECT COUNT(*) FROM protos.mob_drop_live_release WHERE release_id='"+release+"'"),'1')

    def test_live_edit_rejects_item_definition_changes(self):
        tables=self.tables(self.before);columns=self.before['columns']['item_base']
        tables['item_base'][0][columns.index('name_key')]='item.forbidden_edit'
        with self.assertRaises(catalog.api.EditorError):self.publish(self.before,tables)
        self.assertEqual(self.call('read')['live_drops']['release'],'')

    def test_invalid_refinement_is_rejected_before_sql(self):
        tables=self.tables(self.before);columns=self.before['columns']['mob_drop_entry']
        tables['mob_drop_entry'][0][columns.index('refinement_level')]='255'
        with self.assertRaises(catalog.api.EditorError):self.publish(self.before,tables)
        self.assertEqual(self.call('read')['live_drops']['release'],'')

    def test_deleting_all_groups_is_a_valid_persistent_release(self):
        first=self.publish(self.before);tables=copy.deepcopy(first['tables'])
        tables.update(mob_drop_group=[],mob_drop_entry=[])
        empty=self.publish(first,tables)
        self.assertNotEqual(empty['live_drops']['release'],first['live_drops']['release'])
        self.assertEqual(catalog.Catalog(empty,live_drops=True).drop_groups(),[])

    def test_stopped_item_edit_rebases_the_live_list_atomically(self):
        first=self.publish(self.before);doc=catalog.Catalog(first).item(self.key);doc['base']['name_key']='item.live_drop_rebase'
        updated=self.call('commit',tables=catalog.Catalog(first).apply(doc,self.key),expected_hash=first['hash'])
        self.assertEqual(updated['live_drops']['base_hash'],updated['hash'])
        self.assertNotEqual(updated['live_drops']['release'],first['live_drops']['release'])
        self.assertEqual(updated['live_drops']['tables'],first['live_drops']['tables'])


if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--tool',type=Path,required=True);p.add_argument('--mariadb-bin',type=Path,required=True);p.add_argument('--work-dir',type=Path,required=True)
    a,rest=p.parse_known_args();TOOL=a.tool.resolve();MARIADB=a.mariadb_bin;WORK=a.work_dir
    unittest.main(argv=[sys.argv[0],*rest],verbosity=2)
