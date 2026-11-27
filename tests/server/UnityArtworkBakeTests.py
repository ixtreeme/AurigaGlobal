import importlib.util,tempfile,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location("artwork",ROOT/"SRC/Tools/ItemCatalogBuilder/bake_unity_artwork.py")
tool=importlib.util.module_from_spec(spec);spec.loader.exec_module(tool)
class Tests(unittest.TestCase):
    def test_portrait_is_baked_without_touching_custom_art_or_models(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);folder=root/"Metin2/ItemIcons/Tera";folder.mkdir(parents=True)
            for n in [630000,630001]:
                image=folder/(str(n)+".png");image.write_bytes(b"fixture")
                image.with_name(image.name+".meta").write_text("guid: "+"1"*32+"\n")
            first="  - vnum: 630000\n    iconPath: fallback\n    icon: {fileID: 21300000, guid: a0c0e517befe5f8469248e71afaf6291, type: 3}\n    modelPath: item/known.gr2\n    model: {fileID: 77}\n"
            custom="  - vnum: 630001\n    iconPath: custom\n    icon: {fileID: 21300000, guid: "+"2"*32+", type: 3}\n    modelPath: item/custom.gr2\n    model: {fileID: 88}\n"
            result,changes=tool.bake(first+custom,root)
            self.assertEqual(changes,[630000]);self.assertIn(custom,result)
            self.assertIn("model: {fileID: 77}",result);self.assertIn("modelPath: item/known.gr2",result)
            self.assertIn("iconPath: Metin2/ItemIcons/Tera/630000.png",result)
            self.assertEqual(tool.bake(result,root),(result,[]))
if __name__=="__main__":unittest.main()
