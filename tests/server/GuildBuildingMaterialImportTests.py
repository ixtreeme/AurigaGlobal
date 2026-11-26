import importlib.util,unittest
from pathlib import Path
spec=importlib.util.spec_from_file_location("importer",Path(__file__).resolve().parents[2]/"SRC/Tools/ItemCatalogBuilder/migrate_guild_building_materials.py")
tool=importlib.util.module_from_spec(spec);spec.loader.exec_module(tool)
class ImportTests(unittest.TestCase):
 def test_native_costs_and_zero_recipe(self):
  p=tool.build_plan([["14013","90010,20/90011,30/90012,20"],["14100","0"]])
  self.assertEqual(p["recipes"][0][1],[dict(category=6,local=755,count=20),dict(category=6,local=756,count=30),dict(category=6,local=757,count=20)])
  self.assertEqual(p["recipes"][1][1],[])
 def test_duplicate_materials_preserve_total(self):
  self.assertEqual(tool.build_plan([["1","90010,2/90010,3"]])["recipes"][0][1][0]["count"],5)
 def test_unknown_material_is_not_substituted(self):
  with self.assertRaises(ValueError):tool.build_plan([["1","2871,1"]])
 def test_invalid_count(self):
  for raw in ("90010,-1","90010,0","90010,4294967296","90010,1/90010,4294967295"):
   with self.subTest(raw=raw),self.assertRaises(ValueError):tool.build_plan([["1",raw]])
 def test_invalid_source(self):
  for rows in ([["1","90010"]],[["1","90010,1,2"]],[["1",""],["1",""]]):
   with self.subTest(rows=rows),self.assertRaises(ValueError):tool.build_plan(rows)
 def test_hash_tracks_cost(self):
  self.assertNotEqual(tool.build_plan([["1","90010,1"]])["source_hash"],tool.build_plan([["1","90010,2"]])["source_hash"])
if __name__=="__main__":unittest.main()
