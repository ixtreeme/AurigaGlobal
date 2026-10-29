"""Item catalog editor model and native transport.

Offline cases run the real CatalogEditorTool validator without a database.
With --mariadb-bin the commit path also runs against a newly spawned private
MariaDB; no existing database is touched.
"""
import argparse
import copy
from pathlib import Path
import sys
import unittest
import uuid

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'SRC/Tools/ServerManager'))
sys.path.insert(0, str(ROOT / 'db/tools'))
import backend as api
import item_catalog as catalog

TOOL = None
MARIADB = None
WORK = None


def saved(packet, tables, new_key=None):
    """The packet a successful save would read back (sequence included)."""
    result = copy.deepcopy(packet)
    result['tables'] = tables
    if new_key:
        result['sequences'][str(new_key[0])] = str(int(new_key[1]) + 1)
    return result


def key_of(document):
    return int(document['base']['category_id']), int(document['base']['local_id'])


def create(packet, category, change=None):
    document = catalog.Catalog(packet).new(category)
    if change: change(document)
    tables = catalog.Catalog(packet).apply(document)
    return saved(packet, tables, key_of(document)), key_of(document)


def ordered(tables):
    return {name: sorted(rows, key=lambda row: [cell or '' for cell in row]) for name, rows in tables.items()}


def rows(packet, table):
    columns = packet['columns'][table]
    return [dict(zip(columns, row)) for row in packet['tables'][table]]


def automatic(document):
    document['base']['max_stack'] = '1'
    document['recovery'].update(delivery_kind='4', units_per_use='1', allowed_contexts='9', arena_quota_cost='0')
    document['effect'] = dict(delivery_kind='4', pool_kind='1', supply_kind='1', capacity_raw='5000000',
                              tick_interval_ms='1000', max_pool_per_tick_bp='700', active_max_pool_bonus_bp='400')


def toggle(document):
    document['base']['max_stack'] = '1'
    document.update(catalog.Catalog.toggle_defaults(document['base']['local_id']))
    document.update(recovery=None, effect=None, timed=None)


def egg(document, target):
    document['child'].update(container_kind='4',loot_profile_id=None,lock_id='0',hatch_profile_id=None)
    document['hatch']=dict(catalog.Catalog.hatch_defaults(),target_category_id=str(target[0]),target_local_id=str(target[1]))
    document['loot']=None


def food_rule(service='1'):
    return dict(service_kind=service,food_kind='1' if service=='1' else '2',consumption_kind='1',units='1',
                lifetime_fraction_bp='300' if service=='1' else '0',dead_recovery_bp='5000' if service=='2' else '0',experience_price_divisor='0')


def growth(document, maximum=30):
    document['child'].update(companion_kind='3',riding_form_id=None,growth_profile_id=None,clock_kind='4',duration_ms='259200000')
    document['refinement'].update(profile_kind='1',max_level='0');document['levels']=document['levels'][:1]
    document['levels'][0]['bonuses']=[]
    document.update(growth=dict(maximum_level=str(maximum),experience_policy='1'),
        growth_levels=[dict(level=str(i),mob_experience=str(0 if i==maximum else i*1000),item_experience=str(0 if i==maximum else i*100)) for i in range(1,maximum+1)],
        growth_stages=[dict(stage='0',maximum_level=str(maximum),form_id=None)])


def socket_service(document,kind=1):
    document.update(socket_service=dict(service_kind=str(kind),units_per_use='1'),recovery=None,effect=None,timed=None,toggle=None,cooldown=None)


def attributes(document):
    document.update(catalog.Catalog.attribute_defaults());document['attribute_profile']=dict(allow_lock='0' if document['base']['category_id']=='4' else '1')


def attribute_service(document,operation='1',pool='1'):
    document.update(catalog.Catalog.attribute_service_defaults());document.update(socket_service=None,recovery=None,effect=None,timed=None,toggle=None,cooldown=None)
    head=document['attribute_service'];head.update(operation_kind=operation,pool_kind=pool,maximum_count='5' if pool=='1' else '2' if pool=='2' else '1')
    if operation!='1':head.update(add_count='0')
    if operation in ('3','4','5'):document['attribute_tiers']=[]


class OfflineModelTests(unittest.TestCase):
    def test_instance_attribute_profiles_services_and_copy(self):
        packet,key=create(catalog.blank(TOOL),1,attributes);self.check(packet);original=catalog.Catalog(packet).item(key)
        clone=catalog.Catalog(packet).duplicate(key);clone['attribute_values'][0]['value_scaled']='3500';other=key_of(clone)
        packet=saved(packet,catalog.Catalog(packet).apply(clone),other);self.check(packet)
        self.assertEqual(catalog.Catalog(packet).item(key),original)
        for operation,pool in [('1','1'),('2','1'),('3','1'),('4','1'),('5','1'),('1','2'),('2','3')]:
            packet,service=create(packet,5,lambda d:attribute_service(d,operation,pool));self.check(packet)
            self.assertEqual(catalog.Catalog(packet).item(service)['attribute_service']['operation_kind'],operation)
        changed=catalog.Catalog(packet).item(other);changed['attribute_profile']=None;packet=saved(packet,catalog.Catalog(packet).apply(changed,other));self.check(packet)
        self.assertEqual(catalog.Catalog(packet).item(key),original)
        self.assertEqual(len(rows(packet,'attribute_profile')),1)
        bad=catalog.Catalog(packet).item(key);bad['attribute_values'][0]['value_scaled']='0'
        with self.assertRaises(api.EditorError):catalog.validate(packet,bad,key,TOOL)
        bad=catalog.Catalog(packet).new(5);attribute_service(bad);bad['attribute_tiers']=[dict(tier='1',chance_bp='10001')]
        with self.assertRaises(api.EditorError):catalog.validate(packet,bad,None,TOOL)

    def test_socket_services_copy_and_effect_switches(self):
        packet=catalog.blank(TOOL)
        for kind in range(1,4):
            packet,key=create(packet,5,lambda d:socket_service(d,kind))
            self.check(packet);original=catalog.Catalog(packet).item(key)
            self.assertEqual(original['socket_service']['service_kind'],str(kind))
            clone=catalog.Catalog(packet).duplicate(key);clone['socket_service']['units_per_use']='2'
            second=key_of(clone);packet=saved(packet,catalog.Catalog(packet).apply(clone),second);self.check(packet)
            self.assertEqual(catalog.Catalog(packet).item(key),original)
            changed=catalog.Catalog(packet).item(second);fresh=catalog.Catalog(packet).new(5)
            changed.update(recovery=fresh['recovery'],effect=fresh['effect'],socket_service=None)
            packet=saved(packet,catalog.Catalog(packet).apply(changed,second));self.check(packet)
            self.assertIsNone(catalog.Catalog(packet).item(second)['socket_service'])
        bad=catalog.Catalog(packet).new(5);socket_service(bad);bad['socket_service']['units_per_use']='201'
        with self.assertRaises(api.EditorError):catalog.validate(packet,bad,None,TOOL)
        bad['socket_service'].update(units_per_use='1',service_kind='4')
        with self.assertRaises(api.EditorError):catalog.validate(packet,bad,None,TOOL)

    def test_pet_lifecycle_profiles_supply_matrix_and_copy(self):
        def training(d):growth(d);d.update(catalog.Catalog.training_defaults())
        packet,pet=create(catalog.blank(TOOL),15,training);original=catalog.Catalog(packet).item(pet)
        catalog.validate(packet,original,pet,TOOL)
        keys=[]
        for kind in range(1,9):
            d=catalog.Catalog(packet).new(17);d['supply']['supply_kind']=str(kind)
            d['pet_study']['skill_id']='1';d['pet_skin']['form_id']=original['child']['form_id']
            catalog.validate(packet,d,None,TOOL);key=key_of(d);packet=saved(packet,catalog.Catalog(packet).apply(d),key);keys.append(key)
        clone=catalog.Catalog(packet).duplicate(pet);other=key_of(clone);clone['pet_skill_ranks'][0]['value_scaled']='250'
        clone['pet_age_tiers'][0]['max_hp_flat']='600';catalog.validate(packet,clone,None,TOOL)
        packet=saved(packet,catalog.Catalog(packet).apply(clone),other)
        self.assertEqual(catalog.Catalog(packet).item(pet),original)
        self.assertNotEqual(catalog.Catalog(packet).item(other)['child']['training_profile_id'],original['child']['training_profile_id'])
        source=catalog.Catalog(packet).item(keys[0]);clone=catalog.Catalog(packet).duplicate(keys[0]);other_supply=key_of(clone)
        clone['pet_study']['known_success_bp']='8000';catalog.validate(packet,clone,None,TOOL)
        packet=saved(packet,catalog.Catalog(packet).apply(clone),other_supply);self.assertEqual(catalog.Catalog(packet).item(keys[0]),source)
        edited=catalog.Catalog(packet).item(other_supply);edited['supply']['supply_kind']='7';edited['pet_study']=dict(invalid='inactive')
        catalog.validate(packet,edited,other_supply,TOOL)
        packet=saved(packet,catalog.Catalog(packet).apply(edited,other_supply));self.assertIsNone(catalog.Catalog(packet).item(other_supply)['pet_study'])
        for mutate in (lambda d:d['pet_slot_rules'].pop(),lambda d:d['pet_skill_ranks'].pop(0),lambda d:d['pet_age_tiers'][0].update(minimum_age_ms='1')):
            bad=copy.deepcopy(original);mutate(bad)
            with self.assertRaises(api.EditorError):catalog.validate(packet,bad,pet,TOOL)


    def test_pet_hatch_copy_food_contexts_and_invalid_content(self):
        packet,pet=create(catalog.blank(TOOL),15,growth)
        packet,key=create(packet,8,lambda d:egg(d,pet));original=catalog.Catalog(packet).item(key)
        clone=catalog.Catalog(packet).duplicate(key);other=key_of(clone);clone['hatch']['hp_max']='45'
        clone['pet_food_rules']=[food_rule(),food_rule('2')]
        catalog.validate(packet,clone,None,TOOL);packet=saved(packet,catalog.Catalog(packet).apply(clone),other)
        self.assertEqual(catalog.Catalog(packet).item(key),original)
        changed=catalog.Catalog(packet).item(other);self.assertEqual(len(changed['pet_food_rules']),2)
        self.assertNotEqual(changed['child']['hatch_profile_id'],original['child']['hatch_profile_id'])
        for mutate in (lambda d:d.update(hatch=None),lambda d:d['hatch'].update(hp_step='0'),lambda d:d['hatch'].update(target_category_id='1'),
                       lambda d:d['pet_food_rules'].append(d['pet_food_rules'][0]),lambda d:d['pet_food_rules'][0].update(food_kind='3')):
            bad=copy.deepcopy(changed);mutate(bad)
            with self.assertRaises(api.EditorError):catalog.validate(packet,bad,other,TOOL)
        changed['child']['container_kind']='1';changed['loot']=catalog.Catalog(packet).new(8)['loot'];changed['hatch']['hp_step']='invalid inactive'
        catalog.validate(packet,changed,other,TOOL)


    def test_growth_profile_copy_levels_and_gates(self):
        packet,key=create(catalog.blank(TOOL),15,growth)
        original=catalog.Catalog(packet).item(key);catalog.validate(packet,original,key,TOOL)
        self.assertEqual(original['growth']['maximum_level'],'30');self.assertEqual(len(original['growth_levels']),30)
        clone=catalog.Catalog(packet).duplicate(key);other=key_of(clone)
        clone['growth_levels'][0]['mob_experience']='2000'
        tables=catalog.Catalog(packet).apply(clone);packet=saved(packet,tables,other)
        self.assertEqual(catalog.Catalog(packet).item(key),original)
        changed=catalog.Catalog(packet).item(other);self.assertNotEqual(changed['child']['growth_profile_id'],original['child']['growth_profile_id'])
        catalog.validate(packet,changed,other,TOOL)
        for mutate in (lambda d:d['growth_levels'].pop(),lambda d:d['growth_stages'][0].update(maximum_level='29'),
                       lambda d:d['child'].update(clock_kind='3'),lambda d:d['refinement'].update(profile_kind='9')):
            bad=copy.deepcopy(changed);mutate(bad)
            with self.assertRaises(api.EditorError):catalog.validate(packet,bad,other,TOOL)


    def test_companion_forms_copy_levels_and_rejections(self):
        packet,key=create(catalog.blank(TOOL),15)
        original=catalog.Catalog(packet).item(key);catalog.validate(packet,original,key,TOOL)
        clone=catalog.Catalog(packet).duplicate(key);other=key_of(clone)
        clone['child'].update(companion_kind='2',clock_kind='4',duration_ms='300000');clone['riding_same_form']=True
        clone['refinement']['max_level']='30';clone['levels']=[dict(copy.deepcopy(clone['levels'][0]),level=str(i)) for i in range(31)]
        clone['new_companion_form']=dict(code='companion.offline_mount',mob_template_id='20110',model_asset='models/mount.model')
        catalog.validate(packet,clone,None,TOOL);tables=catalog.Catalog(packet).apply(clone);packet=saved(packet,tables,other)
        self.assertEqual(catalog.Catalog(packet).item(key),original)
        item=catalog.Catalog(packet).item(other);self.assertEqual(item['child']['form_id'],item['child']['riding_form_id'])
        self.assertEqual(item['refinement']['max_level'],'30');self.assertEqual(len(rows(packet,'companion_form')),2)
        for mutate in (lambda d:d['child'].update(riding_form_id=None),lambda d:d['base'].update(max_stack='2'),lambda d:d['child'].update(duration_ms='0')):
            bad=copy.deepcopy(item);mutate(bad)
            with self.assertRaises(api.EditorError):catalog.validate(packet,bad,other,TOOL)
        bad=copy.deepcopy(item);bad['new_companion_form']=dict(code='companion.offline_mount',mob_template_id='20111',model_asset='models/mount.model')
        with self.assertRaises(api.EditorError):catalog.validate(packet,bad,other,TOOL)

    def test_soul_levels_scroll_copy_and_state_rules(self):
        def core(d):
            d['refinement']['max_level']='30'
            d['levels']=[dict(level=str(i),**{k:'0' for k in catalog.GAINS},extra_bonus_profile_id=None,bonuses=[]) for i in range(31)]
            d['soul_levels']=[catalog.Catalog.soul_level(i) for i in range(31)]
            d['soul_levels'][11].update(charge_capacity='180',strikes_per_cycle='1000',initial_strikes='1000',third_multiplier_bp='25000')
        packet,key=create(self.blank,14,core);self.check(packet);original=catalog.Catalog(packet).item(key)
        clone=catalog.Catalog(packet).duplicate(key);other=key_of(clone);clone['soul_levels'][11]['recharge_interval_ms']='5000'
        packet=saved(packet,catalog.Catalog(packet).apply(clone),other);self.check(packet)
        self.assertEqual(catalog.Catalog(packet).item(key),original)
        scroll=catalog.Catalog(packet).item(other);scroll['child'].update(soul_kind='2',damage_channel=None,profile_id=None,refine_chance_bp='7500')
        scroll['base']['max_stack']='200';scroll['refinement'].update(profile_kind='1',max_level='0');scroll['levels']=scroll['levels'][:1];scroll['soul_levels']=[]
        packet=saved(packet,catalog.Catalog(packet).apply(scroll,other));self.check(packet)
        self.assertIsNone(catalog.Catalog(packet).item(other)['child']['profile_id'])
        self.assertEqual(len(rows(packet,'soul_profile')),1)
        self.assertEqual(catalog.Catalog(packet).item(key),original)
        for change in (lambda d:d['soul_levels'].pop(),lambda d:d['soul_levels'][0].update(cycle_charge_cost='0'),
                       lambda d:d['base'].update(max_stack='2'),lambda d:d['child'].update(damage_channel=None)):
            bad=copy.deepcopy(original);change(bad)
            with self.assertRaises(api.EditorError):catalog.validate(packet,bad,key,TOOL)

    def test_polymorph_profiles_forms_and_copy_are_independent(self):
        def orb(d):
            d.update(poly_target_fixed=True,new_poly_form=dict(code='form.test_wolf',mob_template_id='101',book_bonus='4'))
        packet,key=create(self.blank,13,orb);self.check(packet)
        original=catalog.Catalog(packet).item(key);self.assertIsNotNone(original['child']['fixed_form_id'])
        clone=catalog.Catalog(packet).duplicate(key);other=key_of(clone)
        clone['poly_skills'][40]['duration_ms']='3000000'
        packet=saved(packet,catalog.Catalog(packet).apply(clone),other);self.check(packet)
        self.assertEqual(catalog.Catalog(packet).item(key),original)
        self.assertEqual(len(rows(packet,'polymorph_form')),1)
        bad=catalog.Catalog(packet).item(other);bad['poly_skills'].pop()
        with self.assertRaises(api.EditorError):catalog.validate(packet,bad,other,TOOL)
        conflicting=catalog.Catalog(packet).new(13)
        conflicting['new_poly_form']=dict(code='form.test_wolf',mob_template_id='102',book_bonus='4')
        with self.assertRaises(api.EditorError):catalog.Catalog(packet).apply(conflicting)

    def test_polymorph_power_defaults_include_source_level_40_boundary(self):
        import re
        source=(ROOT/'SRC/Server/GameServer/core/constants.cpp').read_text(encoding='utf-8-sig')
        body=re.search(r'aiPolymorphPowerByLevel[^=]*=\s*\{([^}]+)\}',source,re.S).group(1)
        initialized=[int(v) for v in re.findall(r'\d+',re.sub(r'//[^\n]*','',body))]
        self.assertEqual(len(initialized),40)
        defaults=catalog.Catalog.polymorph_defaults()['poly_skills']
        self.assertEqual([int(r['power_bp']) for r in defaults],[v*100 for v in initialized]+[0])
        self.assertEqual(defaults[40]['duration_ms'],'2100000')

    def test_tool_progression_copy_and_fishing_recipes(self):
        def tool(d):
            d['child'].update(tool_kind='2',fishing_pool='3')
            d['refinement']['max_level']='30'
            d['levels']=[dict(level=str(i),**{k:'0' for k in catalog.GAINS},extra_bonus_profile_id=None,bonuses=[]) for i in range(31)]
            d['tool_levels']=[catalog.Catalog.tool_level(i,i==30) for i in range(31)]
            d['tool_levels'][10].update(power='55',required_points='100',practice_numerator='1',practice_denominator='5',failure_loss_bp='1000')
        packet,key=create(self.blank,11,tool);self.check(packet);original=catalog.Catalog(packet).item(key)
        clone=catalog.Catalog(packet).duplicate(key);other=key_of(clone);clone['tool_levels'][10]['power']='66'
        packet=saved(packet,catalog.Catalog(packet).apply(clone),other);self.check(packet)
        self.assertEqual(catalog.Catalog(packet).item(key),original)
        self.assertEqual(len(rows(packet,'tool_profile')),2)
        bad=catalog.Catalog(packet).item(key);bad['tool_levels'][10]['practice_denominator']='0'
        with self.assertRaises(api.EditorError):catalog.validate(packet,bad,key,TOOL)
        packet,cooked=create(packet,5)
        packet,dead=create(packet,12,lambda d:d['child'].update(fishing_kind='2',bait_power='0',grill_category_id='5',grill_local_id=str(cooked[1])))
        def live(d):
            d['child'].update(fishing_kind='1',bait_power='0')
            d['loot']=dict(pick_mode='1',entries=[dict(entry_no='1',reward_kind='1',item_category_id='12',item_local_id=str(dead[1]),amount='1',weight='31',rare_attribute_chance_bp='0')])
        packet,fish=create(packet,12,live);self.check(packet)
        self.assertEqual(catalog.Catalog(packet).item(fish)['loot']['entries'][0]['item_local_id'],str(dead[1]))
        bad=catalog.Catalog(packet).item(fish);bad['loot']['entries'][0].update(reward_kind='2',item_category_id=None,item_local_id=None)
        with self.assertRaises(api.EditorError):catalog.validate(packet,bad,fish,TOOL)

    def test_costume_variants_timers_copy_and_refinement(self):
        def change(d):
            d['child'].update(costume_kind='3', weapon_subtype='3', clock_kind='3', duration_ms='604800000')
            d['refinement']['max_level']='30'
            d['levels']=[dict(level=str(i), **{k:'0' for k in catalog.GAINS}, extra_bonus_profile_id=None,
                              bonuses=[dict(bonus_id='21',value_scaled=str(i*10))] if i else []) for i in range(31)]
        packet,key=create(self.blank,4,change);self.check(packet)
        original=catalog.Catalog(packet).item(key)
        self.assertEqual(len(original['visuals']),8)
        self.assertEqual(original['refinement']['profile_kind'],'6')
        clone=catalog.Catalog(packet).duplicate(key);other=key_of(clone)
        clone['visuals'][0]['model_asset']='models/another_costume.model'
        clone['child'].update(costume_kind='2',weapon_subtype=None,clock_kind='1',duration_ms='0')
        packet=saved(packet,catalog.Catalog(packet).apply(clone),other);self.check(packet)
        self.assertEqual(catalog.Catalog(packet).item(key),original)
        self.assertEqual(len(rows(packet,'item_costume_visual')),16)
        modified=catalog.Catalog(packet).item(other)
        for mutate in (lambda d:d['visuals'].pop(), lambda d:d['child'].update(weapon_subtype='1'),
                       lambda d:d['child'].update(duration_ms='1'),lambda d:d['base'].update(max_stack='2')):
            bad=copy.deepcopy(modified);mutate(bad)
            with self.assertRaises(api.EditorError):catalog.validate(packet,bad,other,TOOL)

    def test_stone_refinement_and_clone_keep_one_prototype_per_item(self):
        def change(d):
            d['child'].update(allowed_targets='3',primary_group='stone.test',secondary_group='stone.other')
            d['refinement']['max_level']='30'
            d['levels']=[dict(level=str(i),**{k:'0' for k in catalog.GAINS},extra_bonus_profile_id=None,
                              bonuses=[dict(bonus_id='21',value_scaled=str(i*10))] if i else []) for i in range(31)]
            d['bonuses']=[dict(bonus_id='21',value_scaled='100')]
        packet,key=create(self.blank,7,change);self.check(packet)
        original=catalog.Catalog(packet).item(key)
        self.assertEqual(original['refinement']['max_level'],'30')
        clone=catalog.Catalog(packet).duplicate(key);other=key_of(clone)
        clone['levels'][10]['bonuses'][0]['value_scaled']='500'
        packet=saved(packet,catalog.Catalog(packet).apply(clone),other);self.check(packet)
        self.assertEqual(catalog.Catalog(packet).item(key),original)
        self.assertEqual(len(rows(packet,'item_socket_stone')),2)
        bad=catalog.Catalog(packet).item(other);bad['child']['secondary_group']='stone.test'
        with self.assertRaises(api.EditorError):catalog.validate(packet,bad,other,TOOL)

    def test_quest_token_and_group_dispatch_copy_are_independent(self):
        packet,key=create(self.blank,10)
        token=catalog.Catalog(packet).item(key)
        self.assertEqual(token['child']['dispatch_kind'],'1')
        clone=catalog.Catalog(packet).duplicate(key);other=key_of(clone)
        clone['child'].update(dispatch_kind='3',group_code='quest.horse_training',allowed_contexts='9',allow_observer='0')
        packet=saved(packet,catalog.Catalog(packet).apply(clone),other)
        self.check(packet)
        self.assertEqual(catalog.Catalog(packet).item(key),token)
        self.assertEqual(catalog.Catalog(packet).item(other)['child']['group_code'],'quest.horse_training')
        bad=catalog.Catalog(packet).item(other);bad['child']['allowed_contexts']='0'
        with self.assertRaises(api.EditorError):catalog.validate(packet,bad,other,TOOL)

    def setUp(self):
        self.blank = catalog.blank(TOOL)

    def check(self, packet):
        return catalog.run_tool(dict(action='validate', tables=packet['tables']), packet['columns'], TOOL)['hash']

    def test_blank_registry_is_a_valid_empty_catalog(self):
        self.assertEqual(self.check(self.blank), self.blank['hash'])
        self.assertEqual(set(self.blank['sequences']), {str(i) for i in range(1, 19)})
        self.assertEqual(len(self.blank['bonuses']), len(rows(self.blank, 'bonus_definition')))

    def test_every_editable_category_starts_with_a_valid_document(self):
        packet = self.blank
        for category in catalog.CATEGORIES:
            document = catalog.Catalog(packet).new(category)
            self.assertEqual(key_of(document), (category, 1))
            catalog.validate(packet, document, executable=TOOL)
            packet = saved(packet, catalog.Catalog(packet).apply(document), key_of(document))
        self.check(packet)
        self.assertEqual(len(rows(packet, 'item_base')), len(catalog.CATEGORIES))
        self.assertEqual(catalog.Catalog(packet).new(1)['base']['local_id'], '2')

    def test_editing_one_item_never_changes_an_item_sharing_its_profile(self):
        packet, first = create(self.blank, 1)
        packet, second = create(packet, 1)
        model = catalog.Catalog(packet)
        a, b = model.item(first), model.item(second)
        # Equal policies are deduplicated into one shared row.
        self.assertEqual(a['base']['use_policy_id'], b['base']['use_policy_id'])
        self.assertEqual(a['child']['attack_profile_id'], b['child']['attack_profile_id'])
        b['use']['allowed_classes'] = '1'
        b['attack']['reach_millimeters'] = '4500'
        packet = saved(packet, catalog.Catalog(packet).apply(b, second))
        self.check(packet)
        model = catalog.Catalog(packet)
        a, b = model.item(first), model.item(second)
        self.assertEqual((a['use']['allowed_classes'], a['attack']['reach_millimeters']), ('15', '3000'))
        self.assertEqual((b['use']['allowed_classes'], b['attack']['reach_millimeters']), ('1', '4500'))
        self.assertNotEqual(a['base']['use_policy_id'], b['base']['use_policy_id'])

    def test_refinement_curve_to_plus_thirty_is_one_prototype(self):
        def curve(document):
            document['refinement']['max_level'] = '30'
            document['levels'] = [dict(level=str(level), physical_attack_gain_raw=str(level * 2000), magical_attack_gain_raw='0',
                                       physical_defense_gain_raw='0', magical_defense_gain_raw='0', extra_bonus_profile_id=None, bonuses=[])
                                  for level in range(31)]
            document['child']['physical_attack_base_raw'] = '15000'
        packet, key = create(self.blank, 1, curve)
        self.check(packet)
        self.assertEqual(len(rows(packet, 'item_base')), 1)
        self.assertEqual(len(rows(packet, 'refinement_level')), 31)
        # Shrinking the curve drops the surplus rows instead of leaving them behind.
        document = catalog.Catalog(packet).item(key)
        document['refinement']['max_level'] = '9'
        document['levels'] = document['levels'][:10]
        packet = saved(packet, catalog.Catalog(packet).apply(document, key))
        self.check(packet)
        self.assertEqual(len(rows(packet, 'refinement_level')), 10)

    def test_shared_refinement_curve_is_copied_before_editing(self):
        def curve(document):
            document['refinement']['max_level'] = '1'
            document['levels'].append(dict(level='1', physical_attack_gain_raw='1000', magical_attack_gain_raw='0',
                                           physical_defense_gain_raw='0', magical_defense_gain_raw='0', extra_bonus_profile_id=None, bonuses=[]))
        packet, first = create(self.blank, 1, curve)
        packet, second = create(packet, 1, curve)
        # Imported content may share one curve between items.
        tables = copy.deepcopy(packet['tables'])
        columns = packet['columns']['item_base']
        shared = rows(packet, 'item_base')[0]['refinement_profile_id']
        for row in tables['item_base']:
            row[columns.index('refinement_profile_id')] = shared
        packet = saved(packet, tables)
        before = len(rows(packet, 'refinement_profile'))
        document = catalog.Catalog(packet).item(second)
        document['levels'][1]['physical_attack_gain_raw'] = '5000'
        packet = saved(packet, catalog.Catalog(packet).apply(document, second))
        self.check(packet)
        model = catalog.Catalog(packet)
        self.assertEqual(model.item(first)['levels'][1]['physical_attack_gain_raw'], '1000')
        self.assertEqual(model.item(second)['levels'][1]['physical_attack_gain_raw'], '5000')
        self.assertEqual(model.item(first)['base']['refinement_profile_id'], shared)
        self.assertLessEqual(len(rows(packet, 'refinement_profile')), before + 1)

    def test_base_and_level_bonuses_get_private_profiles_and_are_pruned(self):
        definitions = {d['code']: d for d in self.blank['bonuses']}
        attack = str(definitions['physical_attack.flat']['id'])
        def bonuses(document):
            document['bonuses'] = [dict(bonus_id=attack, value_scaled='5000')]
            document['refinement']['max_level'] = '1'
            document['levels'].append(dict(level='1', physical_attack_gain_raw='0', magical_attack_gain_raw='0',
                                           physical_defense_gain_raw='0', magical_defense_gain_raw='0', extra_bonus_profile_id=None,
                                           bonuses=[dict(bonus_id=attack, value_scaled='1000')]))
        packet, key = create(self.blank, 3, bonuses)
        self.check(packet)
        profiles = {row['profile_id']: row['source_kind'] for row in rows(packet, 'bonus_profile')}
        self.assertEqual(sorted(profiles.values()), ['1', '2'])
        document = catalog.Catalog(packet).item(key)
        self.assertEqual(document['levels'][1]['bonuses'], [dict(bonus_id=attack, value_scaled='1000')])
        document['bonuses'] = []
        document['levels'][1]['bonuses'] = []
        packet = saved(packet, catalog.Catalog(packet).apply(document, key))
        self.check(packet)
        self.assertEqual(rows(packet, 'bonus_profile'), [])
        self.assertEqual(rows(packet, 'bonus_profile_entry'), [])

    def test_shared_potion_cooldown_and_delivery_change_stay_isolated(self):
        def cooled(document):
            document['cooldown'] = dict(duration_ms='3000')
        packet, first = create(self.blank, 5, cooled)
        packet, second = create(packet, 5, cooled)
        # The editor gives each new potion its own effect; imported content
        # may share one recovery head (and its cooldown) between potions.
        tables = copy.deepcopy(packet['tables'])
        shared = rows(packet, 'item_consumable')[0]['recovery_profile_id']
        own = rows(packet, 'item_consumable')[1]['recovery_profile_id']
        for row in tables['item_consumable']:
            row[packet['columns']['item_consumable'].index('recovery_profile_id')] = shared
        tables['recovery_profile'] = [row for row in tables['recovery_profile'] if row[0] != own]
        tables['instant_recovery_profile'] = [row for row in tables['instant_recovery_profile'] if row[0] != own]
        packet = saved(packet, tables)
        self.check(packet)
        model = catalog.Catalog(packet)
        self.assertEqual(model.item(first)['child']['recovery_profile_id'], model.item(second)['child']['recovery_profile_id'])
        document = model.item(second)
        document['cooldown']['duration_ms'] = '9000'
        packet = saved(packet, catalog.Catalog(packet).apply(document, second))
        self.check(packet)
        model = catalog.Catalog(packet)
        self.assertEqual(model.item(first)['cooldown']['duration_ms'], '3000')
        self.assertEqual(model.item(second)['cooldown']['duration_ms'], '9000')
        document = model.item(second)
        document['recovery']['delivery_kind'] = '3'
        document['effect'] = dict(hp_per_tick_raw='500', sp_per_tick_raw='0', tick_interval_ms='1000', duration_ms='10000')
        packet = saved(packet, catalog.Catalog(packet).apply(document, second))
        self.check(packet)
        model = catalog.Catalog(packet)
        self.assertEqual(model.item(second)['effect']['hp_per_tick_raw'], '500')
        self.assertEqual(model.item(first)['effect']['hp_raw'], '1000')
        # Only the first potion's head remains in use besides the new one.
        self.assertEqual(len(rows(packet, 'recovery_profile')), 2)
        self.assertEqual(len(rows(packet, 'instant_recovery_profile')), 1)
        self.assertEqual(len(rows(packet, 'periodic_recovery_profile')), 1)

    def test_rejections_are_explained_in_hungarian(self):
        packet, key = create(self.blank, 6)
        cases = [
            (lambda d: d['base'].update(content_code='Rossz Kód'), 'kisbetűvel'),
            (lambda d: d['base'].update(content_code='material.new_item_1', local_id='2'), 'foglalt'),
            (lambda d: d['refinement'].update(max_level='2'), 'fejlesztés'),
        ]
        for change, expected in cases:
            document = catalog.Catalog(packet).new(6)
            change(document)
            with self.assertRaises(api.EditorError) as raised:
                catalog.validate(packet, document, executable=TOOL)
            self.assertIn(expected, str(raised.exception).lower())
            self.assertIn('Semmi nem lett mentve', str(raised.exception))
        document = catalog.Catalog(packet).item(key)
        document['base']['local_id'] = '7'
        with self.assertRaises(api.EditorError):
            catalog.Catalog(packet).apply(document, key)

    def test_container_loot_is_private_and_scripted_boxes_drop_it(self):
        packet, leather = create(self.blank, 6)
        def chest(document):
            document['child'].update(container_kind='2', lock_id='7')
            document['loot'] = dict(pick_mode='2', entries=[
                dict(reward_kind='1', item_category_id='6', item_local_id=str(leather[1]), amount='5', weight='10000', rare_attribute_chance_bp='0'),
                dict(reward_kind='3', item_category_id=None, item_local_id=None, amount='5000', weight='2500', rare_attribute_chance_bp='0'),
                dict(reward_kind='8', item_category_id=None, item_local_id=None, amount='0', weight='100', rare_attribute_chance_bp='0')])
        packet, first = create(packet, 8, chest)
        packet, key = create(packet, 18, lambda d: d['child'].update(lock_id='7'))
        self.check(packet)
        document = catalog.Catalog(packet).item(first)
        self.assertEqual([e['entry_no'] for e in document['loot']['entries']], ['1', '2', '3'])
        self.assertEqual(document['loot']['pick_mode'], '2')
        # A second box sharing the same contents (imported content) is copied before editing.
        packet, second = create(packet, 8)
        tables = copy.deepcopy(packet['tables'])
        column = packet['columns']['item_container'].index('loot_profile_id')
        shared = rows(packet, 'item_container')[0]['loot_profile_id']
        own = rows(packet, 'item_container')[1]['loot_profile_id']
        tables['item_container'][1][column] = shared
        tables['loot_entry'] = [row for row in tables['loot_entry'] if row[0] != own]
        tables['loot_profile'] = [row for row in tables['loot_profile'] if row[0] != own]
        packet = saved(packet, tables)
        self.check(packet)
        document = catalog.Catalog(packet).item(second)
        document['loot']['entries'][1]['amount'] = '9000'
        packet = saved(packet, catalog.Catalog(packet).apply(document, second))
        self.check(packet)
        model = catalog.Catalog(packet)
        self.assertEqual(model.item(first)['loot']['entries'][1]['amount'], '5000')
        self.assertEqual(model.item(second)['loot']['entries'][1]['amount'], '9000')
        # Turning a box into a quest-opened one removes its now unused private contents.
        before = len(rows(packet, 'loot_profile'))
        document = model.item(second)
        document['child']['container_kind'] = '3'
        packet = saved(packet, catalog.Catalog(packet).apply(document, second))
        self.check(packet)
        self.assertIsNone(catalog.Catalog(packet).item(second)['loot'])
        self.assertEqual(len(rows(packet, 'loot_profile')), before - 1)
        # Content rules come back from the game's validator in Hungarian.
        for change, expected in [
            (lambda d: d['child'].update(lock_id='0'), 'zárazonosító'),
            (lambda d: d['loot']['entries'].clear(), 'üres'),
            (lambda d: d['loot']['entries'][0].update(amount='201'), 'halomban'),
            (lambda d: d['loot']['entries'][2].update(amount='1'), 'mérgezésé 0'),
        ]:
            document = catalog.Catalog(packet).item(first)
            change(document)
            with self.assertRaises(api.EditorError) as raised:
                catalog.validate(packet, document, first, TOOL)
            self.assertIn(expected, str(raised.exception))
        self.assertEqual(catalog.Catalog(packet).item(key)['child']['lock_id'], '7')

    def test_automatic_profiles_copy_without_sharing_charge_capacity_edits(self):
        packet, first = create(self.blank, 5, automatic)
        original = catalog.Catalog(packet).item(first)
        clone = catalog.Catalog(packet).duplicate(first)
        second = key_of(clone)
        clone['effect']['capacity_raw'] = '9000000'
        packet = saved(packet, catalog.Catalog(packet).apply(clone), second)
        self.check(packet)
        model = catalog.Catalog(packet)
        self.assertEqual(model.item(first), original)
        self.assertEqual(model.item(second)['effect']['capacity_raw'], '9000000')
        d = model.item(second)
        d['effect'].update(supply_kind='2', capacity_raw='0', pool_kind='2')
        packet = saved(packet, model.apply(d, second))
        self.check(packet)
        d = catalog.Catalog(packet).item(second)
        self.assertEqual(d['effect']['capacity_raw'], '0')
        d['base']['max_stack'] = '2'
        with self.assertRaisesRegex(api.EditorError, 'nem halmozható'):
            catalog.validate(packet, d, second, TOOL)
        d['base']['max_stack'] = '1'
        d['effect']['capacity_raw'] = '1'
        with self.assertRaises(api.EditorError): catalog.validate(packet, d, second, TOOL)
        d['effect']['capacity_raw'] = '0'
        old = d['child']['recovery_profile_id']
        defaults = model.new(5)
        d.update(recovery=defaults['recovery'], effect=defaults['effect'])
        packet = saved(packet, catalog.Catalog(packet).apply(d, second))
        self.check(packet)
        self.assertIsNone(catalog.Catalog(packet).row('automatic_recovery_profile', old))
        self.assertEqual(catalog.Catalog(packet).item(first), original)

    def test_arena_quota_is_explicit_and_requires_arena_context(self):
        packet, key = create(self.blank, 5, lambda d: d['recovery'].update(arena_quota_cost='3'))
        self.check(packet)
        d = catalog.Catalog(packet).item(key)
        self.assertEqual(d['recovery']['arena_quota_cost'], '3')
        d['recovery']['allowed_contexts'] = '1'
        with self.assertRaisesRegex(api.EditorError, '[Aa]rénakvóta'):
            catalog.validate(packet, d, key, TOOL)
        d['recovery']['arena_quota_cost'] = '0'
        catalog.validate(packet, d, key, TOOL)

    def test_unit_conversion_is_exact(self):
        self.assertEqual(catalog.scaled('12,5'), 12500)
        self.assertEqual(catalog.display(12500), '12.5')
        self.assertEqual(catalog.display(12000), '12')
        with self.assertRaises(api.EditorError):
            catalog.scaled('0.0001')

    def test_duplicate_uses_new_key_and_copy_on_write(self):
        packet, original = create(self.blank, 1)
        before = catalog.Catalog(packet).item(original)
        document = catalog.Catalog(packet).duplicate(original)
        copied = key_of(document)
        self.assertNotEqual(original, copied)
        self.assertNotEqual(document['base']['content_code'], before['base']['content_code'])
        self.assertEqual(document['base']['name_key'], before['base']['name_key'])
        document['attack']['reach_millimeters'] = '4500'
        catalog.validate(packet, document, executable=TOOL)
        packet = saved(packet, catalog.Catalog(packet).apply(document), copied)
        model = catalog.Catalog(packet)
        self.assertEqual(model.item(original), before)
        self.assertEqual(model.item(copied)['attack']['reach_millimeters'], '4500')
        self.assertNotEqual(model.item(original)['child']['attack_profile_id'], model.item(copied)['child']['attack_profile_id'])

    def test_timed_effect_profiles_are_private_and_type_switch_prunes(self):
        def timed(d):
            d.update(catalog.Catalog.timed_defaults(d['base']['local_id']))
            d['cooldown'] = dict(duration_ms='1000')
        packet, first = create(self.blank, 5, timed)
        original = catalog.Catalog(packet).item(first)
        clone = catalog.Catalog(packet).duplicate(first)
        second = key_of(clone)
        packet = saved(packet, catalog.Catalog(packet).apply(clone), second)
        # Simulate imported items sharing one complete effect profile.
        model = catalog.Catalog(packet)
        child = next(r for r in model.tables['item_consumable'] if r['local_id'] == str(second[1]))
        child['timed_effect_profile_id'] = original['child']['timed_effect_profile_id']
        packet = saved(packet, model.encode())
        self.check(packet)
        changed = catalog.Catalog(packet).item(second)
        changed['timed']['duration_ms'] = '120000'
        changed['timed_bonuses'][0]['value_scaled'] = '2500'
        changed['cooldown']['duration_ms'] = '3000'
        packet = saved(packet, catalog.Catalog(packet).apply(changed, second))
        self.check(packet)
        model = catalog.Catalog(packet)
        self.assertEqual(model.item(first), original)
        self.assertEqual(model.item(second)['timed_bonuses'][0]['value_scaled'], '2500')
        self.assertEqual(model.item(second)['cooldown']['duration_ms'], '3000')
        # A complete type switch removes its old private profile and bonus.
        changed = model.item(second)
        old_profile = changed['child']['timed_effect_profile_id']
        old_bonus = changed['timed']['bonus_profile_id']
        defaults = model.new(5)
        changed.update(timed=None, recovery=defaults['recovery'], effect=defaults['effect'], cooldown=None)
        packet = saved(packet, model.apply(changed, second))
        self.check(packet)
        model = catalog.Catalog(packet)
        self.assertIsNone(model.item(second)['child']['timed_effect_profile_id'])
        self.assertIsNone(model.row('timed_effect_profile', old_profile))
        self.assertIsNone(model.row('bonus_profile', old_bonus))
        self.assertEqual(model.item(first), original)

    def test_timed_group_is_explicit_shared_identity(self):
        packet, key = create(self.blank, 5, lambda d: d.update(catalog.Catalog.timed_defaults(d['base']['local_id'])))
        d = catalog.Catalog(packet).item(key)
        d['timed_group']['reapply_kind'] = '2'
        with self.assertRaisesRegex(api.EditorError, 'Meglévő hatáscsoport'):
            catalog.Catalog(packet).apply(d, key)
        d['timed_group']['group_id'] = None
        d['timed_group']['code'] = 'effect.other'
        d['timed']['group_id'] = None
        packet = saved(packet, catalog.Catalog(packet).apply(d, key))
        self.check(packet)
        self.assertEqual(len(rows(packet, 'timed_effect_group')), 2)
        d = catalog.Catalog(packet).item(key)
        d['timed_bonuses'] = []
        with self.assertRaises(api.EditorError): catalog.validate(packet, d, key, TOOL)
        d['timed_bonuses'] = [dict(bonus_id='26', value_scaled='0')]
        with self.assertRaises(api.EditorError): catalog.validate(packet, d, key, TOOL)

    def test_toggle_copy_on_write_groups_and_three_effect_switches(self):
        packet, first = create(self.blank, 5, toggle)
        self.check(packet)
        original = catalog.Catalog(packet).item(first)
        clone = catalog.Catalog(packet).duplicate(first)
        second = key_of(clone)
        clone['toggle']['duration_ms'] = '75000'
        clone['toggle_bonuses'][0]['value_scaled'] = '2500'
        packet = saved(packet, catalog.Catalog(packet).apply(clone), second)
        self.check(packet)
        model = catalog.Catalog(packet)
        self.assertEqual(model.item(first), original)
        document = model.item(second)
        self.assertEqual(document['toggle']['duration_ms'], '75000')
        self.assertEqual(document['toggle_bonuses'][0]['value_scaled'], '2500')
        document['base']['max_stack'] = '2'
        with self.assertRaisesRegex(api.EditorError, 'nem halmozható'):
            catalog.validate(packet, document, second, TOOL)
        document['base']['max_stack'] = '1'
        duration = document['toggle']['duration_ms']
        document['toggle']['duration_ms'] = '0'
        with self.assertRaisesRegex(api.EditorError, '[Kk]apcsolható harmat'):
            catalog.validate(packet, document, second, TOOL)
        document['toggle']['duration_ms'] = duration
        document['toggle_group']['remove_on_death'] = '1'
        with self.assertRaisesRegex(api.EditorError, 'hatáscsoport szabályai'): model.apply(document, second)
        document = model.item(second)
        group_id = document['toggle']['group_id']
        document.update(catalog.Catalog.timed_defaults(second[1]))
        document['toggle'] = None
        packet = saved(packet, catalog.Catalog(packet).apply(document, second))
        self.check(packet)
        self.assertIsNone(catalog.Catalog(packet).item(second)['toggle'])
        document = catalog.Catalog(packet).item(second)
        fresh = catalog.Catalog(packet).new(5)
        document.update(timed=None, recovery=fresh['recovery'], effect=fresh['effect'])
        packet = saved(packet, catalog.Catalog(packet).apply(document, second))
        self.check(packet)
        self.assertIsNotNone(catalog.Catalog(packet).row('toggle_effect_group', group_id, 'group_id'))
        self.assertEqual(catalog.Catalog(packet).item(first), original)

    def test_book_templates_and_shared_profile_edits_are_isolated(self):
        for template in ('master', 'leadership', 'combo', 'language', 'support', 'passive', 'forget', 'horse'):
            document = catalog.Catalog(self.blank).new(9)
            document.update(catalog.Catalog.book_defaults(template))
            with self.subTest(template=template): catalog.validate(self.blank, document, executable=TOOL)
        packet, first = create(self.blank, 9)
        original = catalog.Catalog(packet).item(first)
        clone = catalog.Catalog(packet).duplicate(first)
        second = key_of(clone)
        packet = saved(packet, catalog.Catalog(packet).apply(clone), second)
        # Imported catalogs may intentionally share the full profile.
        model = catalog.Catalog(packet)
        model.row('item_skill_book', second[1], 'local_id')['profile_id'] = original['child']['profile_id']
        packet = saved(packet, model.encode())
        document = catalog.Catalog(packet).item(second)
        document['book_steps'][0].update(required_successes='3', boosted_chance_bp='9000')
        document['book_targets'][0]['skill_id'] = '2'
        packet = saved(packet, catalog.Catalog(packet).apply(document, second))
        self.check(packet)
        model = catalog.Catalog(packet)
        self.assertEqual(model.item(first), original)
        self.assertNotEqual(model.item(second)['child']['profile_id'], original['child']['profile_id'])
        document = model.item(second)
        old = document['child']['profile_id']
        document.update(catalog.Catalog.book_defaults('forget'))
        packet = saved(packet, model.apply(document, second))
        self.check(packet)
        self.assertEqual(catalog.Catalog(packet).item(second)['book']['operation_kind'], '2')
        self.assertEqual(len(catalog.Catalog(packet).book_rows('skill_book_step', old, 'current_level')), 19)
        document = catalog.Catalog(packet).item(second)
        document.update(catalog.Catalog.book_defaults('horse'))
        packet = saved(packet, catalog.Catalog(packet).apply(document, second))
        self.check(packet)
        self.assertEqual(catalog.Catalog(packet).item(first), original)
        self.assertFalse(catalog.Catalog(packet).item(second)['book_targets'])





class PageCase(unittest.TestCase):
    def setUp(self):
        self.qt_signal_errors=[];previous=sys.excepthook
        sys.excepthook=lambda kind,error,traceback:self.qt_signal_errors.append(f'{kind.__name__}: {error}')
        self.addCleanup(setattr,sys,'excepthook',previous)

    def tearDown(self):
        self.app.processEvents()
        self.assertFalse(self.qt_signal_errors,'Unhandled Qt signal errors: '+repr(self.qt_signal_errors))

    """Offscreen Qt page with a stand-in main window; no database."""

    @classmethod
    def setUpClass(cls):
        import os
        os.environ.setdefault('QT_QPA_PLATFORM', 'offscreen')
        try:
            from PySide6.QtWidgets import QApplication, QMainWindow
        except ImportError:
            raise unittest.SkipTest('PySide6 is not installed in this Python')
        import item_page
        cls.app = QApplication.instance() or QApplication([])

        class Owner(QMainWindow):
            root = None
            def __init__(self): super().__init__(); self.errors = []
            def is_remote(self): return False
            def target_id(self): return 'test'
            def busy(self): return False
            def refresh(self): pass
            def error(self, text): self.errors.append(text)
        cls.owner = Owner()
        cls.page = item_page.ItemPage(cls.owner)

    def fixture(self):
        definitions = {d['code']: str(d['id']) for d in catalog.blank(TOOL)['bonuses']}
        def weapon(d):
            d['base'].update(content_code='weapon.sword_iron', name_key='item.sword_iron.name', min_level='15', binding_policy='2',
                             buy_amount='1200', sell_amount='300', weight_grams='1500')
            d['use'].update(allowed_classes='5', allowed_sexes='2', use_during_combat='0')
            d['trade'].update(allow_trade='0')
            d['appearance'] = dict(model_asset='models/weapon/sword_iron.gr2')
            d['child'].update(weapon_subtype='4', physical_attack_base_raw='15500', magical_attack_base_raw='2000')
            d['attack'].update(interval_ticks='30', windup_ticks='12', reach_millimeters='3250', max_targets='3')
            d['bonuses'] = [dict(bonus_id=definitions['critical.chance'], value_scaled='250')]
            d['refinement']['max_level'] = '3'
            d['levels'] = [dict(level=str(l), physical_attack_gain_raw=str(l * 1500), magical_attack_gain_raw=str(l * 500),
                                physical_defense_gain_raw='0', magical_defense_gain_raw='0', extra_bonus_profile_id=None,
                                bonuses=[dict(bonus_id=definitions['pvp.outgoing'], value_scaled='100')] if l == 3 else [])
                           for l in range(4)]
        def armor(d):
            d['child'].update(equip_slot='4', physical_defense_base_raw='8000')
            d['resistance'].update(fire_bp='1500', dark_bp='-250')
        def accessory(d):
            d['child'].update(equip_slot='8')
            d['resistance'].update(ice_bp='75')
        def potion(d):
            d['recovery'].update(delivery_kind='2', units_per_use='2', allowed_contexts='5', allow_during_pvp='0')
            d['effect'] = dict(delivery_kind='2', hp_raw='250000', sp_raw='0', flat_bonus_cap_bp='5000', tick_interval_ms='1500', max_pool_per_tick_bp='750')
            d['cooldown'] = dict(duration_ms='2500')
        def material(d):
            d['child'].update(material_kind='5')
            d['base'].update(max_stack='250')
        def chest(d):
            d['base'].update(max_stack='40')
            d['child'].update(container_kind='2', lock_id='12')
            d['loot'] = dict(pick_mode='2', entries=[
                dict(reward_kind='1', item_category_id='6', item_local_id='1', amount='25', weight='10000', rare_attribute_chance_bp='0'),
                dict(reward_kind='1', item_category_id='1', item_local_id='1', amount='1', weight='1250', rare_attribute_chance_bp='750'),
                dict(reward_kind='2', item_category_id=None, item_local_id=None, amount='150000', weight='5000', rare_attribute_chance_bp='0'),
                dict(reward_kind='3', item_category_id=None, item_local_id=None, amount='2500', weight='3333', rare_attribute_chance_bp='0'),
                dict(reward_kind='4', item_category_id=None, item_local_id=None, amount='101', weight='5', rare_attribute_chance_bp='0'),
                dict(reward_kind='5', item_category_id=None, item_local_id=None, amount='202', weight='6', rare_attribute_chance_bp='0'),
                dict(reward_kind='6', item_category_id=None, item_local_id=None, amount='30', weight='7', rare_attribute_chance_bp='0'),
                dict(reward_kind='7', item_category_id=None, item_local_id=None, amount='2550', weight='8', rare_attribute_chance_bp='0'),
                dict(reward_kind='8', item_category_id=None, item_local_id=None, amount='0', weight='9', rare_attribute_chance_bp='0')])
        packet = catalog.blank(TOOL)
        keys = {}
        def token(d):
            d['base'].update(max_stack='500')
            d['child'].update(wallet_currency='2', amount_per_unit='1000000000000')
        for category, change in ((1, weapon), (2, armor), (3, accessory), (5, potion), (6, material), (8, chest), (9, None), (16, token),
                                 (18, lambda d: d['child'].update(lock_id='12'))):
            packet, keys[category] = create(packet, category, change)
        return packet, keys

class EditorPageTests(PageCase):
    def test_instance_attribute_and_service_forms_ignore_inactive_drafts(self):
        packet,key=create(catalog.blank(TOOL),1,attributes);self.page.loaded(packet,key);panel=self.page.attribute_panel
        self.assertTrue(panel.enabled.isChecked());self.assertEqual(panel.values.rowCount(),28)
        panel.values.item(0,4).setText('3.5');d=self.page.collect();self.assertEqual(d['attribute_values'][0]['value_scaled'],'3500');catalog.validate(packet,d,key,TOOL)
        panel.enabled.setChecked(False);panel.values.item(0,4).setText('invalid inactive');d=self.page.collect();self.assertIsNone(d['attribute_profile']);catalog.validate(packet,d,key,TOOL)
        packet,service=create(packet,5,attribute_service);self.page.loaded(packet,service);panel=self.page.attribute_service_panel
        self.assertEqual(self.page.consumable_kind.currentData(),'5');panel.success.edit.setText('73')
        d=self.page.collect();self.assertEqual(d['attribute_service']['success_bp'],'7300');catalog.validate(packet,d,service,TOOL)
        panel.operation.widget.setCurrentIndex(panel.operation.widget.findData('3'));panel.success.edit.setText('invalid inactive');panel.add_count.edit.setText('invalid inactive');panel.tiers.item(0,1).setText('invalid inactive')
        d=self.page.collect();self.assertEqual(d['attribute_service']['success_bp'],'10000');self.assertEqual(d['attribute_service']['add_count'],'0');self.assertFalse(d['attribute_tiers']);catalog.validate(packet,d,service,TOOL)
        self.page.consumable_kind.setCurrentIndex(0);d=self.page.collect();self.assertIsNone(d['attribute_service']);catalog.validate(packet,d,service,TOOL);self.page.dirty=False

    def test_socket_service_form_switches_ignore_inactive_drafts(self):
        packet,key=create(catalog.blank(TOOL),5,lambda d:socket_service(d,2));self.page.loaded(packet,key)
        self.assertEqual(self.page.consumable_kind.currentData(),'4')
        fields={f.column:f for f in self.page.socket_service_form.fields}
        fields['service_kind'].widget.setCurrentIndex(fields['service_kind'].widget.findData('3'))
        fields['units_per_use'].edit.setText('2')
        self.page.cooldown_check.setChecked(True);self.page.cooldown_edit.setText('invalid inactive')
        self.page.effect_forms[1].fields[0].edit.setText('invalid inactive')
        d=self.page.collect();self.assertEqual(d['socket_service'],dict(service_kind='3',units_per_use='2'))
        self.assertIsNone(d['cooldown']);catalog.validate(packet,d,key,TOOL)
        packet=saved(packet,catalog.Catalog(packet).apply(d,key));self.page.loaded(packet,key)
        self.page.consumable_kind.setCurrentIndex(0);fields['units_per_use'].edit.setText('invalid inactive')
        d=self.page.collect();self.assertIsNone(d['socket_service']);catalog.validate(packet,d,key,TOOL)
        self.page.dirty=False

    def test_pet_training_and_supply_forms_keep_inactive_drafts(self):
        packet,pet=create(catalog.blank(TOOL),15,growth);self.page.loaded(packet,pet)
        panel=self.page.companion_panel.training;self.assertFalse(panel.enabled.isChecked());panel.enabled.setChecked(True)
        document=self.page.collect();catalog.validate(packet,document,pet,TOOL)
        packet=saved(packet,catalog.Catalog(packet).apply(document,pet));self.page.loaded(packet,pet)
        panel.ranks.item(0,2).setText('2.5');document=self.page.collect();self.assertEqual(document['pet_skill_ranks'][0]['value_scaled'],'250');catalog.validate(packet,document,pet,TOOL)
        panel.add_skill(dict(skill_id='2',code='pet.hp_test',effect_kind='1'));panel.ranks.item(0,0).setText('2')
        self.assertEqual(panel.ranks.item(0,3).text(),'HP');panel.ranks.item(0,2).setText('500');self.assertEqual(self.page.collect()['pet_skill_ranks'][0]['value_scaled'],'500')
        panel.ranks.item(0,0).setText('1');self.assertEqual(panel.ranks.item(0,3).text(),'%')
        panel.enabled.setChecked(False);panel.ranks.item(0,2).setText('invalid inactive');document=self.page.collect();self.assertIsNone(document['training']);catalog.validate(packet,document,pet,TOOL)
        d=catalog.Catalog(packet).new(17);d['supply']['supply_kind']='1';d['pet_study']['skill_id']='1';key=key_of(d)
        packet=saved(packet,catalog.Catalog(packet).apply(d),key);self.page.loaded(packet,key);supply=self.page.pet_supply_panel
        supply.known.widget.setChecked(False);supply.delay.edit.setText('invalid inactive');supply.chance.edit.setText('invalid inactive')
        document=self.page.collect();self.assertEqual(document['pet_study']['known_success_bp'],'0');catalog.validate(packet,document,key,TOOL)
        supply.kind.widget.setCurrentIndex(supply.kind.widget.findData('8'));supply.duration.edit.setText('invalid inactive')
        document=self.page.collect();self.assertEqual(document['pet_skin']['duration_ms'],'0');self.assertEqual(document['base']['max_stack'],'1');catalog.validate(packet,document,key,TOOL)
        self.assertFalse(self.page.stack_field.edit.isEnabled());self.page.dirty=False


    def test_pet_hatch_and_feeding_forms_ignore_inactive_fields(self):
        packet,pet=create(catalog.blank(TOOL),15,growth);packet,key=create(packet,8,lambda d:egg(d,pet))
        self.page.loaded(packet,key);hatch=self.page.pet_hatch_panel
        self.assertTrue(self.page.loot_table.isHidden());self.assertTrue(self.page.loot_mode_row.isHidden())
        self.assertFalse(self.page.pet_food_panel.kind.widget.model().item(self.page.pet_food_panel.kind.widget.findData('3')).isEnabled())
        self.assertEqual(hatch.target.currentData(),f'{pet[0]}:{pet[1]}')
        by_field={field.column:field for field in hatch.form.fields};by_field['hp_max'].edit.setText('4.5')
        food=self.page.pet_food_panel;food.cube_enabled.setChecked(True)
        food.cube_consumption.widget.setCurrentIndex(food.cube_consumption.widget.findData('2'))
        food.cube_units.edit.setText('invalid inactive');food.divisor.edit.setText('invalid inactive')
        food.dead_fraction.edit.setText('invalid inactive')
        document=self.page.collect();self.assertEqual(document['hatch']['hp_max'],'45')
        self.assertEqual(document['pet_food_rules'][0]['units'],'0');catalog.validate(packet,document,key,TOOL)
        food.direct_enabled.setChecked(True)
        with self.assertRaises(api.EditorError):self.page.collect()
        food.dead_fraction.edit.setText('50');document=self.page.collect();self.assertEqual(len(document['pet_food_rules']),2);catalog.validate(packet,document,key,TOOL)
        self.page.container_kind_field.widget.setCurrentIndex(self.page.container_kind_field.widget.findData('1'))
        self.assertFalse(self.page.loot_table.isHidden());self.assertFalse(self.page.loot_mode_row.isHidden());self.assertTrue(hatch.isHidden())
        by_field['hp_step'].edit.setText('invalid inactive');fresh=catalog.Catalog(packet).new(8)['loot']
        self.page.loot_table.load(fresh['entries'],fresh['pick_mode'])
        document=self.page.collect();self.assertIsNone(document['hatch']);catalog.validate(packet,document,key,TOOL)
        self.page.dirty=False


    def test_growth_panel_profiles_gates_and_inactive_drafts(self):
        packet,key=create(catalog.blank(TOOL),15);self.page.loaded(packet,key)
        panel=self.page.companion_panel;panel.kind.widget.setCurrentIndex(panel.kind.widget.findData('3'))
        self.assertFalse(self.page.refinement.isEnabled());self.assertFalse(panel.riding.isEnabled())
        g=panel.growth;g.maximum.setValue(120)
        for ceiling in (40,60,80):
            g.stages.item(g.stages.rowCount()-1,1).setText(str(ceiling));g.add_stage()
        g.levels.item(79,1).setText('7999');g.maximum.setValue(30);g.maximum.setValue(120)
        self.assertEqual(g.levels.item(79,1).text(),'7999')
        document=self.page.collect();self.assertEqual(document['refinement']['profile_kind'],'1')
        self.assertEqual(document['growth']['maximum_level'],'120');self.assertEqual(len(document['growth_stages']),4)
        catalog.validate(packet,document,key,TOOL)
        self.assertFalse(g.stages.cellWidget(0,2).isEnabled())
        panel.follower.setCurrentIndex(panel.follower.findData('new'))
        panel.new_follower.fields[0].edit.setText('growth.ui_new_initial')
        document=self.page.collect();self.assertIsNone(document['growth_stages'][0]['form_id'])
        catalog.validate(packet,document,key,TOOL)
        g.levels.item(0,1).setText('bad EXP')
        with self.assertRaises(api.EditorError):self.page.collect()
        panel.kind.widget.setCurrentIndex(panel.kind.widget.findData('1'))
        fixed=self.page.collect();catalog.validate(packet,fixed,key,TOOL)
        self.page.dirty=False


    def test_companion_panel_mount_pet_inactive_fields_and_maximum(self):
        packet,key=create(catalog.blank(TOOL),15);self.page.dirty=False;self.page.loaded(packet,key);panel=self.page.companion_panel
        self.assertFalse(self.page.stack_field.edit.isEnabled());self.assertFalse(self.page.appearance_field.widget.isEnabled())
        self.page.refinement.maximum.setValue(30);panel.kind.widget.setCurrentIndex(1);panel.clock.widget.setCurrentIndex(2);panel.duration.edit.setText('2.5')
        d=self.page.collect();self.assertEqual(d['refinement']['max_level'],'30');self.assertEqual(d['child']['duration_ms'],'2500')
        catalog.validate(packet,d,key,TOOL);validated=catalog.Catalog(packet).apply(d,key)
        self.assertEqual(catalog.Catalog(saved(packet,validated)).item(key)['child']['riding_form_id'],d['child']['form_id'])
        panel.riding.setCurrentIndex(panel.riding.findData('new'))
        panel.new_riding.fields[1].edit.setText('bad');panel.kind.widget.setCurrentIndex(0)
        panel.clock.widget.setCurrentIndex(0);panel.duration.edit.setText('invalid inactive duration')
        d=self.page.collect();self.assertIsNone(d['child']['riding_form_id']);self.assertEqual(d['child']['duration_ms'],'0')
        self.assertIsNone(d['new_riding_form']);catalog.validate(packet,d,key,TOOL)

    def test_soul_panel_levels_scroll_and_inactive_fields(self):
        packet,key=create(catalog.blank(TOOL),14);self.page.dirty=False;self.page.loaded(packet,key);panel=self.page.soul_panel
        self.assertFalse(self.page.stack_field.edit.isEnabled())
        panel.max_level.setValue(30);panel.table.item(11,1).setText('180');panel.table.item(11,9).setText('250')
        panel.max_level.setValue(10);panel.max_level.setValue(30)
        d=self.page.collect();self.assertEqual(d['refinement']['max_level'],'30');self.assertEqual(len(d['soul_levels']),31)
        self.assertEqual(d['soul_levels'][11]['third_multiplier_bp'],'25000');catalog.validate(packet,d,key,TOOL)
        panel.kind.widget.setCurrentIndex(panel.kind.widget.findData('2'));panel.chance.edit.setText('75')
        self.assertTrue(self.page.stack_field.edit.isEnabled());self.assertFalse(panel.channel.widget.isEnabled());self.assertFalse(panel.core.isVisible())
        self.page.stack_field.edit.setText('200');scroll=self.page.collect()
        self.assertEqual(scroll['child']['damage_channel'],None);self.assertEqual(scroll['child']['refine_chance_bp'],'7500')
        self.assertEqual(scroll['refinement']['profile_kind'],'1');self.assertEqual(scroll['refinement']['max_level'],'0');catalog.validate(packet,scroll,key,TOOL)
        panel.chance.edit.setText('hibás, de inaktív');panel.kind.widget.setCurrentIndex(0);self.assertFalse(self.page.stack_field.edit.isEnabled());d=self.page.collect()
        self.assertEqual(d['base']['max_stack'],'1');self.assertEqual(d['refinement']['max_level'],'30');catalog.validate(packet,d,key,TOOL)
        self.page.dirty=False

    def test_polymorph_form_rules_and_book_mode(self):
        packet,key=create(catalog.blank(TOOL),13);self.page.loaded(packet,key);p=self.page.poly_panel
        p.target.setCurrentIndex(1);p.form.setCurrentIndex(p.form.findData('new'))
        document=self.page.collect();catalog.validate(packet,document,key,TOOL)
        packet=saved(packet,catalog.Catalog(packet).apply(document,key));self.page.loaded(packet,key)
        self.assertIsNotNone(catalog.Catalog(packet).item(key)['child']['fixed_form_id'])
        p=self.page.poly_panel;p.kind.widget.setCurrentIndex(p.kind.widget.findData('2'))
        self.assertFalse(self.page.stack_field.edit.isEnabled())
        self.assertTrue(p.skills.isColumnHidden(1));self.assertFalse(p.masteries.isHidden())
        document=self.page.collect();catalog.validate(packet,document,key,TOOL)
        self.assertEqual(document['poly']['max_book_grade'],'30')
        self.assertEqual(document['poly_masteries'][0]['duration_ms'],'10000')
        self.assertEqual(document['poly_masteries'][0]['bonus_duration_ms'],'9000')
        p.masteries.item(1,2).setText('20')
        with self.assertRaises(api.EditorError):catalog.validate(packet,self.page.collect(),key,TOOL)
        self.page.dirty=False

    def test_tool_and_fishing_forms_keep_progression_and_kind_rules(self):
        from PySide6.QtCore import Qt
        packet,key=create(catalog.blank(TOOL),11);self.page.loaded(packet,key)
        panel=self.page.tool_panel;panel.kind.setCurrentIndex(panel.kind.findData('2'))
        self.assertTrue(panel.pool.isEnabled());self.page.refinement.maximum.setValue(30)
        self.assertEqual(panel.table.rowCount(),31)
        panel.table.item(10,1).setText('55');panel.table.item(10,4).setText('1');panel.table.item(10,5).setText('5')
        self.assertFalse(bool(panel.table.item(10,8).flags() & Qt.ItemFlag.ItemIsEditable))
        panel.table.cellWidget(10,7).setCurrentIndex(1);panel.table.item(10,8).setText('2')
        self.assertFalse(bool(panel.table.item(10,9).flags() & Qt.ItemFlag.ItemIsEditable))
        document=self.page.collect();catalog.validate(packet,document,key,TOOL)
        self.assertEqual(document['tool_levels'][10]['power'],'55')
        self.assertEqual(document['tool_levels'][10]['failure_level'],'2')
        self.assertEqual(document['tool_levels'][10]['failure_loss_bp'],'0')
        self.assertEqual(document['tool_levels'][30]['required_points'],'0')
        self.page.refinement.maximum.setValue(10);self.page.refinement.maximum.setValue(30)
        self.assertEqual(self.page.collect()['tool_levels'][10]['power'],'55')
        panel.kind.setCurrentIndex(panel.kind.findData('3'));self.assertFalse(panel.pool.isEnabled())
        document=self.page.collect();self.assertEqual(document['child']['fishing_pool'],'1');catalog.validate(packet,document,key,TOOL)
        packet,cooked=create(packet,5);packet,bait=create(packet,12);self.page.loaded(packet,bait)
        fish=self.page.fishing_panel;self.assertTrue(fish.power.widget.isEnabled())
        fish.kind.widget.setCurrentIndex(fish.kind.widget.findData('2'))
        fish.grill.setCurrentIndex(fish.grill.findData('5:'+str(cooked[1])))
        document=self.page.collect();catalog.validate(packet,document,bait,TOOL)
        self.assertEqual(document['child']['grill_local_id'],str(cooked[1]));self.assertEqual(document['child']['bait_power'],'0')
        fish.kind.widget.setCurrentIndex(fish.kind.widget.findData('1'))
        fish.loot.append(dict(reward_kind='1',item_category_id='5',item_local_id=str(cooked[1]),amount='1',weight='1',rare_attribute_chance_bp='0'))
        self.assertEqual(fish.loot.table.cellWidget(0,0).count(),1)
        self.assertTrue(fish.loot.table.isColumnHidden(4))
        document=self.page.collect();catalog.validate(packet,document,bait,TOOL)
        self.page.dirty=False

    def test_costume_form_visuals_clock_and_bonus_curve(self):
        packet,key=create(catalog.blank(TOOL),4)
        self.page.loaded(packet,key);panel=self.page.costume_panel
        panel.kind.setCurrentIndex(panel.kind.findData('3'))
        self.assertTrue(panel.weapon.isEnabled())
        panel.weapon.setCurrentIndex(panel.weapon.findData('3'))
        panel.clock.setCurrentIndex(panel.clock.findData('3'));panel.duration.setText('604800.123')
        panel.visuals.item(0,2).setText('models/warrior_costume.model')
        self.page.refinement.maximum.setValue(30)
        document=self.page.collect();catalog.validate(packet,document,key,TOOL)
        self.assertEqual(document['child']['duration_ms'],'604800123')
        self.assertEqual(document['child']['weapon_subtype'],'3')
        self.assertEqual(document['refinement']['max_level'],'30')
        self.assertEqual(document['visuals'][0]['model_asset'],'models/warrior_costume.model')
        panel.kind.setCurrentIndex(panel.kind.findData('2'))
        self.assertFalse(panel.weapon.isEnabled())
        panel.clock.setCurrentIndex(panel.clock.findData('1'))
        self.assertFalse(panel.duration.isEnabled())
        document=self.page.collect();catalog.validate(packet,document,key,TOOL)
        self.assertIsNone(document['child']['weapon_subtype'])
        self.assertEqual(document['child']['duration_ms'],'0')
        # Switching the clock back preserves the typed duration until saved.
        panel.clock.setCurrentIndex(panel.clock.findData('4'))
        self.assertEqual(self.page.collect()['child']['duration_ms'],'604800123')
        panel.visuals.item(7,2).setText('')
        with self.assertRaises(api.EditorError):catalog.validate(packet,self.page.collect(),key,TOOL)
        panel.visuals.item(7,2).setText('models/last.model');panel.duration.setText('0.0001')
        with self.assertRaises(api.EditorError):self.page.collect()
        self.page.dirty=False

    def test_socket_stone_form_bonus_curve_and_shard_mode(self):
        packet,key=create(catalog.blank(TOOL),7)
        self.page.loaded(packet);self.page.open_item(key)
        self.assertEqual(self.page.refinement.category,7)
        self.page.refinement.maximum.setValue(30)
        self.page.stone_group_fields[0].widget.setText('stone.test')
        document=self.page.collect()
        self.assertEqual(document['refinement']['max_level'],'30')
        self.assertEqual(len(document['levels']),31)
        self.assertEqual(document['child']['primary_group'],'stone.test')
        catalog.validate(packet,document,key,TOOL)
        self.page.stone_kind_field.widget.setCurrentIndex(self.page.stone_kind_field.widget.findData('2'))
        shard=self.page.collect()
        self.assertEqual(shard['child']['allowed_targets'],'0')
        self.assertEqual(shard['child']['minimum_socket_grade'],'0')
        self.assertEqual(shard['child']['primary_group'],'')
        self.assertEqual(shard['refinement']['max_level'],'0')
        self.assertEqual(shard['bonuses'],[])
        self.assertFalse(self.page.refinement.isEnabled())
        self.assertFalse(self.page.stone_targets_field.widget.isEnabled())
        catalog.validate(packet,shard,key,TOOL)
        self.page.dirty=False

    """Form <-> document round trip of the Qt page."""

    def test_quest_token_fields_and_dispatch_modes_round_trip(self):
        packet,key=create(catalog.blank(TOOL),10)
        self.page.dirty=False;self.page.loaded(packet,key)
        self.assertFalse(self.page.quest_group_field.widget.isEnabled())
        self.page.quest_dispatch_field.widget.setCurrentIndex(2)
        self.assertTrue(self.page.quest_group_field.widget.isEnabled())
        self.assertEqual(self.page.quest_contexts_field.value(),'1')
        self.page.quest_group_field.edit.setText('quest.test_group')
        self.page.quest_contexts_field.load('9')
        document=self.page.collect()
        self.assertEqual(document['child']['dispatch_kind'],'3')
        self.assertEqual(document['child']['group_code'],'quest.test_group')
        catalog.validate(packet,document,key,TOOL)
        self.page.quest_dispatch_field.widget.setCurrentIndex(0)
        token=self.page.collect()
        self.assertEqual(token['child']['group_code'],'')
        self.assertEqual(token['child']['allowed_contexts'],'0')
        self.assertFalse(self.page.quest_observer_field.widget.isEnabled())
        catalog.validate(packet,token,key,TOOL)
        self.page.dirty=False

    def test_form_is_locked_during_background_work_and_copy_is_a_new_draft(self):
        from unittest import mock
        packet, keys = self.fixture()
        self.page.loaded(packet, keys[1])
        self.assertTrue(self.page.tabs.isEnabled())
        with mock.patch.object(self.owner, 'busy', return_value=True):
            self.page.controls()
            self.assertFalse(self.page.tabs.isEnabled())
            self.assertFalse(self.page.save_button.isEnabled())
            self.assertFalse(self.page.copy_button.isEnabled())
        self.page.controls()
        self.assertTrue(self.page.tabs.isEnabled())
        self.page.duplicate()
        self.assertIsNone(self.page.original_key)
        copied = self.page.collect()
        self.assertNotEqual(key_of(copied), keys[1])
        self.assertEqual(copied['child']['physical_attack_base_raw'], '15500')
        catalog.validate(packet, copied, executable=TOOL)
        self.page.discard()

    def test_every_field_round_trips_through_the_form(self):
        packet, keys = self.fixture()
        self.page.loaded(packet)
        self.assertEqual(self.page.list.rowCount(), len(keys))
        for category, key in keys.items():
            self.page.open_item(key)
            self.assertFalse(self.page.dirty)
            document = self.page.collect()
            expected = catalog.Catalog(packet).apply(catalog.Catalog(packet).item(key), key)
            self.assertEqual(ordered(catalog.Catalog(packet).apply(document, key)), ordered(expected), catalog.CATEGORIES[category])
            self.assertEqual(ordered(expected), ordered(packet['tables']))
        self.assertEqual(self.owner.errors, [])

    def test_container_kind_and_pick_mode_switches(self):
        packet, keys = self.fixture()
        self.page.loaded(packet)
        self.page.open_item(keys[8])
        loot = self.page.loot_table
        self.assertTrue(self.page.lock_field.widget.isEnabled())
        self.assertEqual([loot.table.item(row, 5).text() for row in range(3)], ['100%', '12.5%', '50%'])
        # Switching to weighted keeps the stored numbers and shows the resulting share.
        self.page.loot_mode.setCurrentIndex(self.page.loot_mode.findData('1'))
        self.assertEqual(loot.table.cellWidget(1, 3).text(), '1250')
        document = self.page.collect()
        self.assertEqual(document['loot']['pick_mode'], '1')
        self.assertEqual([e['weight'] for e in document['loot']['entries']][:3], ['10000', '1250', '5000'])
        catalog.validate(packet, document, keys[8], TOOL)
        # A quest-opened box drops its built-in contents and its lock.
        self.page.container_kind_field.widget.setCurrentIndex(self.page.container_kind_field.widget.findData('3'))
        self.assertFalse(loot.isEnabled())
        self.assertEqual(self.page.lock_field.edit.text(), '0')
        document = self.page.collect()
        self.assertIsNone(document['loot'])
        packet = saved(packet, catalog.Catalog(packet).apply(document, keys[8]))
        catalog.run_tool(dict(action='validate', tables=packet['tables']), packet['columns'], TOOL)
        self.assertEqual(rows(packet, 'loot_profile'), [])
        # An item reward without a chosen item is caught before the validator runs.
        self.page.loaded(packet, keys[8])
        self.page.container_kind_field.widget.setCurrentIndex(self.page.container_kind_field.widget.findData('1'))
        loot.append(dict(reward_kind='1', amount='1', weight='1'))
        with self.assertRaises(api.EditorError) as raised:
            self.page.collect()
        self.assertIn('válassz tárgyat', str(raised.exception))
        self.page.dirty = False

    def test_automatic_form_controls_stack_supply_and_arena(self):
        packet, keys = self.fixture()
        self.page.loaded(packet, keys[5])
        self.page.delivery_field.widget.setCurrentIndex(3)
        self.assertFalse(self.page.stack_field.edit.isEnabled())
        self.assertFalse(self.page.recovery_contexts_field.boxes[2].isEnabled())
        d = self.page.collect()
        self.assertEqual(d['base']['max_stack'], '1')
        self.assertEqual(d['recovery']['units_per_use'], '1')
        self.assertFalse(int(d['recovery']['allowed_contexts']) & 4)
        catalog.validate(packet, d, keys[5], TOOL)
        self.page.capacity_field.edit.setText('9,5')
        self.page.supply_field.widget.setCurrentIndex(1)
        self.assertFalse(self.page.capacity_field.widget.isEnabled())
        d = self.page.collect()
        self.assertEqual(d['effect']['capacity_raw'], '0')
        catalog.validate(packet, d, keys[5], TOOL)
        self.page.supply_field.widget.setCurrentIndex(0)
        self.assertEqual(self.page.collect()['effect']['capacity_raw'], '9500')
        self.page.supply_field.widget.setCurrentIndex(1)
        packet = saved(packet, catalog.Catalog(packet).apply(d, keys[5]))
        self.page.loaded(packet, keys[5])
        self.assertEqual(ordered(catalog.Catalog(packet).apply(self.page.collect(), keys[5])), ordered(packet['tables']))
        self.page.supply_field.widget.setCurrentIndex(0)
        self.assertTrue(self.page.capacity_field.widget.isEnabled())
        self.assertGreater(int(self.page.collect()['effect']['capacity_raw']), 0)
        self.page.consumable_kind.setCurrentIndex(1)
        self.assertTrue(self.page.stack_field.edit.isEnabled())
        catalog.validate(packet, self.page.collect(), keys[5], TOOL)
        self.page.dirty = False

    def test_arena_quota_form_tracks_context(self):
        packet, keys = self.fixture()
        self.page.loaded(packet, keys[5])
        self.page.recovery_contexts_field.boxes[2].setChecked(True)
        self.assertTrue(self.page.arena_cost_field.widget.isEnabled())
        self.page.arena_cost_field.edit.setText('2')
        d = self.page.collect()
        self.assertEqual(d['recovery']['arena_quota_cost'], '2')
        catalog.validate(packet, d, keys[5], TOOL)
        self.page.recovery_contexts_field.boxes[2].setChecked(False)
        self.assertFalse(self.page.arena_cost_field.widget.isEnabled())
        self.assertEqual(self.page.collect()['recovery']['arena_quota_cost'], '0')
        self.page.dirty = False

    def test_form_edits_reach_the_validator(self):
        packet, keys = self.fixture()
        self.page.loaded(packet)
        self.page.open_item(keys[1])
        field = next(f for f in self.page.category_forms[1].fields if f.column == 'physical_attack_base_raw')
        field.edit.setText('16,25')
        field.edit.textEdited.emit('16,25')
        self.assertTrue(self.page.dirty)
        document = self.page.collect()
        self.assertEqual(document['child']['physical_attack_base_raw'], '16250')
        catalog.validate(packet, document, keys[1], TOOL)
        self.page.refinement.maximum.setValue(5)
        document = self.page.collect()
        self.assertEqual([l['physical_attack_gain_raw'] for l in document['levels']], ['0', '1500', '3000', '4500', '6000', '7500'])
        catalog.validate(packet, document, keys[1], TOOL)
        field.edit.setText('abc')
        with self.assertRaises(api.EditorError) as raised:
            self.page.collect()
        self.assertIn('Fizikai támadás', str(raised.exception))
        self.page.dirty = False

    def test_timed_form_switches_group_rules_and_round_trips(self):
        packet, keys = self.fixture()
        self.page.loaded(packet, keys[5])
        self.page.consumable_kind.setCurrentIndex(1)
        self.assertTrue(self.page.dirty)
        d = self.page.collect()
        self.assertEqual(d['timed']['duration_ms'], '60000')
        self.assertEqual(d['timed_bonuses'], [dict(bonus_id='26', value_scaled='1500')])
        catalog.validate(packet, d, keys[5], TOOL)
        packet = saved(packet, catalog.Catalog(packet).apply(d, keys[5]))
        self.page.loaded(packet, keys[5])
        self.assertFalse(self.page.timed_group_form.isEnabled())
        self.assertEqual(self.page.consumable_kind.currentData(), '2')
        self.assertEqual(ordered(catalog.Catalog(packet).apply(self.page.collect(), keys[5])), ordered(packet['tables']))
        self.page.timed_group_field.widget.setCurrentIndex(0)
        self.assertTrue(self.page.timed_group_form.isEnabled())
        code = next(f for f in self.page.timed_group_form.fields if f.column == 'code')
        code.edit.setText('effect.new_rules')
        d = self.page.collect()
        catalog.validate(packet, d, keys[5], TOOL)
        packet = saved(packet, catalog.Catalog(packet).apply(d, keys[5]))
        self.page.loaded(packet, keys[5])
        self.page.consumable_kind.setCurrentIndex(0)
        d = self.page.collect()
        self.assertIsNone(d['timed'])
        catalog.validate(packet, d, keys[5], TOOL)
        self.page.dirty = False

    def test_toggle_form_round_trip_and_type_switches(self):
        packet, keys = self.fixture()
        self.page.loaded(packet, keys[5])
        self.page.consumable_kind.setCurrentIndex(2)
        self.assertFalse(self.page.stack_field.edit.isEnabled())
        d = self.page.collect()
        self.assertEqual(d['base']['max_stack'], '1')
        self.assertEqual(d['toggle']['duration_ms'], '3600000')
        self.assertEqual(d['toggle']['variant_id'], '1')
        self.assertIsNone(d['recovery'])
        self.assertIsNone(d['timed'])
        catalog.validate(packet, d, keys[5], TOOL)
        packet = saved(packet, catalog.Catalog(packet).apply(d, keys[5]))
        self.page.loaded(packet, keys[5])
        self.assertFalse(self.page.toggle_group_form.isEnabled())
        self.assertEqual(ordered(catalog.Catalog(packet).apply(self.page.collect(), keys[5])), ordered(packet['tables']))
        # Switching an already saved non-recovery item used to collect into None.
        for index in [0, 1, 2, 0]:
            self.page.consumable_kind.setCurrentIndex(index)
            d = self.page.collect()
            catalog.validate(packet, d, keys[5], TOOL)
            self.assertEqual(sum(d.get(k) is not None for k in ('recovery', 'timed', 'toggle')), 1)
        self.page.dirty = False

    def test_book_panel_keeps_step_edits_and_switches_operation(self):
        packet, keys = self.fixture()
        self.page.loaded(packet, keys[9])
        panel = self.page.book_panel
        self.assertFalse(panel.add_target.isEnabled())
        panel.target_mode.widget.setCurrentIndex(1)
        panel.add_target_row()
        self.assertEqual([r['skill_id'] for r in self.page.collect()['book_targets']], ['1', '2'])
        panel.targets.selectRow(1); panel.delete_target()
        panel.target_mode.widget.setCurrentIndex(0)
        panel.add_step()
        self.assertEqual(len(panel.steps), 10)
        panel.delete_step(); panel.add_step()
        self.assertEqual(panel.steps[panel.current]['current_level'], '20')
        edited = self.page.collect()
        packet = saved(packet, catalog.Catalog(packet).apply(edited, keys[9]))
        self.page.loaded(packet, keys[9])
        self.assertEqual(panel.current, 0)
        count = next(f for f in panel.step.fields if f.column == 'required_successes')
        count.edit.setText('3')
        panel.step_list.setCurrentRow(1)
        panel.step_list.setCurrentRow(0)
        self.assertEqual(count.edit.text(), '3')
        document = self.page.collect()
        self.assertEqual(document['book_steps'][0]['required_successes'], '3')
        catalog.validate(packet, document, keys[9], TOOL)
        panel.bonus.widget.setCurrentIndex(0)
        no_bonus = self.page.collect()
        self.assertTrue(all(r['boosted_chance_bp'] == r['success_chance_bp'] for r in no_bonus['book_steps']))
        catalog.validate(packet, no_bonus, keys[9], TOOL)
        panel.bonus.widget.setCurrentIndex(1)
        self.assertEqual(self.page.collect()['book_steps'][0]['boosted_chance_bp'], '10000')
        count.edit.setText('bad')
        panel.step_list.setCurrentRow(1)
        self.assertEqual(panel.current, 0)
        self.assertTrue(panel.problem.text())
        count.edit.setText('3')
        panel.operation.widget.setCurrentIndex(1)
        document = self.page.collect()
        self.assertEqual(document['book']['operation_kind'], '2')
        self.assertEqual(document['book_targets'][0]['refund_pool'], '1')
        catalog.validate(packet, document, keys[9], TOOL)
        panel.operation.widget.setCurrentIndex(0)
        self.assertEqual(self.page.collect()['book_steps'][0]['required_successes'], '3')
        panel.operation.widget.setCurrentIndex(1)
        self.assertEqual(len(self.page.collect()['book_steps']), 19)
        panel.operation.widget.setCurrentIndex(2)
        self.assertFalse(panel.targets.isEnabled())
        document = self.page.collect()
        self.assertEqual(document['book']['operation_kind'], '3')
        self.assertFalse(document['book_targets'])
        catalog.validate(packet, document, keys[9], TOOL)
        packet = saved(packet, catalog.Catalog(packet).apply(document, keys[9]))
        self.page.loaded(packet, keys[9])
        self.assertEqual(ordered(catalog.Catalog(packet).apply(self.page.collect(), keys[9])), ordered(packet['tables']))
        self.page.dirty = False





class RefinementPanelTests(PageCase):
    """New levels inherit the top level; the linear fill reaches level bonuses."""

    def test_plus_button_keeps_level_bonus_and_fill_increases_it(self):
        from item_page import FillDialog
        packet, keys = self.fixture()
        self.page.loaded(packet)
        self.page.open_item(keys[1])
        panel = self.page.refinement
        crit = next(str(d['id']) for d in packet['bonuses'] if d['code'] == 'critical.chance')
        panel.maximum.setValue(1)
        panel.levels[1]['bonuses'] = [dict(bonus_id=crit, value_scaled='100')]
        panel.update_button(1)
        panel.table.item(1, 1).setText('2')  # typed, not yet saved
        for _ in range(3):
            panel.maximum.stepUp()
        self.assertEqual(panel.maximum.value(), 4)
        self.assertEqual(panel.table.item(1, 1).text(), '2')
        for level in range(2, 5):
            self.assertEqual(panel.levels[level]['bonuses'], [dict(bonus_id=crit, value_scaled='100')])
            self.assertEqual(panel.table.item(level, 1).text(), '2')
            self.assertEqual(panel.table.cellWidget(level, 3).text(), 'Kritikus esély: 1 %')
        targets = panel.fill_targets()
        self.assertEqual(targets[0]['kind'], 'bonus')
        dialog = FillDialog(panel, targets, 4, panel.current_value)
        self.assertEqual((dialog.start.value(), dialog.value.text()), (1, '1'))
        dialog.step.setText('0,5')
        panel.apply_fill(*dialog.values())
        self.assertEqual([panel.current_value(targets[0], level) for level in range(1, 5)], [100, 150, 200, 250])
        stat = next(t for t in targets if t['kind'] == 'stat' and t['column'] == 1)
        dialog = FillDialog(panel, targets, 4, panel.current_value)
        dialog.target.setCurrentIndex(targets.index(stat))
        dialog.start.setValue(1); dialog.value.setText('2'); dialog.step.setText('1,5')
        panel.apply_fill(*dialog.values())
        self.assertEqual([panel.table.item(level, 1).text() for level in range(5)], ['0', '2', '3.5', '5', '6.5'])
        dialog.step.setText('-5')
        with self.assertRaises(api.EditorError):
            dialog.values()
        document = self.page.collect()
        catalog.validate(packet, document, keys[1], TOOL)
        saved_packet = saved(packet, catalog.Catalog(packet).apply(document, keys[1]))
        reopened = catalog.Catalog(saved_packet).item(keys[1])
        self.assertEqual([l['bonuses'] for l in reopened['levels'][1:]],
                         [[dict(bonus_id=crit, value_scaled=str(v))] for v in (100, 150, 200, 250)])
        self.assertEqual([l['physical_attack_gain_raw'] for l in reopened['levels']], ['0', '2000', '3500', '5000', '6500'])
        self.page.dirty = False

    def test_fill_dialog_previews_live_keeps_on_close_and_reverts_on_request(self):
        from PySide6.QtCore import QTimer
        from PySide6.QtWidgets import QDialogButtonBox
        from item_page import FillDialog
        packet, keys = self.fixture()
        self.page.loaded(packet)
        self.page.open_item(keys[1])
        panel = self.page.refinement
        percent = next(str(d['id']) for d in packet['bonuses'] if d['code'] == 'physical_attack.percent')
        panel.maximum.setValue(1)
        panel.levels[1]['bonuses'] = [dict(bonus_id=percent, value_scaled='1500')]
        panel.update_button(1)
        for _ in range(5):
            panel.maximum.stepUp()
        self.page.dirty = False
        seen = {}

        def labels():
            return [panel.table.cellWidget(level, 3).text() for level in range(1, 7)]

        def interact(close, step='1'):
            def run():
                dialog = next(w for w in self.app.topLevelWidgets() if isinstance(w, FillDialog) and w.isVisible())
                dialog.step.setText(step)
                dialog.step.textEdited.emit(step)
                seen['live'] = labels()
                seen['dirty_while_open'] = self.page.dirty
                box = dialog.findChild(QDialogButtonBox)
                seen['buttons'] = [button.text() for button in box.buttons()]
                close(dialog, box)
            QTimer.singleShot(200, run)

        expected = ['Fizikai támadás %%: %d %%' % value for value in range(15, 21)]
        original = ['Fizikai támadás %: 15 %'] * 6
        interact(lambda dialog, box: dialog.revert_button.click())
        panel.fill()
        self.assertEqual(seen['live'], expected)
        self.assertFalse(seen['dirty_while_open'])
        self.assertEqual(sorted(seen['buttons']), ['Kész', 'Visszavonás'])
        self.assertEqual(labels(), original)
        self.assertFalse(self.page.dirty)
        # Closing with the window's X (or Esc) keeps what the table shows.
        interact(lambda dialog, box: dialog.close())
        panel.fill()
        self.assertEqual(labels(), expected)
        self.assertTrue(self.page.dirty)
        interact(lambda dialog, box: box.button(QDialogButtonBox.StandardButton.Ok).click(), step='2')
        panel.fill()
        expected = ['Fizikai támadás %%: %d %%' % value for value in range(15, 26, 2)]
        self.assertEqual(labels(), expected)
        document = self.page.collect()
        self.assertEqual([l['bonuses'][0]['value_scaled'] for l in document['levels'][1:]], [str(v * 100) for v in range(15, 26, 2)])
        catalog.validate(packet, document, keys[1], TOOL)
        self.page.dirty = False

    def test_step_set_while_only_plus_one_exists_continues_on_plus(self):
        from PySide6.QtCore import QTimer
        from PySide6.QtWidgets import QDialogButtonBox
        from item_page import FillDialog
        packet, keys = self.fixture()
        self.page.loaded(packet)
        self.page.open_item(keys[1])
        panel = self.page.refinement
        pve = next(str(d['id']) for d in packet['bonuses'] if d['code'] == 'pve.outgoing')
        panel.maximum.setValue(1)
        panel.levels[1]['bonuses'] = [dict(bonus_id=pve, value_scaled='300')]
        panel.update_button(1)
        seen = {}

        def dialog_then(action):
            def run():
                dialog = next(w for w in self.app.topLevelWidgets() if isinstance(w, FillDialog) and w.isVisible())
                action(dialog)
                dialog.findChild(QDialogButtonBox).button(QDialogButtonBox.StandardButton.Ok).click()
            QTimer.singleShot(200, run)

        def set_step(dialog):
            dialog.step.setText('1'); dialog.step.textEdited.emit('1')
        dialog_then(set_step)
        panel.fill()  # only +1 exists: the step has nowhere to go yet
        self.assertEqual(panel.levels[1]['bonuses'], [dict(bonus_id=pve, value_scaled='300')])
        panel.maximum.stepUp(); panel.maximum.stepUp()
        values = [panel.bonus_value(level, pve) for level in range(1, 4)]
        self.assertEqual(values, [300, 400, 500])
        self.assertEqual(panel.table.cellWidget(3, 3).text(), 'Sebzés szörnyek ellen: 5 %')
        dialog_then(lambda dialog: seen.update(step=dialog.step.text(), value=dialog.value.text()))
        panel.fill()
        self.assertEqual((seen['step'], seen['value']), ('1', '3'))
        self.assertEqual([panel.bonus_value(level, pve) for level in range(1, 4)], [300, 400, 500])
        catalog.validate(packet, self.page.collect(), keys[1], TOOL)
        # Without a remembered step, "+" continues the last two levels.
        self.page.open_item(keys[2])
        self.page.open_item(keys[1])
        panel = self.page.refinement
        self.assertEqual(panel.rules, {})
        panel.maximum.setValue(2)
        panel.levels[1]['bonuses'] = [dict(bonus_id=pve, value_scaled='1500')]
        panel.levels[2]['bonuses'] = [dict(bonus_id=pve, value_scaled='2000')]
        panel.table.item(1, 1).setText('2'); panel.table.item(2, 1).setText('3,5')
        panel.maximum.stepUp()
        self.assertEqual(panel.bonus_value(3, pve), 2500)
        self.assertEqual(panel.table.item(3, 1).text(), '5')
        self.page.dirty = False

    def test_equal_inherited_bonuses_share_one_profile_until_changed(self):
        packet, keys = self.fixture()
        self.page.loaded(packet)
        self.page.open_item(keys[3])
        panel = self.page.refinement
        crit = next(str(d['id']) for d in packet['bonuses'] if d['code'] == 'critical.chance')
        panel.maximum.setValue(1)
        panel.levels[1]['bonuses'] = [dict(bonus_id=crit, value_scaled='50')]
        packet = saved(packet, catalog.Catalog(packet).apply(self.page.collect(), keys[3]))
        self.page.loaded(packet, keys[3])
        before = len(rows(packet, 'bonus_profile'))
        # Pressing "+" on a saved level reuses its bonus profile while equal.
        panel.maximum.setValue(3)
        document = self.page.collect()
        catalog.validate(packet, document, keys[3], TOOL)
        packet = saved(packet, catalog.Catalog(packet).apply(document, keys[3]))
        self.assertEqual(len(rows(packet, 'bonus_profile')), before)
        self.page.loaded(packet, keys[3])
        panel.levels[3]['bonuses'] = [dict(bonus_id=crit, value_scaled='75')]
        document = self.page.collect()
        packet = saved(packet, catalog.Catalog(packet).apply(document, keys[3]))
        self.check(packet)
        model = catalog.Catalog(packet).item(keys[3])
        self.assertEqual([l['bonuses'][0]['value_scaled'] for l in model['levels'][1:]], ['50', '50', '75'])
        self.assertEqual(len(rows(packet, 'bonus_profile')), before + 1)
        self.page.dirty = False

    def check(self, packet):
        return catalog.run_tool(dict(action='validate', tables=packet['tables']), packet['columns'], TOOL)['hash']


class SqlCommitTests(unittest.TestCase):
    def test_zzzzzzzzzzzzz_instance_attributes_atomic_copy_and_rollback(self):
        packet=self.call('read');d=catalog.Catalog(packet).new(1);attributes(d);key=key_of(d);packet=self.commit(packet,d)
        original=catalog.Catalog(packet).item(key);clone=catalog.Catalog(packet).duplicate(key);clone['attribute_values'][0]['value_scaled']='3500';packet=self.commit(packet,clone)
        self.assertEqual(catalog.Catalog(packet).item(key),original)
        service=catalog.Catalog(packet).new(5);attribute_service(service);service_key=key_of(service);packet=self.commit(packet,service)
        for table in ('attribute_value','attribute_tier_chance','item_attribute_binding'):
            self.sandbox.client.query("CREATE TRIGGER protos.ic_reject_attributes BEFORE INSERT ON protos."+table+" FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='test attribute write failure'")
            try:
                if table=='attribute_tier_chance':fresh=catalog.Catalog(packet).duplicate(service_key);fresh['attribute_tiers'][0]['chance_bp']='3900'
                else:fresh=catalog.Catalog(packet).duplicate(key);fresh['attribute_values'][0]['value_scaled']='4900'
                with self.assertRaises(api.EditorError):self.commit(packet,fresh)
            finally:self.sandbox.client.query('DROP TRIGGER protos.ic_reject_attributes')
            after=self.call('read');self.assertEqual(after['hash'],packet['hash']);self.assertEqual(after['sequences'],packet['sequences']);self.assertEqual(ordered(after['tables']),ordered(packet['tables']))
        edited=catalog.Catalog(packet).item(service_key);edited.update(attribute_service=None,socket_service=dict(service_kind='1',units_per_use='1'));packet=self.commit(packet,edited,service_key)
        self.assertIsNone(catalog.Catalog(packet).item(service_key)['attribute_service'])

    def test_zzzzzzzzzzzz_socket_service_commit_copy_and_atomic_rollback(self):
        packet=self.call('read');d=catalog.Catalog(packet).new(5);socket_service(d,2);key=key_of(d)
        packet=self.commit(packet,d);original=catalog.Catalog(packet).item(key)
        clone=catalog.Catalog(packet).duplicate(key);clone['socket_service']['units_per_use']='3'
        packet=self.commit(packet,clone);self.assertEqual(catalog.Catalog(packet).item(key),original)
        for table in ('socket_service_profile','item_consumable'):
            self.sandbox.client.query("CREATE TRIGGER protos.ic_reject_socket_service BEFORE INSERT ON protos."+table+" FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='test socket service write failure'")
            try:
                fresh=catalog.Catalog(packet).new(5);socket_service(fresh,3);fresh['socket_service']['units_per_use']='7'
                with self.assertRaises(api.EditorError):self.commit(packet,fresh)
            finally:self.sandbox.client.query('DROP TRIGGER protos.ic_reject_socket_service')
            after=self.call('read');self.assertEqual(after['hash'],packet['hash']);self.assertEqual(after['sequences'],packet['sequences'])
            self.assertEqual(ordered(after['tables']),ordered(packet['tables']))
        original['socket_service']['service_kind']='1';packet=self.commit(packet,original,key)
        self.assertEqual(catalog.Catalog(packet).item(key)['socket_service']['service_kind'],'1')

    def test_zzzzzzzzzzz_pet_lifecycle_profiles_supply_and_rollback(self):
        packet=self.call('read');d=catalog.Catalog(packet).new(15);growth(d);d.update(catalog.Catalog.training_defaults());pet=key_of(d);packet=self.commit(packet,d)
        original=catalog.Catalog(packet).item(pet)
        clone=catalog.Catalog(packet).duplicate(pet);clone['pet_skill_ranks'][0]['value_scaled']='250';packet=self.commit(packet,clone)
        self.assertEqual(catalog.Catalog(packet).item(pet),original)
        supplies=[]
        for kind in range(1,9):
            d=catalog.Catalog(packet).new(17);d['supply']['supply_kind']=str(kind);d['pet_study']['skill_id']='1';d['pet_skin']['form_id']=original['child']['form_id']
            supplies.append(key_of(d));packet=self.commit(packet,d)
        for table in ('pet_skill_rank','pet_enchant_rule'):
            self.sandbox.client.query("CREATE TRIGGER protos.ic_reject_lifecycle BEFORE INSERT ON protos."+table+" FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='test lifecycle write failure'")
            try:
                if table=='pet_skill_rank':
                    fresh=catalog.Catalog(packet).duplicate(pet);fresh['pet_skill_ranks'][1]['value_scaled']='299'
                else:
                    fresh=catalog.Catalog(packet).new(17);fresh['supply']['supply_kind']='6';fresh['pet_enchant']['step']='7'
                with self.assertRaises(api.EditorError):self.commit(packet,fresh)
            finally:self.sandbox.client.query('DROP TRIGGER protos.ic_reject_lifecycle')
            after=self.call('read');self.assertEqual(after['hash'],packet['hash']);self.assertEqual(after['sequences'],packet['sequences']);self.assertEqual(ordered(after['tables']),ordered(packet['tables']))
        d=catalog.Catalog(packet).item(supplies[0]);d['supply']['supply_kind']='8';d['pet_skin']=dict(form_id=original['child']['form_id'],clock_kind='4',duration_ms='10000')
        packet=self.commit(packet,d,supplies[0]);saved_document=catalog.Catalog(packet).item(supplies[0]);self.assertIsNone(saved_document['pet_study']);self.assertEqual(saved_document['pet_skin']['duration_ms'],'10000')


    def test_zzzzzzzzzz_pet_hatch_food_copy_and_atomic_rollback(self):
        packet=self.call('read');pet=catalog.Catalog(packet).new(15);growth(pet,3);pet_key=key_of(pet);packet=self.commit(packet,pet)
        document=catalog.Catalog(packet).new(8);egg(document,pet_key);key=key_of(document)
        document['pet_food_rules']=[food_rule(),food_rule('2')];packet=self.commit(packet,document)
        original=catalog.Catalog(packet).item(key)
        clone=catalog.Catalog(packet).duplicate(key);other=key_of(clone);clone['hatch']['hp_max']='45';packet=self.commit(packet,clone)
        self.assertEqual(catalog.Catalog(packet).item(key),original)
        self.assertEqual(len(catalog.Catalog(packet).item(other)['pet_food_rules']),2)
        self.sandbox.client.query("CREATE TRIGGER protos.ic_reject_pet_food BEFORE INSERT ON protos.pet_feed_rule FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='test pet food write failure'")
        try:
            fresh=catalog.Catalog(packet).new(8);egg(fresh,pet_key);fresh['hatch']['gold_cost']='123456';fresh['pet_food_rules']=[food_rule()]
            with self.assertRaises(api.EditorError):self.commit(packet,fresh)
        finally:self.sandbox.client.query('DROP TRIGGER protos.ic_reject_pet_food')
        after=self.call('read');self.assertEqual(after['hash'],packet['hash']);self.assertEqual(after['sequences'],packet['sequences'])
        self.assertEqual(ordered(after['tables']),ordered(packet['tables']))
        changed=catalog.Catalog(packet).item(other);changed['child']['container_kind']='1';changed['loot']=catalog.Catalog(packet).new(8)['loot'];changed['pet_food_rules']=[]
        packet=self.commit(packet,changed,other);self.assertIsNone(catalog.Catalog(packet).item(other)['child']['hatch_profile_id'])
        self.assertEqual(catalog.Catalog(packet).item(key),original)


    def test_zzzzzzzzz_growth_profile_copy_and_rollback(self):
        packet=self.call('read');d=catalog.Catalog(packet).new(15);growth(d,120);key=key_of(d)
        d['new_companion_form']=dict(code='growth.sql_young',mob_template_id='34041',model_asset='models/growth_young.model')
        packet=self.commit(packet,d);original=catalog.Catalog(packet).item(key)
        self.assertEqual(len(original['growth_levels']),120)
        clone=catalog.Catalog(packet).duplicate(key);other=key_of(clone);clone['growth_levels'][0]['mob_experience']='2500'
        packet=self.commit(packet,clone);self.assertEqual(catalog.Catalog(packet).item(key),original)
        self.assertNotEqual(catalog.Catalog(packet).item(other)['child']['growth_profile_id'],original['child']['growth_profile_id'])
        self.sandbox.client.query("CREATE TRIGGER protos.ic_reject_growth BEFORE INSERT ON protos.growth_pet_stage FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='test growth write failure'")
        try:
            fresh=catalog.Catalog(packet).new(15);growth(fresh,60)
            fresh['new_companion_form']=dict(code='growth.sql_rollback',mob_template_id='34042',model_asset='models/growth_adult.model')
            with self.assertRaises(api.EditorError):self.commit(packet,fresh)
        finally:self.sandbox.client.query('DROP TRIGGER protos.ic_reject_growth')
        after=self.call('read');self.assertEqual(after['hash'],packet['hash']);self.assertEqual(after['sequences'],packet['sequences'])
        for table in ('companion_form','growth_pet_profile','growth_pet_level','growth_pet_stage','item_companion'):
            self.assertEqual(rows(after,table),rows(packet,table))


    def test_zzzzzzzz_companion_copy_immutable_forms_and_rollback(self):
        packet=self.call('read');d=catalog.Catalog(packet).new(15);key=key_of(d)
        d['new_companion_form']=dict(code='companion.sql_pet',mob_template_id='34001',model_asset='models/pet.model')
        packet=self.commit(packet,d);original=catalog.Catalog(packet).item(key)
        clone=catalog.Catalog(packet).duplicate(key);other=key_of(clone)
        clone['child'].update(companion_kind='2',clock_kind='3',duration_ms='5000');clone['riding_same_form']=True
        clone['new_companion_form']=dict(code='companion.sql_mount',mob_template_id='20110',model_asset='models/mount.model')
        packet=self.commit(packet,clone);self.assertEqual(catalog.Catalog(packet).item(key),original)
        changed=catalog.Catalog(packet).item(other);changed['child'].update(companion_kind='1',riding_form_id=None,clock_kind='1',duration_ms='0')
        packet=self.commit(packet,changed,other);self.assertIsNone(catalog.Catalog(packet).item(other)['child']['riding_form_id'])
        altered=copy.deepcopy(packet['tables']);col=packet['columns']['companion_form'].index('mob_template_id');altered['companion_form'][0][col]='20199'
        with self.assertRaises(api.EditorError):self.call('commit',expected_hash=packet['hash'],tables=altered)
        self.sandbox.client.query("CREATE TRIGGER protos.ic_reject_companion BEFORE INSERT ON protos.item_companion FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='test companion write failure'")
        try:
            fresh=catalog.Catalog(packet).new(15);fresh['new_companion_form']=dict(code='companion.rollback',mob_template_id='20111',model_asset='models/rollback.model')
            with self.assertRaises(api.EditorError):self.commit(packet,fresh)
        finally:self.sandbox.client.query('DROP TRIGGER protos.ic_reject_companion')
        after=self.call('read');self.assertEqual(after['hash'],packet['hash']);self.assertEqual(after['sequences'],packet['sequences'])
        self.assertEqual(rows(after,'companion_form'),rows(packet,'companion_form'))

    def test_zzzzzzz_soul_profile_copy_and_atomic_rollback(self):
        packet=self.call('read');d=catalog.Catalog(packet).new(14);key=key_of(d)
        d['refinement']['max_level']='30'
        d['levels']=[dict(level=str(i),**{k:'0' for k in catalog.GAINS},extra_bonus_profile_id=None,bonuses=[]) for i in range(31)]
        d['soul_levels']=[catalog.Catalog.soul_level(i) for i in range(31)]
        d['soul_levels'][30].update(charge_capacity='180',strikes_per_cycle='1000',initial_strikes='1000',recharge_interval_ms='5000',third_multiplier_bp='25000')
        packet=self.commit(packet,d);original=catalog.Catalog(packet).item(key)
        self.assertEqual(original['soul_levels'][30]['recharge_interval_ms'],'5000')
        clone=catalog.Catalog(packet).duplicate(key);other=key_of(clone);clone['child']['damage_channel']='2';clone['soul_levels'][30]['third_multiplier_bp']='30000'
        packet=self.commit(packet,clone);self.assertEqual(catalog.Catalog(packet).item(key),original)
        self.assertNotEqual(catalog.Catalog(packet).item(other)['child']['profile_id'],original['child']['profile_id'])
        changed=catalog.Catalog(packet).item(other);changed['child'].update(soul_kind='2',damage_channel=None,profile_id=None,refine_chance_bp='2500')
        changed['refinement'].update(profile_kind='1',max_level='0');changed['levels']=changed['levels'][:1];changed['soul_levels']=[]
        packet=self.commit(packet,changed,other)
        self.assertEqual(catalog.Catalog(packet).item(other)['child']['profile_id'],None)
        self.assertEqual(catalog.Catalog(packet).item(key),original)
        restored=catalog.Catalog(packet).item(other);restored['child'].update(soul_kind='1',damage_channel='2',refine_chance_bp='0')
        restored['refinement'].update(profile_kind='8',max_level='0');restored['soul_levels']=[catalog.Catalog.soul_level(0)]
        packet=self.commit(packet,restored,other)
        self.assertEqual(catalog.Catalog(packet).item(other)['child']['damage_channel'],'2')
        self.assertEqual(catalog.Catalog(packet).item(key),original)
        scroll=catalog.Catalog(packet).new(14);scroll['child'].update(soul_kind='2',damage_channel=None,profile_id=None,refine_chance_bp='7500')
        scroll['base']['max_stack']='200';scroll['refinement'].update(profile_kind='1',max_level='0');scroll['soul_levels']=[]
        packet=self.commit(packet,scroll);self.assertEqual(catalog.Catalog(packet).item(key_of(scroll))['child']['refine_chance_bp'],'7500')
        self.sandbox.client.query("CREATE TRIGGER protos.ic_reject_soul BEFORE INSERT ON protos.soul_level FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='test Soul write failure'")
        try:
            fresh=catalog.Catalog(packet).new(14)
            with self.assertRaises(api.EditorError):self.commit(packet,fresh)
        finally:self.sandbox.client.query('DROP TRIGGER protos.ic_reject_soul')
        after=self.call('read');self.assertEqual(after['hash'],packet['hash']);self.assertEqual(after['sequences'],packet['sequences'])

    def test_zzzzzz_polymorph_commit_copy_immutable_forms_and_rollback(self):
        packet=self.call('read');d=catalog.Catalog(packet).new(13);key=key_of(d)
        d.update(poly_target_fixed=True,new_poly_form=dict(code='form.sql_wolf',mob_template_id='101',book_bonus='4'))
        packet=self.commit(packet,d);original=catalog.Catalog(packet).item(key)
        clone=catalog.Catalog(packet).duplicate(key);clone['poly_skills'][40]['duration_ms']='3000000'
        other=key_of(clone);packet=self.commit(packet,clone);self.assertEqual(catalog.Catalog(packet).item(key),original)
        book=catalog.Catalog(packet).item(other)
        book['poly'].update(polymorph_kind='2',mob_level_grace='0',grace_loss_numerator='0',grace_loss_denominator='1',max_book_grade='30',practice_per_grade='50',book_chance_base_percent='-5')
        book['base']['max_stack']='1'
        for row in book['poly_skills']:row.update(duration_ms='0',attack_bonus_bp='0')
        book['poly_masteries']=[dict(mastery=str(i),duration_ms=str(10000+i*5000),bonus_duration_ms=str(9000+i*5000)) for i in range(4)]
        packet=self.commit(packet,book,other)
        reread=catalog.Catalog(packet).item(other)
        self.assertEqual(reread['poly']['book_chance_base_percent'],'-5')
        self.assertEqual(reread['poly_masteries'][3]['duration_ms'],'25000')
        self.assertEqual(catalog.Catalog(packet).item(key),original)
        altered=copy.deepcopy(packet['tables']);col=packet['columns']['polymorph_form'].index('mob_template_id');altered['polymorph_form'][0][col]='102'
        with self.assertRaises(api.EditorError):self.call('commit',tables=altered,expected_hash=packet['hash'])
        self.assertEqual(self.call('read')['hash'],packet['hash'])
        self.sandbox.client.query("CREATE TRIGGER protos.ic_reject_poly BEFORE INSERT ON protos.polymorph_skill_rule FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='test polymorph write failure'")
        try:
            new=catalog.Catalog(packet).new(13);new['poly_skills'][0]['duration_ms']='123000'
            with self.assertRaises(api.EditorError):self.commit(packet,new)
        finally:self.sandbox.client.query('DROP TRIGGER protos.ic_reject_poly')
        after=self.call('read');self.assertEqual(after['hash'],packet['hash']);self.assertEqual(after['sequences'],packet['sequences'])

    def test_zzzzz_gathering_profiles_fish_and_rollback(self):
        packet=self.call('read');d=catalog.Catalog(packet).new(11);key=key_of(d)
        d['refinement']['max_level']='30'
        d['levels']=[dict(level=str(i),**{k:'0' for k in catalog.GAINS},extra_bonus_profile_id=None,bonuses=[]) for i in range(31)]
        d['tool_levels']=[catalog.Catalog.tool_level(i,i==30) for i in range(31)]
        d['tool_levels'][10].update(power='55',required_points='100',practice_numerator='1',practice_denominator='5',failure_kind='2',failure_level='2')
        packet=self.commit(packet,d);original=catalog.Catalog(packet).item(key)
        clone=catalog.Catalog(packet).duplicate(key);clone['tool_levels'][0]['power']='77'
        packet=self.commit(packet,clone);self.assertEqual(catalog.Catalog(packet).item(key),original)
        self.assertEqual(original['tool_levels'][10]['failure_level'],'2')
        bait=catalog.Catalog(packet).new(12);packet=self.commit(packet,bait)
        cooked=catalog.Catalog(packet).new(5);cooked_key=key_of(cooked);packet=self.commit(packet,cooked)
        dead=catalog.Catalog(packet).new(12);dead_key=key_of(dead)
        dead['child'].update(fishing_kind='2',bait_power='0',grill_category_id='5',grill_local_id=str(cooked_key[1]));packet=self.commit(packet,dead)
        live=catalog.Catalog(packet).new(12);live_key=key_of(live);live['child'].update(fishing_kind='1',bait_power='0')
        live['loot']=dict(pick_mode='1',entries=[dict(entry_no='1',reward_kind='1',item_category_id='12',item_local_id=str(dead_key[1]),amount='1',weight='31',rare_attribute_chance_bp='0')])
        packet=self.commit(packet,live)
        self.assertEqual(catalog.Catalog(packet).item(live_key)['loot']['entries'][0]['item_local_id'],str(dead_key[1]))
        new=catalog.Catalog(packet).new(11)
        self.sandbox.client.query("CREATE TRIGGER protos.ic_reject_tool BEFORE INSERT ON protos.tool_level FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='test tool write failure'")
        try:
            with self.assertRaises(api.EditorError):self.commit(packet,new)
        finally:self.sandbox.client.query('DROP TRIGGER protos.ic_reject_tool')
        after=self.call('read');self.assertEqual(after['hash'],packet['hash']);self.assertEqual(after['sequences'],packet['sequences'])

    def test_zzzz_costume_visual_edit_copy_and_atomic_rollback(self):
        packet=self.call('read');d=catalog.Catalog(packet).new(4);key=key_of(d)
        d['child'].update(costume_kind='3',weapon_subtype='1',clock_kind='2',duration_ms='100000')
        packet=self.commit(packet,d);original=catalog.Catalog(packet).item(key)
        clone=catalog.Catalog(packet).duplicate(key);other=key_of(clone)
        clone['visuals'][0]['model_asset']='models/costume_sql_clone.model'
        packet=self.commit(packet,clone)
        self.assertEqual(catalog.Catalog(packet).item(key),original)
        updated=catalog.Catalog(packet).item(other)
        updated['visuals'][0]['model_asset']='models/costume_sql_edited.model'
        packet=self.commit(packet,updated,other)
        self.assertEqual(catalog.Catalog(packet).item(other)['visuals'][0]['model_asset'],'models/costume_sql_edited.model')
        new=catalog.Catalog(packet).new(4)
        self.sandbox.client.query("CREATE TRIGGER protos.ic_reject_costume BEFORE INSERT ON protos.item_costume_visual FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='test costume visual write failure'")
        try:
            with self.assertRaises(api.EditorError):self.commit(packet,new)
        finally:self.sandbox.client.query('DROP TRIGGER protos.ic_reject_costume')
        after=self.call('read');self.assertEqual(after['hash'],packet['hash']);self.assertEqual(after['sequences'],packet['sequences'])

    def test_zzz_stone_native_commit_curve_and_failed_write_rollback(self):
        packet=self.call('read');d=catalog.Catalog(packet).new(7);key=key_of(d)
        d['child'].update(primary_group='stone.sql_test',secondary_group='stone.sql_second')
        d['refinement']['max_level']='30'
        d['levels']=[dict(level=str(i),**{k:'0' for k in catalog.GAINS},extra_bonus_profile_id=None,
                          bonuses=[dict(bonus_id='21',value_scaled=str(i*10))] if i else []) for i in range(31)]
        packet=self.commit(packet,d);original=catalog.Catalog(packet).item(key)
        clone=catalog.Catalog(packet).duplicate(key);other=key_of(clone)
        clone['levels'][10]['bonuses'][0]['value_scaled']='2000'
        packet=self.commit(packet,clone)
        self.assertEqual(catalog.Catalog(packet).item(key),original)
        self.assertEqual(catalog.Catalog(packet).item(other)['refinement']['max_level'],'30')
        new=catalog.Catalog(packet).new(7)
        self.sandbox.client.query("CREATE TRIGGER protos.ic_reject_stone BEFORE INSERT ON protos.item_socket_stone FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='test stone write failure'")
        try:
            with self.assertRaises(api.EditorError):self.commit(packet,new)
        finally:self.sandbox.client.query('DROP TRIGGER protos.ic_reject_stone')
        after=self.call('read');self.assertEqual(after['hash'],packet['hash']);self.assertEqual(after['sequences'],packet['sequences'])

    def test_zz_quest_native_commit_copy_and_rollback(self):
        packet=self.call('read');document=catalog.Catalog(packet).new(10);key=key_of(document)
        packet=self.commit(packet,document);original=catalog.Catalog(packet).item(key)
        clone=catalog.Catalog(packet).duplicate(key);other=key_of(clone)
        clone['child'].update(dispatch_kind='2',group_code='quest.sql_group',allowed_contexts='1',allow_observer='0')
        packet=self.commit(packet,clone)
        self.assertEqual(catalog.Catalog(packet).item(key),original)
        self.assertEqual(catalog.Catalog(packet).item(other)['child']['group_code'],'quest.sql_group')
        bad=catalog.Catalog(packet).item(other);bad['child']['group_code']='../script.lua'
        with self.assertRaises(api.EditorError):self.commit(packet,bad,other)
        self.assertEqual(self.call('read')['hash'],packet['hash'])
        new=catalog.Catalog(packet).new(10)
        self.sandbox.client.query("CREATE TRIGGER protos.ic_reject_quest BEFORE INSERT ON protos.item_quest FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='test quest write failure'")
        try:
            with self.assertRaises(api.EditorError):self.commit(packet,new)
        finally:self.sandbox.client.query('DROP TRIGGER protos.ic_reject_quest')
        after=self.call('read');self.assertEqual(after['hash'],packet['hash']);self.assertEqual(after['sequences'],packet['sequences'])

    @classmethod
    def setUpClass(cls):
        if not MARIADB:
            raise unittest.SkipTest('Pass --mariadb-bin for the private MariaDB commit test')
        from backup_sandbox import Sandbox
        from mariadb_client import statements
        cls.sandbox = Sandbox(Path(WORK) / uuid.uuid4().hex, MARIADB)
        cls.sandbox.start()
        cls.sandbox.verify()
        for path in sorted((ROOT / 'db/migrations/item_combat').glob('[0-9][0-9][0-9]_*.sql')):
            for statement in statements(path.read_text(encoding='utf-8')):
                cls.sandbox.client.query(statement)
        cls.auth = dict(port=cls.sandbox.port, user='root', password=cls.sandbox.password)

    @classmethod
    def tearDownClass(cls):
        cls.sandbox.stop()

    def call(self, action, **extra):
        return catalog.run_tool(dict(action=action, connection=self.auth, **extra), executable=TOOL)

    def commit(self, packet, document, original_key=None, expected=None):
        tables = catalog.Catalog(packet).apply(document, original_key)
        return self.call('commit', tables=tables, expected_hash=expected or packet['hash'])

    def test_create_edit_retire_and_reject_stale_saves(self):
        packet = self.call('read')
        self.assertEqual(packet['hash'], catalog.blank(TOOL)['hash'])
        created = {}
        for category in catalog.CATEGORIES:
            document = catalog.Catalog(packet).new(category)
            packet = self.commit(packet, document)
            created[category] = key_of(document)
        self.assertEqual(packet['sequences'], {**{str(i): '1' for i in range(1, 19)}, **{str(c): '2' for c in catalog.CATEGORIES}})
        stale = packet
        document = catalog.Catalog(packet).item(created[1])
        document['base']['name_key'] = 'item.sword.renamed'
        document['child']['physical_attack_base_raw'] = '42000'
        packet = self.commit(packet, document, created[1])
        reread = self.call('read')
        self.assertEqual(reread['hash'], packet['hash'])
        self.assertEqual(catalog.Catalog(reread).item(created[1])['child']['physical_attack_base_raw'], '42000')
        # A save built on an older read must not overwrite the newer content.
        old = catalog.Catalog(stale).item(created[2])
        old['base']['min_level'] = '9'
        with self.assertRaises(api.EditorError) as raised:
            self.commit(stale, old, created[2])
        self.assertIn('modositotta', str(raised.exception))
        # Another client creates a material first: the older read cannot
        # issue the same local ID again, and a skipped ID is refused natively.
        other = catalog.Catalog(reread).new(6)
        reread = self.commit(reread, other)
        late = catalog.Catalog(stale).new(6)
        self.assertEqual(key_of(late), key_of(other))
        with self.assertRaises(api.EditorError):
            self.commit(stale, late)
        with self.assertRaises(api.EditorError) as raised:
            catalog.Catalog(reread).apply(late)
        self.assertIn('foglalt', str(raised.exception))
        skipped = copy.deepcopy(reread)
        skipped['sequences']['6'] = '9'
        with self.assertRaises(api.EditorError):
            self.commit(skipped, catalog.Catalog(skipped).new(6))
        self.assertEqual(self.call('read')['sequences']['6'], '3')
        retired = catalog.Catalog(reread).item(created[6])
        retired['base']['retired'] = '1'
        packet = self.commit(reread, retired, created[6])
        self.assertEqual(catalog.Catalog(packet).item(created[6])['base']['retired'], '1')
        # Physical deletion of an issued key is refused.
        tables = copy.deepcopy(packet['tables'])
        tables['item_material'] = []
        tables['item_base'] = [row for row in tables['item_base'] if row[0] != '6']
        with self.assertRaises(api.EditorError):
            self.call('commit', tables=tables, expected_hash=packet['hash'])
        self.assertEqual(self.call('read')['hash'], packet['hash'])

    def test_timed_effect_sql_commit_replace_type_and_group_guard(self):
        packet = self.call('read')
        document = catalog.Catalog(packet).new(5)
        document.update(catalog.Catalog.timed_defaults(document['base']['local_id']))
        key = key_of(document)
        document['timed']['duration_ms'] = str(2 ** 63 - 1)
        document['timed_bonuses'] += [dict(bonus_id='29', value_scaled='7'), dict(bonus_id='33', value_scaled='2500')]
        packet = self.commit(packet, document)
        actual = catalog.Catalog(self.call('read')).item(key)
        self.assertEqual(actual['timed_bonuses'], document['timed_bonuses'])
        self.assertEqual(actual['timed']['duration_ms'], str(2 ** 63 - 1))
        group_id = actual['timed']['group_id']
        # The native writer also protects group identity, independent of the UI.
        model = catalog.Catalog(packet)
        model.row('timed_effect_group', group_id, 'group_id')['clock_kind'] = '2'
        with self.assertRaises(api.EditorError):
            self.call('commit', tables=model.encode(), expected_hash=packet['hash'])
        self.assertEqual(self.call('read')['hash'], packet['hash'])
        clone = catalog.Catalog(packet).duplicate(key)
        other = key_of(clone)
        packet = self.commit(packet, clone)
        document = catalog.Catalog(packet).item(other)
        document['timed_bonuses'][0]['value_scaled'] = '4500'
        document['cooldown'] = dict(duration_ms='1250')
        packet = self.commit(packet, document, other)
        self.assertEqual(catalog.Catalog(packet).item(key), actual)
        document = catalog.Catalog(packet).item(other)
        defaults = catalog.Catalog(packet).new(5)
        document.update(timed=None, recovery=defaults['recovery'], effect=defaults['effect'], cooldown=None)
        packet = self.commit(packet, document, other)
        self.assertIsNone(catalog.Catalog(packet).item(other)['timed'])
        document = catalog.Catalog(packet).item(other)
        document.update(catalog.Catalog.timed_defaults(document['base']['local_id']))
        packet = self.commit(packet, document, other)
        self.assertIsNotNone(catalog.Catalog(packet).item(other)['timed'])

    def test_z_automatic_and_arena_commit_type_switch_and_capacity(self):
        packet = self.call('read')
        document = catalog.Catalog(packet).new(5)
        automatic(document)
        key = key_of(document)
        packet = self.commit(packet, document)
        before = catalog.Catalog(packet).item(key)
        self.assertEqual(before['effect']['capacity_raw'], '5000000')
        clone = catalog.Catalog(packet).duplicate(key)
        other = key_of(clone)
        clone['effect'].update(supply_kind='2', capacity_raw='0', pool_kind='2')
        packet = self.commit(packet, clone)
        self.assertEqual(catalog.Catalog(packet).item(key), before)
        document = catalog.Catalog(packet).item(other)
        old = document['child']['recovery_profile_id']
        defaults = catalog.Catalog(packet).new(5)
        document.update(recovery=defaults['recovery'], effect=defaults['effect'])
        document['recovery']['arena_quota_cost'] = '3'
        packet = self.commit(packet, document, other)
        self.assertEqual(catalog.Catalog(packet).item(other)['recovery']['arena_quota_cost'], '3')
        self.assertIsNone(catalog.Catalog(packet).row('automatic_recovery_profile', old))
        document = catalog.Catalog(packet).item(key)
        document['base']['max_stack'] = '2'
        with self.assertRaises(api.EditorError): self.commit(packet, document, key)
        self.assertEqual(self.call('read')['hash'], packet['hash'])

    def test_invalid_document_writes_nothing(self):
        packet = self.call('read')
        document = catalog.Catalog(packet).new(2)
        document['child']['equip_slot'] = '1'
        with self.assertRaises(api.EditorError) as raised:
            self.commit(packet, document)
        self.assertIn('Semmi nem lett mentve', str(raised.exception))
        self.assertEqual(self.call('read')['hash'], packet['hash'])

    def test_full_refinement_curve_with_unique_bonuses_commits_atomically(self):
        packet = self.call('read')
        document = catalog.Catalog(packet).new(1)
        definitions = {d['code']: str(d['id']) for d in packet['bonuses']}
        bonus = definitions['physical_attack.flat']
        document['refinement']['max_level'] = '255'
        document['levels'] = [dict(level=str(level), physical_attack_gain_raw=str(level * 1000),
                                  magical_attack_gain_raw='0', physical_defense_gain_raw='0', magical_defense_gain_raw='0',
                                  extra_bonus_profile_id=None, bonuses=[] if level == 0 else
                                  [dict(bonus_id=definitions[code], value_scaled=str(level * scale)) for code, scale in
                                   [('physical_attack.flat', 2000), ('magical_attack.flat', 3000), ('critical.chance', 1)]]) for level in range(256)]
        key = key_of(document)
        catalog.validate(packet, document, executable=TOOL)
        packet = self.commit(packet, document)
        loaded = catalog.Catalog(packet).item(key)
        self.assertEqual(len(loaded['levels']), 256)
        self.assertEqual(next(e['value_scaled'] for e in loaded['levels'][255]['bonuses'] if e['bonus_id']==bonus), '510000')
        self.assertEqual(len(loaded['levels'][255]['bonuses']), 3)
        # Updating a complete curve and removing its private bonus profiles
        # must obey the same transaction bounds without partial writes.
        for level in loaded['levels']:
            level['physical_attack_gain_raw'] = str(int(level['level']) * 3000)
            for entry in level['bonuses']: entry['value_scaled'] = str(int(entry['value_scaled']) * 2)
        packet = self.commit(packet, loaded, key)
        loaded = catalog.Catalog(packet).item(key)
        self.assertEqual(loaded['levels'][255]['physical_attack_gain_raw'], '765000')
        loaded['refinement']['max_level'] = '0'
        loaded['levels'] = loaded['levels'][:1]
        packet = self.commit(packet, loaded, key)
        self.assertEqual(len(catalog.Catalog(packet).item(key)['levels']), 1)

    def test_database_rejection_rolls_back_batched_profiles_and_id_allocation(self):
        packet = self.call('read')
        document = catalog.Catalog(packet).new(1)
        self.sandbox.client.query("CREATE TRIGGER protos.ic_reject_weapon BEFORE INSERT ON protos.item_weapon "
                                  "FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='test only failure'")
        try:
            with self.assertRaises(api.EditorError): self.commit(packet, document)
        finally:
            self.sandbox.client.query('DROP TRIGGER protos.ic_reject_weapon')
        result = self.call('read')
        self.assertEqual(result['hash'], packet['hash'])
        self.assertEqual(result['sequences'], packet['sequences'])

    def test_z_toggle_sql_round_trip_replacement_and_group_guard(self):
        packet = self.call('read')
        document = catalog.Catalog(packet).new(5)
        toggle(document)
        document['toggle'].update(duration_ms=str(2 ** 63 - 1), requirements='3', variant_id='65535')
        document['cooldown'] = dict(duration_ms='1250')
        key = key_of(document)
        packet = self.commit(packet, document)
        original = catalog.Catalog(self.call('read')).item(key)
        self.assertEqual(original['toggle']['duration_ms'], str(2 ** 63 - 1))
        self.assertEqual(original['toggle']['requirements'], '3')
        self.assertEqual(original['toggle']['variant_id'], '65535')
        model = catalog.Catalog(packet)
        model.row('toggle_effect_group', original['toggle']['group_id'], 'group_id')['remove_on_death'] = '1'
        with self.assertRaises(api.EditorError):
            self.call('commit', tables=model.encode(), expected_hash=packet['hash'])
        self.assertEqual(self.call('read')['hash'], packet['hash'])
        clone = catalog.Catalog(packet).duplicate(key)
        other = key_of(clone)
        packet = self.commit(packet, clone)
        document = catalog.Catalog(packet).item(other)
        document['toggle_bonuses'][0]['value_scaled'] = '2500'
        packet = self.commit(packet, document, other)
        self.assertEqual(catalog.Catalog(packet).item(key), original)
        document = catalog.Catalog(packet).item(other)
        document.update(catalog.Catalog.timed_defaults(other[1]))
        document['toggle'] = None
        packet = self.commit(packet, document, other)
        self.assertIsNone(catalog.Catalog(packet).item(other)['toggle'])
        document = catalog.Catalog(packet).item(other)
        fresh = catalog.Catalog(packet).new(5)
        document.update(timed=None, recovery=fresh['recovery'], effect=fresh['effect'])
        packet = self.commit(packet, document, other)
        self.assertIsNotNone(catalog.Catalog(packet).item(other)['recovery'])
        self.assertEqual(catalog.Catalog(packet).item(key), original)

    def test_z_skill_book_full_rules_commit_copy_and_operation_change(self):
        packet = self.call('read')
        document = catalog.Catalog(packet).new(9)
        key = key_of(document)
        document['book'].update(target_mode='2', allowed_masteries='7')
        document['book_targets'] = [dict(skill_id=str(i), refund_pool='0', forbidden_empire='0') for i in range(1,255)]
        document['book_steps'] = [dict(document['book_steps'][0], current_level=str(i), next_level=str(i+1),
                                       required_successes='65535', experience_cost=str(2 ** 63 - 1),
                                       delay_minimum_ms=str(2 ** 63 - 1), delay_maximum_ms=str(2 ** 63 - 1)) for i in range(40)]
        packet = self.commit(packet, document)
        actual = catalog.Catalog(self.call('read')).item(key)
        self.assertEqual(len(actual['book_targets']), 254)
        self.assertEqual(len(actual['book_steps']), 40)
        self.assertEqual(actual['book_steps'][39]['experience_cost'], str(2 ** 63 - 1))
        clone = catalog.Catalog(packet).duplicate(key)
        other = key_of(clone)
        packet = self.commit(packet, clone)
        d = catalog.Catalog(packet).item(other)
        d.update(catalog.Catalog.book_defaults('forget'))
        packet = self.commit(packet, d, other)
        self.assertEqual(catalog.Catalog(packet).item(key), actual)
        d = catalog.Catalog(packet).item(other)
        self.assertEqual(len(d['book_steps']), 19)
        d.update(catalog.Catalog.book_defaults('horse'))
        packet = self.commit(packet, d, other)
        self.assertEqual(catalog.Catalog(packet).item(key), actual)
        self.assertFalse(catalog.Catalog(packet).item(other)['book_targets'])
        # A storage rejection leaves profile rows and key allocation unchanged.
        d = catalog.Catalog(packet).new(9)
        self.sandbox.client.query("CREATE TRIGGER protos.ic_reject_book BEFORE INSERT ON protos.item_skill_book "
                                  "FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='test only book failure'")
        try:
            with self.assertRaises(api.EditorError): self.commit(packet, d)
        finally:
            self.sandbox.client.query('DROP TRIGGER protos.ic_reject_book')
        after = self.call('read')
        self.assertEqual(after['hash'], packet['hash'])
        self.assertEqual(after['sequences'], packet['sequences'])




    def test_z_refinement_recipe_commit_validation_and_rollback(self):
        packet=self.call('read')
        document=catalog.Catalog(packet).new(1)
        document['refinement']['max_level']='30'
        document['levels']=[dict(level=str(i),physical_attack_gain_raw=str(i*1000),magical_attack_gain_raw='0',
                                 physical_defense_gain_raw='0',magical_defense_gain_raw='0',extra_bonus_profile_id=None,bonuses=[]) for i in range(31)]
        weapon=('1',document['base']['local_id'])
        packet=self.commit(packet,document)
        material=catalog.Catalog(packet).new(6);material_key=('6',material['base']['local_id'])
        packet=self.commit(packet,material)
        model=catalog.Catalog(packet)
        recipe=dict(recipe_id='1',code='refine.sql_test',source_category_id=weapon[0],source_local_id=weapon[1],source_level='9',
                    target_category_id=weapon[0],target_local_id=weapon[1],target_level='10',gold_cost='5000',
                    success_chance_bp='7500',failure_kind='2',failure_level=None)
        model.tables['refinement_recipe'].append(recipe)
        model.tables['refinement_ingredient'].append(dict(recipe_id='1',category_id=material_key[0],local_id=material_key[1],refinement_level='0',amount='7'))
        previous=packet['hash']
        packet=self.call('commit',tables=model.encode(),expected_hash=previous)
        self.assertNotEqual(packet['hash'],previous)
        self.assertEqual(catalog.Catalog(packet).tables['refinement_ingredient'][0]['amount'],'7')
        # Refine recipe data now participates in native validation, commit and
        # retirement rules; an item edit cannot leave an orphan recipe behind.
        changed=catalog.Catalog(packet).item(tuple(map(int,material_key)));changed['base']['retired']='1'
        with self.assertRaises(api.EditorError):self.commit(packet,changed,tuple(map(int,material_key)))
        self.assertEqual(self.call('read')['hash'],packet['hash'])
        model=catalog.Catalog(packet);model.tables['refinement_recipe'][0]['gold_cost']='6000'
        changed=self.call('commit',tables=model.encode(),expected_hash=packet['hash'])
        self.assertNotEqual(changed['hash'],packet['hash'])
        # An identical retry is idempotent; a different stale edit must fail.
        self.assertEqual(self.call('commit',tables=model.encode(),expected_hash=packet['hash'])['hash'],changed['hash'])
        model.tables['refinement_recipe'][0]['gold_cost']='6500'
        with self.assertRaises(api.EditorError):self.call('commit',tables=model.encode(),expected_hash=packet['hash'])
        self.assertEqual(self.call('read')['hash'],changed['hash'])
        model=catalog.Catalog(changed);model.tables['refinement_recipe']=[];model.tables['refinement_ingredient']=[]
        removed=self.call('commit',tables=model.encode(),expected_hash=changed['hash'])
        self.assertEqual(catalog.Catalog(removed).tables['refinement_recipe'],[])



def main():
    global TOOL, MARIADB, WORK
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--tool', type=Path, required=True)
    parser.add_argument('--mariadb-bin', type=Path)
    parser.add_argument('--work-dir', type=Path)
    args, rest = parser.parse_known_args()
    TOOL = args.tool.resolve()
    MARIADB = args.mariadb_bin
    WORK = args.work_dir or Path(__file__).resolve().parents[2] / 'build/catalog-editor-sql-isolated'
    unittest.main(argv=[sys.argv[0], *rest], verbosity=2)


if __name__ == '__main__':
    main()
