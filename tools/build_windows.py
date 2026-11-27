#!/usr/bin/env python3
"""Guarded native Windows build/install driver used by the batch entry points."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
sys.dont_write_bytecode = True
os.environ['PYTHONDONTWRITEBYTECODE'] = '1'
from project_guard import (GuardError, build_path, cache_values, install_prefix,
                           no_symlinks, operation_lock, require, source)
SOURCE_ROOT = Path(__file__).resolve().parents[1]
IS_WINDOWS = os.name == 'nt'
GENERATORS = {18: 'Visual Studio 18 2026', 17: 'Visual Studio 17 2022'}

def visual_studios():
    candidates = [shutil.which('vswhere')]
    for name in ('ProgramFiles(x86)', 'ProgramFiles'):
        if os.environ.get(name):
            candidates.append(str(Path(os.environ[name])/'Microsoft Visual Studio/Installer/vswhere.exe'))
    tool = next((p for p in candidates if p and Path(p).is_file()), None)
    if not tool:
        return []
    result = subprocess.run([tool, '-products', '*', '-requires',
        'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-format', 'json', '-utf8'],
        check=True, capture_output=True, encoding='utf-8-sig')
    entries = json.loads(result.stdout)
    valid = [e for e in entries if e.get('isComplete', True) and
             int(e.get('installationVersion', '0').split('.')[0]) in GENERATORS]
    return sorted(valid, key=lambda e: tuple(map(int,e['installationVersion'].split('.'))), reverse=True)

def select_toolchain(explicit_cmake=None, explicit_generator=None):
    studios = visual_studios()
    candidates = [explicit_cmake] if explicit_cmake else [shutil.which('cmake')]
    if not explicit_cmake:
        if os.environ.get('ProgramFiles'):
            candidates.append(str(Path(os.environ['ProgramFiles'])/'CMake/bin/cmake.exe'))
        candidates += [str(Path(e['installationPath'])/'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe') for e in studios]
    diagnostics = []
    for candidate in dict.fromkeys(p for p in candidates if p):
        if not Path(candidate).is_file():
            continue
        try:
            result = subprocess.run([candidate,'-E','capabilities'],check=True,capture_output=True,text=True)
            data = json.loads(result.stdout)
            ver = data['version']
            if (ver['major'],ver['minor']) < (3,21):
                diagnostics.append(f'{candidate}: CMake 3.21+ required'); continue
            available = {g['name'] for g in data['generators']}
            if explicit_generator:
                require(explicit_generator in available, f'{candidate} does not support {explicit_generator}')
                match = next((e for e in studios if GENERATORS[int(e['installationVersion'].split('.')[0])] == explicit_generator), None)
                return str(Path(candidate).resolve()), explicit_generator, match['installationPath'] if match else None
            for studio in studios:
                generator = GENERATORS[int(studio['installationVersion'].split('.')[0])]
                if generator in available:
                    return str(Path(candidate).resolve()), generator, studio['installationPath']
            diagnostics.append(f'{candidate}: no matching installed Visual Studio C++ generator')
        except (OSError, ValueError, KeyError, subprocess.CalledProcessError, GuardError) as error:
            diagnostics.append(str(error))
    raise GuardError('No usable CMake/Visual Studio C++ toolchain found. Install the Visual Studio '
        'Desktop development with C++ workload and Windows SDK, plus CMake 3.21+ '
        '(or the Visual Studio CMake component). Use --cmake PATH or --generator NAME if needed. '
        + '; '.join(diagnostics))

def run_stage(command, log):
    print('Running: '+subprocess.list2cmdline([str(x) for x in command]),flush=True)
    no_symlinks(log)
    with log.open('w',encoding='utf-8') as handle:
        # Argument arrays preserve spaces and avoid cmd.exe/PowerShell reparsing.
        result = subprocess.run(command,stdout=handle,stderr=subprocess.STDOUT)
    if result.returncode:
        print(log.read_text(encoding='utf-8',errors='replace')[-12000:],file=sys.stderr)
        print(f'Failure log: {log}',file=sys.stderr)
        raise subprocess.CalledProcessError(result.returncode,command)
    print(f'Completed. Log: {log}',flush=True)

def run(root, configuration, jobs, test, install, check=False, cmake=None, generator=None):
    require(jobs > 0, 'Jobs must be positive')
    root = no_symlinks(root).resolve()
    require(root == SOURCE_ROOT, 'Build driver belongs to another source tree')
    meta = source(root,'jsd.engine','0.5.22')
    cmake, generator, instance = select_toolchain(cmake,generator)
    folder = 'windows-vs2026' if generator == GENERATORS[18] else 'windows-vs2022'
    build = root/'build'/folder
    build_path(root,build,meta)
    install_prefix(root,root,meta)
    cache = cache_values(build)
    require(not cache.get('CMAKE_GENERATOR') or cache['CMAKE_GENERATOR'] == generator,
            f'Build cache uses another generator: {build}')
    require(not cache.get('CMAKE_GENERATOR_PLATFORM') or cache['CMAKE_GENERATOR_PLATFORM'] == 'x64',
            f'Build cache uses another architecture: {build}; use a separate x64 build directory')
    if instance and cache.get('CMAKE_GENERATOR_INSTANCE'):
        require(Path(cache['CMAKE_GENERATOR_INSTANCE'].split(',')[0]).resolve() == Path(instance).resolve(),
                f'Build cache belongs to a different Visual Studio installation: {build}')
    print(f'Engine 0.5.22 / {generator} / x64 / {configuration}\nCMake: {cmake}\nBuild: {build}',flush=True)
    if check:
        print('Preflight passed; no files changed.'); return
    with operation_lock(root):
        meta = source(root,'jsd.engine','0.5.22')
        build_path(root,build,meta); install_prefix(root,root,meta)
        build.mkdir(parents=True,exist_ok=True)
        # Claim ownership before logs appear in a fresh guarded build directory.
        from project_guard import record_build
        record_build(root,build,meta)
        configure = [cmake,'-S',str(root),'-B',str(build),'-G',generator,'-A','x64',
                     '-DJSD_BUILD_TESTS=' + ('ON' if test else 'OFF'),
                     '-DCMAKE_INSTALL_PREFIX='+str(root),
                     '-DJSD_GUARD_PYTHON:FILEPATH='+sys.executable]
        if instance: configure.append('-DCMAKE_GENERATOR_INSTANCE='+instance)
        run_stage(configure,build/'configure.log')
        run_stage([cmake,'--build',str(build),'--config',configuration,'--parallel',str(jobs)],build/f'build-{configuration.lower()}.log')
        if test:
            ctest = Path(cmake).with_name('ctest.exe')
            require(ctest.is_file(),f'ctest.exe is missing beside {cmake}')
            run_stage([str(ctest),'--test-dir',str(build),'-C',configuration,'--output-on-failure'],build/f'test-{configuration.lower()}.log')
        if install:
            run_stage([cmake,'--install',str(build),'--config',configuration],build/f'install-{configuration.lower()}.log')
            print(f'Installed engine into {root}',flush=True)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('mode',nargs='?',type=str.title,choices=('Release','Debug'))
    parser.add_argument('--root',type=Path,default=SOURCE_ROOT)
    parser.add_argument('--configuration',choices=('Release','Debug'))
    parser.add_argument('--jobs',type=int,default=os.cpu_count() or 4)
    parser.add_argument('--test',action='store_true')
    parser.add_argument('--install',action='store_true')
    parser.add_argument('--check',action='store_true')
    parser.add_argument('--cmake')
    parser.add_argument('--generator',choices=tuple(GENERATORS.values()))
    args = parser.parse_args()
    require(IS_WINDOWS,'Use build.sh on Linux; the batch installer requires Windows')
    require(not(args.mode and args.configuration and args.mode!=args.configuration),'Conflicting configurations')
    run(args.root,args.configuration or args.mode or 'Release',args.jobs,args.test,args.install,args.check,args.cmake,args.generator)

if __name__ == '__main__':
    try:
        main()
    except subprocess.CalledProcessError as error:
        print(f'Windows build stopped: command exited {error.returncode}',file=sys.stderr)
        sys.exit(error.returncode if 1 <= error.returncode <= 255 else 2)
    except (GuardError,OSError,ValueError,KeyError) as error:
        print(f'Windows build stopped: {error}',file=sys.stderr)
        sys.exit(2)
