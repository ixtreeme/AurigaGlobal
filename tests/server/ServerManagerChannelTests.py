"""Channel topology, real native config publication and lean runtime ownership."""
import copy
import json
import os
from pathlib import Path
import shutil
import sys
import tempfile
import unittest
from unittest import mock

os.environ.setdefault('QT_QPA_PLATFORM', 'offscreen')
REPO = Path(__file__).resolve().parents[2]
sys.path[:0] = [str(REPO / 'SRC/Tools/ServerManager'), str(REPO / 'SRC/Tools')]
import backend as api
import channel_config as topology
from server_package import manage


def config():
    value = json.loads((REPO / 'config/server.json.example').read_text(encoding='utf-8'))
    value['game_defaults']['gameplay'] = {'user_limit': 32768, 'nested': {'flag': True}}
    return value


class ChannelModelTests(unittest.TestCase):
    def test_new_channel_inherits_settings_and_skips_idle_cores(self):
        before = config()
        before['game_servers']['ch1-idle'] = copy.deepcopy(before['game_servers']['ch1-core1'])
        before['game_servers']['ch1-idle'].update(map_allow=[], network={'port': 31000, 'p2p_port': 31001})
        before['game_servers']['ch1-idle']['server']['server_id'] = 90
        original = copy.deepcopy(before)
        result, names = topology.add_channel(before, 7, source=1)
        self.assertEqual(len(names), 1)
        new = result['game_servers'][names[0]]
        self.assertEqual(new['map_allow'], before['game_servers']['ch1-core1']['map_allow'])
        self.assertEqual(set(new), {'server', 'network', 'map_allow', 'working_directory'})
        self.assertEqual(set(new['server']), {'hostname', 'channel', 'server_id'})
        self.assertNotIn(new['network']['port'], topology.used_ports(before))
        self.assertEqual(topology.effective(result, names[0])['gameplay'], before['game_defaults']['gameplay'])
        self.assertEqual(before, original)

    def test_map_transfer_is_channel_local_and_allocates_unique_identity(self):
        before, _ = topology.add_channel(config(), 2, source=1)
        result, name = topology.add_core(before, 1, [21], transfer=True)
        self.assertNotIn(21, result['game_servers']['ch1-core1']['map_allow'])
        self.assertIn(21, result['game_servers'][name]['map_allow'])
        self.assertEqual(result['game_servers']['ch2-core1'], before['game_servers']['ch2-core1'])
        self.assertEqual(len({c['server']['server_id'] for c in result['game_servers'].values()}), 3)
        with self.assertRaises(api.EditorError): topology.add_core(before, 1, [21])

    def test_invalid_requests_preserve_original(self):
        before = config(); original = copy.deepcopy(before)
        cases = [lambda: topology.add_channel(before, 1, source=1),
                 lambda: topology.add_channel(before, 256, source=1),
                 lambda: topology.add_channel(before, 3, maps=[]),
                 lambda: topology.add_core(before, 1, list(range(1, 66))),
                 lambda: topology.add_core(before, 1, [21, 21]),
                 lambda: topology.add_core(before, 5, [21]),
                 lambda: topology.remove(before, channel=1)]
        for operation in cases:
            with self.subTest(operation=operation), self.assertRaises(api.EditorError): operation()
        self.assertEqual(before, original)

    def test_ambiguous_template_is_rejected_instead_of_copying_duplicate_maps(self):
        before = config()
        before['game_servers']['other'] = copy.deepcopy(before['game_servers']['ch1-core1'])
        with self.assertRaisesRegex(api.EditorError, 'több core'):
            topology.add_channel(before, 2, source=1)

    def test_remove_does_not_change_shared_or_other_channel_data(self):
        before, names = topology.add_channel(config(), 7, source=1)
        result = topology.remove(before, channel=7)
        self.assertEqual(result, config())
        result, second = topology.add_core(before, 7, [301])
        result = topology.remove(result, name=second)
        self.assertEqual(result, before)

    def test_default_start_follows_remaining_channel_after_deleting_ch1(self):
        value, names = topology.add_channel(config(), 7, source=1)
        value = topology.remove(value, channel=1)
        self.assertEqual(manage.select_instances(value, None), ['database', *names, 'login'])
        self.assertEqual(manage.select_instances(value, None, all_channels=True), ['database', *names, 'login'])

    def test_compaction_preserves_effective_settings_unknown_fields_and_overrides(self):
        before = config()
        core = before['game_servers']['ch1-core1']
        core['server']['max_level'] = before['game_defaults']['server']['max_level']
        core['gameplay'] = copy.deepcopy(before['game_defaults']['gameplay'])
        core['unknown'] = {'empty': {}}
        core['gameplay']['nested']['override'] = 2
        after = topology.compact(before)
        self.assertEqual(topology.effective(before, 'ch1-core1'), topology.effective(after, 'ch1-core1'))
        self.assertNotIn('max_level', after['game_servers']['ch1-core1']['server'])
        self.assertEqual(after['game_servers']['ch1-core1']['gameplay'], {'nested': {'override': 2}})
        self.assertEqual(after['game_servers']['ch1-core1']['unknown'], {'empty': {}})

    def test_export_has_only_public_endpoints_and_updates_after_entry_removal(self):
        before, _ = topology.add_channel(config(), 99, maps=[301])
        before, name = topology.add_core(before, 1, [21], transfer=True)
        before = topology.remove(before, name='ch1-core1')
        packet = topology.client_manifest(before)
        self.assertEqual([row['id'] for row in packet['channels']], [1])
        self.assertEqual(packet['channels'][0]['port'], before['game_servers'][name]['network']['port'])
        self.assertEqual(packet['mark_port'], before['game_servers']['ch99-core1']['network']['port'])
        self.assertEqual(set(packet), {'schema_version', 'auth_port', 'mark_port', 'channels'})
        self.assertNotIn('password', json.dumps(packet))

    def test_runtime_only_seeds_game_inputs_and_skips_disabled_guild_marks(self):
        with tempfile.TemporaryDirectory(prefix='auriga-channel-runtime-') as temp:
            root = Path(temp)
            defaults = root / 'runtime_defaults'; defaults.mkdir()
            for name in ('CMD', 'BANIP', 'CRC', 'VERSION', 'not-used.bin'): (defaults / name).write_text('default')
            (defaults / 'mark').mkdir(); (defaults / 'mark/mark_index').write_text('mark')
            value = config(); value['game_defaults']['guild'] = {'mark_server': False}
            with mock.patch.object(manage, 'ROOT', root): result = manage.prepare_runtime(value)
            self.assertEqual(result['seeded_files'], 4)
            self.assertEqual(list((root / 'run/database').iterdir()), [])
            self.assertEqual(list((root / 'run/login').iterdir()), [])
            self.assertEqual({p.name for p in (root / 'run/ch1-core1').iterdir()}, {'CMD', 'BANIP', 'CRC', 'VERSION'})
            self.assertFalse((root / 'run/ch1-core1/data').exists())


class ChannelGuiTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        from PySide6.QtCore import QSettings
        from PySide6.QtWidgets import QApplication
        from PySide6.QtGui import QFontDatabase
        import window
        cls.temporary = tempfile.TemporaryDirectory(prefix='auriga-channel-gui-settings-')
        QSettings.setDefaultFormat(QSettings.Format.IniFormat)
        QSettings.setPath(QSettings.Format.IniFormat, QSettings.Scope.UserScope, cls.temporary.name)
        cls.app = QApplication.instance() or QApplication([])
        if os.name == 'nt': QFontDatabase.addApplicationFont('C:/Windows/Fonts/segoeui.ttf')
        cls.app.setStyle('Fusion'); cls.app.setStyleSheet(window.STYLE)

    @classmethod
    def tearDownClass(cls): cls.temporary.cleanup()

    def test_map_filter_does_not_discard_hidden_checked_maps(self):
        from channel_editor import MapDialog
        dialog = MapDialog(config(), 1, {1: 'Village', 21: 'Desert', 41: 'Forest'}, [1, 41])
        dialog.search.setText('Desert'); dialog.accept()
        self.assertEqual(dialog.result_maps, [1, 41])
        dialog.close()

    def test_many_channel_controls_wrap_inside_a_small_window(self):
        import window
        value = config()
        for channel in range(2, 17): value, _ = topology.add_channel(value, channel, source=1)
        view = window.MainWindow(); view.timer.stop()
        with mock.patch.object(view, 'state', return_value={'config': value}): view.update_channels()
        view.resize(1024, 720); view.show()
        self.app.processEvents(); self.app.processEvents()
        self.assertEqual(len(view.channel_boxes), 16)
        self.assertGreater(view.channel_layout.rowCount(), 1)
        self.assertLessEqual(view.overview.widget().width(), view.overview.viewport().width())
        view.close(); view.deleteLater(); self.app.processEvents()

    def test_planner_can_cancel_without_mutating_config_and_export_requires_saved_endpoints(self):
        from channel_editor import ChannelDialog
        original = config(); dialog = ChannelDialog(original, original)
        candidate, _ = topology.add_channel(original, 7, source=1)
        dialog.apply(lambda: (candidate, (7, None)))
        self.assertEqual(dialog.selected(), (7, None))
        self.assertFalse(dialog.export_button.isEnabled())
        dialog.reject()
        self.assertEqual(topology.channels(original), [1])

    def test_accepting_planner_updates_existing_draft_and_uses_standard_save(self):
        from PySide6.QtWidgets import QDialog
        import window
        view = window.MainWindow(); view.timer.stop()
        editor = view.config_editor
        document = api.Document.from_bytes('server.json', json.dumps(config()).encode())
        editor.display(document)
        editor.config_form.commit(('locale',), 'latin1')
        candidate, _ = topology.add_channel(editor.config_form.value, 7, source=1)
        fake = mock.Mock(value=candidate)
        fake.DialogCode.Accepted = QDialog.DialogCode.Accepted
        fake.exec.return_value = QDialog.DialogCode.Accepted
        with mock.patch.object(window, 'ChannelDialog', return_value=fake): editor.open_topology()
        self.assertTrue(editor.dirty)
        self.assertEqual(editor.config_form.value['common']['locale'], 'latin1')
        self.assertIn(7, topology.channels(api.strict_json(editor.text())))
        editor.dirty = False; view.close(); view.deleteLater(); self.app.processEvents()


class ChannelNativeTests(unittest.TestCase):
    def test_real_native_save_create_delete_backup_and_runtime_preparation(self):
        native = REPO / 'build/c26b-game-native/SRC/Server/GameServer/ServerPackageCheck.exe'
        package_bin = Path('D:/AurigaGlobal/Server2-Local/bin')
        if not native.is_file() or not package_bin.is_dir(): self.skipTest('Requires local native server build')
        with tempfile.TemporaryDirectory(prefix='auriga-channel-native-') as temp:
            root = Path(temp)
            (root / '.auriga-package.json').write_text('{}')
            shutil.copy2(REPO / 'SRC/Tools/server_package/manage.py', root / 'manage.py')
            shutil.copy2(REPO / 'SRC/Tools/run_server.py', root / 'run_server.py')
            shutil.copytree(package_bin, root / 'bin')
            before = config(); original = json.dumps(before).encode()
            (root / 'server.json').write_bytes(original)
            after, names = topology.add_channel(before, 7, source=1)
            payload = json.dumps(after).encode()
            state = dict(running=False, database_kind='managed', database_running=True)
            with mock.patch.object(api, 'describe', return_value=state):
                shared_edit = copy.deepcopy(after)
                shared_edit['common']['sql']['player']['port'] = 3399
                with self.assertRaises(api.EditorError):
                    api.save_document(root, 'server.json', json.dumps(shared_edit).encode(), api.sha(original))
                output = api.save_document(root, 'server.json', payload, api.sha(original))
            self.assertIn('CONFIG_OK', output)
            self.assertEqual(api.strict_json((root / 'server.json').read_text())['game_servers'][names[0]]['map_allow'], [1, 21, 41])
            manager = api.load_manager(root)
            self.assertIn(names[0], manager.select_instances(after, [7]))
            manager.prepare_runtime(after, [names[0]])
            self.assertEqual({p.name for p in (root / 'run' / names[0]).iterdir()}, {'mark'})
            removed = topology.remove(after, channel=7)
            api.save_document(root, 'server.json', json.dumps(removed).encode(), api.sha(payload))
            self.assertEqual(api.strict_json((root / 'server.json').read_text()), before)
            self.assertTrue(any(path.read_bytes() == original for path in api.backup_files(root, 'server.json')))
            self.assertFalse((root / 'run/mariadb').exists())


if __name__ == '__main__': unittest.main()
