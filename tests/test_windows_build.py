"""Portable tests for Windows tool selection, stage failures and guarded paths."""
import contextlib
import importlib.util
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
import build_windows as driver
import project_guard as guard

class WindowsBuild(unittest.TestCase):
    def test_visual_studio_and_bundled_cmake(self):
        with tempfile.TemporaryDirectory(prefix='tools with spaces ') as temp:
            root=Path(temp)
            cmake=root/'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
            cmake.parent.mkdir(parents=True);cmake.touch()
            studio={'installationPath':str(root),'installationVersion':'18.0.1.0'}
            caps={'version':{'major':4,'minor':2},'generators':[{'name':driver.GENERATORS[18]}]}
            with patch.object(driver,'visual_studios',return_value=[studio]),patch.object(driver.shutil,'which',return_value=None),patch.object(driver.subprocess,'run',return_value=subprocess.CompletedProcess([],0,json.dumps(caps),'')):
                actual=driver.select_toolchain()
            self.assertEqual(actual,(str(cmake.resolve()),driver.GENERATORS[18],str(root)))

    def test_explicit_unsupported_generator_refused(self):
        with tempfile.TemporaryDirectory() as temp:
            exe=Path(temp)/'cmake.exe';exe.touch()
            caps={'version':{'major':3,'minor':21},'generators':[{'name':driver.GENERATORS[17]}]}
            with patch.object(driver,'visual_studios',return_value=[]),patch.object(driver.subprocess,'run',return_value=subprocess.CompletedProcess([],0,json.dumps(caps),'')):
                with self.assertRaises(guard.GuardError):
                    driver.select_toolchain(str(exe),driver.GENERATORS[18])

    def test_check_writes_nothing_and_build_failure_never_installs(self):
        with tempfile.TemporaryDirectory(prefix='engine with spaces ') as temp:
            root=Path(temp);meta={'project_id':'jsd.engine','version':'0.5.21'}
            cmake=root/'cmake.exe'
            common=[patch.object(driver,'SOURCE_ROOT',root),
                    patch.object(driver,'source',return_value=meta),
                    patch.object(driver,'select_toolchain',return_value=(str(cmake),driver.GENERATORS[17],None))]
            with contextlib.ExitStack() as stack:
                for item in common:stack.enter_context(item)
                with contextlib.redirect_stdout(io.StringIO()):
                    driver.run(root,'Release',2,False,True,check=True)
                self.assertEqual(list(root.iterdir()),[])
                commands=[]
                def stage(command,log):
                    commands.append(command)
                    if '--build' in command:raise subprocess.CalledProcessError(37,command)
                with patch.object(driver,'run_stage',side_effect=stage),contextlib.redirect_stdout(io.StringIO()):
                    with self.assertRaises(subprocess.CalledProcessError) as caught:
                        driver.run(root,'Release',2,True,True)
                self.assertEqual(caught.exception.returncode,37)
                self.assertEqual(len(commands),2)
                self.assertIn('-DJSD_GUARD_PYTHON:FILEPATH='+sys.executable,commands[0])
                self.assertIn(str(root),commands[0])
                self.assertFalse(any('--install' in c or '--test-dir' in c for c in commands))
                self.assertTrue((root/'build/windows-vs2022/JSD_BUILD.json').is_file())

    def test_stage_failure_keeps_diagnostic_log_and_exit_code(self):
        with tempfile.TemporaryDirectory() as temp:
            log=Path(temp)/'build.log'
            def fail(command,stdout,stderr):
                stdout.write('fatal build diagnostic\n');return subprocess.CompletedProcess(command,19)
            with patch.object(driver.subprocess,'run',side_effect=fail),contextlib.redirect_stdout(io.StringIO()),contextlib.redirect_stderr(io.StringIO()):
                with self.assertRaises(subprocess.CalledProcessError) as caught:
                    driver.run_stage(['cmake','--build','C:\\engine path'],log)
            self.assertEqual(caught.exception.returncode,19)
            self.assertEqual(log.read_text(),'fatal build diagnostic\n')

if __name__=='__main__':unittest.main()
