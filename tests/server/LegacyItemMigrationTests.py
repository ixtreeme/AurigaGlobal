"""Offline migration safety/accounting regressions; no existing DB endpoint."""
import copy
import json
from pathlib import Path
import random
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'SRC/Tools/ItemCatalogBuilder'))
import legacy_item_migration as migration
import upgrade_exported_catalog as upgrade


def proto(old_id, level=0, next_id=0, name='Iron Sword', kind=1, subtype=0):
    row = {field: 0 for field in migration.PROTO_FIELDS}
    row.update({field: name + '+' + str(level) for field in migration.NAME_FIELDS})
    row.update(vnum=old_id, type=kind, subtype=subtype, refined_vnum=next_id,
               refine_set=level + 1 if next_id else 0, size=1)
    return row


def recipe(recipe_id=1, material=200, count=3):
    row = dict(id=recipe_id, cost=500, prob=75)
    for i in range(5): row.update({'vnum'+str(i): 0, 'count'+str(i): 0})
    row.update(vnum0=material, count0=count)
    return row


def source():
    return {
        'player.item_proto': [proto(10, 0, 87), proto(87, 1, 965), proto(965, 2), proto(200, name='Silk', kind=5)],
        'player.refine_proto': [recipe(1), recipe(2)],
        'player.item': [dict(id=500, owner_id=7, window='INVENTORY', pos=0, count=1, vnum=87,
                             socket0=28030, socket1=0, socket2=1, attrtype0=17, attrvalue0=8),
                        dict(id=501, owner_id=7, window='INVENTORY', pos=1, count=9, vnum=200)],
        'protos.item_base': [dict(category_id=1, local_id=1, content_code='weapon.user_item')],
        'protos.item_local_id_sequence': [dict(category_id=1, next_local_id=8)],
    }


class MappingTests(unittest.TestCase):
    def plan(self, data=None): return migration.analyze(data if data is not None else source(), 'a'*64)

    def test_family_uses_explicit_chain_not_id_digits(self):
        plan = self.plan()
        rows = {r['source_id']:r for r in plan['mapping']}
        self.assertEqual(rows[10]['target'], dict(category_id=1, local_id=8))
        self.assertEqual([rows[i]['target'] for i in (10,87,965)], [rows[10]['target']]*3)
        self.assertEqual([rows[i]['refinement_level'] for i in (10,87,965)], [0,1,2])
        self.assertEqual(rows[200]['target'], dict(category_id=6, local_id=1))
        self.assertEqual(plan['summary']['source_prototypes'], 4)
        self.assertEqual(plan['summary']['proposed_prototypes'], 2)

    def test_every_instance_keeps_identity_quantity_owner_and_level(self):
        plan = self.plan()
        self.assertEqual(plan['instances'][0]['instance_id'], 500)
        self.assertEqual(plan['instances'][0]['refinement_level'], 1)
        self.assertEqual(plan['instances'][0]['owner_id'], 7)
        self.assertEqual(plan['instances'][0]['source_row_hash'], migration.value_hash(source()['player.item'][0]))
        self.assertEqual(plan['summary']['mapped_total_quantity'], 10)
        self.assertEqual(plan['summary']['source_total_quantity'], 10)
        self.assertFalse(plan['instances'][0]['state_conversion_complete'])
        self.assertFalse(plan['installable'])

    def test_existing_item_and_issued_sequence_are_reserved(self):
        plan = self.plan()
        self.assertEqual(plan['summary']['preserved_catalog_items'], 1)
        self.assertEqual(plan['next_local_ids']['1'], 9)
        self.assertNotIn(dict(category_id=1,local_id=1), [r['target'] for r in plan['mapping']])

    def test_input_permutation_has_identical_complete_output(self):
        expected = self.plan()
        data = source()
        for _ in range(20):
            for rows in data.values(): random.Random(3491 + _).shuffle(rows)
            self.assertEqual(self.plan(data), expected)

    def test_recipe_cost_probability_and_material_refinement(self):
        plan = self.plan()
        self.assertEqual(plan['recipes'][0]['cost'], 500)
        self.assertEqual(plan['recipes'][0]['success_chance_bp'], 7500)
        self.assertEqual(plan['recipes'][0]['materials'][0]['target'], dict(category_id=6,local_id=1))
        self.assertEqual(plan['recipes'][0]['materials'][0]['amount'], 3)
        self.assertEqual(plan['transitions'][0]['kind'], 'same_prototype_level')

    def test_recipe_runtime_draft_has_no_legacy_lookup_fields(self):
        draft=self.plan()['refinement_recipe_draft']
        self.assertEqual(len(draft['recipes']),2)
        recipe=draft['recipes'][0]
        self.assertEqual(recipe['source'],recipe['target'])
        self.assertEqual((recipe['source_level'],recipe['target_level']),(0,1))
        self.assertEqual(recipe['gold_cost'],500)
        self.assertEqual(recipe['failure_kind'],2)
        self.assertEqual(recipe['ingredients'][0]['amount'],3)
        self.assertNotIn('source_id',recipe)
        self.assertNotIn('source_recipe_id',recipe)
        self.assertFalse(draft['installable'])

    def test_issued_recipe_ids_and_codes_are_preserved(self):
        data=source();data['protos.refinement_recipe']=[dict(recipe_id=42,code='refine.weapon.iron_sword.at_0.smith')]
        draft=self.plan(data)['refinement_recipe_draft']
        self.assertEqual([r['recipe_id'] for r in draft['recipes']],[43,44])
        self.assertNotIn('refine.weapon.iron_sword.at_0.smith',[r['code'] for r in draft['recipes']])

    def test_dedicated_refinement_does_not_borrow_a_smith_recipe(self):
        data=source()
        for row in data['player.item_proto'][:3]:row['type']=13
        result=self.plan(data)['refinement_recipe_draft']
        self.assertEqual(result['recipes'],[])
        self.assertEqual([r['reason'] for r in result['blocked']],['dedicated_refinement_behavior_required']*2)

    def test_auxiliary_stores_get_noncolliding_instance_ids_and_keep_counts(self):
        data=source()
        data['player.account_mount_inventory']=[dict(id=500,account_id=9,slot=4,vnum=10,count=1)]
        data['player.guild_renewal_storage']=[dict(guild_id=12,slot=1,vnum=200,count=700)]
        plan=self.plan(data)
        self.assertEqual([r['instance_id'] for r in plan['instances']],[500,501,502,503])
        self.assertEqual(plan['summary']['source_instances'],4)
        self.assertEqual(plan['summary']['source_total_quantity'],711)
        self.assertEqual(plan['summary']['mapped_total_quantity'],711)
        self.assertEqual(plan['instances'][2]['window'],'ACCOUNT_MOUNT')
        self.assertEqual(plan['instances'][3]['owner_id'],12)

    def test_duplicate_auxiliary_location_aborts(self):
        data=source();row=dict(id=1,account_id=9,slot=4,vnum=10,count=1)
        data['player.account_mount_inventory']=[row,dict(row,id=2)]
        with self.assertRaises(migration.MigrationError):self.plan(data)

    def test_missing_owner_quarantines_but_does_not_delete_item(self):
        plan=migration.analyze(source(),'a'*64,dict(player=[],account=[7],guild=[]))
        self.assertEqual(plan['summary']['owner_status'],dict(missing=2))
        self.assertEqual(plan['summary']['instance_status'],dict(owner_quarantine=2))
        self.assertEqual(plan['summary']['source_total_quantity'],10)
        self.assertEqual(plan['summary']['mapped_total_quantity'],10)
        self.assertEqual(plan['instances'][0]['owner_kind'],'player')

    def test_account_ownership_never_matches_a_same_numbered_player(self):
        data=source();data['player.account_mount_inventory']=[dict(id=1,account_id=7,slot=0,vnum=10,count=1)]
        plan=migration.analyze(data,'a'*64,dict(player=[7],account=[],guild=[]))
        self.assertEqual(plan['summary']['owner_status'],dict(valid=2,missing=1))
        self.assertEqual(plan['instances'][2]['owner_kind'],'account')
        self.assertEqual(plan['instances'][2]['status'],'owner_quarantine')

    def test_unknown_container_owner_kind_is_not_guessed(self):
        data=source();data['player.item'][0]['window']='CUSTOM_STORE'
        plan=migration.analyze(data,'a'*64,dict(player=[7],account=[],guild=[]))
        self.assertIsNone(plan['instances'][0]['owner_kind'])
        self.assertIsNone(plan['instances'][0]['owner_valid'])
        self.assertEqual(plan['summary']['diagnostics']['unknown_instance_owner_kind'],1)

    def test_incomplete_or_duplicate_owner_keys_are_rejected(self):
        for owners in (dict(player=[7]),dict(player=[7,7],account=[],guild=[]),dict(player=[True],account=[],guild=[])):
            with self.subTest(owners=owners),self.assertRaises(migration.MigrationError):migration.analyze(source(),'a'*64,owners)

    def test_shop_products_and_barter_costs_both_map(self):
        data=source();row=dict(shop_vnum=20031,item_vnum=87,count=1,gold=40)
        for i in range(1,6):row['itemvnum'+str(i)]=200 if i==1 else 0
        data['player.shop_item']=[row]
        result=self.plan(data)['relational_reference_drafts']
        refs=result['tables']['player.shop_item'][0]['references']
        self.assertEqual(refs['item_vnum']['refinement_level'],1)
        self.assertEqual(refs['itemvnum1']['item'],dict(category_id=6,local_id=1))
        self.assertIsNone(refs['itemvnum2'])
        self.assertEqual(result['blockers'],[])

    def test_guild_counters_and_building_ids_are_not_item_ids(self):
        data=source();data['player.guild_renewal_kisado_item']=[dict(item0=99999,item1=0)]
        data['player.object_proto']=[dict(vnum=99999,upgrade_vnum=99998,materials='200,3')]
        result=self.plan(data)['relational_reference_drafts']
        self.assertEqual(result['counter_tables']['player.guild_renewal_kisado_item'],1)
        self.assertEqual(result['semantic_payloads_requiring_conversion']['player.object_proto'],1)
        self.assertEqual(result['blockers'],[])

    def test_unknown_shop_reference_is_not_silently_removed(self):
        data=source();data['player.myshop_pricelist']=[dict(owner_id=1,item_vnum=99999,price=20)]
        result=self.plan(data)['relational_reference_drafts']
        self.assertEqual(len(result['tables']['player.myshop_pricelist']),1)
        self.assertEqual(result['blockers'][0]['source_id'],99999)

    def test_name_alone_never_merges_unlinked_items(self):
        data = source(); data['player.item_proto'][0]['refined_vnum'] = 0
        self.assertNotEqual(self.plan(data)['mapping'][0]['target'], self.plan(data)['mapping'][1]['target'])

    def test_different_type_or_policy_splits_family(self):
        for field, value in (('subtype',1), ('type',2), ('antiflag',1), ('size',2), ('limittype0',1)):
            with self.subTest(field=field):
                data = source(); data['player.item_proto'][1][field]=value
                rows={r['source_id']:r for r in self.plan(data)['mapping']}
                self.assertNotEqual(rows[10]['target'],rows[87]['target'])

    def test_new_semantic_item_is_transformation(self):
        data = source(); row=data['player.item_proto'][1]
        for field in migration.NAME_FIELDS: row[field]='Steel Sword+0'
        plan = self.plan(data)
        self.assertEqual(plan['transitions'][0]['kind'],'item_transformation')

    def test_self_reference_is_reported_and_not_a_free_step(self):
        data=source(); data['player.item_proto'][0]['refined_vnum']=10
        plan=self.plan(data)
        self.assertEqual(plan['summary']['diagnostics']['self_refinement_reference'],1)
        self.assertNotIn(10,[r['source_id'] for r in plan['transitions']])

    def test_missing_recipe_is_reported_instead_of_zero_cost(self):
        data=source(); data['player.refine_proto']=[]
        plan=self.plan(data)
        self.assertEqual(plan['summary']['diagnostics']['missing_used_refinement_recipe'],2)
        self.assertTrue(all(not t['recipe_found'] for t in plan['transitions']))

    def test_material_gap_and_missing_material_are_visible(self):
        data=source(); data['player.refine_proto'][0].update(vnum0=0,count0=0,vnum2=999,count2=1)
        plan=self.plan(data)
        self.assertEqual(plan['summary']['diagnostics']['missing_recipe_material'],1)
        self.assertEqual(plan['summary']['diagnostics']['recipe_material_after_zero_slot'],1)

    def test_retired_items_are_accounted_but_not_restored(self):
        data=source(); data['player.item_proto'].append(proto(5000,kind=29))
        data['player.item'].append(dict(id=502,owner_id=8,window='EQUIPMENT',pos=0,count=1,vnum=5000))
        plan=self.plan(data)
        self.assertEqual(plan['summary']['prototype_status']['retired'],1)
        self.assertEqual(plan['summary']['instance_status']['retired'],1)
        self.assertEqual(plan['instances'][-1]['count'],1)

    def test_zero_source_placeholder_is_quarantined_without_zero_target(self):
        data=source(); data['player.item_proto'].append(proto(0,kind=0))
        row=next(r for r in self.plan(data)['mapping'] if r['source_id']==0)
        self.assertEqual(row['status'],'quarantine');self.assertIsNone(row['target'])

    def test_unknown_fields_and_ranges_do_not_disappear(self):
        data=source();data['player.item_proto'][0].update(new_flag=1,vnum_range=7)
        codes=self.plan(data)['summary']['diagnostics']
        self.assertEqual(codes['unaccounted_proto_columns'],1)
        self.assertEqual(codes['range_proto_requires_expansion'],1)

    def test_orphan_instance_is_preserved_as_blocker(self):
        data=source();data['player.item'][0]['vnum']=9999
        plan=self.plan(data)
        self.assertEqual(plan['summary']['diagnostics']['orphan_instance'],1)
        self.assertEqual(plan['instances'][0]['status'],'orphan')
        self.assertEqual(plan['summary']['mapped_total_quantity'],10)

    def test_duplicate_source_or_instance_ids_abort(self):
        for name in ('player.item_proto','player.item','player.refine_proto'):
            with self.subTest(table=name):
                data=source();data[name].append(copy.deepcopy(data[name][0]))
                with self.assertRaises(migration.MigrationError):self.plan(data)

    def test_level_reset_is_not_collapsed(self):
        data=source()
        for name in migration.NAME_FIELDS: data['player.item_proto'][1][name]='Iron Sword+0'
        plan=self.plan(data)
        self.assertEqual(plan['refinement_edges'][0]['reason'],'level_reset_or_gap')

    def test_no_suffix_chain_is_finite_and_can_cross_nine(self):
        data=source();data['player.item']=[];data['player.refine_proto']=[]
        data['player.item_proto']=[proto(100+i,i,101+i if i<30 else 0) for i in range(31)]
        for row in data['player.item_proto']:
            for name in migration.NAME_FIELDS: row[name]='Aqua Sword x13'
        plan=self.plan(data)
        self.assertEqual(len(plan['families']),1)
        self.assertEqual(plan['families'][0]['maximum_candidate'],30)
        self.assertEqual(plan['mapping'][10]['refinement_level'],10)

    def test_fan_in_chain_does_not_assign_two_sources_to_same_level(self):
        data=source();data['player.item_proto'].append(proto(20,0,87))
        plan=self.plan(data)
        self.assertEqual(plan['summary']['diagnostics']['merged_refinement_chains'],1)
        self.assertEqual(len(plan['mapping']),5)

    def test_suffixless_cycle_is_detected_without_losing_any_rows(self):
        data=source()
        data['player.item_proto'][2]['refined_vnum']=10
        for row in data['player.item_proto'][:3]:
            for name in migration.NAME_FIELDS:row[name]='Iron Sword'
        plan=self.plan(data)
        self.assertEqual(plan['summary']['diagnostics']['refinement_cycle'],1)
        self.assertEqual(len(plan['mapping']),4)
        self.assertFalse(plan['installable'])

    def test_suffixless_family_cannot_silently_exceed_technical_ceiling(self):
        data=source();data['player.item']=[]
        data['player.item_proto']=[proto(1000+i,0,1001+i if i<256 else 0) for i in range(257)]
        for row in data['player.item_proto']:
            for name in migration.NAME_FIELDS:row[name]='Aqua Sword'
        plan=self.plan(data)
        self.assertEqual(plan['summary']['diagnostics']['family_exceeds_supported_levels'],1)
        self.assertEqual(len(plan['mapping']),257)
        self.assertIn('level_out_of_range',plan['summary']['refinement_draft_blockers'])

    def test_overflow_allocation_is_rejected(self):
        data=source();data['protos.item_local_id_sequence'][0]['next_local_id']=1<<32
        with self.assertRaises(migration.MigrationError):self.plan(data)

    def test_recipe_and_state_data_changes_change_source_hashes(self):
        a=self.plan();data=source();data['player.item'][0]['socket0']=0;data['player.refine_proto'][0]['cost']=501
        b=self.plan(data)
        self.assertNotEqual(a['instances'][0]['source_row_hash'],b['instances'][0]['source_row_hash'])
        self.assertNotEqual(a['recipes'][0]['source_row_hash'],b['recipes'][0]['source_row_hash'])


class DecoderTests(unittest.TestCase):
    def test_unicode_null_signed_int_and_binary_are_lossless(self):
        packet=dict(columns=[dict(name='name',encoding='utf8-hex',data_type='varchar'),
                             dict(name='expiry',encoding='text',data_type='bigint'),
                             dict(name='opaque',encoding='binary-hex',data_type='blob')],
                    rows=[['Árvíztűrő+10'.encode().hex(),'-9223372036854775808','00ff'],[None,'0',None]])
        decoded=migration.decode_table(packet)
        self.assertEqual(decoded[0],dict(name='Árvíztűrő+10',expiry=-(1<<63),opaque=dict(binary_hex='00ff')))
        self.assertIsNone(decoded[1]['name'])

    def test_malformed_shapes_and_numbers_fail_closed(self):
        col=dict(name='n',encoding='text',data_type='int')
        for packet in (dict(columns=[col,col],rows=[]),dict(columns=[col],rows=[[]]),
                       dict(columns=[col],rows=[['1e3']]),dict(columns=[col],rows=[['01']])):
            with self.subTest(packet=packet),self.assertRaises(migration.MigrationError):migration.decode_table(packet)

    def test_duplicate_json_keys_are_rejected(self):
        with self.assertRaises(migration.MigrationError):migration.strict_json('{"version":1,"version":2}')

    def test_checkpoint_tampering_is_rejected_before_reading_rows(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);(root/'table.json').write_text('{}')
            manifest=dict(format_version=1,complete=True,files={'table.json':'a'*64},tables={})
            (root/'manifest.json').write_text(json.dumps(manifest))
            with self.assertRaises(migration.MigrationError):migration.read_checkpoint(root)


class CatalogUpgradeTests(unittest.TestCase):
    def fixture(self):
        blank=dict(columns={'catalog_format':['singleton','schema_version','canonical_domain'],
                            'item_category':['category_id','code','prototype_table'],
                            'bonus_definition':['bonus_id','code','allowed_sources'],
                            'item_base':['category_id','local_id','content_code'],
                            'item_consumable':['category_id','local_id','recovery_profile_id','timed_effect_profile_id','toggle_effect_profile_id']},
                   tables={'catalog_format':[['1','12','auriga.item_catalog.v12']],
                           'item_category':[['1','weapon','item_weapon']],
                           'bonus_definition':[['1','attack.percent','7']],
                           'item_base':[],'item_consumable':[]},sequences={'1':'1'},hash='old')
        tables={'protos.catalog_format':[dict(singleton=1,schema_version=7,canonical_domain='auriga.item_catalog.v7')],
                'protos.item_category':[dict(category_id=1,code='weapon',prototype_table='item_weapon')],
                'protos.bonus_definition':[dict(bonus_id=1,code='attack.percent',allowed_sources=3)],
                'protos.item_base':[dict(category_id=1,local_id=1,content_code='weapon.user_item')],
                'protos.item_local_id_sequence':[dict(category_id=1,next_local_id=10)]}
        return tables,blank

    def test_preserves_user_item_and_issued_gaps_with_additive_registry(self):
        tables,blank=self.fixture();candidate,counts=upgrade.upgrade(tables,blank)
        self.assertEqual(candidate['tables']['item_base'],[['1','1','weapon.user_item']])
        self.assertEqual(candidate['sequences']['1'],'10')
        self.assertEqual(candidate['tables']['catalog_format'][0][1],'12')
        self.assertEqual(counts['item_base'],1)
        self.assertNotIn('hash',candidate)

    def test_unknown_source_format_and_nonempty_dropped_tables_reject(self):
        tables,blank=self.fixture();tables['protos.catalog_format'][0]['schema_version']=6
        with self.assertRaises(migration.MigrationError):upgrade.upgrade(tables,blank)
        tables,blank=self.fixture();tables['protos.unknown_profile']=[dict(value=7)]
        with self.assertRaises(migration.MigrationError):upgrade.upgrade(tables,blank)

    def test_bonus_reinterpretation_or_source_revocation_rejects(self):
        for field,value in [('code','different.meaning'),('allowed_sources','1')]:
            tables,blank=self.fixture();blank['tables']['bonus_definition'][0][blank['columns']['bonus_definition'].index(field)]=value
            with self.assertRaises(migration.MigrationError):upgrade.upgrade(tables,blank)

    def test_user_field_loss_and_unmapped_required_new_column_reject(self):
        tables,blank=self.fixture();tables['protos.item_base'][0]['unhandled']=9
        with self.assertRaises(migration.MigrationError):upgrade.upgrade(tables,blank)
        tables,blank=self.fixture();blank['columns']['item_base'].append('new_required')
        with self.assertRaises(migration.MigrationError):upgrade.upgrade(tables,blank)

    def test_nullable_new_effect_choices_are_explicit(self):
        tables,blank=self.fixture()
        tables['protos.item_consumable']=[dict(category_id=5,local_id=8,recovery_profile_id=1)]
        candidate,_=upgrade.upgrade(tables,blank)
        self.assertEqual(candidate['tables']['item_consumable'],[['5','8','1',None,None]])


if __name__=='__main__':unittest.main()
