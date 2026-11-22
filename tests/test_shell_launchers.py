"""Exercise the shipped launch chain when ZIP extraction loses execute bits."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import make_update
from test_version_guards import fixture


@unittest.skipUnless(os.name == 'posix' and shutil.which('bash'), 'POSIX shell launcher tests require bash')
class ShellLaunchers(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='jsd launcher spaces ')
        self.root = Path(self.temp.name) / 'engine with spaces'
        self.root.mkdir()
        self.log = Path(self.temp.name) / 'calls.jsonl'
        self.env = dict(os.environ, JSD_TEST_LOG=str(self.log), PYTHONDONTWRITEBYTECODE='1')
        for name in ('build.sh', 'run-showcase.sh', 'run-project.sh', 'run-demo.sh', 'run.sh', 'test.sh'):
            shutil.copyfile(ROOT / name, self.root / name)
            (self.root / name).chmod(0o644)
        (self.root / 'tools').mkdir()
        (self.root / 'tools/build_project.py').write_text(
            'import json,os,sys\n'
            'with open(os.environ["JSD_TEST_LOG"],"a") as f: f.write(json.dumps(["build",sys.argv[1:]])+"\\n")\n'
            'sys.exit(int(os.environ.get("JSD_TEST_BUILD_STATUS","0")))\n')
        (self.root / 'bin').mkdir()
        for name in ('jsd-showcase', 'jsd-project-player', 'jsd-graphics-lab', 'jsd-physics-tests', 'jsd-engine-tests', 'jsd-showcase-physics-tests'):
            exe = self.root / 'bin' / name
            exe.write_text('#!/usr/bin/env python3\nimport json,os,sys\n'
                           'from pathlib import Path\n'
                           'with open(os.environ["JSD_TEST_LOG"],"a") as f: f.write(json.dumps([Path(sys.argv[0]).name,sys.argv[1:]])+"\\n")\n')
            exe.chmod(0o755)

    def tearDown(self):
        self.temp.cleanup()

    def run_script(self, name, args=(), status=0, direct=False):
        if self.log.exists():
            self.log.unlink()
        env = dict(self.env, JSD_TEST_BUILD_STATUS=str(status))
        command = [str(self.root / name), *args] if direct else ['bash', str(self.root / name), *args]
        result = subprocess.run(command, cwd=self.temp.name,
                                env=env, text=True, capture_output=True)
        calls = [json.loads(line) for line in self.log.read_text().splitlines()] if self.log.exists() else []
        return result, calls

    def test_showcase_without_child_execute_bit(self):
        (self.root / 'run-showcase.sh').chmod(0o755)
        result, calls = self.run_script('run-showcase.sh', ['--frames', '1', '--no-audio'], direct=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(calls[-1], ['jsd-showcase', ['--frames', '1', '--no-audio']])
        self.assertIn('-DJSD_BUILD_SHOWCASE=ON', calls[0][1])
        self.assertIn(str(self.root), calls[0][1])

    def test_nested_dispatch_routes(self):
        for name, args, binary in [('run.sh', [], 'jsd-graphics-lab'),
                                   ('run-demo.sh', ['--showcase', '--no-ui'], 'jsd-showcase'),
                                   ('run-showcase.sh', ['--project', 'a scene.jsdproject'], 'jsd-project-player')]:
            with self.subTest(name=name):
                result, calls = self.run_script(name, args)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(calls[-1][0], binary)

    def test_build_failure_stops_launch(self):
        result, calls = self.run_script('run-showcase.sh', status=37)
        self.assertEqual(result.returncode, 37)
        self.assertEqual([c[0] for c in calls], ['build'])

    def test_test_runner_without_child_execute_bit(self):
        result, calls = self.run_script('test.sh', ['--with-showcase'])
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(calls[0][0], 'build')
        self.assertEqual(calls[-1][0], 'jsd-showcase')

    def test_mode_only_update_is_included(self):
        base = Path(self.temp.name) / 'base'
        target = Path(self.temp.name) / 'target'
        fixture(base, 'jsd.engine', '0.5.8')
        fixture(target, 'jsd.engine', '0.5.14')
        # Both files deliberately lack +x; the packager must still encode 0755.
        (base / 'build.sh').chmod(0o644)
        (target / 'build.sh').chmod(0o644)
        component = make_update.component(base, target, 'jsd.engine', Path(self.temp.name) / 'package')
        entry = next(f for f in component['files'] if f['path'] == 'build.sh')
        self.assertEqual(entry['mode'], 0o755)
        self.assertEqual(entry['before_sha256'], entry['sha256'])


if __name__ == '__main__':
    unittest.main()
