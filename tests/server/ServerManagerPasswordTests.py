"""Password profiles, askpass isolation and optional real loopback OpenSSH/SFTP.

--native requires Paramiko ONLY for the disposable test SSH server.
--askpass-exe tests the actual frozen manager as OpenSSH's password helper.
"""
import argparse
import copy
import json
import hashlib
import base64
import logging
import os
from pathlib import Path
import select
import shutil
import socket
import subprocess
import sys
import tempfile
import threading
import time
import unittest
from unittest import mock

os.environ.setdefault('QT_QPA_PLATFORM', 'offscreen')
REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / 'SRC/Tools/ServerManager'))
import ssh_connection as ssh
import ssh_password as passwords
from backend import EditorError
from credential_store import WindowsCredentialStore
from ssh_trust import host_question

NATIVE = GUI = False
ASKPASS = None
PYTHON = sys.executable


def profile(root):
    value = ssh.new_profile()
    value['mode'] = 'direct'
    value['known_hosts'] = str(root / 'known_hosts')
    Path(value['known_hosts']).touch()
    value['target'].update(host='127.0.0.1', user='targetuser', auth='password')
    return value


class PasswordTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='auriga-password-test-')
        self.root = Path(self.temp.name)
        self.addCleanup(self.temp.cleanup)
        self.addCleanup(passwords.forget)
        self.profile = profile(self.root)

    def test_old_profiles_upgrade_to_key_authentication(self):
        old = copy.deepcopy(self.profile)
        for role in ('target', 'jump'): old[role].pop('auth')
        upgraded = ssh.validate_profile(old)
        self.assertEqual(upgraded['target']['auth'], 'key')
        self.assertNotIn('auth', old['target'])

    def test_password_is_never_in_profile_config_environment_or_arguments(self):
        secret = 'test-\u00e1\u00e9-"%&$`special'
        passwords.remember(self.profile, 'target', secret)
        self.profile['target']['key_file'] = str(self.root / 'missing-unused-key')
        store = ssh.ProfileStore(self.root / 'profiles.json');store.load();store.save([self.profile])
        with mock.patch.dict(os.environ, {}, clear=False):
            session = ssh.Session(self.profile)
        self.addCleanup(session.close)
        broker = session.password_broker
        for command in (session.ssh('cat'), session.sftp(self.root / 'batch'), session.probe()):
            self.assertNotIn(secret, repr(command))
        self.assertNotIn(secret, repr(broker.environment))
        for path in self.root.rglob('*'):
            if path.is_file(): self.assertNotIn(secret.encode(), path.read_bytes())
        for path in Path(session.directory.name).iterdir():
            self.assertNotIn(secret.encode(), path.read_bytes())
        command = session.sftp(self.root / 'batch')[1]
        self.assertLess(command.index('BatchMode=no'), command.index('-b'))
        session.close();self.assertFalse(broker.thread.is_alive())

    def test_broker_rejects_wrong_token_wrong_host_and_confirmation_prompt(self):
        passwords.remember(self.profile, 'target', 'ephemeral')
        session = ssh.Session(self.profile);self.addCleanup(session.close)
        broker = session.password_broker
        scope = broker.environment['AURIGA_SSH_ASKPASS_SCOPE']
        def ask(prompt, token):
            with socket.create_connection(('127.0.0.1',broker.port), timeout=2) as connection:
                connection.sendall((json.dumps(dict(token=token,scope=scope,prompt=prompt))+'\n').encode())
                return passwords.read_line(connection)['password']
        prompt="targetuser@127.0.0.1's password: "
        self.assertIsNone(ask(prompt,'incorrect'))
        self.assertIsNone(ask("another@127.0.0.1's password:",broker.token))
        self.assertIsNone(ask('Are you sure you want to continue connecting?',broker.token))
        for wrong in ('Password:', '(another@127.0.0.1) Password:', '(targetuser@other.host) Password:',
                      '(targetuser@127.0.0.1) Verification code:', '(targetuser@127.0.0.1) New Password:'):
            self.assertIsNone(ask(wrong,broker.token))
        self.assertEqual(ask(prompt,broker.token),'ephemeral')
        self.assertIsNone(ask('(targetuser@127.0.0.1) Password:',broker.token))
        scope = broker.environment['AURIGA_SSH_ASKPASS_SCOPE']
        self.assertEqual(ask('(targetuser@127.0.0.1) Password:',broker.token),'ephemeral')

    def test_missing_and_invalid_passwords_fail_before_start(self):
        with self.assertRaisesRegex(EditorError,'jelszava hiányzik'): ssh.Session(self.profile)
        for password in ('bad\nvalue','bad\0value','x'*1024):
            with self.assertRaises(EditorError): passwords.remember(self.profile,'target',password)
        passwords.remember(self.profile,'target','saved-in-memory')
        changed=copy.deepcopy(self.profile);changed['target']['user']='different'
        self.assertEqual(passwords.recalled(changed,'target'),'')
        passwords.forget(self.profile['id'])
        self.assertEqual(passwords.recalled(self.profile,'target'),'')

    def test_host_question_rejects_other_hosts_and_non_fingerprint_prompts(self):
        prompt=("The authenticity of host '127.0.0.1 (127.0.0.1)' can't be established.\n"
                "ED25519 key fingerprint is SHA256:"+'A'*43+".\n"
                'Are you sure you want to continue connecting (yes/no/[fingerprint])? ')
        info=host_question(self.profile,prompt)
        self.assertEqual(info['host'],'127.0.0.1')
        self.assertEqual(info['key_type'],'ED25519')
        for wrong in (prompt.replace('127.0.0.1','192.0.2.10'),prompt.replace('SHA256:','MD5:'),
                      'Are you sure you want to continue connecting?',"WARNING: REMOTE HOST IDENTIFICATION HAS CHANGED!"):
            self.assertIsNone(host_question(self.profile,wrong))


class MemoryVault:
    def __init__(self):self.values={}
    def read(self,name):return self.values.get(name)
    def write(self,name,user,password):self.values[name]=password
    def delete(self,name):self.values.pop(name,None)


class RememberPasswordTests(unittest.TestCase):
    setUp = PasswordTests.setUp

    def test_stale_profile_save_never_touches_persisted_credentials(self):
        first=ssh.ProfileStore(self.root/'profiles.json');second=ssh.ProfileStore(first.path)
        first.load();second.load();first.save([self.profile])
        transaction=mock.MagicMock()
        with self.assertRaisesRegex(EditorError,'másik ablak'):
            second.save([self.profile],transaction=transaction)
        transaction.__enter__.assert_not_called()

    def test_saved_password_reloads_after_cache_reset_and_uncheck_removes_it(self):
        self.profile['target']['remember_password']=True
        vault=MemoryVault()
        passwords.remember(self.profile,'target','saved-secret')
        with mock.patch.object(passwords,'system_store',return_value=vault):
            store=ssh.ProfileStore(self.root/'profiles.json');store.load()
            with passwords.saved_preferences(self.profile):store.save([self.profile])
            self.assertNotIn('saved-secret',store.path.read_text())
            passwords.forget()
            restored=store.load()[0]
            self.assertEqual(passwords.recalled(restored,'target'),'saved-secret')
            unrelated=copy.deepcopy(restored);unrelated['target']['host']='another.example.invalid'
            self.assertEqual(passwords.recalled(unrelated,'target'),'')
            disabled=copy.deepcopy(restored);disabled['target']['remember_password']=False
            with passwords.saved_preferences(disabled,restored):store.save([disabled])
            self.assertEqual(vault.values,{})
            self.assertEqual(passwords.recalled(disabled,'target'),'saved-secret')
            passwords.forget()
            self.assertEqual(passwords.recalled(disabled,'target'),'')

    def test_failed_profile_write_restores_previous_credential(self):
        self.profile['target']['remember_password']=True
        name=passwords.credential_name(self.profile,'target')
        vault=MemoryVault();vault.values[name]='original'
        passwords.remember(self.profile,'target','edited-password')
        with mock.patch.object(passwords,'system_store',return_value=vault):
            with self.assertRaisesRegex(EditorError,'profile conflict'):
                with passwords.saved_preferences(self.profile,self.profile):
                    self.assertEqual(vault.values[name],'edited-password')
                    raise EditorError('profile conflict')
        self.assertEqual(vault.values[name],'original')

    def test_route_change_and_profile_delete_remove_only_owned_credentials(self):
        self.profile['mode']='proxyjump'
        self.profile['jump'].update(host='jump.example.invalid',user='jumpuser',auth='password',remember_password=True)
        self.profile['target']['remember_password']=True
        vault=MemoryVault();vault.values['unrelated-app']='untouched'
        for role in ('target','jump'):passwords.remember(self.profile,role,role+'-secret')
        with mock.patch.object(passwords,'system_store',return_value=vault):
            with passwords.saved_preferences(self.profile):pass
            oldname=passwords.credential_name(self.profile,'target')
            changed=copy.deepcopy(self.profile);changed['target']['port']=2222
            passwords.remember(changed,'target','new-route-secret')
            with passwords.saved_preferences(changed,self.profile):pass
            self.assertNotIn(oldname,vault.values)
            self.assertEqual(vault.values[passwords.credential_name(changed,'target')],'new-route-secret')
            with passwords.saved_preferences(previous=changed):pass
        self.assertEqual(vault.values,{'unrelated-app':'untouched'})

    def test_empty_draft_does_not_revive_saved_password_and_storage_failure_is_visible(self):
        self.profile['target']['remember_password']=True
        vault=MemoryVault();name=passwords.credential_name(self.profile,'target');vault.values[name]='old'
        passwords.remember(self.profile,'target','')
        with mock.patch.object(passwords,'system_store',return_value=vault):
            with self.assertRaisesRegex(EditorError,'töltsd ki'):
                with passwords.saved_preferences(self.profile):self.fail('Empty password saved')
            self.assertEqual(vault.values[name],'old')
            passwords.remember(self.profile,'target','new')
            with mock.patch.object(vault,'write',side_effect=EditorError('Storage unavailable')):
                with self.assertRaisesRegex(EditorError,'Storage unavailable'):
                    with passwords.saved_preferences(self.profile):self.fail('Profile saved after failed credential write')
        self.assertFalse((self.root/'profiles.json').exists())

    def test_windows_credential_survives_a_fresh_python_process_and_is_deleted(self):
        if os.name!='nt':self.skipTest('Windows Credential Manager required')
        self.profile['target']['remember_password']=True
        vault=WindowsCredentialStore();name=passwords.credential_name(self.profile,'target')
        self.addCleanup(vault.delete,name)
        secret='Windows-\u00e1\u00e9-"%&$`test'
        passwords.remember(self.profile,'target',secret)
        with passwords.saved_preferences(self.profile):pass
        self.assertEqual(vault.read(name),secret)
        child=("import sys,json,hashlib;sys.path.insert(0,sys.argv[1]);import ssh_password as p;"
               "v=json.load(sys.stdin);s=p.recalled(v['profile'],'target');"
               "assert hashlib.sha256(s.encode()).hexdigest()==v['hash'];print('RESTORED_OK')")
        result=subprocess.run([getattr(sys,'_base_executable',sys.executable),'-B','-c',child,str(REPO/'SRC/Tools/ServerManager')],
            input=json.dumps(dict(profile=self.profile,hash=hashlib.sha256(secret.encode()).hexdigest())),
            capture_output=True,text=True,timeout=15,creationflags=subprocess.CREATE_NO_WINDOW)
        self.assertEqual(result.returncode,0,result.stderr)
        self.assertEqual(result.stdout.strip(),'RESTORED_OK')
        with passwords.saved_preferences(previous=self.profile):pass
        self.assertIsNone(vault.read(name))

    def test_frozen_worker_restores_saved_credential(self):
        if not ASKPASS or os.name!='nt':self.skipTest('Pass --askpass-exe on Windows')
        self.profile['target']['remember_password']=True
        vault=WindowsCredentialStore();name=passwords.credential_name(self.profile,'target')
        self.addCleanup(vault.delete,name)
        secret='frozen-\u00e1\u00e9-saved-test'
        passwords.remember(self.profile,'target',secret)
        with passwords.saved_preferences(self.profile):pass
        root=self.root/'worker-package';root.mkdir()
        (root/'.auriga-package.json').write_text('{}')
        (root/'server.json').write_text('{"schema_version":1,"game_servers":{}}')
        (root/'test-profile.json').write_text(json.dumps(dict(profile=self.profile,hash=hashlib.sha256(secret.encode()).hexdigest())))
        (root/'manage.py').write_text('import json,hashlib\nfrom pathlib import Path\nimport ssh_password\n'
            'def main():\n'
            ' v=json.loads((Path(__file__).parent/"test-profile.json").read_text())\n'
            ' s=ssh_password.recalled(v["profile"],"target")\n'
            ' assert hashlib.sha256(s.encode()).hexdigest()==v["hash"]\n'
            ' print("FROZEN_CREDENTIAL_RESTORED")\n'
            ' return 0\n')
        log=root/'run/check.log'
        result=subprocess.run([str(ASKPASS.resolve()),'--worker',str(root),str(log),'status'],
            capture_output=True,timeout=20,creationflags=subprocess.CREATE_NO_WINDOW)
        self.assertEqual(result.returncode,0,log.read_text(encoding='utf-8') if log.exists() else 'Missing worker log')
        self.assertIn('FROZEN_CREDENTIAL_RESTORED',log.read_text(encoding='utf-8'))
        self.assertNotIn(secret,log.read_text(encoding='utf-8'))


class PasswordGuiTests(unittest.TestCase):
    setUp = PasswordTests.setUp

    def test_closing_pending_host_approval_rejects_without_late_dialog(self):
        if not GUI:self.skipTest('Pass --gui')
        from PySide6.QtWidgets import QApplication,QWidget
        from ssh_trust import gui_approver
        app=QApplication.instance() or QApplication([])
        owner=QWidget();controller=gui_approver(owner)
        result=[]
        worker=threading.Thread(target=lambda:result.append(controller.approve({})))
        worker.start()
        deadline=time.monotonic()+2
        while not controller.requests and time.monotonic()<deadline:time.sleep(.001)
        controller.close();worker.join(2);app.processEvents()
        self.assertFalse(worker.is_alive())
        self.assertEqual(result,[False])
        self.assertIsNone(controller.dialog)
        owner.close();app.processEvents()

    def test_password_mode_disables_all_manual_key_fields_and_ignores_inactive_drafts(self):
        if not GUI:self.skipTest('Pass --gui')
        from PySide6.QtWidgets import QApplication,QWidget
        from remote_page import RemotePage
        app=QApplication.instance() or QApplication([])
        owner=QWidget();owner.error=lambda message:self.fail(message)
        page=RemotePage(owner,self.root/'password-only.json');page.load(self.profile);page.show();app.processEvents()
        try:
            self.assertFalse(page.target['key_row'].isEnabled())
            self.assertFalse(page.target['host_key_alias'].isEnabled())
            self.assertFalse(page.identity_row.isEnabled())
            self.assertFalse(page.identity_row.isVisible())
            page.target['key_file'].setText('not-an-absolute-key')
            page.target['host_key_alias'].setText('not a valid alias')
            page.known_hosts.setText('not-an-absolute-identity-file')
            page.target['password'].setText('session-only-password')
            Path(self.profile['known_hosts']).unlink()
            with mock.patch.object(ssh,'Session') as session:page.save()
            session.assert_not_called()
            saved=page.store.load()[0]
            self.assertEqual(saved['target']['key_file'],'')
            self.assertEqual(saved['target']['host_key_alias'],'')
            self.assertEqual(saved['known_hosts'],self.profile['known_hosts'])
            self.assertEqual(saved['target']['auth'],'password')
            page.target['auth'].setCurrentIndex(0);app.processEvents()
            self.assertTrue(page.target['key_row'].isEnabled())
            self.assertTrue(page.target['host_key_alias'].isEnabled())
            self.assertTrue(page.identity_row.isVisible())
        finally:page.shutdown();page.close();owner.close();app.processEvents()
    def test_password_field_is_masked_and_save_keeps_only_auth_choice(self):
        if not GUI: self.skipTest('Pass --gui')
        from PySide6.QtWidgets import QApplication, QWidget, QLineEdit
        from remote_page import RemotePage
        app=QApplication.instance() or QApplication([])
        owner=QWidget();owner.error=lambda message:self.fail(message)
        page=RemotePage(owner,self.root/'gui-profiles.json');page.load(self.profile);page.show()
        app.processEvents()
        self.assertEqual(page.target['password'].echoMode(),QLineEdit.EchoMode.Password)
        self.assertFalse(page.target['key_row'].isVisible())
        self.assertTrue(page.target['password'].isVisible())
        page.target['password'].setText('gui-secret')
        page.save()
        self.assertNotIn('gui-secret',page.store.path.read_text())
        self.assertEqual(page.store.load()[0]['target']['auth'],'password')
        self.assertEqual(passwords.recalled(self.profile,'target'),'gui-secret')
        self.assertEqual(page.target['password'].text(),'gui-secret')
        page.target['auth'].setCurrentIndex(0);app.processEvents()
        self.assertFalse(page.target['password'].isVisible())
        self.assertTrue(page.target['key_row'].isVisible())
        page.shutdown();page.close();owner.close();app.processEvents()

    def test_remember_checkbox_roundtrip_and_clear_with_profile_save(self):
        if not GUI:self.skipTest('Pass --gui')
        from PySide6.QtWidgets import QApplication,QWidget
        from remote_page import RemotePage
        app=QApplication.instance() or QApplication([])
        owner=QWidget();owner.error=lambda message:self.fail(message)
        vault=MemoryVault()
        with mock.patch.object(passwords,'system_store',return_value=vault):
            page=RemotePage(owner,self.root/'gui-profiles.json');page.load(self.profile)
            self.assertFalse(page.target['remember_password'].isChecked())
            page.target['password'].setText('checkbox-secret')
            page.target['remember_password'].setChecked(True);page.save()
            self.assertTrue(page.store.load()[0]['target']['remember_password'])
            self.assertNotIn('checkbox-secret',page.store.path.read_text())
            self.assertEqual(len(vault.values),1)
            passwords.forget();page.reload()
            self.assertEqual(page.target['password'].text(),'checkbox-secret')
            self.assertTrue(page.target['remember_password'].isChecked())
            page.target['remember_password'].setChecked(False);page.save()
            self.assertEqual(vault.values,{})
            self.assertFalse(page.store.load()[0]['target']['remember_password'])
            passwords.forget();page.reload()
            self.assertEqual(page.target['password'].text(),'')
            page.target['password'].setText('delete-with-profile')
            page.target['remember_password'].setChecked(True);page.save()
            from PySide6.QtWidgets import QMessageBox
            with mock.patch.object(QMessageBox,'question',return_value=QMessageBox.StandardButton.Yes):page.remove()
            self.assertEqual(vault.values,{})
            self.assertEqual(page.store.load(),[])
            page.shutdown();page.close();owner.close();app.processEvents()


class NativePasswordTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not NATIVE: raise unittest.SkipTest('Pass --native with Paramiko installed')
        global paramiko
        import paramiko
        logging.getLogger('paramiko').setLevel(logging.CRITICAL)

    def setUp(self):
        self.temp=tempfile.TemporaryDirectory(prefix='auriga-native-password-')
        self.root=Path(self.temp.name);self.servers=[]
        self.addCleanup(self.temp.cleanup)
        self.addCleanup(passwords.forget)
        self.addCleanup(self.stop_servers)
        self.profile=profile(self.root)
        self.password='native-\u00e1\u00e9-"%&$`secret'
        self.target=self.server('targetuser',self.password)
        self.profile['target']['port']=self.target.port
        passwords.remember(self.profile,'target',self.password)

    def stop_servers(self):
        for server in self.servers:
            server.stopping.set();server.listener.close()
            for transport in server.transports: transport.close()
            server.thread.join(2)

    def server(self,user,password,auth='password',prompts=None):
        test=self
        class Files(paramiko.SFTPServerInterface):
            def canonicalize(self,path): return '/'
            def stat(self,path):
                try:return paramiko.SFTPAttributes.from_stat((test.root/Path(path).name).stat())
                except OSError as error:return paramiko.SFTPServer.convert_errno(error.errno)
            lstat=stat
            def open(self,path,flags,attr):
                try:
                    fd=os.open(test.root/Path(path).name, flags|os.O_BINARY,0o600)
                    handle=paramiko.SFTPHandle(flags)
                    stream=os.fdopen(fd,'w+b' if flags&os.O_RDWR else 'wb' if flags&os.O_WRONLY else 'rb')
                    if flags&os.O_WRONLY or flags&os.O_RDWR:handle.writefile=stream
                    else:handle.readfile=stream
                    handle.stat=lambda:paramiko.SFTPAttributes.from_stat(os.fstat(stream.fileno()))
                    return handle
                except OSError as error:return paramiko.SFTPServer.convert_errno(error.errno)
        class Server:
            def __init__(self):
                self.stopping=threading.Event();self.transports=[];self.attempts=[];self.password_checks=[];self.forward_events=[]
                self.key=paramiko.RSAKey.generate(2048)
                self.listener=socket.socket();self.listener.bind(('127.0.0.1',0));self.listener.listen()
                self.listener.settimeout(.2);self.port=self.listener.getsockname()[1]
                with (test.root/'known_hosts').open('a') as output:
                    output.write('[127.0.0.1]:%d %s %s\n'%(self.port,self.key.get_name(),self.key.get_base64()))
                self.thread=threading.Thread(target=self.serve,daemon=True);self.thread.start()
            def serve(self):
                while not self.stopping.is_set():
                    try:connection,_=self.listener.accept()
                    except socket.timeout:continue
                    except OSError:break
                    threading.Thread(target=self.client,args=(connection,),daemon=True).start()
            def client(self,connection):
                server=self
                class Handler(paramiko.ServerInterface):
                    def check_auth_password(self,username,provided):
                        server.attempts.append(username)
                        server.password_checks.append((provided==password,len(provided.encode('utf-8'))))
                        return paramiko.AUTH_SUCCESSFUL if username==user and provided==password else paramiko.AUTH_FAILED
                    def get_allowed_auths(self,username):return auth
                    def check_auth_interactive(self,username,submethods):
                        self.username=username
                        return paramiko.InteractiveQuery('', '', *(prompts or [('Password:',False)]))
                    def check_auth_interactive_response(self,responses):
                        server.attempts.append(self.username)
                        server.password_checks.extend((provided==password,len(provided.encode('utf-8'))) for provided in responses)
                        return paramiko.AUTH_SUCCESSFUL if self.username==user and responses==[password] else paramiko.AUTH_FAILED
                    def check_channel_request(self,kind,chanid):return paramiko.OPEN_SUCCEEDED
                    def check_channel_direct_tcpip_request(self,chanid,origin,destination):
                        server.forward_events.append('requested')
                        destinations[chanid]=destination;return paramiko.OPEN_SUCCEEDED
                    def check_channel_exec_request(self,channel,command):
                        def respond():
                            try:
                                data=bytearray()
                                if command in (b'cat',b'python3 -u -'):
                                    while True:
                                        block=channel.recv(65536)
                                        if not block:break
                                        data.extend(block)
                                status=0
                                answer=(b'AURIGA_OS=FreeBSD\nAURIGA_RELEASE=15.1-RELEASE\nAURIGA_USER=targetuser\n'
                                        b'AURIGA_PYTHON=/usr/local/bin/python3\nAURIGA_DIRECTORY=unset\nAURIGA_PACKAGE=unset\n')
                                if command==b'cat':answer=bytes(data)
                                elif command==b'python3 -u -':
                                    result=subprocess.run([PYTHON,'-B','-u','-'],input=bytes(data),capture_output=True,
                                        timeout=20,creationflags=subprocess.CREATE_NO_WINDOW if os.name=='nt' else 0)
                                    answer=result.stdout
                                    if result.stderr:channel.sendall_stderr(result.stderr)
                                    status=result.returncode
                                channel.sendall(answer)
                                channel.send_exit_status(status);channel.shutdown_write()
                                time.sleep(.05)
                            except (OSError,EOFError,paramiko.SSHException):pass
                            finally:channel.close()
                        threading.Thread(target=respond,daemon=True).start();return True
                destinations={};channels=[]
                transport=paramiko.Transport(connection);self.transports.append(transport)
                transport.add_server_key(self.key)
                transport.set_subsystem_handler('sftp',paramiko.SFTPServer,Files)
                try:
                    transport.start_server(server=Handler())
                    while transport.is_active() and not self.stopping.is_set():
                        channel=transport.accept(.1)
                        if channel is None:continue
                        channels.append(channel)
                        if channel.get_id() in destinations:
                            server.forward_events.append('accepted')
                            def bridge(channel,destination):
                                try:
                                    with socket.create_connection(destination,timeout=5) as remote:
                                        server.forward_events.append('connected')
                                        while not self.stopping.is_set():
                                            ready,_,_=select.select([remote,channel],[],[],.2)
                                            for source in ready:
                                                data=source.recv(65536)
                                                server.forward_events.append(('remote' if source is remote else 'channel',len(data)))
                                                if not data:return
                                                (channel if source is remote else remote).sendall(data)
                                finally:channel.close()
                            threading.Thread(target=bridge,args=(channel,destinations[channel.get_id()]),daemon=True).start()
                except (OSError,EOFError,paramiko.SSHException):pass
                finally:transport.close()
        server=Server();self.servers.append(server);return server

    def session(self, **kwargs):
        if ASKPASS:
            with mock.patch.object(sys,'frozen',True,create=True),mock.patch.object(sys,'executable',str(ASKPASS)):
                session=ssh.Session(self.profile, **kwargs)
        else:session=ssh.Session(self.profile, **kwargs)
        self.addCleanup(session.close);return session

    def execute(self,session,command,data=b''):
        executable,arguments=command
        return subprocess.run([executable,*arguments],input=data,capture_output=True,timeout=25,
            env=dict(os.environ,**session.password_broker.environment),
            creationflags=subprocess.CREATE_NO_WINDOW if os.name=='nt' else 0)

    def interactive_target(self, prompts=None):
        self.target=self.server('targetuser',self.password,auth='keyboard-interactive',prompts=prompts)
        self.profile['target']['port']=self.target.port
        passwords.remember(self.profile,'target',self.password)

    def test_keyboard_interactive_pam_password_keeps_rpc_input_and_reconnects(self):
        self.interactive_target()
        session=self.session()
        for payload in (b'PAM_RPC_PAYLOAD',b'NEXT_RPC'):
            result=self.execute(session,session.ssh('cat'),payload)
            self.assertEqual(result.returncode,0,result.stderr.decode(errors='replace'))
            self.assertEqual(result.stdout,payload)
        self.assertEqual(self.target.password_checks,[(True,len(self.password.encode('utf-8')))]*2)

    def test_freebsd_pam_remote_hostname_prompt_is_recognized(self):
        self.interactive_target([('Password for targetuser@freebsd.example.invalid:',False)])
        session=self.session();result=self.execute(session,session.ssh('cat'),b'FREEBSD_PAM')
        self.assertEqual(result.returncode,0,result.stderr.decode(errors='replace'))
        self.assertEqual(result.stdout,b'FREEBSD_PAM')

    def test_keyboard_interactive_sftp_works_on_first_host_contact(self):
        self.interactive_target();(self.root/'known_hosts').write_text('')
        session=self.session(approve_host=lambda info:True)
        (self.root/'local.txt').write_bytes(b'PAM_SFTP\x00\xff')
        batch=self.root/'batch';batch.write_text('put "'+(self.root/'local.txt').as_posix()+'" "/pam-upload.txt"\n')
        result=self.execute(session,session.sftp(batch))
        self.assertEqual(result.returncode,0,result.stderr.decode(errors='replace'))
        self.assertEqual((self.root/'pam-upload.txt').read_bytes(),b'PAM_SFTP\x00\xff')

    def test_keyboard_interactive_wrong_password_is_not_repeated_or_logged(self):
        self.interactive_target();passwords.remember(self.profile,'target','wrong-pam-password')
        session=self.session();result=self.execute(session,session.ssh('cat'))
        self.assertNotEqual(result.returncode,0)
        self.assertEqual(len(self.target.password_checks),1)
        self.assertNotIn(b'wrong-pam-password',result.stdout+result.stderr)

    def test_password_to_pam_fallback_does_not_submit_same_wrong_secret_twice(self):
        self.target=self.server('targetuser',self.password,auth='password,keyboard-interactive')
        self.profile['target']['port']=self.target.port
        passwords.remember(self.profile,'target','wrong-both-methods')
        session=self.session();result=self.execute(session,session.ssh('cat'))
        self.assertNotEqual(result.returncode,0)
        self.assertEqual(self.target.password_checks,[(False,len('wrong-both-methods')),(False,0)])
        self.assertNotIn(b'wrong-both-methods',result.stdout+result.stderr)

    def test_keyboard_interactive_otp_and_password_change_never_receive_password(self):
        for prompt in ('Verification code:', 'New Password:', 'Retype Password:'):
            with self.subTest(prompt=prompt):
                self.interactive_target([(prompt,False)])
                session=self.session();result=self.execute(session,session.ssh('cat'))
                self.assertNotEqual(result.returncode,0)
                self.assertEqual(self.target.password_checks,[(False,0)])

    def test_keyboard_interactive_second_password_prompt_is_not_answered(self):
        self.interactive_target([('Password:',False),('Password:',False)])
        session=self.session();result=self.execute(session,session.ssh('cat'))
        self.assertNotEqual(result.returncode,0)
        self.assertEqual(self.target.password_checks,[(True,len(self.password.encode('utf-8'))),(False,0)])

    def test_keyboard_interactive_proxyjump_with_different_passwords(self):
        self.interactive_target()
        jump=self.server('jumpuser','pam-jump-secret',auth='keyboard-interactive')
        self.profile['mode']='proxyjump'
        self.profile['jump'].update(host='127.0.0.1',port=jump.port,user='jumpuser',auth='password')
        passwords.remember(self.profile,'jump','pam-jump-secret')
        session=self.session();result=self.execute(session,session.ssh('cat'),b'PAM_JUMP')
        self.assertEqual(result.returncode,0,result.stderr.decode(errors='replace'))
        self.assertEqual(result.stdout,b'PAM_JUMP')
        self.assertEqual(jump.password_checks,[(True,len('pam-jump-secret'))])
        self.assertEqual(self.target.password_checks,[(True,len(self.password.encode('utf-8')))])

    def test_real_ssh_password_keeps_rpc_stdin_and_unicode_password(self):
        session=self.session();payload=b'RPC_PAYLOAD\n{"password":"not-ssh-data"}\n'
        result=self.execute(session,session.ssh('cat'),payload)
        self.assertEqual(result.returncode,0,result.stderr.decode(errors='replace'))
        self.assertEqual(result.stdout,payload)
        self.assertEqual(self.target.attempts,['targetuser'])

    def test_real_sftp_batch_upload_authenticates_with_password(self):
        session=self.session();(self.root/'local.txt').write_bytes(b'SFTP bytes\x00\xff')
        batch=self.root/'batch';batch.write_text('put "'+(self.root/'local.txt').as_posix()+'" "/uploaded.txt"\n')
        result=self.execute(session,session.sftp(batch))
        self.assertEqual(result.returncode,0,result.stderr.decode(errors='replace'))
        self.assertEqual((self.root/'uploaded.txt').read_bytes(),b'SFTP bytes\x00\xff')

    def test_first_host_approval_is_exact_and_remembered_for_next_connection(self):
        (self.root/'known_hosts').write_text('')
        questions=[]
        session=self.session(approve_host=lambda info:questions.append(info) or True)
        result=self.execute(session,session.ssh('cat'),b'FIRST_TRUST')
        self.assertEqual(result.returncode,0,result.stderr.decode(errors='replace'))
        self.assertEqual(result.stdout,b'FIRST_TRUST')
        self.assertEqual(len(questions),1)
        fingerprint='SHA256:'+base64.b64encode(hashlib.sha256(self.target.key.asbytes()).digest()).decode().rstrip('=')
        self.assertEqual(questions[0]['fingerprint'],fingerprint)
        self.assertEqual(questions[0]['port'],self.target.port)
        self.assertEqual(questions[0]['role'],'target')
        self.assertIn(self.target.key.get_base64(),(self.root/'known_hosts').read_text())
        second=self.session(approve_host=lambda info:self.fail('Known host asked again'))
        again=self.execute(second,second.ssh('cat'),b'SECOND')
        self.assertEqual(again.returncode,0,again.stderr.decode(errors='replace'))

    def test_first_host_rejection_never_sends_password_or_saves_host(self):
        (self.root/'known_hosts').write_text('')
        questions=[];session=self.session(approve_host=lambda info:questions.append(info) or False)
        result=self.execute(session,session.ssh('cat'))
        self.assertNotEqual(result.returncode,0)
        self.assertEqual(len(questions),1)
        self.assertEqual(self.target.attempts,[])
        self.assertEqual((self.root/'known_hosts').read_text(),'')

    def test_remote_build_worker_runs_after_first_host_approval(self):
        self.interactive_target()
        import remote_jobs as jobs
        cmake=shutil.which('cmake');ninja=shutil.which('ninja')
        if not ninja and os.name=='nt':
            candidate=Path('C:/Program Files/Microsoft Visual Studio/18/Professional/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe')
            if candidate.is_file():ninja=str(candidate)
        if not cmake or not ninja:self.skipTest('CMake and Ninja required')
        source=self.root/'source';source.mkdir();build=self.root/'build'
        (source/'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.20)\nproject(RemoteSmoke NONE)\n'
            'add_custom_target(GameServer COMMAND "${CMAKE_COMMAND}" -E touch "${CMAKE_BINARY_DIR}/built.ok")\n')
        request=dict(action='build',id='ab'*12,source=str(source),build=str(build),
            configure=[cmake,'-S',str(source),'-B',str(build),'-G','Ninja','-DCMAKE_MAKE_PROGRAM='+ninja],
            compile=[cmake,'--build',str(build),'--config','Release','--parallel','2','--target','GameServer'])
        (self.root/'known_hosts').write_text('');questions=[]
        session=self.session(approve_host=lambda info:questions.append(info) or True)
        result=self.execute(session,session.ssh('python3 -u -'),jobs.worker_input(request))
        self.assertEqual(result.returncode,0,(result.stdout+result.stderr).decode(errors='replace'))
        self.assertIn(b'AURIGA_BUILD_SUCCESS',result.stdout)
        self.assertTrue((build/'built.ok').is_file())
        self.assertEqual(len(questions),1)
        self.assertIn(self.target.key.get_base64(),(self.root/'known_hosts').read_text())

    def test_changed_host_is_not_reapproved_or_overwritten(self):
        wrong=paramiko.RSAKey.generate(2048)
        original='[127.0.0.1]:%d %s %s\n'%(self.target.port,wrong.get_name(),wrong.get_base64())
        (self.root/'known_hosts').write_text(original)
        session=self.session(approve_host=lambda info:self.fail('Changed key must never be accepted'))
        result=self.execute(session,session.ssh('cat'))
        self.assertNotEqual(result.returncode,0)
        self.assertEqual(self.target.attempts,[])
        self.assertEqual((self.root/'known_hosts').read_text(),original)

    def test_first_host_approval_works_for_sftp_without_prior_probe(self):
        (self.root/'known_hosts').write_text('')
        session=self.session(approve_host=lambda info:True)
        (self.root/'local.txt').write_bytes(b'FIRST_SFTP')
        batch=self.root/'batch';batch.write_text('put "'+(self.root/'local.txt').as_posix()+'" "/first-upload.txt"\n')
        result=self.execute(session,session.sftp(batch))
        self.assertEqual(result.returncode,0,result.stderr.decode(errors='replace'))
        self.assertEqual((self.root/'first-upload.txt').read_bytes(),b'FIRST_SFTP')

    def test_first_host_approval_covers_both_proxyjump_hops(self):
        jump=self.server('jumpuser','jump-password')
        self.profile['mode']='proxyjump'
        self.profile['jump'].update(host='127.0.0.1',port=jump.port,user='jumpuser',auth='password')
        passwords.remember(self.profile,'jump','jump-password')
        (self.root/'known_hosts').write_text('')
        questions=[];session=self.session(approve_host=lambda info:questions.append(info) or True)
        result=self.execute(session,session.ssh('cat'),b'NEW_JUMP')
        self.assertEqual(result.returncode,0,result.stderr.decode(errors='replace'))
        self.assertEqual(result.stdout,b'NEW_JUMP')
        self.assertEqual([item['role'] for item in questions],['jump','target'])

    def test_gui_approval_pauses_timeout_and_continues_original_probe(self):
        if not GUI:self.skipTest('Pass --gui')
        from PySide6.QtWidgets import QApplication,QWidget,QMessageBox
        from PySide6.QtCore import QTimer
        from remote_page import RemotePage
        app=QApplication.instance() or QApplication([])
        owner=QWidget();errors=[];owner.error=errors.append
        page=RemotePage(owner,self.root/'qt-trust-profile.json');page.load(self.profile)
        (self.root/'known_hosts').unlink()  # GUI creates it; no manual key file required.
        original=QMessageBox.exec;questions=[]
        def accept_dialog(dialog):
            questions.append(dialog.text())
            self.assertFalse(page.probe_timer.isActive())
            accept=next(button for button in dialog.buttons() if dialog.buttonRole(button)==QMessageBox.ButtonRole.AcceptRole)
            QTimer.singleShot(0,accept.click)
            return original(dialog)
        with mock.patch.object(QMessageBox,'exec',accept_dialog):
            if ASKPASS:
                with mock.patch.object(sys,'frozen',True,create=True),mock.patch.object(sys,'executable',str(ASKPASS)):
                    page.check()
            else:page.check()
            deadline=time.monotonic()+12
            while page.probe_process and time.monotonic()<deadline:app.processEvents();time.sleep(.01)
        try:
            self.assertIsNone(page.probe_process)
            self.assertEqual(errors,[])
            self.assertEqual(len(questions),1)
            self.assertIn('SHA256:',questions[0])
            self.assertIn('FreeBSD 15.1-RELEASE',page.output.toPlainText())
        finally:page.shutdown();page.close();owner.close();app.processEvents()

    def test_real_proxyjump_uses_separate_jump_and_target_passwords(self):
        jump=self.server('jumpuser','different-jump-password')
        self.profile['mode']='proxyjump'
        self.profile['jump'].update(host='127.0.0.1',port=jump.port,user='jumpuser',auth='password')
        passwords.remember(self.profile,'jump','different-jump-password')
        session=self.session();result=self.execute(session,session.ssh('cat'),b'VIA_JUMP')
        self.assertEqual(result.returncode,0,result.stderr.decode(errors='replace'))
        self.assertEqual(result.stdout,b'VIA_JUMP')
        self.assertEqual(jump.attempts,['jumpuser']);self.assertEqual(self.target.attempts,['targetuser'])

    def test_wrong_password_does_not_retry_or_echo_secret(self):
        passwords.remember(self.profile,'target','incorrect-password')
        session=self.session();result=self.execute(session,session.ssh('cat'))
        self.assertNotEqual(result.returncode,0)
        self.assertEqual(self.target.attempts,['targetuser'])
        self.assertNotIn(b'incorrect-password',result.stdout+result.stderr)

    def test_real_qt_probe_receives_password_environment(self):
        self.qt_password_probe()

    def test_real_qt_probe_receives_pam_password_environment(self):
        self.interactive_target()
        self.qt_password_probe()

    def qt_password_probe(self):
        if not GUI:self.skipTest('Pass --gui')
        from PySide6.QtWidgets import QApplication,QWidget
        from remote_page import RemotePage
        app=QApplication.instance() or QApplication([])
        owner=QWidget();errors=[];owner.error=errors.append
        page=RemotePage(owner,self.root/'qt-profiles.json');page.load(self.profile)
        self.assertEqual(page.target['password'].text(),self.password)
        if ASKPASS:
            with mock.patch.object(sys,'frozen',True,create=True),mock.patch.object(sys,'executable',str(ASKPASS)):
                page.check()
        else:page.check()
        deadline=time.monotonic()+12
        while page.probe_process and time.monotonic()<deadline:
            app.processEvents();time.sleep(.01)
        try:
            self.assertIsNone(page.probe_process)
            self.assertEqual(errors,[])
            self.assertIn('FreeBSD 15.1-RELEASE',page.output.toPlainText(),str(self.target.password_checks))
            self.assertNotIn(self.password,page.output.toPlainText())
        finally:page.shutdown();page.close();owner.close();app.processEvents()

    def test_real_password_tunnel_forwards_local_bytes(self):
        listener=socket.socket();listener.bind(('127.0.0.1',0));listener.listen()
        self.addCleanup(listener.close)
        def echo():
            connection,_=listener.accept()
            with connection:connection.sendall(connection.recv(256))
        threading.Thread(target=echo,daemon=True).start()
        with socket.socket() as probe:
            probe.bind(('127.0.0.1',0));port=probe.getsockname()[1]
        self.profile['forwards']=[dict(local_port=port,remote_host='127.0.0.1',remote_port=listener.getsockname()[1])]
        session=self.session();program,args=session.tunnel()
        process=subprocess.Popen([program,*args],stdin=subprocess.DEVNULL,stdout=subprocess.DEVNULL,stderr=subprocess.PIPE,
            env=dict(os.environ,**session.password_broker.environment),creationflags=subprocess.CREATE_NO_WINDOW if os.name=='nt' else 0)
        try:
            deadline=time.monotonic()+8
            while True:
                try:connection=socket.create_connection(('127.0.0.1',port),timeout=.5);break
                except OSError:
                    if time.monotonic()>deadline or process.poll() is not None:self.fail('Password tunnel did not start')
                    time.sleep(.05)
            with connection:
                connection.settimeout(10);connection.sendall(b'TUNNEL_BYTES')
                try:self.assertEqual(connection.recv(256),b'TUNNEL_BYTES')
                except (TimeoutError,OSError):
                    process.terminate();_,error=process.communicate(timeout=5)
                    self.fail('Tunnel timed out; auth=%r events=%r stderr=%s'%(self.target.attempts,self.target.forward_events,error.decode(errors='replace')))
        finally:
            process.terminate();process.communicate(timeout=5)

    def test_untrusted_host_never_receives_password(self):
        (self.root/'known_hosts').write_text('')
        session=self.session();result=self.execute(session,session.ssh('cat'))
        self.assertNotEqual(result.returncode,0)
        self.assertEqual(self.target.attempts,[])
        self.assertNotIn(self.password.encode(),result.stdout+result.stderr)


if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--native',action='store_true');parser.add_argument('--gui',action='store_true')
    parser.add_argument('--askpass-exe',type=Path)
    args,remaining=parser.parse_known_args();NATIVE,GUI,ASKPASS=args.native,args.gui,args.askpass_exe
    unittest.main(argv=[sys.argv[0],*remaining])
