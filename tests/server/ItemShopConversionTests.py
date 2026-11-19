"""C45: one-time legacy item shop -> native item conversion (offline)."""
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'SRC/Tools/ItemCatalogBuilder'))
import convert_item_shop as tool

CATALOG = [
    dict(category_id=6, local_id=564, content_code='material.sarkany_pikkely', retired=0),
    dict(category_id=5, local_id=40, content_code='imported.sun_elixir_large', retired=0),
    dict(category_id=7, local_id=3, content_code='socket_stone.sietseg_kove', retired=0),
    dict(category_id=6, local_id=9, content_code='material.old', retired=1),
    dict(category_id=6, local_id=10, content_code='material.other', retired=0),
]


class ItemShopConversionTests(unittest.TestCase):
    def test_sources_and_unconverted_rows(self):
        shop = [dict(id=4, itemVnum=71123), dict(id=2, itemVnum=72725), dict(id=3, itemVnum=28443),
                dict(id=1, itemVnum=14590), dict(id=5, itemVnum=50001)]
        proposal = dict(mapping=[
            dict(source_id=71123, content_code='material.sarkany_pikkely', refinement_level=0, status='candidate'),
            dict(source_id=28443, content_code='socket_stone.sietseg_kove_a57c', refinement_level=4, status='candidate'),
            dict(source_id=14590, content_code='companion.sotet_sarkany', refinement_level=0, status='candidate'),
            dict(source_id=50001, content_code='material.old', refinement_level=0, status='candidate'),
        ])
        review = dict(items={'72725': dict(category=5, code='sun_elixir_large')})
        archive = [dict(vnum=28443, category_id=7, local_id=3, refinement_level=4)]
        result = tool.resolve(shop, CATALOG, proposal, review, archive)
        rows = {r['shop_id']: r for r in result['rows']}
        self.assertEqual(sorted(rows), [2, 3, 4])
        self.assertEqual((rows[4]['category_id'], rows[4]['local_id'], rows[4]['source']), (6, 564, 'mapping'))
        self.assertEqual((rows[2]['category_id'], rows[2]['local_id'], rows[2]['source']), (5, 40, 'review'))
        # The proposal code of 28443 is not installed; the actual import decides it.
        self.assertEqual((rows[3]['category_id'], rows[3]['refinement_level'], rows[3]['source']), (7, 4, 'archive'))
        reasons = {u['shop_id']: u['reason'] for u in result['unconverted']}
        self.assertEqual(reasons, {1: 'no_native_item', 5: 'no_native_item'})  # retired never resolves
        self.assertEqual(result['summary']['converted'], 3)

    def test_conflicting_evidence_stays_unconverted(self):
        shop = [dict(id=7, itemVnum=71123)]
        proposal = dict(mapping=[dict(source_id=71123, content_code='material.sarkany_pikkely', refinement_level=0, status='candidate')])
        archive = [dict(vnum=71123, category_id=6, local_id=10, refinement_level=0)]
        result = tool.resolve(shop, CATALOG, proposal, None, archive)
        self.assertEqual(result['rows'], [])
        self.assertEqual(result['unconverted'][0]['reason'], 'conflict')
        self.assertEqual(result['summary']['conflicts'], 1)

    def test_compiled_issue_list_is_evidence(self):
        catalog = CATALOG + [dict(category_id=15, local_id=539, content_code='companion.sotet_sarkany', retired=0)]
        compiled = [[dict(source_id=14590, item=dict(category_id=15, local_id=539), refinement_level=0)]]
        result = tool.resolve([dict(id=1, itemVnum=14590)], catalog, None, None, None, compiled)
        self.assertEqual((result['rows'][0]['category_id'], result['rows'][0]['source']), (15, 'compiled'))
        self.assertEqual(result['summary']['by_source']['compiled'], 1)

    def test_shop_preset_state_is_not_sold(self):
        proposal = dict(mapping=[dict(source_id=71123, content_code='material.sarkany_pikkely', refinement_level=0, status='candidate')])
        shop = [dict(id=1, itemVnum=71123, socket0=0, attrtype0=0), dict(id=2, itemVnum=71123, socket0=604800, attrtype0=0),
                dict(id=3, itemVnum=71123, socket0=0, attrtype3=63)]
        result = tool.resolve(shop, CATALOG, proposal)
        self.assertEqual([r['shop_id'] for r in result['rows']], [1])
        self.assertEqual({u['shop_id']: u['reason'] for u in result['unconverted']}, {2: 'shop_preset_state', 3: 'shop_preset_state'})

    def test_quarantined_proposals_and_sql_are_deterministic(self):
        shop = [dict(id=9, itemVnum=71123), dict(id=8, itemVnum=71123)]
        proposal = dict(mapping=[dict(source_id=71123, content_code='material.sarkany_pikkely', refinement_level=0, status='quarantine')])
        self.assertEqual(tool.resolve(shop, CATALOG, proposal)['rows'], [])
        proposal['mapping'][0]['status'] = 'candidate'
        first = tool.to_sql(tool.resolve(shop, CATALOG, proposal))
        self.assertEqual(first, tool.to_sql(tool.resolve(list(reversed(shop)), CATALOG, proposal)))
        self.assertIn("(8,71123,6,564,0,'mapping'),\n(9,71123,6,564,0,'mapping');", first)
        self.assertTrue(first.rstrip().endswith('COMMIT;'))


if __name__ == '__main__':
    unittest.main()
