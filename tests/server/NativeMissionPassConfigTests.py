import copy,importlib.util,json,unittest
from pathlib import Path
root=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location("native_pass",root/"SRC/Tools/ItemCatalogBuilder/publish_native_mission_pass.py")
tool=importlib.util.module_from_spec(spec);spec.loader.exec_module(tool)
class ConfigTests(unittest.TestCase):
 def setUp(self):self.doc=json.loads((root/"config/mission_pass.json.example").read_text())
 def test_sample(self):
  p=tool.plan(self.doc);self.assertEqual(len(p["rewards"]),80);self.assertEqual(p["season"][-1],0)
 def test_enable_flag_is_operational(self):
  other=copy.deepcopy(self.doc);other["enabled"]=True
  self.assertEqual(tool.plan(self.doc)["source_hash"],tool.plan(other)["source_hash"])
 def test_raw_rewards_rejected(self):
  r=self.doc["rewards"][0];r["item"]=90011
  with self.assertRaises(ValueError):tool.plan(self.doc)
 def test_old_schema_rejected(self):
  self.doc["schema_version"]=1
  with self.assertRaises(ValueError):tool.plan(self.doc)
 def test_duplicate_slot(self):
  self.doc["rewards"].append(copy.deepcopy(self.doc["rewards"][0]))
  with self.assertRaises(ValueError):tool.plan(self.doc)
 def test_invalid_native_key(self):
  for field,value in (("category",0),("category",65536),("local",0),("refinement",256),("count",201),("count",True)):
   with self.subTest(field=field,value=value):
    d=copy.deepcopy(self.doc);d["rewards"][0][field]=value
    with self.assertRaises(ValueError):tool.plan(d)
 def test_source_hash_covers_objectives(self):
  other=copy.deepcopy(self.doc);other["missions"][0]["goal"]+=1
  self.assertNotEqual(tool.plan(self.doc)["source_hash"],tool.plan(other)["source_hash"])
if __name__=="__main__":unittest.main()
