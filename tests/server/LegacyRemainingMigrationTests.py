"""Imported consumable state must neither refill nor silently erase a payload."""
from pathlib import Path
import copy,sys,unittest
ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'SRC/Tools/ItemCatalogBuilder'))
from convert_legacy_remaining import convert_mixed_instances
from convert_legacy_equipment import SOURCE_ITEM_FIELDS
from legacy_item_migration import MigrationError

class RemainingTests(unittest.TestCase):
    def setUp(self):
        self.row={k:0 for k in SOURCE_ITEM_FIELDS};self.row.update(id=17,owner_id=3,window='INVENTORY',pos=0,count=1,vnum=72725,lockedattr=-1,socket0=1,socket1=313293,socket2=7000000)
        self.catalog=dict(accepted=[dict(source_id=72725,item=dict(category_id=5,local_id=410),refinement_level=0,semantics='automatic',capacity=7000000)])
    def test_exact_active_reserve(self):
        before=copy.deepcopy(self.row);i=convert_mixed_instances([self.row],self.catalog,3)[0]
        self.assertEqual(i['automatic'],dict(active=True,remaining_raw=6686707000,next_tick_ms=1000));self.assertEqual(i['source'],before);self.assertEqual(self.row,before)
    def test_inactive_phase_is_zero(self):
        self.row['socket0']=0;self.assertEqual(convert_mixed_instances([self.row],self.catalog,3)[0]['automatic']['next_tick_ms'],0)
    def test_unknown_payload_cannot_be_deleted(self):
        for field,value in (('socket0',2),('socket1',7000001),('socket2',1),('attrtype7',1),('lockedattr',0),('window','SAFEBOX'),('owner_id',4),('vnum',1)):
            row=dict(self.row,**{field:value})
            with self.subTest(field=field),self.assertRaises(MigrationError):convert_mixed_instances([row],self.catalog,3)
    def test_stateless_stack_and_extra_storage(self):
        self.catalog['accepted'][0].update(semantics='currency',item=dict(category_id=16,local_id=7))
        self.row.update(socket0=0,socket1=0,socket2=0,count=58,window='EXTRA_INVENTORY',pos=1440)
        i=convert_mixed_instances([self.row],self.catalog,3)[0]
        self.assertEqual(i['count'],58);self.assertEqual(i['placement'],dict(kind=1,storage_id=2,anchor=1440));self.assertIsNone(i['automatic'])
        self.row['socket0']=1
        with self.assertRaises(MigrationError):convert_mixed_instances([self.row],self.catalog,3)
if __name__=='__main__':unittest.main()
