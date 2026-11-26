"""Frozen original skill data remain usable after retiring the legacy growth motor."""
from pathlib import Path
from types import SimpleNamespace
import sys
import unittest
from unittest.mock import patch

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'SRC/Tools/ItemCatalogBuilder'))
import full_catalog_pet_books as rules
from legacy_item_migration import MigrationError

class OriginalPetStudyContractTests(unittest.TestCase):
    def test_original_ranks_and_terms_survive_retirement(self):
        compiler=SimpleNamespace(rule_inputs={})
        rows=rules.original_table(compiler)
        self.assertEqual(len(rows),12)
        self.assertEqual([row[1] for row in rows],[78,80,79,81,120,116,117,66,68,64,27,39])
        self.assertEqual([row[2:12] for row in rows],
            [[rank*(2 if n in (7,8) else 1) for rank in range(1,11)] for n in range(12)])
        self.assertEqual(len(compiler.rule_inputs['original_pet_study_contract']),64)
        self.assertFalse((rules.SERVER/'GameServer/pet/New_PetSystem.cpp').exists())

    def test_changed_contract_is_rejected(self):
        original=rules.json.loads
        for key,value in [('known_success_bp',7000),('known_cooldown_ms',0),('max_rank',20),('rows',[])]:
            def changed(text, key=key, value=value):
                data=original(text);data[key]=value;return data
            with self.subTest(key=key),patch.object(rules.json,'loads',changed):
                with self.assertRaises(MigrationError):
                    rules.original_table(SimpleNamespace(rule_inputs={}))

if __name__=='__main__':unittest.main()
