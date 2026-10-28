"""Real native catalog + offline source/asset conversion regression tests."""
import argparse
import copy
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'SRC/Tools/ItemCatalogBuilder'))
import convert_quest_tokens as converter
from legacy_item_migration import analyze, MigrationError, NAME_FIELDS, PROTO_FIELDS
from upgrade_exported_catalog import native

HELPER = None
GUID = 'a' * 32


def token(identity=30135):
    result = {field: 0 for field in PROTO_FIELDS}
    result.update({field: 'Soul Orb' for field in NAME_FIELDS})
    result.update(vnum=identity, type=0, size=1, flag=4, locale_name_hu='Lélekgömb')
    return result


class ConversionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls): cls.blank = native(HELPER, dict(action='blank'))

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.project = Path(self.temp.name)
        for path in ('Assets/Metin2/Generated/ItemIcons/icon/item/orb.tga',
                     'Assets/Metin2/Converted/item/etc/item_bag.fbx'):
            asset = self.project / path
            asset.parent.mkdir(parents=True, exist_ok=True)
            asset.write_bytes(b'owned test asset')
            asset.with_name(asset.name + '.meta').write_text('fileFormatVersion: 2\nguid: ' + GUID + '\n')
        self.visual = dict(iconPath='icon/item/orb.tga', modelPath='item/etc/item_bag.gr2',
                           icon=dict(file_id='21300000', guid=GUID), model=dict(file_id='919', guid=GUID))

    def convert(self, rows=None, preserved=None, visuals=None, extras=()):
        packet = copy.deepcopy(preserved or self.blank)
        existing = converter.Catalog(packet)
        tables = {'player.item_proto': rows or [token()], 'player.item': [],
                  'player.item_extra_proto': list(extras),
                  'protos.item_base': existing.tables['item_base'],
                  'protos.item_local_id_sequence': [dict(category_id=int(k), next_local_id=int(v)) for k,v in packet['sequences'].items()]}
        plan = analyze(tables, 'a'*64)
        return converter.convert(tables, plan, packet, self.project,
                                 visuals if visuals is not None else {row['vnum']: self.visual for row in tables['player.item_proto']}, {})

    def test_real_source_semantics_native_round_trip(self):
        row = token(); row.update(antiflag=(1 << 0) | (1 << 2) | (1 << 7), limittype0=1, limitvalue0=42, size=3)
        result = self.convert([row]); model = converter.Catalog(result['packet'])
        self.assertEqual(result['summary']['converted'], 1)
        base = model.items()[0]
        self.assertEqual((base['max_stack'], base['min_level'], base['inventory_height']), ('50000','42','3'))
        document = model.item((10,1))
        self.assertEqual(document['use']['allowed_classes'], '14')
        self.assertEqual(document['use']['allowed_sexes'], '1')
        self.assertEqual(document['trade']['allow_drop'], '0')
        self.assertEqual(document['child']['dispatch_kind'], '1')
        self.assertEqual(document['refinement']['max_level'], '0')
        self.assertEqual(result['localization']['quest.lelekgomb']['texts']['hu']['name'], 'Lélekgömb')
        self.assertEqual(len(native(HELPER, dict(action='validate', tables=result['packet']['tables']))['hash']),64)
        self.assertFalse(result['installable_release'])
        self.assertNotIn('vnum', result['packet']['columns']['item_base'])

    def test_issued_item_profiles_and_category_gaps_preserved(self):
        packet = copy.deepcopy(self.blank); model = converter.Catalog(packet)
        weapon = model.new(1); model.apply(weapon)
        packet['tables'] = model.encode(); packet['sequences']['1'] = '8'; packet['sequences']['10'] = '90'
        before = copy.deepcopy(packet)
        result = self.convert(preserved=packet)
        new = converter.Catalog(result['packet'])
        self.assertEqual(next(r for r in new.items() if r['category_id']=='1'), model.items()[0])
        self.assertEqual(result['accepted'][0]['item']['local_id'],90)
        self.assertEqual(result['packet']['sequences']['10'],'91')
        self.assertEqual(result['packet']['sequences']['1'],'8')
        self.assertEqual(packet,before)
        native(HELPER, dict(action='validate', tables=result['packet']['tables']))

    def test_nonstackable_remains_one(self):
        row=token();row['flag']=0
        self.assertEqual(converter.Catalog(self.convert([row])['packet']).items()[0]['max_stack'],'1')

    def test_unrepresented_behavior_never_disappears(self):
        for name,value in [('value0',1),('value5',50),('gold',1),('shop_buy_price',10),('wearflag',128),
                           ('flag',8196),('antiflag',1<<13),('socket_pct',1),('refined_vnum',123),
                           ('subtype',1),('applytype0',17),('limittype0',7),('limitvalue1',30),('future_column',1)]:
            with self.subTest(name=name):
                row=token();row[name]=value
                result=self.convert([row]);self.assertEqual(result['accepted'],[])
                self.assertEqual(result['blocked'][0]['source_id'],30135)

    def test_script_item_is_not_silently_made_passive(self):
        row=token();row['type']=18
        result=self.convert([row]);self.assertEqual(result['accepted'],[])
        self.assertIn('script_handler_conversion_required',result['blocked'][0]['reasons'])

    def test_extra_prototype_appearance_and_bonus_need_conversion(self):
        result=self.convert(extras=[dict(vnum=30135)])
        self.assertIn('extra_proto_conversion_required',result['blocked'][0]['reasons'])

    def test_asset_guids_and_paths_must_resolve(self):
        for mutate in (lambda v: v['icon'].update(guid='b'*32),
                       lambda v: v.update(iconPath='icon/item/../../secret.tga'),
                       lambda v: v.update(modelPath='item/etc/missing.gr2'),
                       lambda v: v['icon'].update(file_id='0')):
            visual=copy.deepcopy(self.visual);mutate(visual)
            result=self.convert(visuals={30135:visual})
            self.assertEqual(result['accepted'],[])
            self.assertEqual(result['summary']['blocked'],1)

    def test_all_gender_or_class_exclusion_is_blocked(self):
        for anti in (3,0x3c):
            row=token();row['antiflag']=anti
            self.assertEqual(self.convert([row])['accepted'],[])

    def test_input_order_is_deterministic(self):
        a,b=token(30135),token(30136);b.update(locale_name_hu='Másik tárgy')
        self.assertEqual(self.convert([a,b]),self.convert([b,a]))

    def test_source_names_override_damaged_unity_names(self):
        locales={'hu':{30135:dict(name='L\ufffdlek',description='pontos leírás',summary='leírás')},
                 'fr':{30135:dict(name='ab\ufffd',description='',summary='')}}
        result=converter.translation_for(token(),'quest.soul_orb',locales)
        self.assertEqual(result['texts']['hu']['name'],'Lélekgömb')
        self.assertEqual(result['texts']['hu']['description'],'pontos leírás')
        self.assertEqual(result['review_issues'],[dict(language='fr',field='name',reason='replacement_character')])

    def test_narrow_unity_reader_rejects_duplicates_and_keeps_zero_references(self):
        file=self.project/'catalog.asset'
        file.write_text('  entries:\n  - vnum: 2\n    iconPath: icon/item/orb.tga\n    modelPath: item/etc/item_bag.gr2\n    icon: {fileID: 0}\n')
        self.assertEqual(converter.read_visuals(file)[2]['icon']['file_id'],'0')
        with file.open('a') as stream:stream.write('  - vnum: 2\n')
        with self.assertRaises(MigrationError):converter.read_visuals(file)


def main():
    global HELPER
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--helper',required=True,type=Path)
    args,rest=parser.parse_known_args();HELPER=args.helper.resolve(strict=True)
    unittest.main(argv=[sys.argv[0],*rest],verbosity=2)


if __name__=='__main__':main()
