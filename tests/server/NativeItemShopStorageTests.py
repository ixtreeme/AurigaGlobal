from pathlib import Path
import importlib.util
import unittest
ROOT=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location("native_shop_migration",ROOT/"SRC/Tools/ItemCatalogBuilder/migrate_item_shop_storage.py")
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)

def source():
    return dict(offers=[[1,4,2,99000001,7,10,"1970-01-01 00:00:01","2026-11-26 12:00:00",100,3,10]],
        mapping=[[1,99000001,6,58,0]],packages=[],package_members=[],history=[])
class NativeShopStorage(unittest.TestCase):
    def test_typed_members_and_merchant_controls(self):
        plan=module.build_plan(source())
        self.assertEqual(plan["members"],[(1,1,6,58,0,1)])
        self.assertEqual(plan["offers"][0][:10],(1,4,2,7,10,"1970-01-01 00:00:01","2026-11-26 12:00:00",100,3,10))
        self.assertEqual(plan["offers"][0][10],1)
        self.assertNotIn(99000001,plan["offers"][0])
    def test_changed_or_unconverted_rows_do_not_become_offers(self):
        data=source();data["offers"][0][3]=99000002
        self.assertEqual(module.build_plan(data)["offers"],[])
        self.assertEqual(module.build_plan(data)["skipped"],1)
    def test_prior_limits_share_a_native_cohort_and_duplicate_history_survives(self):
        data=source();data["offers"].append([2,*data["offers"][0][1:]])
        data["mapping"].append([2,*data["mapping"][0][1:]])
        row=[7,"2026-11-26 12:00:00",99000001,1,7]
        data["history"]=[row,row.copy()]
        plan=module.build_plan(data)
        self.assertEqual([r[10] for r in plan["offers"]],[1,1])
        self.assertEqual(len(plan["history"]),2)
        self.assertEqual(plan["history"][0][4:],(1,6,58,0))
    def test_ambiguous_history_does_not_invent_a_native_item(self):
        data=source();data["mapping"].append([2,99000001,6,59,0])
        data["history"]=[[7,"2026-11-26 12:00:00",99000001,2,7]]
        self.assertEqual(module.build_plan(data)["history"][0][5:],(0,0,0))
    def test_unmapped_history_is_kept_without_an_item_reference(self):
        data=source();data["history"]=[[7,"2026-11-26 12:00:00",637,4,9]]
        self.assertEqual(module.build_plan(data)["history"][0],(7,"2026-11-26 12:00:00",4,9,0,0,0,0))
    def test_package_contents_use_six_typed_members(self):
        data=source();data["packages"]=[[1,"Csomag"]]
        data["package_members"]=[[1,i,6,i,0,i] for i in range(1,7)]
        plan=module.build_plan(data)
        self.assertEqual(len(plan["members"]),6)
        self.assertEqual(plan["offers"][0][10:12],(0,"Csomag"))
    def test_incomplete_or_duplicated_package_is_refused(self):
        data=source();data["packages"]=[[1,"Csomag"]];data["package_members"]=[]
        with self.assertRaises(ValueError):module.build_plan(data)
        data["package_members"]=[[1,i,6,1,0,1] for i in range(1,7)]
        with self.assertRaises(ValueError):module.build_plan(data)
    def test_existing_receipt_never_reads_archived_sources_or_overwrites_native_edits(self):
        class Db:
            def __init__(self):self.calls=[]
            def query(self,q):
                self.calls.append(q)
                if "GET_LOCK" in q:return [["1"]]
                if "FROM player.ishop_offer_import" in q:return [["1","1","2","ab"*32]]
                if "RELEASE_LOCK" in q:return [["1"]]
                raise AssertionError("Unexpected query after completed import: "+q)
        db=Db();result=module.migrate(db,True)
        self.assertTrue(result["already_applied"]);self.assertEqual(result["history"],2)
        self.assertFalse(any("INSERT" in q or "ishop_data" in q for q in db.calls))
    def test_literal_encoding_cannot_change_sql_structure(self):
        text="Csomag'); DROP TABLE player.player; --"
        self.assertNotIn("DROP TABLE",module._literal(text))
        self.assertIn(text.encode().hex(),module._literal(text))
    def test_runtime_and_fresh_schema_have_no_legacy_shop_dependency(self):
        runtime=(ROOT/"SRC/Server/GameServer/item/TypedItemShop.cpp").read_text()
        schema=(ROOT/"db/migrations/item_instance/056_native_item_shop_storage.sql").read_text()
        for name in ("ishop_data","ishop_log","ishop_native_item","legacy_vnum","itemVnum"):
            self.assertNotIn(name,runtime);self.assertNotIn(name,schema)
if __name__=="__main__":unittest.main()
