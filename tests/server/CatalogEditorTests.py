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


class OfflineModelTests(unittest.TestCase):
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

    def test_unit_conversion_is_exact(self):
        self.assertEqual(catalog.scaled('12,5'), 12500)
        self.assertEqual(catalog.display(12500), '12.5')
        self.assertEqual(catalog.display(12000), '12')
        with self.assertRaises(api.EditorError):
            catalog.scaled('0.0001')


class PageCase(unittest.TestCase):
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
        for category, change in ((1, weapon), (2, armor), (3, accessory), (5, potion), (6, material), (8, chest), (16, token),
                                 (18, lambda d: d['child'].update(lock_id='12'))):
            packet, keys[category] = create(packet, category, change)
        return packet, keys

class EditorPageTests(PageCase):
    """Form <-> document round trip of the Qt page."""

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

    def test_invalid_document_writes_nothing(self):
        packet = self.call('read')
        document = catalog.Catalog(packet).new(2)
        document['child']['equip_slot'] = '1'
        with self.assertRaises(api.EditorError) as raised:
            self.commit(packet, document)
        self.assertIn('Semmi nem lett mentve', str(raised.exception))
        self.assertEqual(self.call('read')['hash'], packet['hash'])


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
