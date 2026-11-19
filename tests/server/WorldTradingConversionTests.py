from pathlib import Path
import json
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'SRC/Tools/ItemCatalogBuilder'))
from convert_world_trading import convert_shops,quest_shop_routes,convert_cubes


class WorldTradingConversion(unittest.TestCase):
    @classmethod
    def setUpClass(cls):cls.catalog=json.loads((ROOT/'config/item_combat/full_proto_converted_catalog.json').read_text(encoding='utf-8'))

    def row(self,shop=1,item=19):
        row=dict(shop_vnum=str(shop),item_vnum=str(item),count='1',gold='7')
        for i in range(1,6):row['itemvnum'+str(i)]='0';row['itemcount'+str(i)]='0'
        return row

    def convert(self,headers,rows,routes=None):
        with patch('convert_world_trading.quest_shop_routes',return_value=routes or {}):
            return convert_shops(self.catalog,dict(shop=headers,shop_item=rows),ROOT)

    def test_original_price_and_magic_policy(self):
        row=self.row();document,audit=self.convert([dict(vnum='1',npc_vnum='9001')],[row])
        offer=document['shops'][0]['offers'][0]
        self.assertEqual(offer['gold'],7)
        self.assertEqual(offer['level'],9)
        self.assertTrue(offer['try_magic'])
        self.assertNotIn('item_vnum',offer)
        self.assertEqual(audit['active_source_offers'],1)

    def test_missing_currency_never_makes_a_free_reward(self):
        row=self.row();row['itemvnum1']=str(self.catalog['blocked'][0]['source_id']);row['itemcount1']='3'
        document,audit=self.convert([dict(vnum='1',npc_vnum='9001')],[row])
        self.assertEqual(document['shops'],[])
        self.assertEqual(len(audit['excluded']),1)
        self.assertTrue(audit['excluded'][0]['reason'].startswith('merge_deferred:'))

    def test_duplicate_currency_lanes_are_summed(self):
        row=self.row();row.update(itemvnum1='10',itemcount1='3',itemvnum2='10',itemcount2='5')
        document,_=self.convert([dict(vnum='1',npc_vnum='9001')],[row])
        costs=document['shops'][0]['offers'][0]['costs']
        self.assertEqual(len(costs),1);self.assertEqual(costs[0]['count'],8);self.assertEqual(costs[0]['level'],0)

    def test_free_price_is_explicit_and_zero_identity_slots_are_ignored(self):
        row=self.row();row.update(gold='0',itemcount1='999')
        document,audit=self.convert([dict(vnum='1',npc_vnum='9001')],[row])
        offer=document['shops'][0]['offers'][0]
        self.assertEqual(offer['gold'],0);self.assertEqual(offer['costs'],[]);self.assertEqual(audit['free_offers'],1)

    def test_orphan_and_unbound_stock_do_not_leak_to_other_npcs(self):
        document,audit=self.convert([dict(vnum='1',npc_vnum='0')],[self.row(1),self.row(99)])
        self.assertEqual(document['shops'],[]);self.assertEqual(document['routes'],[])
        self.assertEqual(audit['unbound_shops'],[1]);self.assertEqual(len(audit['orphan_rows']),1)

    def test_quest_only_stock_requires_its_authored_route(self):
        document,_=self.convert([dict(vnum='71',npc_vnum='0')],[self.row(71)],{71:{9293}})
        self.assertEqual(document['shops'],[])
        self.assertEqual(document['routes'][0]['npc_race'],9293);self.assertEqual(document['routes'][0]['source_id'],71)

    def test_multiple_original_lists_share_npc_but_keep_explicit_routes(self):
        headers=[dict(vnum='1',npc_vnum='9001'),dict(vnum='2',npc_vnum='9001')]
        document,_=self.convert(headers,[self.row(1),self.row(2,3009)])
        self.assertEqual(len(document['shops']),1);self.assertEqual(len(document['shops'][0]['offers']),2)
        self.assertEqual({row['source_id'] for row in document['routes']},{1,2})

    def test_cubes_preserve_original_gaya_copy_and_deferred_boundary(self):
        cubes=json.loads((ROOT/'config/item_combat/original_cube.source.json').read_text(encoding='utf-8'));prototypes=json.loads((ROOT/'config/item_combat/original_cube_prototypes.source.json').read_text(encoding='utf-8'))['tables']['player.item_proto']
        document,audit=convert_cubes(self.catalog,cubes,prototypes)
        self.assertEqual((audit['converted_source_recipes'],audit['gaya_recipes'],audit['copy_recipes'],audit['workshops']),(173,70,60,13))
        self.assertEqual(len(audit['excluded']),7);self.assertTrue(all(r['reason'].startswith('merge_deferred:') for r in audit['excluded']))
        recipes={r['id']:r for w in document['workshops'] for r in w['recipes']}
        for ordinal,row in enumerate(cubes['cubes'],1):
            if ordinal not in recipes:continue
            recipe=recipes[ordinal];self.assertEqual((recipe['gold'],recipe['gaya'],recipe['chance_bp']),(row['gold'],row['gaya'],row['percent']*100))
            self.assertNotIn('vnum',str(recipe));self.assertFalse(any(ord(c)<32 for c in recipe['name']))
        self.assertIsNone(recipes[80]['copy_source']);self.assertIsNotNone(recipes[99]['copy_source'])
        self.assertEqual(document,json.loads((ROOT/'config/typed_crafting.generated.json').read_text(encoding='utf-8')))

    def test_mixed_original_encodings_and_disabled_calls(self):
        with tempfile.TemporaryDirectory(prefix='world-trading-',dir=ROOT/'build') as directory:
            root=Path(directory).resolve();self.assertTrue(root.is_relative_to((ROOT/'build').resolve()))
            (root/'event.lua').write_bytes(b'-- when 20031.chat."old" begin npc.open_shop(71) end\nwhen 9293.chat."\xe1rus" begin npc.open_shop(71) end')
            self.assertEqual(quest_shop_routes(root),{71:{9293}})


if __name__=='__main__':unittest.main()
