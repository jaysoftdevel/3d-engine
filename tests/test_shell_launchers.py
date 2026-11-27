import json, os, pathlib, shutil, subprocess, tempfile, unittest
ROOT=pathlib.Path(__file__).resolve().parents[1]
class ShellLaunchers(unittest.TestCase):
    def test_removed_standalone_content(self):
        for name in ('examples','projects','run.sh','run-demo.sh','run-project.sh','include/jsd/showcase_terrain.hpp'):
            self.assertFalse((ROOT/name).exists(),name)
        cmake=(ROOT/'CMakeLists.txt').read_text()
        for target in ('jsd-showcase','jsd-graphics-lab','jsd-project-player'):
            self.assertNotIn('add_executable('+target,cmake)
    @unittest.skipUnless(os.name=='posix' and shutil.which('bash'),'requires bash')
    def test_runner_uses_build_tree(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=pathlib.Path(tmp); shutil.copyfile(ROOT/'test.sh',p/'test.sh')
            (p/'build.sh').write_text('printf "%s\n" "$@" > "$LOG"\nexit "${FAIL:-0}"\n')
            c=p/'ctest'; c.write_text('#!/bin/sh\nprintf "%s\n" "$@" >> "$LOG"\n');c.chmod(0o755)
            env=dict(os.environ,PATH=str(p)+os.pathsep+os.environ['PATH'],LOG=str(p/'log'),JSD_BUILD_TYPE='Release',JSD_BUILD_DIR=str(p/'custom build'))
            subprocess.run(['bash',str(p/'test.sh')],check=True,env=env)
            self.assertEqual((p/'log').read_text().splitlines(),['Release','-DJSD_BUILD_TESTS=ON','--test-dir',str(p/'custom build'),'--output-on-failure'])
            result=subprocess.run(['bash',str(p/'test.sh')],env=dict(env,FAIL='23'))
            self.assertEqual(result.returncode,23)
            self.assertNotIn('--test-dir',(p/'log').read_text())
if __name__=='__main__': unittest.main()
