"""Native catalogue validation and loss-prevention checks for offline materials."""
import argparse
import copy
from pathlib import Path
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'SRC/Tools/ItemCatalogBuilder'))
import convert_plain_materials as converter
from legacy_item_migration import analyze, NAME_FIELDS, PROTO_FIELDS
from upgrade_exported_catalog import native

HELPER=None


def material(identity=137,subtype=0):
    row={field:0 for field in PROTO_FIELDS}
    row.update({field:'Orkfog' for field in NAME_FIELDS})
    row.update(vnum=identity,type=5,subtype=subtype,size=1,flag=4,gold=700,shop_buy_price=60)
    return row


class Materials(unittest.TestCase):
    def setUp(self):
        self.blank=native(HELPER,dict(action='blank'))
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.project=Path(self.temp.name)
        path=self.project/'Assets/Metin2/Generated/ItemIcons/icon/item/fang.tga'
        path.parent.mkdir(parents=True);path.write_bytes(b'test icon source')
        path.with_suffix('.tga.meta').write_text('fileFormatVersion: 2\nguid: '+'a'*32+'\n')
        self.visual=dict(iconPath='icon/item/fang.tga',icon=dict(file_id='21300000',guid='a'*32))

    def convert(self,rows=None,preserved=None):
        rows=rows if rows is not None else [material()];packet=preserved or self.blank
        tables={'player.item_proto':rows,'protos.item_base':[dict(zip(packet['columns']['item_base'],r)) for r in packet['tables']['item_base']],
                'protos.item_local_id_sequence':[dict(category_id=int(k),next_local_id=int(v)) for k,v in packet['sequences'].items()]}
        plan=analyze(tables,'a'*64)
        return converter.convert(tables,plan,packet,self.project,{r['vnum']:self.visual for r in rows},{})

    def test_actual_fields_compile_and_native_validate(self):
        row=material();row.update(size=3,limittype0=1,limitvalue0=31,antiflag=1|4|128|256|8192)
        result=self.convert([row]);model=converter.Catalog(result['packet']);doc=model.item((6,1))
        self.assertEqual((doc['base']['buy_amount'],doc['base']['sell_amount']),('700','60'))
        self.assertEqual((doc['base']['inventory_height'],doc['base']['max_stack'],doc['base']['min_level']),('3','50000','31'))
        self.assertEqual(doc['base']['icon_asset'],'icon/item/fang.tga')
        self.assertEqual({k:doc['trade'][k] for k in ('allow_trade','allow_drop','allow_sell')},dict(allow_trade='0',allow_drop='0',allow_sell='0'))
        self.assertEqual((doc['use']['allowed_classes'],doc['use']['allowed_sexes']),('14','1'))
        self.assertEqual(len(native(HELPER,dict(action='validate',tables=result['packet']['tables']))['hash']),64)
        self.assertFalse(result['installable_release'])

    def test_each_material_kind_is_explicit(self):
        for subtype in range(5):
            with self.subTest(subtype=subtype):
                result=self.convert([material(subtype=subtype)])
                self.assertEqual(converter.Catalog(result['packet']).item((6,1))['child']['material_kind'],str(subtype+1))
                native(HELPER,dict(action='validate',tables=result['packet']['tables']))

    def test_existing_native_content_profiles_and_issued_ids_are_unchanged(self):
        packet=copy.deepcopy(self.blank);model=converter.Catalog(packet)
        model.apply(model.new(1));packet['tables']=model.encode();packet['sequences']['6']='41'
        before=copy.deepcopy(packet);result=self.convert(preserved=packet)
        self.assertEqual(result['accepted'][0]['item'],dict(category_id=6,local_id=41))
        for table,rows in before['tables'].items():
            for row in rows:self.assertIn(row,result['packet']['tables'][table],table)
        self.assertEqual(packet,before)
        native(HELPER,dict(action='validate',tables=result['packet']['tables']))

    def test_unmapped_state_and_flags_are_never_defaulted(self):
        for field,value in [('weight',1),('flag',1),('antiflag',1<<17),('value0',2),('applytype0',1),('wearflag',1),
                            ('refined_vnum',200),('refine_set',1),('addon_type',-1),('socket_pct',10),('subtype',5),
                            ('limittype1',7),('vnum_range',20),('future_field',0)]:
            with self.subTest(field=field):
                row=material();row[field]=value
                result=self.convert([row]);self.assertEqual(result['accepted'],[])
                self.assertEqual(result['summary']['blocked'],1)

    def test_icon_metadata_mismatch_blocks_instead_of_using_placeholder(self):
        self.visual['icon']['guid']='b'*32
        result=self.convert();self.assertEqual(result['blocked'][0]['reasons'],['asset_guid_mismatch'])

    def test_mixed_source_and_blocked_rows_remain_accounted(self):
        a,b=material(),material(999);b['value0']=1
        result=self.convert([b,a]);self.assertEqual([r['source_id'] for r in result['accepted']],[137])
        self.assertEqual([r['source_id'] for r in result['blocked']],[999])
        self.assertEqual(result['summary']['source_material_rows'],2)

    def test_quantity_contract_distinguishes_nonstackable_and_saved_material(self):
        for flag,stack in ((0,'1'),(2,'1'),(4,'50000'),(6,'50000'),(8196,'50000')):
            row=material();row['flag']=flag
            self.assertEqual(converter.Catalog(self.convert([row])['packet']).item((6,1))['base']['max_stack'],stack)


def main():
    global HELPER
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--helper',required=True,type=Path)
    args,rest=parser.parse_known_args();HELPER=args.helper.resolve(strict=True)
    unittest.main(argv=[sys.argv[0],*rest])


if __name__=='__main__':main()
