"""Item test bench (ItemBenchTool) against a NEW owned MariaDB, never a live DB.

Content is written with the real CatalogEditorTool commit path; the bench then
grants, moves, refines (success, failure fall-back, missing gold), services,
follows a catalog edit and refuses a catalog that invalidates its items.
"""
import argparse
from pathlib import Path
import sys
import unittest
import uuid

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'SRC/Tools/ServerManager'))
sys.path.insert(0, str(ROOT / 'db/tools'))
sys.path.insert(0, str(Path(__file__).resolve().parent))
import backend as api
import item_bench as bench
import item_catalog as catalog
from backup_sandbox import Sandbox
from mariadb_client import statements

ARGS = None
SANDBOX = None


def setUpModule():
    global SANDBOX
    SANDBOX = Sandbox(ARGS.work_dir / uuid.uuid4().hex, ARGS.mariadb_bin)
    SANDBOX.start(); SANDBOX.verify()
    SANDBOX.client.query('CREATE DATABASE player CHARACTER SET utf8mb4 COLLATE utf8mb4_bin')
    paths = sorted((ROOT / 'db/migrations/item_combat').glob('[0-9][0-9][0-9]_*.sql'))
    if [int(path.name[:3]) for path in paths] != list(range(len(paths))): raise RuntimeError('incomplete catalog migrations')
    for path in [*paths, ROOT / 'db/migrations/item_instance/001_item_persistence.sql']:
        for statement in statements(path.read_text(encoding='utf-8-sig')): SANDBOX.client.query(statement)


def tearDownModule():
    if SANDBOX: SANDBOX.stop()


def auth():
    return dict(port=SANDBOX.port, user='root', password=SANDBOX.password)


def editor(action, **fields):
    return catalog.run_tool(dict(fields, action=action, connection=auth()), fields.get('columns'), ARGS.catalog_tool)


def commit(packet, document, key=None):
    tables = catalog.Catalog(packet).apply(document, key)
    editor('commit', tables=tables, expected_hash=packet['hash'])
    return editor('read')


def use(action, **fields):
    return bench.run_tool(dict(fields, action=action, connection=auth()), ARGS.bench_tool)


def stable(state):
    # Every working request claims a new session, like a login: compare the rest.
    return {k: v for k, v in state.items() if k != 'session'}


def item(state, key):
    return next(i for i in state['items'] if (i['category'], i['local']) == tuple(key) and not i['destroyed'])


class BenchTests(unittest.TestCase):
    def test_bench_flow(self):
        packet = editor('read')
        material = catalog.Catalog(packet).new(6); material['base']['max_stack'] = '200'
        material_key = (6, int(material['base']['local_id'])); packet = commit(packet, material)
        tool = catalog.Catalog(packet).new(5)
        tool.update(catalog.Catalog.attribute_service_defaults()); tool.update(socket_service=None, recovery=None, effect=None, timed=None, toggle=None, cooldown=None)
        tool['attribute_service'].update(maximum_count='5'); tool_key = (5, int(tool['base']['local_id'])); packet = commit(packet, tool)
        sword = catalog.Catalog(packet).new(1)
        sword['refinement']['max_level'] = '3'
        sword['levels'] = [dict(level=str(i), physical_attack_gain_raw=str(i * 1000), magical_attack_gain_raw='0', physical_defense_gain_raw='0',
                                magical_defense_gain_raw='0', extra_bonus_profile_id=None, bonuses=[]) for i in range(4)]
        sword.update(catalog.Catalog.attribute_defaults()); sword['attribute_profile'] = dict(allow_lock='1')
        ingredient = [dict(category_id='6', local_id=str(material_key[1]), refinement_level='0', amount='2')]
        recipe = lambda level, gold, chance, failure, fall: dict(recipe_id=None, code=None, source_level=str(level - 1), target_level=str(level),
                                                                  target_category_id=None, target_local_id=None, gold_cost=str(gold),
                                                                  success_chance_bp=str(chance), failure_kind=failure, failure_level=fall,
                                                                  ingredients=ingredient)
        sword['recipes'] = [recipe(1, 100, 10000, '1', None), recipe(2, 300, 5000, '3', '0'), recipe(3, 500, 0, '1', None)]
        sword_key = (1, int(sword['base']['local_id'])); packet = commit(packet, sword)
        recipes = {int(r['target_level']): int(r['recipe_id']) for r in catalog.Catalog(packet).item(sword_key)['recipes']}

        status = use('status')
        self.assertTrue(status['installed']); self.assertFalse(status['owner'])
        with self.assertRaises(api.EditorError): use('load')
        state = use('setup', gold=1000)['state']
        self.assertEqual((state['owner'], state['wallets'][0]['balance'], state['items']), (bench.BENCH_OWNER, 1000, []))
        state = use('grant', category=6, local=material_key[1], count=9)['state']
        state = use('grant', category=1, local=sword_key[1])['state']
        state = use('grant', category=5, local=tool_key[1], count=3)['state']
        with self.assertRaises(api.EditorError): use('grant', category=6, local=material_key[1], count=201)    # above the stack
        with self.assertRaises(api.EditorError): use('grant', category=1, local=sword_key[1], level=4)       # no such level
        blade = item(state, sword_key)
        self.assertEqual((blade['level'], blade['cell'], item(state, material_key)['cell']), (0, 1, 0))
        state = use('move', item=blade['id'], cell=20)['state']; self.assertEqual(item(state, sword_key)['cell'], 20)
        with self.assertRaises(api.EditorError): use('move', item=blade['id'], cell=0)                       # occupied
        # +0 -> +1 always succeeds; Yang and both materials go in one commit.
        result = use('refine', item=blade['id'], recipe=recipes[1])
        self.assertTrue(result['success']); self.assertEqual(result['roll'], 0)
        state = result['state']; blade = item(state, sword_key)
        self.assertEqual((blade['level'], item(state, material_key)['count'], state['wallets'][0]['balance']), (1, 7, 900))
        # +1 -> +2 at 50%: a forced failing roll falls back to +0.
        result = use('refine', item=blade['id'], recipe=recipes[2], roll=10000)
        self.assertFalse(result['success']); state = result['state']
        self.assertEqual((item(state, sword_key)['level'], item(state, material_key)['count'], state['wallets'][0]['balance']), (0, 5, 600))
        with self.assertRaises(api.EditorError): use('refine', item=blade['id'], recipe=recipes[2])          # wrong source level
        use('refine', item=blade['id'], recipe=recipes[1])
        state = use('refine', item=blade['id'], recipe=recipes[2], roll=1)['state']
        self.assertEqual((item(state, sword_key)['level'], item(state, material_key)['count']), (2, 1))
        before = use('load')['state']
        with self.assertRaises(api.EditorError): use('refine', item=blade['id'], recipe=recipes[3])          # one material left
        self.assertEqual(stable(use('load')['state']), stable(before))
        state = use('grant', category=6, local=material_key[1], count=5)['state']
        state = use('wallet', gold=10)['state']; self.assertEqual(state['wallets'][0]['balance'], 10)
        with self.assertRaises(api.EditorError): use('refine', item=blade['id'], recipe=recipes[3])          # not enough Yang
        use('wallet', gold=5000)
        result = use('refine', item=blade['id'], recipe=recipes[3])                                          # 0%: always fails, keeps +2
        self.assertFalse(result['success']); self.assertEqual(item(result['state'], sword_key)['level'], 2)
        # Attribute service: adds a bonus, consumes one service unit.
        tool_item = item(result['state'], tool_key)
        result = use('attribute', tool=tool_item['id'], target=blade['id'])
        self.assertTrue(result['success'])
        self.assertEqual(len(item(result['state'], sword_key)['attributes']), 1)
        self.assertEqual(item(result['state'], tool_key)['count'], 2)
        # A catalog edit (here: price) is followed after a full reload check.
        changed = catalog.Catalog(packet).item(sword_key); changed['base']['buy_amount'] = '777'
        packet = commit(packet, changed, sword_key)
        result = use('load'); self.assertTrue(result['catalog_followed'])
        self.assertEqual(result['state']['items'], use('load')['state']['items'])
        self.assertFalse(use('load')['catalog_followed'])
        # A catalog that drops the +2 the stored sword has is refused; the pin stays.
        lowered = catalog.Catalog(packet).item(sword_key)
        lowered['refinement']['max_level'] = '1'; lowered['levels'] = lowered['levels'][:2]; lowered['recipes'] = lowered['recipes'][:1]
        packet = commit(packet, lowered, sword_key)
        with self.assertRaises(api.EditorError) as refused: use('load')
        self.assertIn('Állítsd vissza', str(refused.exception))
        self.assertFalse(use('status')['pin_matches'])
        # Destroy, then a reset starts over on the current catalog.
        state = use('setup', reset=True, gold=50)['state']
        self.assertEqual((state['items'], state['wallets'][0]['balance']), ([], 50))
        state = use('grant', category=6, local=material_key[1], count=2)['state']
        state = use('destroy', item=item(state, material_key)['id'])['state']
        self.assertTrue(all(i['destroyed'] for i in state['items']))
        self.assertTrue(use('status')['pin_matches'])

        # Real characters (D1c): listed, provisioned on request, claimed per action.
        self.assertEqual(use('characters')['characters'], [])                   # no player table: empty, not an error
        SANDBOX.client.query('CREATE TABLE player.player(id INT UNSIGNED PRIMARY KEY, name VARCHAR(24) NOT NULL)')
        SANDBOX.client.query("INSERT INTO player.player VALUES(5,'Teszt'),(6,'Masik')")
        self.assertEqual([(c['id'], c['name'], c['provisioned']) for c in use('characters')['characters']], [(5, 'Teszt', False), (6, 'Masik', False)])
        with self.assertRaises(api.EditorError): use('load', owner=5)          # not provisioned yet
        with self.assertRaises(api.EditorError): use('load', owner=0)
        first = use('setup', owner=5, gold=70)['state']
        self.assertEqual((first['owner'], first['wallets'][0]['balance'], first['items']), (5, 70, []))
        sessions = [use('load', owner=5)['state']['session'] for _ in range(2)]
        self.assertEqual(sessions[1], sessions[0] + 1)
        hero = use('grant', owner=5, category=1, local=sword_key[1], level=1)['state']
        self.assertEqual([(i['level'], i['category']) for i in hero['items']], [(1, 1)])
        self.assertEqual([c['provisioned'] for c in use('characters')['characters']], [True, False])
        self.assertEqual(use('load')['state']['items'], [])                    # the test character is separate
        # Every owner is checked before the catalog pin moves.
        changed = catalog.Catalog(packet).item(sword_key); changed['base']['buy_amount'] = '778'
        packet = commit(packet, changed, sword_key)
        self.assertTrue(use('load', owner=5)['catalog_followed'])
        lowered = catalog.Catalog(packet).item(sword_key)
        lowered['refinement']['max_level'] = '0'; lowered['levels'] = lowered['levels'][:1]; lowered['recipes'] = []
        packet = commit(packet, lowered, sword_key)
        with self.assertRaises(api.EditorError) as refused: use('load')        # the TEST character is fine, #5 is not
        self.assertIn('#5', str(refused.exception))
        self.assertFalse(use('status')['pin_matches'])
        reset = use('setup', owner=5, reset=True, gold=1)['state']
        self.assertEqual((reset['items'], reset['wallets'][0]['balance']), ([], 1))
        self.assertTrue(use('status')['pin_matches'])


def main():
    global ARGS
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mariadb-bin', type=Path, required=True)
    parser.add_argument('--bench-tool', type=Path, required=True)
    parser.add_argument('--catalog-tool', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    ARGS, rest = parser.parse_known_args()
    unittest.main(argv=[sys.argv[0], *rest], verbosity=2)


if __name__ == '__main__':
    main()
