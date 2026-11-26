import importlib.util,tempfile,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location("prepare",ROOT/"SRC/Tools/prepare_quests.py")
prepare=importlib.util.module_from_spec(spec);spec.loader.exec_module(prepare)
class Tests(unittest.TestCase):
    def test_quest_functions_expose_only_typed_item_operations(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);target=root/"quest_functions"
            target.write_bytes(b"pc.typed_item_count\nitem.get_vnum\nitem.set_socket\npc.give_item2\npc.remove_item\nd.remove_item\nselect_item\nnpc.get_vnum\ndnd.get_vnum\n")
            prepare.retire_equipment_queries(root)
            self.assertEqual(target.read_bytes(),b"pc.typed_item_count\nselect_item\nnpc.get_vnum\ndnd.get_vnum\n")
            self.assertEqual(prepare.retire_equipment_queries(root),[])
if __name__=="__main__":unittest.main()
