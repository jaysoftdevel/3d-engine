"""Exercise the actual OS lock implementation and source/version guards."""
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import project_guard as guard

class PortableGuards(unittest.TestCase):
    def test_source_identity(self):
        self.assertEqual(guard.source(ROOT, 'jsd.engine', '0.5.22')['api_version'], 1)
        with self.assertRaises(guard.GuardError):
            guard.source(ROOT, 'jsd.engine', '0.5.6')
        with self.assertRaises(guard.GuardError):
            guard.source(ROOT, 'jsd.editor')

    def test_lock_contention_and_release(self):
        code = '''
import sys
from pathlib import Path
sys.path.insert(0, sys.argv[1])
from project_guard import operation_lock, GuardError
try:
    with operation_lock(Path(sys.argv[2])): pass
except GuardError:
    sys.exit(7)
'''
        with tempfile.TemporaryDirectory() as temp:
            command = [sys.executable, '-B', '-c', code, str(ROOT/'tools'), temp]
            with guard.operation_lock(Path(temp)):
                self.assertEqual(subprocess.run(command).returncode, 7)
            self.assertEqual(subprocess.run(command).returncode, 0)

if __name__ == '__main__':
    unittest.main()
