import importlib.util,json,sys,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location("loot_manifest",ROOT/"SRC/Tools/ItemCatalogBuilder/build_loot_measurement_migration.py")
builder=importlib.util.module_from_spec(spec);spec.loader.exec_module(builder)
class Tests(unittest.TestCase):
    def test_active_alias_and_archived_item(self):
        catalog={"hash":"fixture","columns":{"item_base":[]},"tables":{"item_base":[
            {"category_id":1,"local_id":58,"retired":0,"name_key":"item.sword.name","icon_asset":"icon/sword.tga"},
            {"category_id":1,"local_id":59,"retired":1,"name_key":"item.retired.name","icon_asset":"icon/old.tga"}]}}
        art='  - vnum: 10\n    name: "Sword"\n    iconPath: old/sword.tga\n  - vnum: 20\n    name: Retired reward\n    iconPath: old/reward.tga\n'
        result=builder.build(catalog,"10,1,58\n20,1,59\n",art)
        self.assertEqual(result["entries"][0],dict(prototype=10,category=1,local=58,name="item.sword.name",icon="icon/sword.tga"))
        self.assertEqual(result["entries"][1]["category"],0)
        self.assertEqual(result["entries"][1]["name"],"Retired reward")
        self.assertEqual(result["entries"][1]["icon"],"old/reward.tga")
        with self.assertRaises(ValueError):builder.build(catalog,"10,1,58\n10,1,59\n",art)
        with self.assertRaises(ValueError):builder.build(catalog,"10,1,58\n",art+art)
if __name__=="__main__":unittest.main()
