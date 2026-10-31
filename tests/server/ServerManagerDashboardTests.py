"""Dashboard interaction regressions; no server or database is started."""
import copy
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock

os.environ.setdefault('QT_QPA_PLATFORM', 'offscreen')
REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / 'SRC/Tools/ServerManager'))
from PySide6.QtCore import QSettings, Qt
from PySide6.QtGui import QFontDatabase
from PySide6.QtTest import QTest
from PySide6.QtWidgets import QApplication
import window as ui


class DashboardTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.settings = tempfile.TemporaryDirectory(prefix='auriga-dashboard-settings-')
        QSettings.setDefaultFormat(QSettings.Format.IniFormat)
        QSettings.setPath(QSettings.Format.IniFormat, QSettings.Scope.UserScope, cls.settings.name)
        cls.app = QApplication.instance() or QApplication([])
        cls.app.setStyle('Fusion')
        if os.name == 'nt':
            QFontDatabase.addApplicationFont('C:/Windows/Fonts/segoeui.ttf')
        cls.app.setStyleSheet(ui.STYLE)

    @classmethod
    def tearDownClass(cls):
        cls.settings.cleanup()

    def setUp(self):
        self.window = ui.MainWindow()
        self.window.timer.stop()
        config = json.loads((REPO / 'config/server.json.example').read_text())
        template = next(iter(config['game_servers'].values()))
        config['game_servers'] = {}
        for channel in range(1, 7):
            for core in (1, 2):
                value = copy.deepcopy(template)
                value.setdefault('server', {})['channel'] = channel
                config['game_servers']['ch%d-core%d' % (channel, core)] = value
        value = copy.deepcopy(template)
        value.setdefault('server', {})['channel'] = 99
        config['game_servers']['ch99-core99'] = value
        rows = [dict(name='database', channel='—', port=5300, running=True)]
        for index, (name, core) in enumerate(config['game_servers'].items()):
            channel = ui.api.merge(config.get('game_defaults', {}), core)['server']['channel']
            rows.append(dict(name=name, channel=channel, port=30000 + index, running=channel == 1))
        rows.append(dict(name='login', channel='—', port=30001, running=True))
        self.state = dict(config=config, rows=rows, running=True, phase='Fut', core_control=True,
                          database_kind='managed', database_initialized=True, database_running=True,
                          can_start=True, can_setup=False, hotreload=True, crashes={})
        self.state_patch = mock.patch.object(self.window, 'state', side_effect=lambda: self.state)
        self.state_patch.start()
        self.window.update_channels()
        self.window.refresh()
        self.window.resize(1320, 860)
        self.window.show()
        self.app.processEvents()

    def tearDown(self):
        self.state_patch.stop()
        self.window.close()
        self.window.deleteLater()
        self.app.processEvents()

    def select(self, name):
        row = next(i for i in range(self.window.core_table.rowCount())
                   if self.window.core_table.item(i, 0).text() == name)
        self.window.core_table.selectRow(row)

    def test_filtering_removes_hidden_selection_and_targets_visible_core(self):
        w = self.window
        self.select('ch1-core1')
        self.assertTrue(w.stop_cores_button.isEnabled())
        w.core_search.setText('CH2')
        self.assertEqual(w.selected_cores(self.state), [])
        self.assertFalse(w.stop_cores_button.isEnabled())
        self.assertEqual(w.process_count.text(), '2 / 15 példány')
        w.core_filter.setCurrentIndex(1)
        self.assertTrue(w.process_empty.isVisible())
        w.core_filter.setCurrentIndex(2)
        self.select('ch2-core1')
        with mock.patch.object(w, 'job') as job:
            w.start_cores_button.click()
        job.assert_called_once_with(['start-cores', '--instance', 'ch2-core1'])

    def test_status_poll_preserves_selection_by_name_when_rows_change_order(self):
        w = self.window
        self.select('ch2-core1')
        self.state['rows'].reverse()
        w.refresh()
        self.assertEqual([row['name'] for row in w.selected_cores(self.state)], ['ch2-core1'])
        with mock.patch.object(w, 'job') as job:
            w.start_cores_button.click()
        job.assert_called_once_with(['start-cores', '--instance', 'ch2-core1'])

    def test_no_channel_and_pending_recovery_block_starting(self):
        w = self.window
        for box in w.channel_boxes: box.setChecked(False)
        self.assertFalse(w.start_button.isEnabled())
        self.assertIn('legalább egy', w.channel_summary.text())
        w.channel_boxes[1].setChecked(True)
        self.assertTrue(w.start_button.isEnabled())
        self.select('ch2-core1')
        self.state['deployment_pending'] = 'run/deployments/pending/result.json'
        w.refresh()
        self.assertIn('blokkolva', w.guidance_title.text())
        self.assertIn('result.json', w.target_hint.text())
        self.assertFalse(w.start_button.isEnabled())
        self.assertFalse(w.start_cores_button.isEnabled())
        with mock.patch.object(w, 'job') as job:
            w.start_button.click()
            w.start_cores_button.click()
        job.assert_not_called()

    def test_restore_guidance_takes_precedence_and_opens_backups(self):
        self.state.update(backup_restore_pending=True, deployment_pending='run/deployments/test/result.json')
        self.window.refresh()
        self.assertIn('visszaállítás', self.window.guidance_title.text())
        with mock.patch.object(self.window, 'job') as job:
            self.window.guidance_action.click()
        self.assertEqual(self.window.pages.currentWidget(), self.window.backup_page)
        job.assert_not_called()

    def test_disconnect_clears_stale_metrics_and_disables_channels(self):
        w = self.window
        self.assertIn('Fut', w.stack_metric.text())
        self.state = None
        w.refresh()
        self.assertEqual(w.core_table.rowCount(), 0)
        self.assertEqual(w.db_metric.text(), 'SQL: —')
        self.assertEqual(w.db_metric.property('tone'), 'muted')
        self.assertTrue(all(not box.isEnabled() for box in w.channel_boxes))
        self.assertFalse(w.start_button.isEnabled())

    def test_local_deployment_result_opens_in_redacted_console(self):
        w = self.window
        with tempfile.TemporaryDirectory(prefix='auriga-dashboard-result-') as folder:
            root = Path(folder).resolve()
            (root / 'server.json').write_text('{"sql":{"password":"PRIVATE_TEST_SECRET"}}')
            (root / 'result.json').write_text('{"reason":"failed: PRIVATE_TEST_SECRET"}')
            self.state['deployment_pending'] = 'result.json'
            w.root = root
            try:
                w.refresh()
                w.guidance_action.click()
                self.assertTrue(w.console_panel.isVisible())
                self.assertIn('failed:', w.console.toPlainText())
                self.assertNotIn('PRIVATE_TEST_SECRET', w.console.toPlainText())
                self.state['deployment_pending'] = '../outside.json'
                w.refresh()
                with mock.patch.object(w, 'error') as error, mock.patch.object(ui.api, 'tail') as tail:
                    w.guidance_action.click()
                error.assert_called_once()
                tail.assert_not_called()
            finally:
                w.root = None

    def test_operation_log_remains_accessible_while_busy(self):
        with mock.patch.object(self.window, 'busy', return_value=True):
            self.window.refresh()
            self.assertTrue(self.window.guidance_action.isEnabled())
            self.window.guidance_action.click()
        self.assertTrue(self.window.console_panel.isVisible())

    def test_tools_are_reachable_at_minimum_window_size(self):
        w = self.window
        w.resize(1100, 760)
        self.app.processEvents()
        self.assertFalse(w.maintenance_panel.isVisible())
        w.overview.ensureWidgetVisible(w.maintenance_toggle)
        QTest.mouseClick(w.maintenance_toggle, Qt.MouseButton.LeftButton)
        self.app.processEvents()
        self.assertTrue(w.maintenance_panel.isVisible())
        w.overview.ensureWidgetVisible(w.db_check_button)
        with mock.patch.object(w, 'job') as job:
            QTest.mouseClick(w.db_check_button, Qt.MouseButton.LeftButton)
        job.assert_called_once_with(['database-check'])
        self.assertLessEqual(w.overview.widget().width(), w.overview.viewport().width())


if __name__ == '__main__':
    unittest.main()
