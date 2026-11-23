"""Keep shipped launchers aligned with real release metadata before tool/dependency work."""
import contextlib
import io
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from test_version_guards import fixture
import build_windows
import project_guard

CURRENT = json.loads((ROOT / 'JSD_PROJECT.json').read_text())['version']

class BuildEntrypoints(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='jsd release entrypoints ')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name) / 'engine source'
        fixture(self.root, 'jsd.engine', CURRENT)
        self.bin = Path(self.temp.name) / 'fake tools'
        self.bin.mkdir()
        self.log = Path(self.temp.name) / 'tool-calls'
        self.env = dict(os.environ, PATH=str(self.bin) + os.pathsep + os.environ['PATH'],
                        JSD_HEADLESS_ONLY='ON', JSD_AUTO_INSTALL_DEPS='0',
                        JSD_TEST_LOG=str(self.log), PYTHONDONTWRITEBYTECODE='1',
                        JSD_BUILD_DIR=str(self.root / 'build/release'),
                        JSD_INSTALL_PREFIX=str(Path(self.temp.name) / 'sdk'))
        for name in ('build.sh', 'install_deps.sh', 'tools/project_guard.py',
                     'tools/build_project.py', 'cmake/build-common.sh'):
            dest = self.root / name
            dest.parent.mkdir(exist_ok=True, parents=True)
            shutil.copyfile(ROOT / name, dest)
        self.fake('cmake', 'import sys\nif sys.argv[1:]==["--version"]: print("cmake version 3.26.0")\n')
        self.env['CMAKE'] = str(self.bin / 'cmake')
        self.fake('apt-get', '')
        self.fake('sudo', 'import subprocess,sys\nsys.exit(subprocess.call(sys.argv[1:]))\n')

    def fake(self, name, body):
        p = self.bin / name
        p.write_text('#!' + sys.executable + '\nimport os,json,sys\n'
                     'with open(os.environ["JSD_TEST_LOG"],"a") as f: f.write(json.dumps([sys.argv[0],sys.argv[1:]])+"\\n")\n' + body)
        p.chmod(0o755)

    def shell(self, script):
        return subprocess.run(['bash', str(self.root / script)], cwd=self.temp.name,
                              env=self.env, text=True, capture_output=True)

    @unittest.skipUnless(os.name == 'posix' and shutil.which('bash'), 'POSIX launcher')
    def test_current_linux_launcher_reaches_configure_build_install(self):
        result = self.shell('build.sh')
        self.assertEqual(result.returncode, 0, result.stderr)
        calls = [json.loads(line)[1] for line in self.log.read_text().splitlines()]
        self.assertEqual(calls[0], ['--version'])
        self.assertEqual(calls[1][:2], ['-S', str(self.root)])
        self.assertEqual(calls[2][:2], ['--build', str(self.root / 'build/release')])
        self.assertEqual(calls[3][:2], ['--install', str(self.root / 'build/release')])
        self.assertIn('Built jsd.engine ' + CURRENT, result.stdout)

    @unittest.skipUnless(os.name == 'posix' and shutil.which('bash'), 'POSIX installer')
    def test_current_dependency_installer_passes_guard(self):
        result = self.shell('install_deps.sh')
        self.assertEqual(result.returncode, 0, result.stderr)
        calls = [json.loads(line) for line in self.log.read_text().splitlines()]
        apt = [args for name, args in calls if Path(name).name == 'apt-get']
        self.assertEqual(apt[0], ['update'])
        self.assertEqual(apt[1][:2], ['install', '-y'])

    @unittest.skipUnless(os.name == 'posix' and shutil.which('bash'), 'POSIX guards')
    def test_wrong_version_and_project_stop_before_external_tools(self):
        for pid, version in [('jsd.engine', '0.5.20'), ('jsd.editor', CURRENT)]:
            fixture(self.root, pid, version)
            # fixture rewrites build.sh, so restore the real launcher.
            shutil.copyfile(ROOT / 'build.sh', self.root / 'build.sh')
            for script in ('build.sh', 'install_deps.sh'):
                with self.subTest(project=pid, script=script):
                    result = self.shell(script)
                    self.assertNotEqual(result.returncode, 0)
                    self.assertFalse(self.log.exists(), result.stdout + result.stderr)
                    self.assertFalse((self.root / 'build').exists())

    def test_windows_preflight_uses_current_release(self):
        with patch.object(build_windows, 'SOURCE_ROOT', self.root), \
             patch.object(build_windows, 'select_toolchain', return_value=('cmake.exe', build_windows.GENERATORS[17], None)), \
             contextlib.redirect_stdout(io.StringIO()):
            build_windows.run(self.root, 'Release', 2, False, False, check=True)
        self.assertFalse((self.root / 'build').exists())
        fixture(self.root, 'jsd.engine', '0.5.20')
        with patch.object(build_windows, 'SOURCE_ROOT', self.root), \
             patch.object(build_windows, 'select_toolchain') as toolchain:
            with self.assertRaises(project_guard.GuardError):
                build_windows.run(self.root, 'Release', 2, False, False, check=True)
            toolchain.assert_not_called()

if __name__ == '__main__':
    unittest.main()
