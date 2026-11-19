from pathlib import Path
from types import SimpleNamespace
import hashlib,json,sys,tempfile,time,unittest
from unittest import mock
ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'SRC/Tools'))
from server_package import drop_reload
from server_package import manage


class Acknowledgements(unittest.TestCase):
    def setUp(self):
        self.folder=tempfile.TemporaryDirectory();self.addCleanup(self.folder.cleanup);self.root=Path(self.folder.name)
        self.children=[dict(name=n,identity=dict(pid=i+100),token=str(i+1)*48,directory=str(self.root/n)) for i,n in enumerate(('ch1-core1','ch1-core2'))]
        for c in self.children:Path(c['directory']).mkdir()
        (self.root/'run').mkdir();self.write(self.root/'run/supervisor.json',dict(phase='listening',supervisor=dict(pid=1),children=self.children))
        self.api=SimpleNamespace(ROOT=self.root,config=lambda:dict(game_servers={c['name']:{} for c in self.children}),
            owned=lambda identity:bool(identity and identity['pid']),read_json=lambda p:json.loads(p.read_text()),
            instance_directory=lambda cfg,name:self.root/name)
        self.version='a'*64;self.release='b'*32
        for c in self.children:self.report(c)

    def write(self,path,value):path.write_text(json.dumps(value))

    def report(self,c,**changes):
        row=dict(protocol=1,pid=c['identity']['pid'],instance=c['name'],token_hash=hashlib.sha256(c['token'].encode()).hexdigest(),
                 updated_at=time.time(),supported=True,base_hash='c'*64,hash=self.version,release=self.release,error='',pending=False)
        row.update(changes);self.write(Path(c['directory'])/'.auriga-drops-status.json',row)

    def test_every_running_core_acknowledges_the_exact_version(self):
        result=drop_reload.wait(self.api,self.version,self.release,0)
        self.assertTrue(result['activated']);self.assertEqual(len(result['cores']),2)

    def test_a_lagging_core_keeps_publication_pending(self):
        self.report(self.children[1],hash='d'*64)
        self.assertFalse(drop_reload.wait(self.api,self.version,self.release,0)['activated'])

    def test_a_rejected_reload_never_reports_success(self):
        self.report(self.children[1],error='invalid drop item')
        self.assertFalse(drop_reload.wait(self.api,self.version,self.release,0)['activated'])

    def test_old_process_and_old_status_cannot_acknowledge(self):
        for change in (dict(pid=9999),dict(token_hash='bad'),dict(updated_at=time.time()-60)):
            self.report(self.children[1],**change)
            self.assertFalse(drop_reload.wait(self.api,self.version,self.release,0)['activated'])

    def test_a_stopped_stack_retains_saved_data_for_next_boot(self):
        self.write(self.root/'run/supervisor.json',dict(phase='stopping',supervisor=dict(pid=1),children=self.children))
        result=drop_reload.wait(self.api,self.version,self.release,0)
        self.assertFalse(result['activated']);self.assertFalse(result['running'])

    def test_status_does_not_leak_supervisor_tokens(self):
        self.assertNotIn(self.children[0]['token'],json.dumps(drop_reload.status(self.api)))

    def test_drop_writer_lock_is_independent_of_the_running_supervisor(self):
        with mock.patch.object(manage,'ROOT',self.root),manage.supervisor_lock():
            with manage.named_lock('drop-publication'):
                with self.assertRaises(RuntimeError):
                    with manage.named_lock('drop-publication'):pass

if __name__=='__main__':unittest.main()
