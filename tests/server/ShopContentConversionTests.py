"""C46: offline costume / mount / stole conversion rules (convert_shop_content.py)."""
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'SRC/Tools/ItemCatalogBuilder'))
import convert_shop_content as tool
from legacy_item_migration import MigrationError, NAME_FIELDS


def proto(**fields):
    row = dict(vnum=71125, type=28, subtype=2, size=1, flag=0, antiflag=0, wearflag=0, weight=0, vnum_range=0, immuneflag=0,
               refined_vnum=0, refine_set=0, magic_pct=0, addon_type=0, socket_pct=0, gold=0, shop_buy_price=0,
               limittype0=0, limitvalue0=0, limittype1=0, limitvalue1=0, **{f'value{i}': 0 for i in range(6)},
               **{f'applytype{i}': 0 for i in range(5)}, **{f'applyvalue{i}': 0 for i in range(5)})
    row.update({name: 'Harci Vadkan' for name in NAME_FIELDS})
    row.update(value1=20115)
    row.update(fields)
    return row


class ShopContentConversionTests(unittest.TestCase):
    def test_stole_table_follows_build_flags(self):
        table, inputs = tool.stole_table(ROOT / 'SRC/Server')
        self.assertEqual(len(table), 6)
        self.assertEqual(table[0][0], 63)                    # APPLY_ATTBONUS_MONSTER
        self.assertEqual(table[1][0], 116)                   # APPLY_ATTBONUS_METIN (ENABLE_STRONG_METIN in CommonDefines.h)
        self.assertEqual(table[0][1][12:16], [16, 16, 18, 20])  # grade 4 variants
        rules = tool.stole_rules(table, 4)
        self.assertEqual(len(rules), 24)
        self.assertEqual({r['variant_no'] for r in rules}, {'0', '1', '2', '3'})
        self.assertTrue(inputs['stole_length'] and inputs['service'])

    def test_limits_map_to_native_clocks(self):
        self.assertEqual(tool.clock(proto()), (0, 1, 0))
        self.assertEqual(tool.clock(proto(limittype0=10, limitvalue0=1)), (0, 1, 0))
        self.assertEqual(tool.clock(proto(limittype0=1, limitvalue0=30, limittype1=7, limitvalue1=86400)), (30, 2, 86400000))
        self.assertEqual(tool.clock(proto(limittype0=8, limitvalue0=60)), (0, 3, 60000))
        self.assertEqual(tool.clock(proto(limittype0=9, limitvalue0=60)), (0, 4, 60000))
        for bad in (dict(limittype0=7, limitvalue0=0), dict(limittype0=6, limitvalue0=1), dict(limittype0=7, limitvalue0=5, limittype1=9, limitvalue1=5)):
            with self.assertRaises(MigrationError):
                tool.clock(proto(**bad))

    def test_policy_admits_only_inert_fields(self):
        tool.family_policy(proto(magic_pct=100, antiflag=1 << 14, value5=1), 'mount', set())
        self.assertEqual(len(tool.satisfied(proto(magic_pct=100, antiflag=1 << 14), 'mount')), 2)
        body = proto(subtype=0, value1=0, value3=41728, magic_pct=100)
        with self.assertRaisesRegex(MigrationError, 'birth_attribute_policy_not_native'):
            tool.family_policy(body, 'costume_body', set())
        for fields, reason in ((dict(refine_set=3000), 'refinement_chain'), (dict(antiflag=1 << 16), 'unsupported_anti_flags'),
                               (dict(value3=7), 'unmapped_family_value'), (dict(value5=2), 'invalid_mount_race_or_flight'),
                               (dict(wearflag=128), 'unmapped_wearflag')):
            with self.assertRaisesRegex(MigrationError, reason):
                tool.family_policy(proto(**fields), 'mount', set())
        with self.assertRaisesRegex(MigrationError, 'extra_proto'):
            tool.family_policy(proto(), 'mount', {71125})


if __name__ == '__main__':
    unittest.main()
