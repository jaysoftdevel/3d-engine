#!/usr/bin/env python3
"""Build the native Windows x64 engine and showcase with guarded project paths."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys
sys.dont_write_bytecode = True
os.environ['PYTHONDONTWRITEBYTECODE'] = '1'
from project_guard import (GuardError, build_path, install_prefix, no_symlinks,
                           operation_lock, require, source)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument('--configuration', choices=('Release', 'Debug'), default='Release')
    parser.add_argument('--jobs', type=int, default=os.cpu_count() or 4)
    parser.add_argument('--test', action='store_true')
    parser.add_argument('--install', action='store_true')
    args = parser.parse_args()
    require(os.name == 'nt', 'Use build.sh on Linux; build_windows.py requires Windows')
    require(args.jobs > 0, 'Jobs must be positive')
    root = no_symlinks(args.root).resolve()
    require(root == Path(__file__).resolve().parents[1], 'Build driver belongs to another source tree')
    cmake = shutil.which('cmake')
    require(cmake, 'CMake 3.21+ must be on PATH')
    meta = source(root, 'jsd.engine', '0.5.4')
    build = root / 'build/windows-vs2022'
    with operation_lock(root):
        meta = source(root, 'jsd.engine', '0.5.4')
        build_path(root, build, meta)
        install_prefix(root, root, meta)
        subprocess.run([cmake, '-S', str(root), '-B', str(build), '-G', 'Visual Studio 17 2022',
                        '-A', 'x64', '-DJSD_BUILD_SHOWCASE=ON', '-DJSD_BUILD_TESTS=ON',
                        '-DCMAKE_INSTALL_PREFIX='+str(root)], check=True)
        subprocess.run([cmake, '--build', str(build), '--config', args.configuration,
                        '--parallel', str(args.jobs)], check=True)
        if args.test:
            ctest = Path(cmake).with_name('ctest.exe')
            require(ctest.is_file(), 'ctest.exe must be beside cmake.exe')
            subprocess.run([str(ctest), '--test-dir', str(build), '-C', args.configuration,
                            '--output-on-failure'], check=True)
        if args.install:
            subprocess.run([cmake, '--install', str(build), '--config', args.configuration], check=True)

if __name__ == '__main__':
    try:
        main()
    except (GuardError, OSError, subprocess.CalledProcessError) as error:
        print(f'Windows build stopped: {error}', file=sys.stderr)
        sys.exit(2)
