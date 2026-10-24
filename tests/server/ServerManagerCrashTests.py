"""Crash retention, read-only debugger boundaries, target RPC and Qt regression checks."""
import base64
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import time
import types
import unittest
from unittest import mock

os.environ.setdefault('QT_QPA_PLATFORM','offscreen')
REPO=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(REPO/'SRC/Tools/ServerManager'))
sys.path.insert(0,str(REPO/'SRC/Tools'))
from server_package import manage
import crash_tools as crashes
import server_targets as targets


def elf(kind,machine=62):
    raw=bytearray(64);raw[:6]=b'\x7fELF\x02\x01';struct.pack_into('<HH',raw,16,kind,machine)
    return raw


def fixture(root):
    (root/'bin').mkdir(parents=True)
    (root/'run/ch1-core1').mkdir(parents=True)
    (root/'.auriga-package.json').write_text('{}')
    shutil.copy2(REPO/'config/server.json.example',root/'server.json')
    shutil.copy2(REPO/'SRC/Tools/server_package/manage.py',root/'manage.py')
    program=root/'bin/GameServer';program.write_bytes(elf(2))
    core=root/'run/ch1-core1/GameServer.core';core.write_bytes(elf(4))
    return program,core


class CrashTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory(prefix='auriga-crash-test-');self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name).resolve();self.program,self.core=fixture(self.root)
        self.previous=manage.ROOT;manage.ROOT=self.root
        self.addCleanup(setattr,manage,'ROOT',self.previous)

    def record(self):
        self.core.unlink()
        launch=manage.crash_launch(self.program,self.core.parent,{})
        self.core.write_bytes(elf(4))
        (self.core.parent/'console.log').write_text('last crash details')
        process=types.SimpleNamespace(pid=412,poll=lambda:3)
        return manage.record_crash('ch1-core1',process,self.core.parent,launch,'listening')

    def test_archive_core_binary_hash_and_log_survive_runtime_cleanup(self):
        event=self.record()
        self.assertFalse(self.core.exists())
        self.assertEqual(event['exit_code'],3)
        self.assertEqual((self.root/event['cores'][0]).read_bytes(),elf(4))
        self.assertEqual(hashlib.sha256((self.root/event['binary']).read_bytes()).hexdigest(),event['binary_sha256'])
        self.assertEqual((self.root/'run/crashes'/event['id']/'console.log.tail.log').read_text(),'last crash details')
        inventory=crashes.inventory(self.root)
        self.assertEqual((len(inventory['events']),len(inventory['cores'])),(1,1))
        self.assertEqual(inventory['cores'][0]['binary'],event['binary'])
        self.assertEqual(crashes.summary(self.root)['latest']['instance'],'ch1-core1')

    def test_old_core_is_not_attached_to_new_exit_and_changed_binary_is_not_archived(self):
        launch=manage.crash_launch(self.program,self.core.parent,{})
        self.program.write_bytes(elf(2)+b'changed')
        event=manage.record_crash('login',types.SimpleNamespace(pid=6,poll=lambda:0),self.core.parent,launch,'starting')
        self.assertEqual(event['cores'],[]);self.assertIsNone(event['binary'])
        self.assertTrue(event['errors']);self.assertTrue(self.core.exists())

    def test_unchanged_running_process_is_not_recorded(self):
        self.assertIsNone(manage.record_crash('login',types.SimpleNamespace(poll=lambda:None),self.core.parent,{},'listening'))
        self.assertFalse((self.root/'run/crashes').exists())

    def test_inventory_ignores_malformed_events_and_non_core_elf(self):
        bad=self.root/'run/crashes/bad';bad.mkdir(parents=True);(bad/'event.json').write_text('{bad')
        self.core.write_bytes(elf(2));self.assertEqual(crashes.inventory(self.root)['cores'],[])
        self.assertEqual(crashes.summary(self.root),dict(count=0,latest=None))

    def test_paths_core_format_architecture_and_reviewed_state_checked_before_debugger(self):
        for name in ('../secret','bad\nname',str(self.program)):
            with self.subTest(name=name),self.assertRaises(ValueError):crashes.analyze(self.root,name,str(self.program))
        with self.assertRaisesRegex(ValueError,'módosult'):
            crashes.analyze(self.root,str(self.core),str(self.program),dict(size=0,mtime_ns=0))
        self.program.write_bytes(elf(2,3))
        with self.assertRaisesRegex(ValueError,'architektúrája'):crashes.analyze(self.root,str(self.core),str(self.program))

    def test_debugger_autoload_and_init_scripts_are_disabled(self):
        command=crashes.debugger_command(self.core,self.program,'ELF',dict(gdb='gdb'))
        self.assertIn('-nx',command);self.assertIn('set auto-load off',command)
        self.assertIn('set debuginfod enabled off',command)
        self.assertNotIn('run',command);self.assertNotIn('attach',command)
        command=crashes.debugger_command(self.core,self.program,'ELF',dict(lldb='lldb'))
        self.assertIn('--no-lldbinit',command)
        self.assertIn('settings set target.load-script-from-symbol-file false',command)
        with self.assertRaisesRegex(ValueError,'CDB'):crashes.debugger_command(self.core,self.program,'minidump',{})

    def test_real_debugger_subprocess_output_errors_timeout_and_size_limits(self):
        def command(script):return mock.patch.object(crashes,'debugger_command',return_value=[sys.executable,'-c',script])
        with command('print("#0 auriga_crash_probe at probe.cpp:8")'):
            result=crashes.analyze(self.root,str(self.core),str(self.program))
            self.assertIn('auriga_crash_probe',result['report']);self.assertIn('ugyanabból a buildből',result['report'])
        with command('print("corrupt core");raise SystemExit(2)'),self.assertRaisesRegex(ValueError,'corrupt core'):
            crashes.analyze(self.root,str(self.core),str(self.program))
        with command('import time;time.sleep(20)'),mock.patch.object(crashes.time,'monotonic',side_effect=[0,91]),self.assertRaises(TimeoutError):
            crashes.analyze(self.root,str(self.core),str(self.program))
        with command('print("x"*2048)'),mock.patch.object(crashes,'MAX_REPORT',1024),self.assertRaisesRegex(ValueError,'Túl nagy'):
            crashes.analyze(self.root,str(self.core),str(self.program))

    def test_recorded_hash_rejects_wrong_binary_even_with_same_architecture(self):
        event=self.record();self.program.write_bytes(elf(2)+b'new build')
        with self.assertRaisesRegex(ValueError,'ellenőrzőösszege'):
            crashes.analyze(self.root,event['cores'][0],str(self.program))

    def rpc(self,operation,arguments,support_patch=''):
        request=dict(schema_version=1,id='a'*24,target='b'*24,root=str(self.root),operation=operation,arguments=arguments)
        payload=targets.worker_input(request)
        if support_patch:
            source=payload.decode();encoded=source.rsplit('    main(',1)[1].split(')',1)[0]
            import ast
            value=json.loads(base64.b64decode(ast.literal_eval(encoded)))
            value['support']['crash_tools']+='\n'+support_patch
            payload=(source.rsplit('    main(',1)[0]+'    main('+repr(base64.b64encode(json.dumps(value).encode()).decode())+')\n').encode()
        result=subprocess.run([sys.executable,'-B','-'],input=payload,capture_output=True,timeout=15)
        self.assertEqual(result.returncode,0,result.stderr)
        return json.loads(next(line.removeprefix('AURIGA_TARGET_RESULT ') for line in result.stdout.decode().splitlines() if line.startswith('AURIGA_TARGET_RESULT ')))

    def test_production_rpc_inventory_and_analysis_redact_known_password(self):
        packet=self.rpc('crashes',{})
        self.assertTrue(packet['ok'],packet);self.assertEqual(len(packet['result']['cores']),1)
        config=json.loads((self.root/'server.json').read_text());config['common']['sql']['player']['password']='PRIVATE_CRASH_VALUE'
        (self.root/'server.json').write_text(json.dumps(config))
        patch='debugger_command=lambda *args:[sys.executable,"-c","print(\'PRIVATE_CRASH_VALUE #0 probe\')"]\nimport sys'
        packet=self.rpc('crash-analyze',dict(core=str(self.core),binary=str(self.program)),patch)
        self.assertTrue(packet['ok'],packet)
        self.assertIn('#0 probe',packet['result']['report']);self.assertNotIn('PRIVATE_CRASH_VALUE',packet['result']['report'])


class CrashGuiTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        from PySide6.QtWidgets import QApplication
        from PySide6.QtCore import QSettings
        cls.app=QApplication.instance() or QApplication([])
        cls.settings=tempfile.TemporaryDirectory(prefix='auriga-crash-settings-')
        QSettings.setDefaultFormat(QSettings.Format.IniFormat)
        QSettings.setPath(QSettings.Format.IniFormat,QSettings.Scope.UserScope,cls.settings.name)

    @classmethod
    def tearDownClass(cls):cls.settings.cleanup()

    def setUp(self):
        import window
        self.temp=tempfile.TemporaryDirectory(prefix='auriga-crash-gui-');self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name).resolve();fixture(self.root)
        self.view=window.MainWindow(self.root);self.addCleanup(self.view.close)
        self.view.error=lambda message:self.fail(message)
        self.view.timer.stop() if hasattr(self.view,'timer') else None
        self.page=self.view.crash_page

    def wait(self):
        end=time.monotonic()+5
        while self.page.active() and time.monotonic()<end:self.app.processEvents();time.sleep(.01)
        self.assertFalse(self.page.active())

    def test_async_inventory_analysis_export_data_and_target_reset(self):
        self.page.refresh_list();self.wait()
        self.assertEqual(self.page.core_choice.count(),1)
        self.assertEqual(self.page.events.rowCount(),1, 'a core without an event must appear in the main table')
        self.assertEqual(self.page.events.item(0,1).text(),'ch1-core1')
        self.assertEqual(self.page.events.item(0,2).text(),'—')
        self.assertEqual(self.page.events.item(0,3).text(),'Nincs eseménynapló')
        self.assertEqual(self.page.events.item(0,4).text(),'run/ch1-core1/GameServer.core')
        self.page.core_choice.setCurrentIndex(0)
        self.assertIn('GameServer.core',self.page.core_path.text())
        value=dict(report='#0 probe',debugger='test debugger',checked_at='now')
        with mock.patch.object(crashes,'analyze',return_value=value):self.page.analyze();self.wait()
        self.assertEqual(self.page.last_report,'#0 probe')
        self.assertTrue(self.page.export_button.isEnabled())
        self.page.binary_path.setText('bin/same-build-custom-program')
        self.page.refresh_list();self.wait()
        self.assertEqual(self.page.last_report,'#0 probe','refresh must retain the current debugger report')
        self.assertEqual(self.page.binary_path.text(),'bin/same-build-custom-program')
        self.view.target_epoch+=1;self.page.next_poll=time.monotonic()+30
        self.page.tick(self.view.state())
        self.assertEqual(self.page.last_report,'');self.assertEqual(self.page.core_choice.count(),0)
        self.assertEqual(self.page.events.rowCount(),0);self.assertEqual(self.page.rows,[])

    def test_mixed_events_and_unrecorded_cores_select_the_correct_binary(self):
        from PySide6.QtCore import Qt
        from PySide6.QtTest import QTest
        root=self.root
        other=root/'run/ch1-core2/GameServer.core';other.parent.mkdir();other.write_bytes(elf(4))
        event=dict(schema_version=1,id='fixture-event',timestamp='2026-10-23T18:00:00+00:00',instance='ch1-core1',
                   pid=1234,reason='SIGSEGV',cores=['run/ch1-core1/GameServer.core'],binary='run/crashes/binaries/exact/GameServer')
        folder=root/'run/crashes/fixture-event';folder.mkdir(parents=True);(folder/'event.json').write_text(json.dumps(event))
        self.view.show();self.view.navigation.setCurrentRow(6)
        self.page.received(crashes.inventory(root));self.app.processEvents()
        self.assertEqual(self.page.events.rowCount(),2,'a linked core must not be added twice')
        self.assertEqual(self.page.core_choice.count(),2)
        self.assertIn('1 esemény, 2 core fájl',self.page.status.text())
        standalone=next(i for i,row in enumerate(self.page.rows) if row['core'])
        item=self.page.events.item(standalone,1)
        QTest.mouseClick(self.page.events.viewport(),Qt.MouseButton.LeftButton,pos=self.page.events.visualItemRect(item).center())
        self.assertEqual(self.page.core_path.text(),'run/ch1-core2/GameServer.core')
        self.assertEqual(self.page.binary_path.text(),'bin/GameServer.exe' if os.name=='nt' else 'bin/GameServer')
        self.assertIn('A PID és a kilépési jel ismeretlen',self.page.report.toPlainText())
        self.page.events.selectRow(0);self.page.select_event()
        self.assertEqual(self.page.core_path.text(),event['cores'][0])
        self.assertEqual(self.page.binary_path.text(),event['binary'])
        self.assertIn('pid: 1234',self.page.last_report)
        index=next(i for i in range(self.page.core_choice.count()) if self.page.core_choice.itemData(i)['path']==other.relative_to(root).as_posix())
        self.page.core_choice.setCurrentIndex(index)
        self.assertEqual(self.page.events.currentRow(),standalone)
        self.assertNotIn('pid: 1234',self.page.report.toPlainText())

    def test_event_without_a_core_clears_the_previous_analysis_target(self):
        value=crashes.inventory(self.root)
        event=dict(id='no-core',timestamp='2026-10-23T18:00:00+00:00',instance='login',pid=5,reason='Unexpected exit',cores=[],binary=None)
        value['events']=[event]
        self.page.received(value)
        self.assertTrue(self.page.core_path.text())
        self.page.events.selectRow(0);self.page.select_event()
        self.assertEqual(self.page.core_choice.currentIndex(),-1)
        self.assertEqual(self.page.core_path.text(),'');self.assertEqual(self.page.binary_path.text(),'')

    def test_replaced_core_invalidates_its_previous_debugger_report(self):
        value=crashes.inventory(self.root);self.page.received(value)
        self.page.analyzed(dict(report='#0 previous crash',debugger='test',checked_at='now'))
        core=value['cores'][0];core['mtime_ns']+=1
        self.page.received(value)
        self.assertNotIn('#0 previous crash',self.page.report.toPlainText())

    def test_persistent_alert_acknowledgement_is_target_specific(self):
        latest=dict(id='test-event',instance='ch1-core1',reason='SIGSEGV',timestamp='now')
        state=dict(self.view.state(),crashes=dict(count=1,latest=latest))
        self.page.tick(state);self.assertFalse(self.view.crash_alert.isHidden())
        with mock.patch.object(self.view,'state',return_value=state):self.page.acknowledge()
        self.page.tick(state);self.assertTrue(self.view.crash_alert.isHidden())
        self.view.settings.remove(self.page.key())


if __name__=='__main__':unittest.main()
