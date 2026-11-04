"""Qt form/source/navigation and native-validation dispatch without services."""
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from types import SimpleNamespace
from unittest import mock

os.environ.setdefault('QT_QPA_PLATFORM', 'offscreen')
REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / 'SRC/Tools/ServerManager'))
from PySide6.QtWidgets import QApplication
import backend as api
from gathering_rules_form import GatheringRulesForm
from item_page import ItemPage
from window import Editor


def document():
    return dict(version=1, sites=[dict(id=1, name='Tó', map=41, race=9009,
        kind=2, pool=2, range=400, duration_ms=15000, minimum_power=1,
        power_step=2, base_chance_bp=1234, power_chance_bp=56,
        bait_chance_bp=78, rewards=[dict(category=12, local=6601, count=2, weight=3)])])


class GatheringEditorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls): cls.app = QApplication.instance() or QApplication([])

    def setUp(self):
        self.form = GatheringRulesForm(); self.errors = []
        self.form.error.connect(self.errors.append); self.form.set_value(document())

    def tearDown(self): self.form.close(); self.form.deleteLater(); self.app.processEvents()

    def test_exact_roundtrip_and_isolated_copy(self):
        expected = document(); self.form.set_value(expected); expected['sites'][0]['name'] = 'Más'
        self.assertEqual(self.form.value, document())
        returned = self.form.value; returned['sites'].clear()
        self.assertEqual(len(self.form.value['sites']), 1)

    def test_water_conversion_uses_server_contract(self):
        self.form.mode.setCurrentIndex(1)
        result = self.form.value
        self.assertEqual(result['version'], 2)
        self.assertEqual((result['sites'][0]['race'], result['sites'][0]['kind'], result['sites'][0]['pool']), (0, 2, 2))
        self.assertFalse(self.form.kind.isEnabled()); self.assertFalse(self.form.inputs['race'].isEnabled())
        self.form.mode.setCurrentIndex(0); self.form.kind.setCurrentIndex(0)
        result = self.form.value['sites'][0]
        self.assertEqual((result['race'], result['kind'], result['pool'], result['bait_chance_bp']), (1, 1, 1, 0))

    def test_new_water_defaults_include_pool_and_minimum_duration(self):
        self.form.add_site(); self.form.inputs['duration_ms'].setValue(1)
        self.form.mode.setCurrentIndex(1)
        row = self.form.value['sites'][1]
        self.assertEqual((row['kind'], row['pool'], row['duration_ms']), (2, 2, 5000))
        self.assertEqual(row['rewards'], [])  # Content author must choose a real reward.

    def test_large_ids_and_exact_percentage_seconds(self):
        self.form.inputs['id'].setText('4294967295')
        self.form.inputs['race'].setText('4000000000')
        self.form.inputs['duration_ms'].setValue(5.123)
        self.form.inputs['base_chance_bp'].setValue(12.34)
        result = self.form.value['sites'][0]
        self.assertEqual((result['id'], result['race'], result['duration_ms'], result['base_chance_bp']),
            (4294967295, 4000000000, 5123, 1234))

    def test_bad_numeric_input_blocks_switch_without_losing_current_site(self):
        self.form.add_site(); self.form.sites.setCurrentRow(0)
        self.form.inputs['id'].setText('4294967296'); self.form.sites.setCurrentRow(1)
        self.assertEqual(self.form.sites.currentRow(), 0); self.assertEqual(len(self.errors), 1)
        self.form.inputs['id'].setText('1'); self.form.sites.setCurrentRow(1)
        self.assertEqual(self.form.sites.currentRow(), 1)

    def test_edit_selection_preserves_each_site(self):
        self.form.name.setText('Átnevezett tó'); self.form.add_site()
        self.form.name.setText('Érclelőhely'); self.form.sites.setCurrentRow(0)
        self.assertEqual([site['name'] for site in self.form.value['sites']], ['Átnevezett tó', 'Érclelőhely'])
        self.form.delete_site(); self.assertEqual([site['name'] for site in self.form.value['sites']], ['Érclelőhely'])
        self.form.delete_site(); self.assertEqual(self.form.value['sites'], []); self.assertFalse(self.form.detail.isEnabled())

    def test_multiple_rewards_and_invalid_draft(self):
        self.form.add_reward()
        with self.assertRaises(api.EditorError): _ = self.form.value
        self.form.rewards.item(1, 1).setText('8801'); self.form.rewards.item(1, 2).setText('7')
        result = self.form.value['sites'][0]['rewards']
        self.assertEqual(result[1], dict(category=6, local=8801, count=7, weight=1))
        self.form.delete_reward(); self.assertEqual(self.form.value['sites'][0]['rewards'], document()['sites'][0]['rewards'])

    def test_source_values_are_rejected_without_silent_clamping_or_downgrade(self):
        for key, value in [('version', 3), ('id', -1), ('duration_ms', 0), ('range', 1001),
            ('base_chance_bp', 10001), ('kind', []), ('kind', True), ('pool', False), ('map', 1.5)]:
            bad = document()
            if key == 'version': bad[key] = value
            else: bad['sites'][0][key] = value
            with self.subTest(key=key, value=value), self.assertRaises(api.EditorError): self.form.set_value(bad)
        self.assertEqual(self.form.value, document())

    def test_unknown_source_fields_are_not_erased(self):
        bad = document(); bad['sites'][0]['unknown'] = 7
        with self.assertRaises(api.EditorError): self.form.set_value(bad)
        bad = document(); bad['sites'][0]['rewards'][0]['unknown'] = 7
        with self.assertRaises(api.EditorError): self.form.set_value(bad)

    def test_programmatic_load_does_not_mark_dirty(self):
        changes = []; self.form.changed.connect(lambda: changes.append(True))
        self.form.set_value(document()); self.assertEqual(changes, [])
        self.form.name.setText('Másik tó'); self.assertEqual(changes, [True])

    def test_editor_form_source_roundtrip_and_unsupported_source_repair(self):
        owner = SimpleNamespace(error=self.errors.append, refresh=lambda: None, target_id=lambda: ('test',))
        editor = Editor(owner)
        try:
            def load(value):
                return SimpleNamespace(relative='data/typed_gathering.json', text=json.dumps(value),
                    encoding='utf-8', newline='\n', is_json=True, value=value, parse_error=None)
            self.assertTrue(editor.display(load(document()))); self.assertIs(editor.active_fields(), editor.gathering_form)
            editor.gathering_form.name.setText('Szerkesztett tó'); editor.tabs.setCurrentIndex(1)
            self.assertEqual(json.loads(editor.source.toPlainText())['sites'][0]['name'], 'Szerkesztett tó')
            repaired = document(); repaired['sites'][0]['map'] = 42
            editor.source.setPlainText(json.dumps(repaired)); editor.tabs.setCurrentIndex(0)
            self.assertEqual(json.loads(editor.text()), repaired)
            future = document(); future['version'] = 3
            self.assertTrue(editor.display(load(future))); self.assertEqual(editor.tabs.currentIndex(), 1)
            self.assertEqual(json.loads(editor.text()), future)
            editor.tabs.setCurrentIndex(0); self.assertEqual(editor.tabs.currentIndex(), 1)
            self.assertTrue(self.errors)
        finally: editor.code.shutdown(); editor.close(); editor.deleteLater()

    def test_item_page_shortcut_obeys_existing_dirty_file_guard(self):
        load = mock.Mock(return_value=False); navigate = mock.Mock()
        owner = SimpleNamespace(busy=lambda: False, data_page=SimpleNamespace(editor=SimpleNamespace(load=load)),
            navigation=SimpleNamespace(setCurrentRow=navigate))
        page = SimpleNamespace(owner=owner, local=lambda: True)
        ItemPage.show_gathering(page); navigate.assert_not_called()
        load.return_value = True; ItemPage.show_gathering(page)
        load.assert_called_with('data/typed_gathering.json'); navigate.assert_called_once_with(2)

    def test_backend_uses_native_catalog_aware_validation(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory); candidate = root / 'candidate.json'; candidate.write_text(json.dumps(document()), encoding='utf-8')
            with mock.patch.object(api, 'editable_path', return_value=root / 'data/typed_gathering.json'), \
                mock.patch('item_bench.connection', return_value=(root, None, None, {'test': True})), \
                mock.patch('item_catalog.run_tool', side_effect=[{'tables': {'snapshot': True}, 'columns': {}}, {'sites': 1}]) as native:
                self.assertIn('1 gyűjtőhely', api.validate_candidate(root, 'data/typed_gathering.json', candidate))
                self.assertEqual(native.call_args.args[0], dict(action='validate_gathering', tables={'snapshot': True}, rules_json=candidate.read_text(encoding='utf-8')))
                self.assertEqual(native.call_count, 2)

    def test_backend_never_falls_back_when_database_or_native_rejects(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory); candidate = root / 'candidate.json'; candidate.write_text('{}')
            with mock.patch.object(api, 'editable_path', return_value=root / 'data/typed_gathering.json'), \
                mock.patch('item_bench.connection', side_effect=api.EditorError('Adatbázis nem fut.')):
                with self.assertRaisesRegex(api.EditorError, 'Adatbázis'): api.validate_candidate(root, 'data/typed_gathering.json', candidate)


if __name__ == '__main__': unittest.main()
