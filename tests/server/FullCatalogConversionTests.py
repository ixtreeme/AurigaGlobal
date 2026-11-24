"""Full-prototype definition conversion, using the real native validator."""
import argparse,copy,json,sys,tempfile,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path[:0]=[str(ROOT/'SRC/Tools/ItemCatalogBuilder'),str(ROOT/'SRC/Tools/ServerManager')]
from convert_full_catalog import FullCatalogCompiler,disabled_source
from legacy_item_migration import PROTO_FIELDS,NAME_FIELDS,file_hash,value_hash
from upgrade_exported_catalog import native

class Conversion(unittest.TestCase):
    @classmethod
    def setUpClass(cls):cls.blank=native(HELPER,dict(action='blank'))
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup);self.project=Path(self.temp.name)
        self.visuals={}
    def row(self,vnum=1,kind=1,sub=0,level=0):
        r={k:'' if k in NAME_FIELDS else 0 for k in PROTO_FIELDS}
        r.update(vnum=vnum,type=kind,subtype=sub,size=1,name='Test+'+str(level),locale_name_en='Test +'+str(level),locale_name_hu='Próba +'+str(level),value1=10,value2=20,value3=30,value4=40,value5=level)
        icon='icon/item/'+str(vnum)+'.tga';guid=format(vnum,'032x')
        self.put('Assets/Metin2/Generated/ItemIcons/'+icon,guid)
        self.put('Assets/Metin2/AddressableContent/Equipment/w'+str(vnum)+'.asset',guid)
        self.visuals[vnum]=dict(iconPath=icon,icon=dict(file_id='2800000',guid=guid))
        return r
    def put(self,relative,guid):
        p=self.project/relative;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(b'fixture')
        p.with_name(p.name+'.meta').write_text('guid: '+guid+'\n')
    def compiler(self,rows,extras=(),known=(),groups=(),fishing=()):
        tables={'player.item_proto':rows,'player.item_extra_proto':list(extras)}
        return FullCatalogCompiler(tables,copy.deepcopy(self.blank),self.project,self.visuals,{},known,groups,fishing)
    def validate(self,result):return native(HELPER,dict(action='validate',tables=result['packet']['tables']))['hash']
    def service_row(self,identity,value0=0,value1=0,value2=0):
        row=self.row(identity,3,10);row.update(value0=value0,value1=value1,value2=value2,value3=0,value4=0,value5=0,flag=4);return row
    def energy_compiler(self):
        row=self.service_row(51002);row.update(type=27,subtype=0,limittype0=1,limitvalue0=35)
        c=self.compiler([row]);c.blends={51002:dict(item_vnum=51002,apply_type='ENERGY',apply_value=[1,3,5,7,10],apply_duration=[1800]*5)};return c
    def test_native_money_is_ground_wallet_currency_and_variable_payout(self):
        ground=self.service_row(1);ground.update(type=9,subtype=0,flag=0)
        payout=self.service_row(50026);payout['flag']=0
        result=self.compiler([ground,payout]).compile();self.assertEqual(len(result['accepted']),2);self.validate(result)
        p=result['packet'];rows=lambda table:[dict(zip(p['columns'][table],r)) for r in p['tables'][table]]
        self.assertEqual({r['mode_kind'] for r in rows('currency_token_policy')},{'2','3'})
        self.assertEqual({r['amount_per_unit'] for r in rows('item_currency_token')},{'0','1'})
        self.assertEqual({r['content_code'] for r in rows('item_base')},{'currency.ground_gold','currency.guild_payout'})
    def test_both_original_private_shop_bundles_have_real_handlers_and_lifetime(self):
        paper=self.service_row(50200);silk=self.service_row(71049,value0=864000,value2=1);silk.update(type=16,flag=0,antiflag=106880)
        result=self.compiler([paper,silk]).compile();self.assertEqual(len(result['accepted']),2);self.validate(result)
        p=result['packet'];rows=lambda table:[dict(zip(p['columns'][table],r)) for r in p['tables'][table]]
        self.assertEqual({r['group_code'] for r in rows('item_quest')},{'shop.bundle','shop.silk_bundle'})
        self.assertEqual({r['dispatch_kind'] for r in rows('item_quest')},{'2'})
        self.assertEqual({r['max_stack'] for r in rows('item_base')},{'1','50000'})
        ref=next(r for r in result['accepted'] if r['source_id']==71049);rule=next(r for r in rows('item_definition_rules') if r['local_id']==str(ref['item']['local_id']))
        self.assertEqual((rule['clock_kind'],rule['duration_ms']),('2','864000000'))
    def test_private_shop_conversion_rejects_unverified_silk_duration(self):
        silk=self.service_row(71049,value0=864001,value2=1);silk.update(type=16,flag=0)
        result=self.compiler([silk]).compile();self.assertFalse(result['accepted']);self.assertEqual(result['blocked'][0]['reasons'],['original_silk_bundle_lifetime_changed']);self.validate(result)
    def test_money_editor_document_preserves_explicit_policy(self):
        from item_catalog import Catalog
        row=self.service_row(50026);row['flag']=0;result=self.compiler([row]).compile();self.validate(result)
        ref=result['accepted'][0]['item'];key=(16,ref['local_id']);model=Catalog(result['packet']);doc=model.item(key)
        self.assertEqual(doc['money_policy']['mode_kind'],'2');model.apply(doc,key)
        self.assertTrue(native(HELPER,dict(action='validate',tables=model.encode()))['hash'])
    def test_variable_payout_rejects_unexplained_original_payload(self):
        row=self.service_row(50026,value0=3);row['flag']=0;result=self.compiler([row]).compile()
        self.assertEqual(len(result['blocked']),1);self.assertFalse(result['accepted']);self.validate(result)
    def test_original_energy_birth_distribution_and_lifetime_are_explicit(self):
        c=self.energy_compiler();result=c.compile();self.assertEqual(len(result['accepted']),1);self.validate(result)
        self.assertEqual(result['rule_inputs']['GameServer/item/blend_item.cpp'],
                         file_hash(ROOT/'config/item_combat/original_blend_item.source.cpp'))
        p=result['packet'];rows=lambda t:[dict(zip(p['columns'][t],r)) for r in p['tables'][t]]
        self.assertEqual([(r['percent'],r['chance_bp']) for r in rows('energy_crystal_tier')],[('1','500'),('3','1000'),('5','4500'),('7','2500'),('10','1500')])
        self.assertEqual(rows('timed_effect_profile')[0]['utility_kind'],'3');self.assertEqual(rows('timed_effect_profile')[0]['duration_ms'],'1800000');self.assertEqual(rows('item_base')[0]['min_level'],'35')
    def test_energy_never_invents_missing_birth_values(self):
        c=self.energy_compiler();c.blends={};result=c.compile();self.assertFalse(result['accepted']);self.assertEqual(len(result['blocked']),1)
    def test_energy_rejects_changed_unported_distribution_or_duration(self):
        for field in ('apply_value','apply_duration'):
            c=self.energy_compiler();c.blends[51002][field][0]+=1;result=c.compile();self.assertFalse(result['accepted']);self.assertIn('original_energy_crystal_distribution_changed',result['blocked'][0]['reasons'])
    def test_energy_editor_document_roundtrip_and_copy_on_write(self):
        c=self.energy_compiler();result=c.compile();ref=result['accepted'][0]['item'];key=(ref['category_id'],ref['local_id']);model=c.model
        doc=model.item(key);self.assertEqual(len(doc['energy_tiers']),5);model.apply(doc,key);self.assertTrue(native(HELPER,dict(action='validate',tables=model.encode()))['hash'])
        clone=model.item(key);clone['base']['local_id']=str(int(clone['base']['local_id'])+1);clone['child']['local_id']=clone['base']['local_id'];clone['base']['content_code']='energy.cloned';model.apply(clone)
        old_id=model.item(key)['child']['timed_effect_profile_id'];changed=model.item((key[0],int(clone['base']['local_id'])));changed['energy_tiers'][0]['chance_bp']='600';changed['energy_tiers'][1]['chance_bp']='900';model.apply(changed,(key[0],int(clone['base']['local_id'])))
        self.assertEqual(model.item(key)['energy_tiers'][0]['chance_bp'],'500');self.assertNotEqual(model.item((key[0],int(clone['base']['local_id'])))['child']['timed_effect_profile_id'],old_id);self.assertTrue(native(HELPER,dict(action='validate',tables=model.encode()))['hash'])
    def test_original_costume_transfer_is_a_three_item_native_tool(self):
        row=self.service_row(70065);row['type']=36;row['subtype']=0
        result=self.compiler([row]).compile();self.assertEqual(len(result['accepted']),1);self.validate(result)
        p=result['packet'];tool=dict(zip(p['columns']['item_bonus_tool'],p['tables']['item_bonus_tool'][0]));self.assertEqual((tool['operation_kind'],tool['units'],tool['parameter']),('8','1','0'))
        self.assertEqual(result['accepted'][0]['semantics'],'original_three_item_costume_attribute_transfer')
    def test_costume_transfer_source_exclusions_are_explicit_native_bindings(self):
        from full_catalog_costume_transfer import bind_costume_transfer_targets
        from types import SimpleNamespace
        key=lambda c,n:dict(category_id=c,local_id=n)
        fake=SimpleNamespace(accepted={70065:dict(semantics='original_three_item_costume_attribute_transfer',item=key(20,1)),73001:dict(item=key(4,1)),73013:dict(item=key(4,2)),80001:dict(item=key(19,3)),80002:dict(item=key(4,4))},model=SimpleNamespace(tables=dict(item_costume=[dict(category_id='4',local_id='1',costume_kind='1'),dict(category_id='4',local_id='2',costume_kind='1'),dict(category_id='4',local_id='4',costume_kind='4')],item_regalia=[dict(category_id='19',local_id='3',regalia_kind='1')],costume_transfer_target=[])))
        self.assertEqual(bind_costume_transfer_targets(fake),dict(targets=2,excluded_sources=1));self.assertEqual(fake.model.tables['costume_transfer_target'],[dict(category_id='4',local_id='2'),dict(category_id='19',local_id='3')]);self.assertEqual(bind_costume_transfer_targets(fake)['targets'],2)
    def test_costume_transfer_never_ignores_new_unimplemented_source_values(self):
        row=self.service_row(70065,value0=1);row['type']=36;row['subtype']=0;result=self.compiler([row]).compile();self.assertEqual(len(result['accepted']),0);self.assertEqual(len(result['blocked']),1)
    def test_retirement_policy_never_applies_only_by_an_old_source_id(self):
        from full_catalog_final_policies import retired_original,RETIREMENTS
        from unittest.mock import patch
        row=self.service_row(72320)
        self.assertIsNone(retired_original(row))
        with patch.dict(RETIREMENTS,{72320:dict(source_row_hash=value_hash(row),reason='explicit_test_policy')}):
            self.assertEqual(retired_original(row),'explicit_test_policy')
            row['value0']=1;self.assertIsNone(retired_original(row))
    def test_original_dungeon_keys_have_explicit_native_roles_and_use_actions(self):
        result=self.compiler([self.service_row(n) for n in (89102,89103,89104)]).compile();self.assertEqual(len(result['accepted']),3);self.validate(result)
        p=result['packet'];rows=lambda t:[dict(zip(p['columns'][t],r)) for r in p['tables'][t]]
        bases=rows('item_base');quests=rows('item_quest')
        self.assertEqual({r['content_code'] for r in bases},{'dungeon.rune.fragment','dungeon.rune.floor_key','dungeon.orcs.key'})
        self.assertEqual(sorted((r['dispatch_kind'],r['group_code'],r['allowed_contexts']) for r in quests),[('1','','0'),('2','dungeon.rune.floor_key','1'),('2','dungeon.rune.fragment','1')])
    def test_original_dungeon_key_does_not_silently_ignore_unknown_source_values(self):
        result=self.compiler([self.service_row(89102,value0=1)]).compile();self.assertEqual(len(result['accepted']),0);self.assertEqual(len(result['blocked']),1)
    def test_original_polymorph_book_and_source_form_bonuses(self):
        import struct
        row=self.row(50322,19);row.update(**{'value'+str(n):0 for n in range(6)})
        index=self.project/'Assets/Metin2/Resources/Metin2RacePrefabIndex.asset';index.parent.mkdir(parents=True,exist_ok=True);index.write_text('  races: '+struct.pack('<III',101,103,1901).hex()+'\n')
        catalog=self.project/'Assets/Metin2/GeneratedData/Metin2RaceCatalog.asset';catalog.parent.mkdir(parents=True,exist_ok=True);catalog.write_text(''.join('  - race: '+str(r)+'\n    folder: real_source\n    prefab: {fileID: 1, guid: '+('a'*32)+', type: 3}\n' for r in (101,103,1901)))
        result=self.compiler([row]).compile();self.assertEqual(result['summary']['converted_source_rows'],1);self.validate(result)
        p=result['packet'];rows=lambda t:[dict(zip(p['columns'][t],r)) for r in p['tables'][t]]
        book=next(r for r in rows('polymorph_profile') if r['polymorph_kind']=='2');self.assertEqual((book['max_book_grade'],book['practice_per_grade'],book['book_chance_base_percent']),('255','50','29'))
        self.assertEqual([(r['duration_ms'],r['bonus_duration_ms']) for r in rows('polymorph_mastery_rule')],[('10000','9000'),('15000','14000'),('20000','19000'),('25000','24000')])
        self.assertEqual({int(r['mob_template_id']):r['book_bonus'] for r in rows('polymorph_form')},{101:'4',103:'1',1901:'4'})
        self.assertEqual(result['accepted'][0]['semantics'],'original_polymorph_practice_book')
    def test_original_recall_native_modes_and_shared_distance_repair(self):
        rows=[]
        for identity,mode in ((22000,1),(22010,2),(22011,2),(39043,2)):
            r=self.service_row(identity,value0=mode);r.update(type=3,subtype=1);rows.append(r)
        result=self.compiler(rows).compile();self.assertEqual(len(result['accepted']),4);self.validate(result)
        children=[dict(zip(result['packet']['columns']['item_quest'],r)) for r in result['packet']['tables']['item_quest']]
        self.assertEqual({r['group_code'] for r in children},{'utility.recall_town_restricted','utility.recall_memory_restricted','utility.recall_memory'})
        repaired=next(r for r in result['accepted'] if r['source_id']==22011)
        self.assertEqual(repaired['source_distance_branch_repaired'],'use_saved_memory_destination_instead_of_always_zero_distance')
    def test_original_detectors_use_explicit_capacity_not_practice_or_sockets(self):
        rows=[]
        for identity in (27989,76006):
            r=self.service_row(identity);r.update(type=3,subtype=10);rows.append(r)
        result=self.compiler(rows).compile();self.assertEqual(len(result['accepted']),2);self.validate(result)
        children=[dict(zip(result['packet']['columns']['item_tool'],r)) for r in result['packet']['tables']['item_tool']]
        self.assertTrue(all(r['tool_kind']=='5' and r['maximum_uses']=='6' and r['all_water_pools']=='0' for r in children))
        levels=[dict(zip(result['packet']['columns']['tool_level'],r)) for r in result['packet']['tables']['tool_level']]
        self.assertTrue(all(r['power']=='0' and r['required_points']=='0' and r['practice_numerator']=='0' and r['success_local_id'] is None for r in levels))
    def test_recall_requires_actual_source_icon(self):
        row=self.service_row(22020,value0=2);row.update(type=3,subtype=1);self.visuals[22020]['iconPath']=''
        result=self.compiler([row]).compile();self.assertEqual(result['accepted'],[]);self.assertEqual(result['blocked'][0]['reasons'],['missing_source_icon']);self.validate(result)
    def original_pet_book_compiler(self,rows):
        c=self.compiler(rows)
        c.model.tables['pet_training_profile'].append(dict(profile_id='1',code='test.original_pet_training',hp_evolution_period='5',monster_evolution_period='7',pve_evolution_period='4',evolution_min='1',evolution_max='6'))
        c.model.tables['pet_age_tier'].append(dict(profile_id='1',minimum_age_ms='0',max_hp_flat='500',monster_resistance_bp='200',human_resistance_bp='200'))
        c.model.tables['pet_slot_rule'].extend(dict(profile_id='1',slot=str(i),minimum_stage='0',minimum_age_ms='0') for i in range(4))
        return c
    def test_all_original_pet_books_bind_real_rank_tables_and_owner_cooldown(self):
        rows=[]
        for identity in range(55010,55022):
            row=self.service_row(identity,value0=identity-55009);row['type']=37;rows.append(row)
        result=self.original_pet_book_compiler(rows).compile();self.assertEqual(len(result['accepted']),12);self.validate(result)
        effects=list(range(19,31))
        self.assertEqual([r['native_pet_effect'] for r in result['accepted']],effects)
        self.assertEqual(result['original_pet_skills'],dict(profiles=1,skills=12,ranks=120))
        rules=[dict(zip(result['packet']['columns']['pet_study_rule'],r)) for r in result['packet']['tables']['pet_study_rule']]
        self.assertTrue(all(r['known_success_bp']=='7100' and r['known_cooldown_ms']=='10800000' and r['selected_allowed']=='1' for r in rules))
        ranks=[dict(zip(result['packet']['columns']['pet_skill_rank'],r)) for r in result['packet']['tables']['pet_skill_rank']]
        self.assertEqual(sorted(int(r['value_scaled']) for r in ranks if r['skill_rank']=='10'),[1000]*10+[2000]*2)
    def test_original_pet_books_preserve_issued_identities_and_profiles_on_reproduction(self):
        row=self.service_row(55021,value0=12);row['type']=37;first=self.original_pet_book_compiler([row]).compile();first['packet']['hash']=self.validate(first)
        again=FullCatalogCompiler({'player.item_proto':[row]},first['packet'],self.project,self.visuals,{},first['accepted']).compile();self.validate(again)
        self.assertEqual(first['packet']['tables'],again['packet']['tables']);self.assertEqual(again['summary']['new_native_definitions'],0)
    def test_original_pet_book_requires_a_real_native_training_profile(self):
        row=self.service_row(55010,value0=1);row['type']=37;result=self.compiler([row]).compile()
        self.assertEqual(result['accepted'],[]);self.assertEqual(result['blocked'][0]['reasons'],['missing_native_growth_pet_training_profile']);self.validate(result)
    def test_original_reusable_world_utilities(self):
        rows=[self.service_row(n) for n in (70302,71092)];result=self.compiler(rows).compile();self.assertEqual(len(result['accepted']),2);self.validate(result)
        p=result['packet'];quests=[dict(zip(p['columns']['item_quest'],r)) for r in p['tables']['item_quest']]
        self.assertEqual({r['group_code'] for r in quests},{'utility.spouse_warp','utility.polymorph_cancel'});self.assertTrue(all(r['dispatch_kind']=='2' for r in quests))
    def test_world_assets_gate_existing_keys_and_accept_the_split_runtime_catalog(self):
        from full_catalog_world_rewards import bind_world_reward_availability
        def put(path,text):
            target=self.project/path;target.parent.mkdir(parents=True,exist_ok=True);target.write_text(text,encoding='utf8')
        put('Assets/Metin2/GeneratedData/Metin2RaceCatalog.asset','  - race: 1\n    folder: source_one\n    prefab: {fileID: 1, guid: '+('a'*32)+', type: 3}\n')
        put('Assets/Metin2/Resources/Metin2RacePrefabIndex.asset','  races: 0100000002000000\n')
        put('Assets/Metin2/AddressableContent/Races/Prefabs/2.asset','  race: 2\n  prefab: {fileID: 2, guid: '+('b'*32)+', type: 3}\n')
        group=self.project/'group.json';group.write_text(json.dumps(dict(groups=[dict(vnum=901,leader=dict(vnum=2),members=[dict(vnum=3)])])),encoding='utf8')
        result=dict(accepted=[dict(source_id=100+n,item=dict(category_id=8,local_id=n)) for n in (1,2,3)],packet=dict(columns={'loot_entry':['profile_id','reward_kind','amount'],'item_container':['category_id','local_id','container_kind','loot_profile_id'],'item_definition_rules':['category_id','local_id','refinement_level','available']},tables={'loot_entry':[['1','5','901'],['2','4','3'],['3','4','1']],'item_container':[['8',str(n),'1',str(n)] for n in (1,2,3)],'item_definition_rules':[['8',str(n),'0','1'] for n in (1,2,3)]}))
        original=copy.deepcopy(result);audit=bind_world_reward_availability(result,self.project,group);self.assertEqual(audit['usable'],1);self.assertEqual([r['missing_creature_assets'] for r in audit['deferred']],[[3],[3]]);self.assertEqual(result['accepted'],original['accepted']);self.assertEqual([r[-1] for r in result['packet']['tables']['item_definition_rules']],['0','0','1']);self.assertIn('2',audit['per_race_assets'])
        put('Assets/Metin2/Resources/Metin2RacePrefabIndex.asset','  races: 010000000200000003000000\n');put('Assets/Metin2/AddressableContent/Races/Prefabs/3.asset','  race: 3\n  prefab: {fileID: 3, guid: '+('c'*32)+', type: 3}\n');self.assertEqual(bind_world_reward_availability(original,self.project,group)['usable'],3)

    def test_absorption_sashes_keep_original_grades_and_donor_component_rounding(self):
        from convert_shop_content import SASH_CATALOG
        rows=[self.behavior_row(85000+grade,28,3,value0=grade) for grade in range(1,5)]
        weapon=self.row(100,1,0);weapon.update(applytype0=5,applyvalue0=3,applytype1=5,applyvalue1=4);rows.extend([weapon,self.behavior_row(86080,3,45)])
        path=self.project/SASH_CATALOG;path.parent.mkdir(parents=True,exist_ok=True);path.write_text(json.dumps(dict(entries=[dict(vnum=85000+grade,address='Metin2Sash/test_'+str(grade)) for grade in range(1,5)])),encoding='utf8')
        result=self.compiler(rows).compile();self.assertEqual(len(result['accepted']),6);self.validate(result)
        model=__import__('item_catalog').Catalog(result['packet']);refs={r['source_id']:r for r in result['accepted']}
        for grade in range(1,5):
            ref=refs[85000+grade];doc=model.item((19,ref['item']['local_id']));self.assertEqual(doc['child']['regalia_kind'],'3');self.assertEqual(doc['regalia_rules'],[]);self.assertEqual(doc['bonuses'],[])
            self.assertEqual(int(doc['absorption_sash']['next_local_id']),refs[85000+min(grade+1,4)]['item']['local_id'])
        self.assertEqual(model.tables['sash_donor_profile'][0]['physical_max_raw'],'40000');self.assertEqual(model.tables['sash_donor_profile'][0]['magical_max_raw'],'20000')
        self.assertEqual([r['value_scaled'] for r in model.tables['sash_donor_component']],['3','4'])
        self.assertEqual(model.item((20,refs[86080]['item']['local_id']))['child']['operation_kind'],'7')
        reversed_result=__import__('copy').deepcopy(result);reversed_result['packet']['tables']['sash_donor_component'].reverse();self.assertEqual(self.validate(result),self.validate(reversed_result))

    def test_blood_pill_keeps_one_native_stat_minus_credit(self):
        row=self.behavior_row(70014,3,10);result=self.compiler([row]).compile();self.assertEqual(len(result['accepted']),1);self.validate(result)
        ref=result['accepted'][0];self.assertEqual(ref['semantics'],'stat_reset_credit_character_service');doc=__import__('item_catalog').Catalog(result['packet']).item((5,ref['item']['local_id']))
        self.assertEqual(doc['character_service'],dict(service_kind='2',units_per_use='1',amount='1',minimum_value='0',maximum_value='32767'))
        row['value0']=1;result=self.compiler([row]).compile();self.assertEqual(result['blocked'][0]['reasons'],['unmapped_behavior_value'])

    def test_full_restoration_foods_keep_their_actual_compiled_pool(self):
        rows=[self.behavior_row(n,3,10) for n in (50020,50021,50022)];result=self.compiler(rows).compile();self.assertEqual(len(result['accepted']),3);self.validate(result);model=__import__('item_catalog').Catalog(result['packet'])
        for r,pool in zip(result['accepted'],('hp','sp','stamina')):self.assertEqual(model.item((10,r['item']['local_id']))['child']['group_code'],'utility.restore_'+pool+'_full')
        rows[0]['value0']=100;result=self.compiler(rows[:1]).compile();self.assertEqual(result['blocked'][0]['reasons'],['unmapped_behavior_value'])

    def test_actual_world_utilities_are_compiled_handlers_not_passive_tokens(self):
        entries=((27996,10,'self_poison'),(27124,12,'cleanse'),(72305,13,'invisibility'),(70008,10,'white_flag'),(70057,10,'courage_combined'),(71013,10,'firework_random'),(50103,10,'firework_4'),(50106,10,'firework_7'),(50107,10,'china_firework'),(50108,10,'spinning_top'))
        rows=[self.behavior_row(identity,3,sub) for identity,sub,group in entries]
        next(r for r in rows if r['vnum']==72305)['value1']=180
        result=self.compiler(rows).compile();self.assertEqual(len(result['accepted']),len(entries));self.validate(result);model=__import__('item_catalog').Catalog(result['packet'])
        for identity,sub,group in entries:
            ref=next(r for r in result['accepted'] if r['source_id']==identity);self.assertEqual(ref['item']['category_id'],10);doc=model.item((10,ref['item']['local_id']));self.assertEqual(doc['child']['group_code'],'utility.'+group);self.assertEqual(doc['child']['dispatch_kind'],'2')
        ref=next(r for r in result['accepted'] if r['source_id']==72305);self.assertEqual(ref['source_fixed_duration_seconds'],300)
        rows[0]['value2']=1;result=self.compiler(rows[:1]).compile();self.assertEqual(result['blocked'][0]['reasons'],['unmapped_behavior_value'])

    def test_timed_stoles_preserve_both_source_clock_modes(self):
        from convert_shop_content import SASH_CATALOG
        rows=[];entries=[]
        for identity,kind,seconds in ((85088,8,86400),(85128,7,604800)):
            row=self.row(identity,28,5);row.update(value0=4,value1=0,value2=0,value3=0,value4=0,value5=0,limittype0=kind,limitvalue0=seconds);rows.append(row)
            entries.append(dict(vnum=identity,address='Metin2SashAssets/test_'+str(identity)))
        p=self.project/SASH_CATALOG;p.parent.mkdir(parents=True,exist_ok=True);p.write_text(json.dumps(dict(entries=entries)),encoding='utf-8')
        result=self.compiler(rows).compile();self.assertEqual(len(result['accepted']),2);self.validate(result)
        model=__import__('item_catalog').Catalog(result['packet'])
        for ref in result['accepted']:
            doc=model.item((19,ref['item']['local_id']));self.assertEqual(doc['child']['regalia_kind'],'1');self.assertEqual(doc['child']['stole_grade'],'4');self.assertEqual(len(doc['regalia_rules']),24)
            rule=doc['definition_rules'][0];self.assertEqual(rule['clock_kind'],'3' if ref['source_id']==85088 else '2');self.assertEqual(int(rule['duration_ms']),86400000 if ref['source_id']==85088 else 604800000)
        rows[0]['limitvalue0']=0;result=self.compiler(rows).compile();self.assertEqual(result['blocked'][0]['reasons'],['zero_duration_limit'])
    def test_effect_only_stole_uses_the_original_mse_instead_of_a_dummy_model(self):
        from convert_shop_content import SASH_CATALOG
        row=self.row(85136,28,5);row.update(value0=4,value1=0,value2=0,value3=0,value4=0,value5=0)
        entry=dict(vnum=85136,address='',model='item/wing/empty.gr2',effectPath='effect/stola/ridack_yellowsash.mse',effectBone='Bip01')
        p=self.project/SASH_CATALOG;p.parent.mkdir(parents=True,exist_ok=True);p.write_text(json.dumps(dict(entries=[entry])),encoding='utf-8')
        self.put('Assets/Metin2/Generated/Effects/Definitions/effect/stola/ridack_yellowsash.asset','d'*32)
        self.put('Assets/Metin2/Resources/Metin2EffectCatalog.asset','c'*32)
        (self.project/'Assets/Metin2/Resources/Metin2EffectCatalog.asset').write_text('  - key: effect/stola/ridack_yellowsash.mse\n',encoding='utf-8')
        result=self.compiler([row]).compile();self.assertEqual(len(result['accepted']),1);self.validate(result)
        doc=__import__('item_catalog').Catalog(result['packet']).item((19,result['accepted'][0]['item']['local_id']))
        self.assertEqual(doc['appearance']['model_asset'],'Metin2SashEffect/effect/stola/ridack_yellowsash')
        entry['effectBone']='wrong_bone';p.write_text(json.dumps(dict(entries=[entry])),encoding='utf-8');self.assertEqual(self.compiler([row]).compile()['blocked'][0]['reasons'],['invalid_stole_effect_attachment'])
    def test_source_random_main_book_keeps_only_existing_authored_main_skills(self):
        row=self.behavior_row(50300,17,0);compiler=self.compiler([row]);compiler.secondary_skills={n:dict(skill_id=n,type=1,max_level=20) for n in (1,2,16,121,122)}
        result=compiler.compile();self.assertEqual(len(result['accepted']),1);self.validate(result)
        doc=__import__('item_catalog').Catalog(result['packet']).item((9,result['accepted'][0]['item']['local_id']));self.assertEqual(doc['book']['target_mode'],'2');self.assertEqual([int(r['skill_id']) for r in doc['book_targets']],[1,2,16])
        self.assertEqual(result['accepted'][0]['source_factory_skill_pool'],[1,2,16]);self.assertEqual(len(result['accepted'][0]['source_missing_factory_skills']),45)
        self.assertEqual(self.compiler([row]).compile()['blocked'][0]['reasons'],['missing_main_skill_definitions'])
        other=self.behavior_row(55003,17,0);self.assertEqual(self.compiler([other]).compile()['blocked'][0]['reasons'],['skill_selected_on_instance_not_in_prototype'])
    def test_source_forget_book_shares_the_issued_main_skill_pool_and_refunds_main_points(self):
        row=self.behavior_row(70037,22,0);compiler=self.compiler([row]);compiler.secondary_skills={n:dict(skill_id=n,type=1,max_level=20) for n in (1,2,16)}
        result=compiler.compile();self.assertEqual(len(result['accepted']),1);self.validate(result)
        doc=__import__('item_catalog').Catalog(result['packet']).item((9,result['accepted'][0]['item']['local_id']));self.assertEqual(doc['book']['operation_kind'],'2');self.assertEqual(doc['book']['target_mode'],'2');self.assertEqual({t['refund_pool'] for t in doc['book_targets']},{'1'});self.assertEqual(len(doc['book_steps']),19);self.assertEqual(doc['book_steps'][0]['next_level'],'0')
    def test_non_bonus_book_aids_are_single_use_charges(self):
        rows=[]
        for identity,kind,duration in ((39008,513,536870911),(72310,513,536870911),(71094,512,0)):
            row=self.behavior_row(identity,3,8);row.update(value0=kind,value3=duration);rows.append(row)
        result=self.compiler(rows).compile();self.assertEqual(len(result['accepted']),3);self.validate(result);model=__import__('item_catalog').Catalog(result['packet'])
        for ref in result['accepted']:
            doc=model.item((5,ref['item']['local_id']));self.assertEqual(doc['book_aid']['maximum_charges'],'1');self.assertEqual(doc['book_aid']['charges_per_use'],'1');self.assertEqual(doc['book_aid']['aid_kind'],'1' if ref['source_id']==71094 else '2')
        self.assertIn('source_dispatch_repaired',next(x for x in result['accepted'] if x['source_id']==71094))
        rows[0]['value3']=60;result=self.compiler(rows).compile();self.assertEqual(result['blocked'][0]['reasons'],['finite_book_aid_lifetime_requires_expiry'])
    def test_costume_reset_uses_exact_three_source_distributions(self):
        result=self.compiler([self.behavior_row(70063,3,32)]).compile();self.assertEqual(len(result['accepted']),1);self.validate(result)
        doc=__import__('item_catalog').Catalog(result['packet']).item((20,result['accepted'][0]['item']['local_id']));self.assertEqual(doc['child']['operation_kind'],'3')
        distributions=[[int(next((x['chance_bp'] for x in doc['special_tiers'] if int(x['draw_no'])==draw and int(x['tier'])==tier),'0')) for tier in range(1,6)] for draw in range(3)]
        self.assertEqual(distributions,[[3000,4000,2000,800,200],[5000,4000,1000,0,0],[5000,4000,1000,0,0]])
    def test_seed_and_moon_potions_keep_both_pools_without_an_invented_cooldown(self):
        rows=[]
        for identity,sp in ((50085,0),(50086,500)):
            row=self.behavior_row(identity,3,11);row.update(value0=500,value1=sp);rows.append(row)
        result=self.compiler(rows).compile();self.assertEqual(len(result['accepted']),2);self.validate(result);model=__import__('item_catalog').Catalog(result['packet'])
        for ref in result['accepted']:
            d=model.item((5,ref['item']['local_id']));self.assertIsNone(d['recovery']['cooldown_profile_id']);self.assertEqual(d['effect']['hp_raw'],'500000');self.assertEqual(d['effect']['sp_raw'],'500000' if ref['source_id']==50086 else '0');self.assertTrue(ref['source_unused_seed_bottle_timestamp'])
    def test_normal_attack_defense_potions_are_distinct_from_unique_magic_resistance(self):
        rows=[]
        for identity,apply in ((50093,53),(50094,54),(50819,37)):
            row=self.behavior_row(identity,3,7);row.update(value0=apply,value1=30,value2=10);rows.append(row)
        result=self.compiler(rows).compile();self.assertEqual(len(result['accepted']),2);self.assertEqual(result['blocked'][0]['reasons'],['unsupported_ability_effect']);self.validate(result)
    def test_stone_draw_preserves_missing_prototype_outcomes_and_native_refinement(self):
        rows=[self.behavior_row(27999,3,9)]+[self.row(n,10,0) for n in (28000,28004,28008,28012)]
        for row in rows[1:]:row.update(value0=0,value1=0,value2=0,value3=0,value4=0,value5=0,wearflag=17,applytype0=5,applyvalue0=1)
        result=self.compiler(rows).compile();self.assertEqual(len(result['accepted']),5);self.validate(result);ref=next(x for x in result['accepted'] if x['source_id']==27999)
        d=__import__('item_catalog').Catalog(result['packet']).item((8,ref['item']['local_id']));self.assertEqual(len(d['loot']['entries']),14);self.assertEqual(sum(e['reward_kind']=='9' for e in d['loot']['entries']),10);self.assertEqual({e['weight'] for e in d['loot']['entries']},{'1'})
    def soul_rows(self):
        rows=[]
        for i in range(10):
            r=self.row(70500+i,35,i//5);r.update(value0=0,value1=i%5+1,value2=500 if i%5<2 else 1000,value3=12,value4=15,value5=25,limitvalue1=(60,60,120,180,180)[i%5],refined_vnum=70501+i if i%5<4 else 0);rows.append(r)
        return rows
    def test_soul_grades_preserve_two_channels_exact_charge_and_explicit_next_keys(self):
        rows=self.soul_rows();result=self.compiler(rows).compile();self.assertEqual(len(result['accepted']),10);self.validate(result)
        model=__import__('item_catalog').Catalog(result['packet']);refs={r['source_id']:r for r in result['accepted']}
        for source in rows:
            ref=refs[source['vnum']];doc=model.item((14,ref['item']['local_id']));self.assertEqual(doc['base']['max_stack'],'1');self.assertEqual(doc['child']['damage_channel'],str(source['subtype']+1));self.assertEqual(doc['soul_levels'][0]['charge_capacity'],str(source['limitvalue1']));self.assertEqual(doc['soul_levels'][0]['initial_charge'],'0');self.assertEqual(doc['soul_levels'][0]['initial_strikes'],str(source['value2']));self.assertEqual(ref['source_soul_grade'],source['value1'])
            self.assertEqual(doc['child']['next_local_id'],str(refs[source['refined_vnum']]['item']['local_id']) if source['refined_vnum'] else None)
    def test_soul_missing_grade_or_cross_channel_graph_rolls_back_all_partial_definitions(self):
        for mutate in (lambda r:r[0].update(refined_vnum=999999),lambda r:r[1].update(subtype=1),lambda r:r[1].update(value1=4),lambda r:r[4].update(refined_vnum=70500),lambda r:r[4].update(value3=9)):
            rows=self.soul_rows();mutate(rows);result=self.compiler(rows).compile();self.validate(result)
            self.assertIn(70500,{r['source_id'] for r in result['blocked']});self.assertEqual(len(result['accepted'])+len(result['blocked']),10)
    def test_random_costume_grants_compile_five_uniform_persisted_payload_choices(self):
        rows=[self.behavior_row(70402,3,39),self.behavior_row(70403,3,40)];result=self.compiler(rows).compile();self.assertEqual(len(result['accepted']),2);self.validate(result)
        model=__import__('item_catalog').Catalog(result['packet'])
        for ref in result['accepted']:
            d=model.item((20,ref['item']['local_id']));self.assertEqual(d['base']['max_stack'],'1');self.assertIsNone(d['child']['bonus_id']);self.assertEqual(d['child']['operation_kind'],'4');self.assertEqual(len(d['bonus_choices']),5);self.assertEqual({int(x['value_scaled']) for x in d['bonus_choices']},{500 if ref['source_id']==70402 else 1000})
    def test_horse_point_book_and_verified_paired_addon_dispatch(self):
        result=self.compiler([self.service_row(50060,130,50),self.service_row(70251)]).compile();self.assertEqual(len(result['accepted']),2);self.validate(result);model=__import__('item_catalog').Catalog(result['packet'])
        docs={r['source_id']:model.item((r['item']['category_id'],r['item']['local_id'])) for r in result['accepted']};self.assertEqual(docs[70251]['child']['operation_kind'],'6');self.assertEqual(docs[50060]['book']['operation_kind'],'3');self.assertEqual(docs[50060]['book_steps'][0]['success_chance_bp'],'5000');self.assertEqual(docs[50060]['book_steps'][0]['delay_on_failure'],'0')
        result=self.compiler([self.service_row(70252),self.service_row(50060,131,50)]).compile();self.assertEqual(len(result['blocked']),2)
    def test_rare_services_keep_separate_pools_and_highest_eligible_tier(self):
        rows=[self.service_row(n) for n in (30617,30618,30619,30620)]
        compiler=self.compiler(rows)
        compiler.tables.update({'player.item_attr_rare':[dict(prob=1)],'player.item_attr_rare_8':[dict(prob=30)]})
        result=compiler.compile();self.assertEqual(len(result['accepted']),4);self.validate(result)
        model=__import__('item_catalog').Catalog(result['packet'])
        for ref in result['accepted']:
            doc=model.item((ref['item']['category_id'],ref['item']['local_id']));service=doc['attribute_service'];n=ref['source_id']
            self.assertEqual(service['pool_kind'],'2' if n<30619 else '3')
            self.assertEqual(service['operation_kind'],'1' if n%2 else '2')
            self.assertEqual(service['maximum_count'],{30617:'1',30618:'2',30619:'0',30620:'1'}[n])
            self.assertEqual(doc['attribute_tiers'],[dict(tier='5',chance_bp='10000')])
            self.assertTrue(ref['source_rare_pool']['uniform_selection'])
    def test_rare_services_refuse_unknown_parameters_or_unequal_source_weights(self):
        for table in ([],[dict(prob=0)],[dict(prob=1),dict(prob=2)]):
            compiler=self.compiler([self.service_row(30617)]);compiler.tables['player.item_attr_rare']=table
            result=compiler.compile();self.assertEqual(result['blocked'][0]['reasons'],['rare_source_pool_requires_uniform_native_weights']);self.validate(result)
        compiler=self.compiler([self.service_row(30617,1)]);compiler.tables['player.item_attr_rare']=[dict(prob=1)]
        result=compiler.compile();self.assertEqual(result['blocked'][0]['reasons'],['unmapped_behavior_value'])
    def test_missing_refinement_icon_inherits_only_an_explicit_verified_family(self):
        base=self.row(100,2,3,0);child=self.row(101,2,3,1);base['refined_vnum']=101
        self.visuals[101]['iconPath']='';result=self.compiler([base,child]).compile();self.assertEqual(len(result['accepted']),2);self.validate(result)
        proof=next(v['icon'] for v in result['assets'].values() if v['icon'].get('refinement_icon_source'))
        self.assertEqual(proof['refinement_icon_source'],100);self.assertEqual([r['source_id'] for r in proof['refinement_icon_chain']],[101,100])
        child['name']='Different+1';result=self.compiler([base,child]).compile();self.assertEqual([r['source_id'] for r in result['blocked']],[101])
        child['name']='Test+1';other=self.row(110,2,3,0);other['refined_vnum']=101
        result=self.compiler([base,child,other]).compile();self.assertEqual([r['source_id'] for r in result['blocked']],[101]);self.validate(result)
    def test_inert_prototype_bonuses_are_audited_without_granting_them(self):
        row=self.row(69000,0,0);row.update(applytype0=51,applyvalue0=100)
        result=self.compiler([row]).compile();self.assertEqual(len(result['accepted']),1);self.validate(result)
        self.assertEqual(result['packet']['tables']['bonus_profile_entry'],[])
        self.assertEqual(result['accepted'][0]['inactive_source_fields']['applies'][0],(51,100))
    def test_secondary_books_preserve_source_dispatch_chance_and_level_rules(self):
        rows=[self.service_row(50304,20),self.service_row(50305,70),self.service_row(50306,100),self.service_row(50312,127,50),self.service_row(50600,123,50),self.service_row(50904,40,20,35),self.service_row(50061,131,50)]
        compiler=self.compiler(rows);compiler.secondary_skills={n:dict(skill_id=n,type=0,max_level=40) for n in (122,123,124,127,131)}
        result=compiler.compile();self.assertEqual(len(result['accepted']),7);self.validate(result)
        packet=result['packet'];model=__import__('item_catalog').Catalog(packet)
        docs={r['source_id']:model.item((r['item']['category_id'],r['item']['local_id'])) for r in result['accepted']}
        self.assertEqual([docs[n]['book_steps'][0]['success_chance_bp'] for n in (50304,50305,50306)],['2000','7000','10000'])
        self.assertEqual([docs[n]['book_steps'][0]['boosted_chance_bp'] for n in (50304,50305,50306)],['3000','10000','10000'])
        self.assertEqual([s['min_character_level'] for s in docs[50304]['book_steps']],['30','50'])
        self.assertEqual(docs[50312]['book_targets'][0]['forbidden_empire'],'2');self.assertEqual(docs[50600]['book_targets'][0]['skill_id'],'124')
        self.assertEqual(docs[50904]['book_targets'][0]['skill_id'],'123');self.assertEqual(len(docs[50904]['book_steps']),40)
        self.assertEqual(len(docs[50061]['book_steps']),10)
    def test_secondary_book_unknown_parameters_and_false_skill_mapping_are_blocked(self):
        for row in (self.service_row(50304,0),self.service_row(50312,126,50),self.service_row(50061,131,101)):
            compiler=self.compiler([row]);compiler.secondary_skills={n:dict(skill_id=n,type=0,max_level=40) for n in (122,127,131)};result=compiler.compile();self.assertEqual(len(result['blocked']),1);self.assertFalse(result['blocked'][0]['reasons'][0].startswith('missing_secondary'));self.validate(result)
        row=self.service_row(50304,20);row['value5']=1;compiler=self.compiler([row]);compiler.secondary_skills={122:dict(skill_id=122,type=0,max_level=2)};result=compiler.compile();self.assertEqual(result['blocked'][0]['reasons'],['unmapped_behavior_value'])
    def test_secondary_book_missing_or_lower_skill_cap_is_not_a_playable_book(self):
        row=self.service_row(50902,20,80,15);compiler=self.compiler([row]);result=compiler.compile();self.assertEqual(result['blocked'][0]['reasons'],['missing_secondary_skill_definition:123']);self.validate(result)
        compiler=self.compiler([row]);compiler.secondary_skills={123:dict(skill_id=123,type=0,max_level=20)};result=compiler.compile();self.assertEqual(result['blocked'][0]['reasons'],['secondary_book_exceeds_skill_definition:123']);self.validate(result)
    def test_automatic_recovery_uses_explicit_pool_capacity_and_special_bonus(self):
        rows=[self.service_row(72723,1000000),self.service_row(72726,10000000),self.service_row(72730,1000000),self.service_row(76004,10000)]
        result=self.compiler(rows).compile();self.assertEqual(len(result['accepted']),4);self.validate(result)
        model=__import__('item_catalog').Catalog(result['packet']);docs={r['source_id']:model.item((r['item']['category_id'],r['item']['local_id'])) for r in result['accepted']}
        self.assertEqual(docs[72723]['effect']['capacity_raw'],'1000000000');self.assertEqual(docs[72723]['base']['max_stack'],'1')
        self.assertEqual(docs[72726]['effect']['active_max_pool_bonus_bp'],'400');self.assertEqual(docs[72730]['effect']['pool_kind'],'2')
        self.assertEqual(docs[76004]['effect']['active_max_pool_bonus_bp'],'0');self.assertEqual(docs[76004]['recovery']['allowed_contexts'],'11')
        for row in (self.service_row(72723,0),self.service_row(72726,10,1)):
            result=self.compiler([row]).compile();self.assertEqual(len(result['blocked']),1);self.validate(result)
    def weapon_effect_mapping(self):
        entries=[]
        for identity in (77,78,79,80,81,82,579,582,83,583,231,731):
            entries.append(dict(id=identity,bone='PART_WEAPON',path='effect/weapon_'+str(identity)+'.mse',catalogRegistered=True))
            self.put('Assets/Metin2/Generated/Effects/Definitions/effect/weapon_'+str(identity)+'.asset',format(1000+identity,'032x'))
        mapping=dict(entries=entries,leftHandPairs=[dict(right=79,left=83,mountedOnly=False),dict(right=579,left=583,mountedOnly=False),dict(right=82,left=231,mountedOnly=True),dict(right=582,left=731,mountedOnly=True)])
        path=self.project/'Assets/Metin2/Resources/Metin2CostumeEffectCatalog.json';path.parent.mkdir(parents=True,exist_ok=True);path.write_text(json.dumps(mapping),encoding='utf-8');return path
    def test_weapon_effect_preserves_subtype_order_and_alternate_models(self):
        row=self.row(1,28,9);row.update(**{'value'+str(i):77+i for i in range(6)},limittype0=8,limitvalue0=604800)
        self.weapon_effect_mapping();result=self.compiler([row]).compile();self.assertEqual(len(result['accepted']),1)
        from full_catalog_weapon_effects import FIELDS
        rule=dict(zip(result['packet']['columns']['item_costume'],result['packet']['tables']['item_costume'][0]))
        self.assertEqual([int(rule[k]) for k in FIELDS],[77,79,80,78,81,82,579,582]);self.assertEqual(rule['costume_kind'],'7');self.assertEqual(rule['clock_kind'],'3');self.validate(result)
    def test_weapon_effect_missing_alternate_or_second_hand_is_not_faked(self):
        row=self.row(1,28,9);row.update(**{'value'+str(i):77+i for i in range(6)});path=self.weapon_effect_mapping()
        mapping=json.loads(path.read_text());mapping['entries']=[e for e in mapping['entries'] if e['id']!=579];path.write_text(json.dumps(mapping),encoding='utf-8')
        result=self.compiler([row]).compile();self.assertEqual(len(result['blocked']),1);self.assertEqual(result['packet']['tables']['item_costume'],[]);self.validate(result)
        self.weapon_effect_mapping();(self.project/'Assets/Metin2/Generated/Effects/Definitions/effect/weapon_731.asset').unlink()
        result=self.compiler([row]).compile();self.assertEqual(len(result['blocked']),1);self.validate(result)
    def test_source_weapon_alternate_policy_is_offline_and_per_level(self):
        from full_catalog_weapon_effects import alternate_weapon_effect
        dagger=self.row(1090,1,1);normal=self.row(1000,1,1)
        self.assertTrue(alternate_weapon_effect(dagger));self.assertFalse(alternate_weapon_effect(normal));self.assertFalse(alternate_weapon_effect(self.row(40099,28,4)))
        result=self.compiler([dagger,normal]).compile();self.assertEqual(len(result['accepted']),2);self.validate(result)
        rules=[dict(zip(result['packet']['columns']['item_definition_rules'],r)) for r in result['packet']['tables']['item_definition_rules']]
        flags={int(r['local_id']):r['weapon_effect_alternate'] for r in rules}
        self.assertEqual([flags[r['item']['local_id']] for r in result['accepted']],['0','1'])
    def body_effect_mapping(self):
        path=self.project/'Assets/Metin2/Resources/Metin2CostumeEffectCatalog.json';path.parent.mkdir(parents=True,exist_ok=True)
        path.write_text(json.dumps(dict(entries=[dict(id=794,bone='Bip01',path='effect/armor.mse',catalogRegistered=True)])),encoding='utf-8')
        self.put('Assets/Metin2/Generated/Effects/Definitions/effect/armor.asset',format(794,'032x'))
    def test_body_effect_costume_retains_clock_bonus_and_authored_effect(self):
        row=self.row(1,28,8);row.update(value0=794,value1=0,value2=0,value3=0,value4=0,value5=0,limittype0=7,limitvalue0=604800,applytype0=1,applyvalue0=1000)
        self.body_effect_mapping();result=self.compiler([row]).compile();self.assertEqual(len(result['accepted']),1)
        rule=dict(zip(result['packet']['columns']['item_costume'],result['packet']['tables']['item_costume'][0]))
        self.assertEqual((rule['costume_kind'],rule['body_effect_id'],rule['companion_form_id'],rule['clock_kind'],rule['duration_ms']),('6','794',None,'2','604800000'))
        self.assertEqual(result['packet']['tables']['item_costume_visual'],[]);self.validate(result)
    def test_authored_body_effect_icon_filename_can_contain_internal_spaces(self):
        row=self.row(1,28,8);row.update(value0=794,value1=0,value2=0,value3=0,value4=0,value5=0)
        icon='icon/item/armor effect purple moon.tga';self.visuals[1]['iconPath']=icon
        self.put('Assets/Metin2/Generated/ItemIcons/'+icon,format(1,'032x'));self.body_effect_mapping()
        result=self.compiler([row]).compile();self.assertEqual(len(result['accepted']),1);self.validate(result)
    def test_missing_body_effect_asset_rolls_back_prototype(self):
        row=self.row(1,28,8);row.update(value0=794,value1=0,value2=0,value3=0,value4=0,value5=0)
        self.body_effect_mapping();(self.project/'Assets/Metin2/Generated/Effects/Definitions/effect/armor.asset').unlink()
        result=self.compiler([row]).compile();self.assertEqual(len(result['blocked']),1);self.assertEqual(result['packet']['tables']['item_costume'],[]);self.validate(result)
    def test_body_effect_refuses_weapon_attachment_or_unmapped_values(self):
        row=self.row(1,28,8);row.update(value0=794,value1=1,value2=0,value3=0,value4=0,value5=0)
        self.body_effect_mapping();result=self.compiler([row]).compile();self.assertEqual(result['blocked'][0]['reasons'],['invalid_body_costume_effect'])
        row['value1']=0;path=self.project/'Assets/Metin2/Resources/Metin2CostumeEffectCatalog.json';mapping=json.loads(path.read_text());mapping['entries'][0]['bone']='PART_WEAPON';path.write_text(json.dumps(mapping),encoding='utf-8')
        result=self.compiler([row]).compile();self.assertEqual(result['blocked'][0]['reasons'],['missing_body_costume_effect_mapping:794']);self.validate(result)
    def test_companion_skins_are_worn_costumes_not_summon_items(self):
        rows=[]
        for i,sub in enumerate((6,7),1):
            r=self.row(i,28,sub);r.update(value0=34106 if sub==6 else 20244,value1=0,value2=0,value3=0,value4=0,value5=0,limittype0=8,limitvalue0=604800,applytype0=1,applyvalue0=1000)
            self.put('Assets/Metin2/AddressableContent/Races/Prefabs/'+str(r['value0'])+'.asset',format(100+i,'032x'));rows.append(r)
        result=self.compiler(rows).compile();self.assertEqual(result['summary']['converted_source_rows'],2)
        rules=[dict(zip(result['packet']['columns']['item_costume'],r)) for r in result['packet']['tables']['item_costume']]
        self.assertEqual([r['costume_kind'] for r in rules],['4','5'])
        self.assertTrue(all(r['companion_form_id'] and r['clock_kind']=='3' and r['duration_ms']=='604800000' for r in rules))
        self.assertEqual(result['packet']['tables']['item_companion'],[]);self.assertEqual(result['packet']['tables']['item_costume_visual'],[])
        self.assertEqual(len(result['packet']['tables']['companion_form']),2);self.validate(result)
    def test_missing_companion_skin_model_keeps_source_blocked(self):
        r=self.row(1,28,6);r.update(value0=34106,value1=0,value2=0,value3=0,value4=0,value5=0)
        result=self.compiler([r]).compile();self.assertEqual(len(result['blocked']),1)
        self.assertEqual(result['packet']['tables']['item_costume'],[]);self.assertEqual(result['packet']['tables']['companion_form'],[])
        self.validate(result)
    def test_out_of_range_reward_probability_is_not_silently_clamped(self):
        reward=self.row(1,28,6);reward.update(value0=34106,value1=0,value2=0,value3=0,value4=0,value5=0)
        self.put('Assets/Metin2/AddressableContent/Races/Prefabs/34106.asset',format(101,'032x'))
        box=self.row(2,23);group=dict(vnum=2,items=[dict(item='1',count=1,prob=100,rare=30101)])
        result=self.compiler([reward,box],groups=[group]).compile()
        self.assertEqual([r['source_id'] for r in result['accepted']],[1]);self.assertEqual(result['blocked'][0]['reasons'],['source_reward_rare_field_out_of_range'])
        self.assertEqual(result['packet']['tables']['loot_entry'],[]);self.validate(result)
    def test_incomplete_chain_preserves_exact_stats_and_level(self):
        row=self.row(level=9);row['refined_vnum']=99999
        result=self.compiler([row]).compile();self.assertEqual(result['summary']['converted_source_rows'],1)
        ref=result['accepted'][0];self.assertEqual(ref['refinement_level'],9)
        columns=result['packet']['columns']['item_weapon'];weapon=dict(zip(columns,result['packet']['tables']['item_weapon'][0]))
        self.assertEqual(int(weapon['physical_attack_base_raw']),44000)
        self.assertEqual(int(weapon['magical_attack_base_raw']),24000)
        rules=[dict(zip(result['packet']['columns']['item_definition_rules'],r)) for r in result['packet']['tables']['item_definition_rules']]
        self.assertEqual([int(r['refinement_level']) for r in rules if r['available']=='1'],[9])
        self.validate(result)
    def test_rollbacks_leave_no_orphan_key_or_locale(self):
        broken=self.row(1);self.visuals[1]['icon']['guid']='f'*32
        good=self.row(2)
        result=self.compiler([broken,good]).compile()
        self.assertEqual([r['source_id'] for r in result['blocked']],[1])
        self.assertEqual(result['accepted'][0]['item']['local_id'],1)
        self.assertEqual(len(result['localization']),1);self.validate(result)
    def test_every_source_row_is_accounted_for_and_never_fake_potion(self):
        gear=self.row(1);retired=self.row(2,29);unsupported=self.row(3,38)
        result=self.compiler([gear,retired,unsupported]).compile()
        self.assertEqual([len(result[k]) for k in ('accepted','retired','blocked')],[1,1,1])
        self.assertFalse(result['release_ready']);self.assertEqual(result['packet']['tables']['item_consumable'],[])
        self.validate(result)
    def test_extra_bonuses_and_restrictions_are_retained(self):
        row=self.row();row['antiflag']=131072|64;row['flag']=256
        extra=dict(vnum=1,rarity=7,bonus_type0=5,bonus_value0=12,bonus_type1=1,bonus_value1=250)
        result=self.compiler([row],[extra]).compile();self.validate(result)
        rule=dict(zip(result['packet']['columns']['item_definition_rules'],result['packet']['tables']['item_definition_rules'][0]))
        self.assertEqual([rule[k] for k in ('rarity','allow_account_storage','allow_acquire','allow_unequip')],['7','0','0','0'])
        entries=result['packet']['tables']['bonus_profile_entry'];self.assertTrue(any(r[1:] == ['29','12'] for r in entries));self.assertTrue(any(r[1:] == ['35','250000'] for r in entries))
    def test_reserve_potion_native_profile(self):
        row=self.row(kind=3);row.update(value0=300,value1=0,value2=0,value3=0,value4=0,value5=0)
        result=self.compiler([row]).compile();self.validate(result)
        self.assertEqual(len(result['packet']['tables']['reserve_recovery_profile']),1)
    def test_duplicate_source_rejected(self):
        row=self.row()
        with self.assertRaisesRegex(ValueError,'duplicate_source_prototype'):self.compiler([row,row])
    def test_long_talisman_curve_is_one_native_family(self):
        rows=[self.row(1000+i,kind=2,sub=7,level=i) for i in range(201)]
        for i,row in enumerate(rows):
            row.update(refined_vnum=1001+i if i<200 else 0,applytype0=108,applyvalue0=i+1)
            self.visuals[row['vnum']]=self.visuals[1000]
        result=self.compiler(list(reversed(rows))).compile();self.validate(result)
        self.assertEqual(len(result['packet']['tables']['item_base']),1)
        self.assertEqual(len(result['packet']['tables']['refinement_level']),201)
        self.assertEqual(len(result['accepted']),201)
        self.assertEqual({r['item']['local_id'] for r in result['accepted']},{1})
        self.assertEqual({r['refinement_level'] for r in result['accepted']},set(range(201)))
    def test_missing_box_group_is_explicit_blocker(self):
        row=self.row(kind=23)
        result=self.compiler([row]).compile()
        self.assertEqual(result['blocked'][0]['reasons'],['missing_source_container_reward_group'])
        self.assertEqual(result['packet']['tables']['item_container'],[])
    def test_definition_only_input_no_player_restore(self):
        self.assertNotIn('player.item',self.compiler([self.row()]).tables)
    def test_bad_source_bonus_is_blocked_without_clamping(self):
        row=self.row();row.update(applytype0=3,applyvalue0=999999)
        result=self.compiler([row]).compile()
        self.assertEqual(result['accepted'],[])
        self.assertEqual(result['blocked'][0]['reasons'],['source_bonus_out_of_native_bounds:31'])
    def test_zero_wear_clock_uses_source_ten_hour_default(self):
        row=self.row();row.update(limittype0=9,limitvalue0=0)
        result=self.compiler([row]).compile();self.validate(result)
        rule=dict(zip(result['packet']['columns']['item_definition_rules'],result['packet']['tables']['item_definition_rules'][0]))
        self.assertEqual((rule['clock_kind'],rule['duration_ms']),('4','36000000'))
    def test_translation_cannot_override_server_refinement_level(self):
        row=self.row(level=9);row['locale_name_hu']='Hibás fordítás +2'
        result=self.compiler([row]).compile();self.validate(result)
        self.assertEqual(result['accepted'][0]['refinement_level'],9)
        self.assertIn('locale_name_hu',result['accepted'][0]['source_level_issues'])
    def test_nested_boxes_resolve_after_all_independent_items(self):
        rows=[self.row(1000),self.row(1001,23),self.row(1002,23)]
        group=lambda n,reward:dict(vnum=n,items=[dict(item=str(reward),count=1,prob=100,rare=0)])
        result=self.compiler(rows,groups=[group(1001,1002),group(1002,1000)]).compile();self.validate(result)
        self.assertEqual(len(result['accepted']),3);self.assertEqual(result['blocked'],[])
    def behavior_row(self,n,kind,sub,**values):
        row=self.row(n,kind,sub)
        for i in range(6):row['value'+str(i)]=0
        row.update(values);return row
    def test_forge_scrolls_keep_exact_level_and_failure_rules(self):
        rows=[self.behavior_row(70000+i,3,2,value0=selector,value1=parameter) for i,(selector,parameter) in enumerate([(1,0),(2,0),(5,5)])]
        result=self.compiler(rows).compile();self.validate(result)
        data=[dict(zip(result['packet']['columns']['item_forge_aid'],r)) for r in result['packet']['tables']['item_forge_aid']]
        self.assertEqual(data[0]['failure_kind'],'2')
        self.assertEqual((data[2]['minimum_level'],data[2]['maximum_level']),('5','5'))
        self.assertEqual(len(result['packet']['tables']['forge_aid_level']),256)
    def test_soul_scroll_is_stackable_and_has_no_core_state(self):
        row=self.behavior_row(70603,3,2,value0=9,value1=25,flag=4)
        result=self.compiler([row]).compile();self.validate(result)
        base=dict(zip(result['packet']['columns']['item_base'],result['packet']['tables']['item_base'][0]))
        soul=dict(zip(result['packet']['columns']['item_soul'],result['packet']['tables']['item_soul'][0]))
        self.assertEqual(base['max_stack'],'50000');self.assertEqual((soul['soul_kind'],soul['profile_id'],soul['refine_chance_bp']),('2',None,'2500'))
    def test_rune_uses_real_build_table_and_recharge_clock(self):
        row=self.behavior_row(88970,28,10,value0=86400,limittype0=9,limitvalue0=56160)
        result=self.compiler([row]).compile();self.validate(result)
        rune=dict(zip(result['packet']['columns']['item_regalia'],result['packet']['tables']['item_regalia'][0]))
        self.assertEqual((rune['rune_slot'],rune['capacity_ms'],rune['initial_ms']),('1','86400000','56160000'))
        self.assertEqual(len(result['packet']['tables']['regalia_bonus_rule']),14)
        rule=dict(zip(result['packet']['columns']['item_definition_rules'],result['packet']['tables']['item_definition_rules'][0]))
        self.assertEqual(rule['clock_kind'],'1')
        self.assertIn('common/rune_length.h',result['rule_inputs'])
    def test_rune_charge_bottle_keeps_partial_charge_payload(self):
        row=self.behavior_row(88978,3,46,value0=30,flag=4)
        result=self.compiler([row]).compile();self.validate(result)
        tool=dict(zip(result['packet']['columns']['item_bonus_tool'],result['packet']['tables']['item_bonus_tool'][0]))
        self.assertEqual((tool['operation_kind'],tool['parameter']),('2','3000'))
        base=dict(zip(result['packet']['columns']['item_base'],result['packet']['tables']['item_base'][0]));self.assertEqual(base['max_stack'],'1')
    def test_pendant_binding_matches_database_byte_eligibility(self):
        pendant=self.behavior_row(1000,2,7)
        tool=self.behavior_row(86050,3,19)
        compiler=self.compiler([pendant,tool]);compiler.tables['player.item_attr.effective']=[dict(apply='MAX_HP',apply_index=1,prob=35,talisman=3000,**{'lv'+str(i):i*100 for i in range(1,6)})]
        result=compiler.compile();self.validate(result)
        self.assertEqual(len(result['packet']['tables']['attribute_value']),5)
        profile=dict(zip(result['packet']['columns']['attribute_profile'],result['packet']['tables']['attribute_profile'][0]));self.assertEqual(profile['allow_lock'],'0')
        service=dict(zip(result['packet']['columns']['attribute_service_profile'],result['packet']['tables']['attribute_service_profile'][0]));self.assertEqual((service['target_mask'],service['maximum_count']),('16','3'))
    def test_fifth_bonus_probabilities_do_not_use_default_potion(self):
        rows=[self.behavior_row(70024,3,22),self.behavior_row(86052,3,22)]
        result=self.compiler(rows).compile();self.validate(result)
        services=[dict(zip(result['packet']['columns']['attribute_service_profile'],r)) for r in result['packet']['tables']['attribute_service_profile']]
        self.assertEqual([(r['success_bp'],r['minimum_count'],r['consume_on_failure']) for r in services],[('3000','4','1'),('7500','4','1')])
    def test_all_ordinary_switchers_preserve_source_admission(self):
        ids=(39028,71151,76014,76023,86060,88959)
        rows=[self.behavior_row(n,3,18,antiflag=4096 if n in (71151,76023) else 0) for n in ids]
        result=self.compiler(rows).compile();self.validate(result)
        services={x['profile_id']:x for x in (dict(zip(result['packet']['columns']['attribute_service_profile'],r)) for r in result['packet']['tables']['attribute_service_profile'])}
        children={(x['category_id'],x['local_id']):x for x in (dict(zip(result['packet']['columns']['item_consumable'],r)) for r in result['packet']['tables']['item_consumable'])}
        for ref in result['accepted']:
            k=ref['item'];service=services[children[str(k['category_id']),str(k['local_id'])]['attribute_service_profile_id']]
            self.assertEqual(service['target_reroll_group'],'2' if ref['source_id']==86060 else '1')
            self.assertEqual(service['maximum_target_level'],'30' if ref['source_id'] in (71151,76023) else '0')
            if ref['source_id']==76014:
                chances=[x[-1] for x in result['packet']['tables']['attribute_tier_chance'] if x[0]==service['profile_id']]
                self.assertEqual(chances,['1000','5000','3900','100'])
        self.assertEqual(len(result['accepted']),6)
    def test_zodiac_policy_has_exact_source_boundaries(self):
        from full_catalog_behaviors import source_reroll_group
        self.assertEqual([source_reroll_group(n) for n in (299,300,319,320,328,329,330,19289,19290,19312,19313)],[1,2,2,1,1,2,1,1,2,2,1])
    def test_addon_eligibility_is_retained_without_random_probability(self):
        row=self.row();row['addon_type']=-1
        result=self.compiler([row]).compile();self.validate(result)
        candidates=[dict(zip(result['packet']['columns']['attribute_candidate'],r)) for r in result['packet']['tables']['attribute_candidate']]
        self.assertEqual({(r['bonus_id'],r['weight']) for r in candidates},{('99','0'),('100','0')})
    def test_inactive_save_antiflag_does_not_disable_normal_persistence(self):
        row=self.row();row['antiflag']=4096
        result=self.compiler([row]).compile();self.validate(result)
        self.assertEqual(result['accepted'][0]['inactive_source_antiflags'],['save_bit_unused_by_source_persistence'])
        self.assertIn('GameServer/item/item_manager.cpp',result['rule_inputs'])
    def test_source_ore_compatibility_keeps_early_overrides_and_permanence(self):
        from full_catalog_accessories import source_compatible
        target=dict(vnum=14220,type=2,subtype=3)
        self.assertTrue(source_compatible(50634,target));self.assertFalse(source_compatible(50623,target))
        self.assertTrue(source_compatible(50684,target,True));self.assertFalse(source_compatible(50684,target))
        self.assertFalse(source_compatible(50634,dict(target,vnum=14234)))
        self.assertTrue(source_compatible(50625,dict(target,vnum=16210,subtype=5)))
        self.assertFalse(source_compatible(50633,dict(target,vnum=16210,subtype=5)))
        self.assertFalse(source_compatible(50634,dict(target,type=33)))
    def test_ore_materials_bind_real_native_accessory_services(self):
        rows=[self.behavior_row(14220,2,3),self.behavior_row(50621,3,20),self.behavior_row(50623,3,21),self.behavior_row(50634,3,21),self.behavior_row(50684,3,21,value0=1)]
        result=self.compiler(rows).compile();self.validate(result)
        mapping={x['source_id']:(str(x['item']['category_id']),str(x['item']['local_id'])) for x in result['accepted']}
        rows=result['packet']['tables']['accessory_socket_material']
        self.assertEqual({(r[1],tuple(r[2:])) for r in rows},{('1',mapping[50621]),('2',mapping[50634]),('3',mapping[50684])})
        grades=[dict(zip(result['packet']['columns']['accessory_socket_grade'],r)) for r in result['packet']['tables']['accessory_socket_grade'] if r[0]==rows[0][0]]
        self.assertEqual([r['duration_ms'] for r in grades],['0','86400000','43200000','21600000'])
        self.assertEqual([r['unlock_chance_bp'] for r in grades],['10000','10000','10000','0'])
        self.assertEqual(result['accessory_services']['new_accessory_bindings'],1)
    def tool_row(self,identity,kind=13,**changes):
        row=self.behavior_row(identity,kind,0,**changes)
        name='fishing_rod' if kind==13 else 'digger_1';guid='7'*32
        self.visuals[identity].update(modelPath='item/etc/'+name+'.gr2',model=dict(guid=guid,file_id='1'))
        self.put('Assets/Metin2/Converted/item/etc/'+name+'.fbx',guid)
        wrapper='Assets/Metin2/Resources/Metin2Equipment/typed_tool_'+name+'.asset';self.put(wrapper,'8'*32)
        (self.project/wrapper).write_text('model_guid: '+guid,encoding='utf-8')
        return row
    def test_recovered_professional_rod_requires_its_exact_prefab_and_wrapper(self):
        row=self.tool_row(27210,refined_vnum=27210,value0=20,value1=10,value2=25,value3=39,value4=27210)
        guid='a'*32;self.visuals[27210].update(modelPath='item/etc/canna_2.gr2',model=dict(guid=guid,file_id='1'))
        self.put('Assets/Metin2/Generated/Recovered/Tools/canna_2.prefab',guid)
        wrapper='Assets/Metin2/Resources/Metin2Equipment/typed_tool_canna_2.asset';self.put(wrapper,'b'*32);(self.project/wrapper).write_text('model_guid: '+guid,encoding='utf-8')
        result=self.compiler([row]).compile();self.validate(result);self.assertEqual(len(result['accepted']),1)
        ref=result['accepted'][0];level=dict(zip(result['packet']['columns']['tool_level'],result['packet']['tables']['tool_level'][0]));self.assertEqual(level['success_local_id'],str(ref['item']['local_id']));self.assertEqual(level['refine_chance_bp'],'3900')
        (self.project/wrapper).write_text('wrong_model',encoding='utf-8');self.assertEqual(self.compiler([row]).compile()['blocked'][0]['reasons'],['tool_wrapper_model_mismatch'])
    def test_tool_graph_keeps_distinct_native_keys_and_explicit_failure(self):
        first=self.tool_row(27400,refined_vnum=27410,value0=10,value1=5,value2=20,value3=50,value4=27400)
        final=self.tool_row(27410,value0=20,value1=10,value2=100,value3=50,value4=27400)
        result=self.compiler([first,final]).compile();self.validate(result)
        refs={x['source_id']:x['item'] for x in result['accepted']};self.assertEqual(len(refs),2);self.assertNotEqual(refs[27400],refs[27410])
        levels=[dict(zip(result['packet']['columns']['tool_level'],r)) for r in result['packet']['tables']['tool_level']]
        active=next(x for x in levels if x['success_local_id']);self.assertEqual(active['success_local_id'],str(refs[27410]['local_id']));self.assertEqual(active['failure_local_id'],str(refs[27400]['local_id']))
        self.assertEqual(active['practice_denominator'],'5');self.assertEqual(active['refine_chance_bp'],'5000')
        terminal=next(x for x in levels if not x['success_local_id']);self.assertEqual(terminal['required_points'],'0')
        self.assertTrue(all(r[result['packet']['columns']['item_tool'].index('all_water_pools')]=='1' and r[-1]=='0' for r in result['packet']['tables']['item_tool']))
    def test_tool_graph_asset_failure_rolls_back_the_complete_chain(self):
        first=self.tool_row(27400,refined_vnum=27410,value0=10,value1=5,value2=20,value3=100)
        final=self.tool_row(27410,value0=20);self.visuals[27410]['model']['guid']='f'*32
        result=self.compiler([first,final]).compile();self.assertEqual(len(result['accepted']),0);self.assertEqual(len(result['blocked']),2)
        self.assertEqual(result['packet']['tables']['item_tool'],[]);self.validate(result)
    def test_pick_uses_source_grade_bonus_and_exact_model(self):
        row=self.tool_row(29101,24,value0=30);row['name']='Pick+2'
        result=self.compiler([row]).compile();self.validate(result)
        level=dict(zip(result['packet']['columns']['tool_level'],result['packet']['tables']['tool_level'][0]))
        self.assertEqual((level['display_grade'],level['gather_bonus_bp']),('2','800'))
        self.assertEqual(result['packet']['tables']['item_tool'][0][-1],'0')
    def test_pet_carrier_is_a_stateful_native_supply(self):
        row=self.behavior_row(55002,37,0)
        result=self.compiler([row]).compile();self.validate(result)
        self.assertEqual(result['accepted'][0]['semantics'],'growth_pet_carrier')
        profile=dict(zip(result['packet']['columns']['pet_supply_profile'],result['packet']['tables']['pet_supply_profile'][0]))
        self.assertEqual(profile['supply_kind'],'7')
    def test_polymorph_orb_preserves_instance_bound_form(self):
        row=self.behavior_row(70104,19,0)
        result=self.compiler([row]).compile();self.validate(result)
        self.assertTrue(result['accepted'][0]['requires_instance_form'])
        self.assertEqual(len(result['packet']['tables']['polymorph_skill_rule']),41)
    def test_leadership_books_keep_disjoint_skill_level_ranges(self):
        rows=[self.behavior_row(50301+i,17,10,value0=lo,value1=hi) for i,(lo,hi) in enumerate(((0,20),(20,30),(30,40)))]
        result=self.compiler(rows).compile();self.validate(result)
        self.assertEqual(len(result['packet']['tables']['skill_book_step']),40)
        self.assertTrue(all(r[1]=='121' for r in result['packet']['tables']['skill_book_target']))
    def test_polymorph_learning_book_repairs_mistyped_legacy_dispatch(self):
        row=self.behavior_row(50314,17,10,value0=0,value1=20,value2=80,value3=15)
        result=self.compiler([row]).compile();self.validate(result)
        self.assertIn('source_book_dispatch_repaired',result['accepted'][0])
        step=dict(zip(result['packet']['columns']['skill_book_step'],result['packet']['tables']['skill_book_step'][0]))
        self.assertEqual((step['min_character_level'],step['success_chance_bp'],step['boosted_chance_bp']),('15','8000','10000'))
        target=dict(zip(result['packet']['columns']['skill_book_target'],result['packet']['tables']['skill_book_target'][0]));self.assertEqual(target['skill_id'],'129')
    def test_fish_chain_uses_explicit_grill_target_and_exact_31_percent(self):
        rows=[self.behavior_row(27803,12,0,value0=3),self.behavior_row(27833,12,1,value0=3),self.behavior_row(27863,3,49,value0=1800,applytype0=4,applyvalue0=5),self.behavior_row(27799,5,0)]
        result=self.compiler(rows,fishing=[{},{},{},dict(vnum=27803,dead_vnum=27833,grill_vnum=27863)]).compile();self.validate(result)
        self.assertEqual(result['blocked'],[]);self.assertEqual(len(result['accepted']),4)
        loot=[dict(zip(result['packet']['columns']['loot_entry'],r)) for r in result['packet']['tables']['loot_entry']]
        self.assertEqual([r['weight'] for r in loot],['31','69'])
        refs={r['source_id']:r for r in result['accepted']}
        dead=next(dict(zip(result['packet']['columns']['item_fishing'],r)) for r in result['packet']['tables']['item_fishing'] if r[1]==str(refs[27833]['item']['local_id']))
        self.assertEqual((dead['grill_category_id'],dead['grill_local_id']),('5',str(refs[27863]['item']['local_id'])))
        self.assertEqual(refs[27803]['source_fishing_index'],3)
    def test_conflicting_source_fish_index_is_rejected(self):
        result=self.compiler([self.behavior_row(27803,12,0,value0=3)],fishing=[dict(vnum=27803)]).compile()
        self.assertEqual(result['blocked'][0]['reasons'],['source_fish_index_disagrees_with_explicit_table']);self.validate(result)
    def test_growth_pet_life_services_preserve_finite_cap_and_permanence(self):
        rows=[self.behavior_row(86071,3,43,value0=2),self.behavior_row(86074,3,43)]
        result=self.compiler(rows).compile();self.validate(result)
        self.assertEqual(len(result['accepted']),2)
        supplies=[dict(zip(result['packet']['columns']['pet_supply_profile'],r)) for r in result['packet']['tables']['pet_supply_profile']]
        self.assertEqual([r['supply_kind'] for r in supplies],['4','5'])
        extension=dict(zip(result['packet']['columns']['pet_extension_rule'],result['packet']['tables']['pet_extension_rule'][0]))
        self.assertEqual((extension['added_ms'],extension['maximum_capacity_ms']),('172800000','31535940000'))
    def test_growth_pet_enchant_uses_source_probability_and_trait_caps(self):
        result=self.compiler([self.behavior_row(86075,3,44)]).compile();self.validate(result)
        rule=dict(zip(result['packet']['columns']['pet_enchant_rule'],result['packet']['tables']['pet_enchant_rule'][0]))
        self.assertEqual([rule[k] for k in ('success_bp','step','hp_limit','monster_limit','pve_limit')],['3000','5','150','100','100'])
    def test_unknown_zero_day_pet_extension_is_not_made_permanent(self):
        result=self.compiler([self.behavior_row(9999,3,43)]).compile()
        self.assertEqual(result['blocked'][0]['reasons'],['unrecognized_permanent_pet_extension']);self.validate(result)
    def test_container_reward_preserves_exact_source_refinement(self):
        gear=self.row(1000,level=9);box=self.behavior_row(1001,23,0)
        result=self.compiler([gear,box],groups=[dict(vnum=1001,items=[dict(item='1000',count=1,prob=100,rare=0)])]).compile();self.validate(result)
        self.assertEqual(len(result['accepted']),2)
        reward=dict(zip(result['packet']['columns']['loot_entry'],result['packet']['tables']['loot_entry'][0]))
        self.assertEqual(reward['refinement_level'],'9')
    def test_gold_bar_uses_redemption_price_instead_of_face_name_or_shop_gold(self):
        row=self.behavior_row(80005,3,10,shop_buy_price=500000,gold=999,flag=4)
        result=self.compiler([row]).compile();self.validate(result)
        token=dict(zip(result['packet']['columns']['item_currency_token'],result['packet']['tables']['item_currency_token'][0]))
        self.assertEqual((token['wallet_currency'],token['amount_per_unit']),('1','500000'))
    def test_zero_value_gold_bar_never_becomes_a_placeholder_token(self):
        result=self.compiler([self.behavior_row(80005,3,10)]).compile();self.validate(result)
        self.assertEqual(result['blocked'][0]['reasons'],['currency_token_has_no_redemption_value'])
    def test_dragon_coin_voucher_targets_shared_account_wallet(self):
        row=self.behavior_row(89003,3,10,shop_buy_price=3,flag=4)
        result=self.compiler([row]).compile();self.validate(result)
        token=dict(zip(result['packet']['columns']['item_currency_token'],result['packet']['tables']['item_currency_token'][0]))
        self.assertEqual((token['wallet_currency'],token['amount_per_unit']),('2','3'))
    def test_existing_unity_quest_icon_uses_exact_resource_reference(self):
        row=self.behavior_row(11010,5,0);visual=self.visuals[11010];visual['iconPath']='ui/game/quest/questicon/level_07.tga'
        self.put('Assets/Metin2/Resources/Metin2/QuestIcons/level_07.png',visual['icon']['guid'])
        result=self.compiler([row]).compile();self.validate(result)
        self.assertEqual(len(result['accepted']),1)
        self.assertTrue(next(iter(result['assets'].values()))['icon']['path'].endswith('QuestIcons/level_07.png'))

    def test_alignment_services_keep_source_caps_and_royal_lower_bound(self):
        rows=[self.service_row(n,a,8 if n in (71107,39032) else 0) for n,a in ((70102,5000),(70100,50000),(71107,20000),(39032,20000),(72100,100000),(72101,0))]
        result=self.compiler(rows).compile();self.assertEqual(len(result['accepted']),6);self.validate(result)
        from item_catalog import Catalog
        model=Catalog(result['packet']);rules={r['source_id']:model.item((5,r['item']['local_id']))['character_service'] for r in result['accepted']}
        self.assertEqual((rules[70102]['amount'],rules[70102]['maximum_value']),('5000','2500000'))
        self.assertEqual((rules[72101]['amount'],rules[72101]['minimum_value'],rules[72101]['maximum_value']),('300000','25000000','50000000'))
    def test_alignment_service_refuses_unverified_source_balance(self):
        row=self.service_row(70102,5001);result=self.compiler([row]).compile();self.assertEqual(result['blocked'][0]['reasons'],['alignment_source_values_changed']);self.validate(result)

    def test_source_gain_socket_field_is_an_explicit_count(self):
        rows=[self.row(1000+i,level=0) for i in range(3)]
        for n,row in enumerate(rows,1):row['socket_pct']=n
        result=self.compiler(rows).compile();self.assertEqual(len(result['accepted']),3);self.validate(result)
        rules=[dict(zip(result['packet']['columns']['item_definition_rules'],r)) for r in result['packet']['tables']['item_definition_rules']]
        self.assertEqual(sorted(int(r['birth_open_stone_sockets']) for r in rules),[1,2,3])
    def test_source_71109_returns_the_last_stone_instead_of_clearing_broken_stones(self):
        result=self.compiler([self.service_row(71109)]).compile();self.assertEqual(len(result['accepted']),1);self.validate(result)
        rule=dict(zip(result['packet']['columns']['socket_service_profile'],result['packet']['tables']['socket_service_profile'][0]));self.assertEqual((rule['service_kind'],rule['units_per_use']),('3','1'))

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--helper',required=True,type=Path)
    args,rest=parser.parse_known_args();HELPER=args.helper;unittest.main(argv=[sys.argv[0]]+rest)
