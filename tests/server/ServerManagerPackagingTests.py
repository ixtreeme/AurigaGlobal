"""Reproduce colliding CPython/native OpenSSL DLL names in a frozen bundle."""
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock
import ssl

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / 'SRC/Tools/ServerManager'))
import build_manager


@unittest.skipUnless(os.name == 'nt', 'Windows DLL search isolation')
class PackagingTests(unittest.TestCase):
    def test_python_and_native_openssl_pairs_are_kept_separate(self):
        with tempfile.TemporaryDirectory(prefix='auriga-openssl-bundle-') as temp:
            root = Path(temp)
            python, native, bundle = root / 'python', root / 'native', root / 'bundle'
            internal = bundle / '_internal'
            for directory in (python, native, internal / 'validators'):
                directory.mkdir(parents=True)
            names = ('libcrypto-3-x64.dll', 'libssl-3-x64.dll')
            for name in names:
                (python / name).write_bytes(('python-' + name).encode())
                (native / name).write_bytes(('native-' + name).encode())
            # Dependency collection originally mixed native crypto/Python SSL.
            (internal / names[0]).write_bytes((native / names[0]).read_bytes())
            (internal / names[1]).write_bytes((python / names[1]).read_bytes())
            build_manager.fix_windows_openssl(bundle, native, python)
            for name in names:
                self.assertEqual((internal / name).read_bytes(), (python / name).read_bytes())
                self.assertEqual((internal / 'validators' / name).read_bytes(), (native / name).read_bytes())
                self.assertNotEqual((python / name).read_bytes(), (native / name).read_bytes())

    def test_missing_python_dependency_does_not_partly_change_the_bundle(self):
        with tempfile.TemporaryDirectory(prefix='auriga-openssl-bundle-') as temp:
            root = Path(temp)
            (root / 'python').mkdir()
            (root / 'python/libcrypto-3-x64.dll').write_bytes(b'new crypto')
            internal = root / 'bundle/_internal'
            internal.mkdir(parents=True)
            (internal / 'libcrypto-3-x64.dll').write_bytes(b'old crypto')
            with self.assertRaisesRegex(ValueError, 'Missing CPython OpenSSL dependency'):
                build_manager.fix_windows_openssl(root / 'bundle', root / 'native', root / 'python')
            self.assertEqual((internal / 'libcrypto-3-x64.dll').read_bytes(), b'old crypto')

    def test_python_distribution_trust_policy_is_preserved_unchanged(self):
        with tempfile.TemporaryDirectory(prefix='auriga-openssl-bundle-') as temp:
            root = Path(temp)
            python = root / 'python'
            python.mkdir()
            for name in ('libcrypto-3-x64.dll', 'libssl-3-x64.dll'):
                (python / name).write_bytes(b'python DLL fixture')
            policy = python / 'codex-rejected-cas.pem'
            policy.write_bytes(b'public certificate distrust policy fixture\n')
            internal = root / 'bundle/_internal'
            internal.mkdir(parents=True)
            with mock.patch.object(ssl, '__file__', str(python / 'ssl.py')):
                build_manager.fix_windows_openssl(root / 'bundle', root / 'native', python)
            self.assertEqual((internal / policy.name).read_bytes(), policy.read_bytes())


if __name__ == '__main__':
    unittest.main()
