"""Configure the real system-dependency module against isolated fake SDKs."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

REPO=Path(__file__).resolve().parents[2]


class SystemDependencyTests(unittest.TestCase):
    def configure(self, mysql='mysql', empty=False, override=False, missing_json=False):
        cmake=shutil.which('cmake')
        if not cmake:self.skipTest('CMake required')
        ninja=shutil.which('ninja')
        if not ninja:
            candidate=Path('C:/Program Files/Microsoft Visual Studio/18/Professional/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe')
            if candidate.is_file():ninja=str(candidate)
        if not ninja:self.skipTest('Ninja required')
        with tempfile.TemporaryDirectory(prefix='auriga-system-deps-') as temporary:
            root=Path(temporary);source=root/'source';source.mkdir();prefix=root/'sdk';build=root/'build'
            include=prefix/'include'
            headers=['boost/unordered_map.hpp','stb/stb_image.h','lz4.h','lzo/lzo1x.h']
            if not missing_json:headers.append('nlohmann/json.hpp')
            for header in headers:
                path=include/header;path.parent.mkdir(parents=True,exist_ok=True);path.touch()
            mysql_include=root/'override' if override else include
            for header in ('mysql.h','errmsg.h','mysqld_error.h'):
                path=mysql_include/mysql/header;path.parent.mkdir(parents=True,exist_ok=True);path.touch()
            library=prefix/'lib';library.mkdir()
            for name in ('lz4','lzo2','mysqlclient'):(library/('lib'+name+'.a')).touch()
            (source/'FindThreads.cmake').write_text('add_library(Threads::Threads INTERFACE IMPORTED)\nset(Threads_FOUND TRUE)\n')
            (source/'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.20)\nproject(Deps NONE)\n'
                'set(CMAKE_FIND_LIBRARY_PREFIXES lib)\nset(CMAKE_FIND_LIBRARY_SUFFIXES .a)\n'
                'set(CMAKE_FIND_USE_SYSTEM_ENVIRONMENT_PATH FALSE)\n'
                'set(CMAKE_FIND_USE_CMAKE_SYSTEM_PATH FALSE)\n'
                'list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}")\n'
                'include("'+(REPO/'cmake/AurigaSystemDeps.cmake').as_posix()+'")\n'
                'file(WRITE "${CMAKE_BINARY_DIR}/found.txt" "${AURIGA_MYSQL_INCLUDE_DIR}")\n')
            args=[cmake,'-S',str(source),'-B',str(build),'-G','Ninja','-DCMAKE_MAKE_PROGRAM='+ninja,
                  '-DCMAKE_TOOLCHAIN_FILE=','-DCMAKE_PREFIX_PATH='+prefix.as_posix()]
            if empty:args.append('-DAURIGA_MYSQL_INCLUDE_DIR:PATH=')
            if override:args.append('-DAURIGA_MYSQL_INCLUDE_DIR:PATH='+mysql_include.as_posix())
            result=subprocess.run(args,capture_output=True,text=True,timeout=20)
            if missing_json:
                self.assertNotEqual(result.returncode,0)
                self.assertIn('pkg install nlohmann-json',' '.join((result.stdout+result.stderr).split()))
                return
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            found=Path((build/'found.txt').read_text())
            if mysql=='mysql':self.assertEqual(found.resolve(),mysql_include.resolve())
            else:
                self.assertEqual(found.resolve(),(build/'auriga_mysql_compat').resolve())
                self.assertIn('#include <mariadb/mysql.h>',(found/'mysql/mysql.h').read_text())

    def test_classic_mysql_headers_discovered(self):self.configure()
    def test_previous_empty_cache_recovers(self):self.configure(empty=True)
    def test_explicit_include_override_preserved(self):self.configure(override=True)
    def test_mariadb_headers_get_forwarding_headers(self):self.configure(mysql='mariadb')
    def test_missing_json_stops_configuration_with_package_hint(self):self.configure(missing_json=True)


if __name__=='__main__':unittest.main()
