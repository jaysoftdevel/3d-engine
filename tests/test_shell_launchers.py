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
    def test_runner_passes_configuration_to_timed_build_driver(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=pathlib.Path(tmp); shutil.copyfile(ROOT/'test.sh',p/'test.sh')
            (p/'tools').mkdir()
            (p/'tools/build_project.py').write_text(
                'import json, os, sys\n'
                'with open("invocation.json", "w") as log:\n'
                '    json.dump([sys.argv[1:], os.environ["JSD_BUILD_DIR"]], log)\n'
                'sys.exit(int(os.environ.get("FAIL", "0")))\n'
            )
            env=dict(os.environ,JSD_BUILD_TYPE='Debug',JSD_BUILD_DIR=str(p/'custom build'))
            subprocess.run(['bash',str(p/'test.sh')],check=True,env=env)
            arguments, directory=json.loads((p/'invocation.json').read_text())
            version=json.loads((ROOT/'JSD_PROJECT.json').read_text())['version']
            self.assertEqual(arguments,['--root', str(p), '--project', 'jsd.engine',
                                        '--expected', version, '--test', '--', 'Debug'])
            self.assertEqual(directory,str(p/'custom build'))
            result=subprocess.run(['bash',str(p/'test.sh')],env=dict(env,FAIL='23'))
            self.assertEqual(result.returncode,23)
if __name__=='__main__': unittest.main()
