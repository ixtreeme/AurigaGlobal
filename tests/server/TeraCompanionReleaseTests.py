"""Validate summon/prototype cross-links before publication to an existing live database."""
import csv
import json
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[2]
RELEASE = ROOT / 'integration/tera_companions'

class TeraCompanionReleaseTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.rows = json.loads((RELEASE/'release.json').read_text(encoding='utf-8'))['entries']
        with (RELEASE/'item_proto.csv').open(encoding='utf-8-sig') as source:
            cls.items = {int(x['vnum']): x for x in csv.DictReader(source)}
        with (RELEASE/'mob_proto.csv').open(encoding='utf-8-sig') as source:
            cls.mobs = {int(x['vnum']): x for x in csv.DictReader(source)}
        with (RELEASE/'mob_motion.csv').open(encoding='utf-8-sig') as source:
            cls.motions = {x['folder']: x for x in csv.DictReader(source)}

    def test_coverage_and_packet_width(self):
        self.assertEqual(len(self.rows), 272)
        self.assertEqual(sum(r['kind']=='mount' for r in self.rows), 203)
        self.assertEqual(sum(r['kind']=='pet' for r in self.rows), 69)
        for field in ('key','item','mob'):
            self.assertEqual(len({r[field] for r in self.rows}), len(self.rows))
        for row in self.rows:
            self.assertTrue(0 < row['mob'] <= 65535)
            self.assertGreater(row['scale'], 0)

    def test_permanent_summons_have_no_accidental_template_bonuses(self):
        for row in self.rows:
            item=self.items[row['item']]; mob=self.mobs[row['mob']]
            self.assertEqual(int(item['value1']), row['mob'])
            self.assertEqual((int(item['type']),int(item['subtype'])), (28,2) if row['kind']=='mount' else (18,0))
            for key,value in item.items():
                if key.startswith(('limittype','limitvalue','applytype','applyvalue')):
                    expected=10 if row['kind']=='mount' and key=='limittype0' else 1 if row['kind']=='mount' and key=='limitvalue0' else 0
                    self.assertEqual(int(value),expected,(row['item'],key))
            for key in ('gold_min','gold_max','exp','drop_item','damage_min','damage_max'):
                self.assertEqual(int(mob[key]),0)
            motion=self.motions[mob['folder']]
            self.assertGreater(int(motion['walk_speed']),0)
            self.assertGreater(int(motion['run_speed']),0)
            self.assertGreaterEqual(int(motion['attack_duration']),0)

    def test_pet_handlers_and_mount_inventory_are_disjoint(self):
        quest=(RELEASE/'tera_companions.lua').read_text()
        handlers={int(x) for x in re.findall(r'(\d+)\.use',quest)}
        self.assertEqual(handlers,{r['item'] for r in self.rows if r['kind']=='pet'})
        header=(ROOT/'SRC/Server/GameServer/pet/mount_inventory_helper.h').read_text()
        for row in self.rows:
            if row['kind']=='mount': self.assertRegex(header,r'\b'+str(row['item'])+r'\b')
        self.assertNotIn('spawn_effect',quest)

    def test_every_supported_locale_and_source_motion_survives(self):
        languages={'cz','de','en','es','hu','it','pl','pt','ro','tr'}
        for row in self.rows:
            self.assertEqual(set(row['names']),languages)
            self.assertEqual(set(row['itemNames']),languages)
            self.assertEqual(set(row['descriptions']),languages)
            self.assertTrue(row['clips'])
            self.assertEqual(len(row['clips']),len(row['clipResources']))
            for sound in row['sounds']:
                self.assertTrue(sound['files'])
                self.assertFalse(re.search(r'spawn|summon|recall|extinction',sound['state']+' '+sound['cue'],re.I))

if __name__ == '__main__': unittest.main()
