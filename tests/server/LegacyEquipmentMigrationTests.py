"""Offline gear conversion policy and complete instance-state preservation."""
from pathlib import Path
import copy,sys,unittest
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'SRC/Tools/ItemCatalogBuilder'))
from convert_legacy_equipment import fixed_attack,convert_instances,ZERO_STONES,SOURCE_ITEM_FIELDS
from legacy_equipment_rules import bonus,APPLY_NAMES
from legacy_item_migration import MigrationError

class EquipmentMigrationTests(unittest.TestCase):
    def row(self,**fields):
        row={name:0 for name in SOURCE_ITEM_FIELDS}
        row.update(id=11,owner_id=7,window='INVENTORY',pos=20,count=1,vnum=19,lockedattr=-1)
        row.update(fields);return row
    def compiled(self,category=1):
        return dict(accepted=[dict(source_id=19,item=dict(category_id=category,local_id=31),refinement_level=9),
                              dict(source_id=28342,item=dict(category_id=7,local_id=51),refinement_level=3)],mapping=[],stone_grades={'28342':1})
    def test_damage_is_fixed_exact_mean_plus_refinement(self):
        self.assertEqual(fixed_attack(dict(value1=15,value2=19,value3=13,value4=15,value5=63)),(77000,80000))
        self.assertEqual(fixed_attack(dict(value1=1,value2=2,value3=8,value4=9,value5=4)),(12500,5500))
        with self.assertRaises(MigrationError):fixed_attack(dict(value1=2,value2=1,value3=8,value4=9,value5=4))
    def test_all_eight_attributes_lock_stones_and_source_are_retained(self):
        row=self.row(socket0=1,socket1=28342,socket2=28960,lockedattr=2)
        for i,kind in enumerate((1,4,15,16,34,5,6,19)):row[f'attrtype{i}']=kind;row[f'attrvalue{i}']=i+1
        original=copy.deepcopy(row);item=convert_instances([row],self.compiled(),7)[0]
        self.assertEqual(row,original);self.assertEqual(item['source'],original)
        self.assertEqual(item['attributes'],[bonus(row[f'attrtype{i}'],i+1) for i in range(8)])
        self.assertEqual(item['locked_slot'],2)
        self.assertEqual([s['state'] for s in item['stones']],[2,3,4])
        self.assertEqual(item['stones'][1],dict(state=3,grade=1,level=3,stone=dict(category_id=7,local_id=51)))
    def test_empty_closed_and_high_grade_slots_are_distinct(self):
        item=convert_instances([self.row(socket0=0,socket1=1,socket2=2)],self.compiled(),7)[0]
        self.assertEqual([(s['state'],s['grade']) for s in item['stones']],[(1,0),(2,1),(2,2)])
    def test_accessory_capacity_active_grade_and_time(self):
        for seconds,permanent,remaining in ((700,False,700000),(86410,True,0)):
            item=convert_instances([self.row(socket0=2,socket1=3,socket2=seconds)],self.compiled(3),7)[0]
            self.assertEqual(item['accessory'],dict(grade=2,capacity=3,remaining_ms=remaining,permanent=permanent))
    def test_equipment_slots_are_explicit(self):
        for old,new in ((4,0),(0,1),(1,2),(10,3),(2,4),(3,5),(5,6),(6,7)):
            item=convert_instances([self.row(window='EQUIPMENT',pos=old)],self.compiled(),7)[0]
            self.assertEqual(item['placement'],dict(kind=2,storage_id=1,anchor=new))
        self.assertEqual(convert_instances([self.row(window='EXTRA_INVENTORY',pos=720)],self.compiled(),7)[0]['placement'],dict(kind=1,storage_id=2,anchor=720))
    def test_unknown_or_incomplete_state_fails_closed(self):
        for row in (self.row(owner_id=8),self.row(vnum=88),self.row(socket1=55555),self.row(lockedattr=0),self.row(attrtype0=255),self.row(window='SAFEBOX')):
            with self.subTest(row=row),self.assertRaises(MigrationError):convert_instances([row],self.compiled(),7)
        row=self.row();row.pop('attrvalue7')
        with self.assertRaises(MigrationError):convert_instances([row],self.compiled(),7)
    def test_percentage_proc_and_flat_properties_keep_distinct_units(self):
        self.assertEqual(bonus(1,2500),dict(bonus_id='35',value_scaled='2500000'))
        self.assertEqual(bonus(16,8),dict(bonus_id='46',value_scaled='800'))
        self.assertEqual(bonus(58,500),dict(bonus_id='143',value_scaled='500000'))
        self.assertEqual(APPLY_NAMES['CAST_SPEED'],9)

if __name__=='__main__':unittest.main()
