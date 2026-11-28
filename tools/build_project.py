#!/usr/bin/env python3
"""Guarded, project-local build driver. No cross-project source writes."""
import argparse
import contextlib
import os
import re
from pathlib import Path
import shutil
import subprocess
import sys
from build_report import BuildReport
from project_guard import (GuardError, build_path, cache_values, compatible, digest,
    install_prefix, no_symlinks, operation_lock, output_path, read_json, record_build, require, sdk, source)


def definitions(arguments):
    result = {}
    index = 0
    while index < len(arguments):
        arg = arguments[index]
        if arg == '-D':
            index += 1
            require(index < len(arguments), '-D requires a definition')
            arg += arguments[index]
        require(arg.startswith('-D') and '=' in arg, 'Only CMake -DNAME=VALUE options are accepted; use build/prefix environment variables for paths')
        key, value = arg[2:].split('=', 1)
        key = key.split(':', 1)[0]
        require(key and key not in result, f'Duplicate or empty CMake definition: {key}')
        require(key not in ('CMAKE_HOME_DIRECTORY', 'CMAKE_PROJECT_NAME', 'CMAKE_TOOLCHAIN_FILE', 'CMAKE_PROJECT_INCLUDE', 'CMAKE_PROJECT_INCLUDE_BEFORE') and not key.startswith('CMAKE_INSTALL_') and not key.startswith('CMAKE_PROJECT_') and not key.endswith('_OUTPUT_DIRECTORY'), f'Managed/redirecting CMake option is not allowed: {key}')
        require(key != 'CMAKE_BUILD_TYPE', 'Select release/debug instead of overriding CMAKE_BUILD_TYPE')
        result[key] = value
        index += 1
    return result


def preflight(root, project_id, expected, args, env):
    meta = source(root, project_id, expected)
    require(not env.get('DESTDIR'), 'DESTDIR is not supported by guarded installs; select an explicit project-owned install prefix')
    mode = args[0] if args and (not args[0].startswith('-') or args[0] == '--clean') else env.get('JSD_BUILD_TYPE', 'Release')
    if args and args[0] in ('release', 'Release', 'debug', 'Debug', 'clean', '--clean'):
        args = args[1:]
    clean = mode in ('clean', '--clean') or (args and args[0] == '--clean')
    require(clean or mode in ('release', 'Release', 'debug', 'Debug'), 'Build mode must be release, debug or clean')
    require(not clean or not args, 'clean accepts no additional options')
    opts = definitions(args)
    engine = project_id == 'jsd.engine'
    build = Path(env.get('JSD_BUILD_DIR' if engine else 'JSD_EDITOR_BUILD_DIR', str(root / 'build' / mode.lower()) if engine else str(root / 'build'))).absolute()
    prefix = Path(env.get('JSD_INSTALL_PREFIX' if engine else 'JSD_EDITOR_INSTALL_PREFIX', str(root))).absolute()
    install_prefix(root, prefix, meta)
    if clean:
        candidates = [root / name for name in ('build', 'prebuild', 'prebuilds', 'prebuilt', 'prebuilts')]
        candidates.append(build)
        clean_paths = []
        for path in dict.fromkeys(candidates):
            build_path(root, path, meta, allow_container=True)
            clean_paths.append(path)
        # Evaluate every candidate and output before the first deletion.
        receipt_path = prefix / 'share' / ('jsd-game-engine' if engine else 'jsd-editor') / 'INSTALL.json'
        generated = []
        if receipt_path.exists():
            receipt = read_json(receipt_path)
            require(receipt.get('project_id') == project_id, 'Wrong installed output identity')
            for rel in receipt.get('files', {}):
                p = prefix / rel
                # A source-local engine install shares its public headers with source.
                if prefix == root and rel.startswith('include/'):
                    continue
                from project_guard import safe_file
                generated.append(safe_file(prefix, rel))
            generated.append(receipt_path)
        for pattern in (('bin/jsd-*', 'bin/sdl2-config', 'lib/libjsd-game-engine*', 'lib/libfrontier-engine*', 'lib/libSDL2*', 'lib/libJolt*', 'lib/cmake/JSDGameEngine', 'lib/cmake/FrontierEngine', 'lib/cmake/SDL2', 'lib/pkgconfig/jsd-game-engine.pc', 'lib/pkgconfig/frontier-engine.pc', 'lib/pkgconfig/sdl2.pc', 'share/jsd-game-engine', 'share/aclocal/sdl2.m4', 'include/Jolt', 'include/SDL2') if engine else ('bin/jsd-editor', 'lib/libjsd-game-engine*', 'lib/libSDL2*', 'share/jsd-editor')):
            for path in prefix.glob(pattern):
                require(not path.is_symlink() or (prefix in path.resolve().parents and path.is_file()), f'Generated output escapes prefix: {path}')
                generated.append(path)
        if engine and prefix != root:
            generated.extend(prefix / name for name in ('include/jsd', 'include/frontier', 'include/frontier-compat'))
        return meta, build, prefix, opts, (clean_paths, generated)
    build_path(root, build, meta)
    cached = cache_values(build)
    if not engine:
        installed = opts.get('JSD_EDITOR_USE_INSTALLED_ENGINE', cached.get('JSD_EDITOR_USE_INSTALLED_ENGINE', 'OFF')).upper() in ('ON', 'TRUE', '1', 'YES')
        if installed:
            config = opts.get('JSDGameEngine_DIR', cached.get('JSDGameEngine_DIR'))
            if not config:
                roots = opts.get('CMAKE_PREFIX_PATH', env.get('CMAKE_PREFIX_PATH', '')).split(';')
                found = [Path(p) / 'lib/cmake/JSDGameEngine' for p in roots if p and (Path(p) / 'lib/cmake/JSDGameEngine').is_dir()]
                require(len(found) == 1, 'Specify exactly one installed SDK with -DJSDGameEngine_DIR=/sdk/lib/cmake/JSDGameEngine')
                config = str(found[0])
            dep = sdk(config)
            compatible(dep, meta)
            opts['JSDGameEngine_DIR'] = str(Path(config).resolve())
        else:
            dep_root = Path(opts.get('JSD_ENGINE_SOURCE_DIR', cached.get('JSD_ENGINE_SOURCE_DIR', str(root.parent / '3d-engine')))).resolve()
            require(dep_root != root and dep_root not in root.parents and root not in dep_root.parents, 'Engine and editor must be separate source projects')
            compatible(source(dep_root, 'jsd.engine'), meta)
            opts['JSD_ENGINE_SOURCE_DIR'] = str(dep_root)
    return meta, build, prefix, opts, None


def run(root, project_id, expected, args, test=False):
    root = root.resolve()
    if args in (['--help'], ['-h']):
        print('Usage: ./build.sh [release|debug|clean] [CMake -DNAME=VALUE ...]\nBuild and SDK paths must belong to this project. See docs/VERSION_GUARDS.md.')
        return
    env = dict(os.environ)
    plan = preflight(root, project_id, expected, args, env)
    lock_roots = {root}
    if project_id == 'jsd.editor' and 'JSD_ENGINE_SOURCE_DIR' in plan[3] and plan[3].get('JSD_EDITOR_USE_INSTALLED_ENGINE', cache_values(plan[1]).get('JSD_EDITOR_USE_INSTALLED_ENGINE', 'OFF')).upper() not in ('ON', 'TRUE', '1', 'YES'):
        lock_roots.add(Path(plan[3]['JSD_ENGINE_SOURCE_DIR']))
    with contextlib.ExitStack() as stack:
        for locked in sorted(lock_roots):
            stack.enter_context(operation_lock(locked))
        meta, build, prefix, opts, cleaning = preflight(root, project_id, expected, args, env)
        if cleaning:
            for path in cleaning[0] + cleaning[1]:
                if path.is_symlink() or path.is_file():
                    path.unlink(missing_ok=True)
                elif path.is_dir():
                    shutil.rmtree(path)
            print(f"Cleaned {project_id} {meta['version']}; source files preserved")
            return
        env['JSD_HEADLESS_ONLY'] = opts.get('JSD_HEADLESS_ONLY', env.get('JSD_HEADLESS_ONLY', 'OFF'))
        cmake = env.get('CMAKE', 'cmake')
        require(shutil.which(cmake), f'CMake 3.21+ required: {cmake}')
        probe = subprocess.run([cmake, '--version'], check=True, capture_output=True, text=True, env=env)
        match = re.search(r'cmake version (\d+)\.(\d+)\.(\d+)', probe.stdout)
        require(match and tuple(map(int, match.groups())) >= (3, 21, 0), 'CMake 3.21+ is required; dependency installation/build did not run')
        if project_id == 'jsd.engine':
            # Dependency installation occurs only after every target/path/version gate.
            subprocess.run(['bash', '-c', 'source "$1/cmake/build-common.sh"; jsd_engine_root="$1"; jsd_require_tools', 'jsd-build', str(root)], check=True, env=env)
        selected_mode = args[0] if args and args[0].lower() in ('debug', 'release') else env.get('JSD_BUILD_TYPE', 'Release')
        build_type = 'Debug' if selected_mode.lower() == 'debug' else 'Release'
        if project_id == 'jsd.engine':
            opts.setdefault('JSD_BUILD_TESTS', 'OFF')
        if test:
            opts['JSD_BUILD_TESTS'] = 'ON'
        opts.setdefault('JSD_HEADLESS_ONLY', env.get('JSD_HEADLESS_ONLY', 'OFF'))
        command = [cmake, '-S', str(root), '-B', str(build), '-DCMAKE_BUILD_TYPE=' + build_type, '-DCMAKE_INSTALL_PREFIX=' + str(prefix), '-DCMAKE_INSTALL_LIBDIR=lib']
        command += ['-D' + key + '=' + value for key, value in opts.items()]
        jobs = env.get('JSD_BUILD_JOBS', '4')
        require(jobs.isdigit() and int(jobs) > 0, 'JSD_BUILD_JOBS must be a positive integer')
        name = 'JSD Game Engine' if project_id == 'jsd.engine' else project_id
        # The query and configure log make a fresh directory nonempty before CMake starts.
        no_symlinks(build / 'JSD_BUILD.json')
        record_build(root, build, meta)
        with BuildReport(cmake, build, build_type, f"{name} v{meta['version']}", env) as report:
            report.run('Configure', command)
            report.run('Build', [cmake, '--build', str(build), '--config', build_type, '--parallel', jobs, '--verbose'])
            if project_id == 'jsd.engine':
                report.run('Install', [cmake, '--install', str(build), '--prefix', str(prefix),
                                       '--config', build_type, '--verbose'])
            if test:
                ctest = Path(shutil.which(cmake)).with_name('ctest')
                report.run('Test', [str(ctest), '--test-dir', str(build), '-C', build_type, '--output-on-failure'])
            if project_id == 'jsd.editor' and 'JSDGameEngine_DIR' in opts:
                compatible(sdk(opts['JSDGameEngine_DIR']), meta)
        print(f"Built {project_id} {meta['version']} in {build}")

if __name__ == '__main__':
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--project', required=True)
    parser.add_argument('--expected', required=True)
    parser.add_argument('--test', action='store_true')
    ns, rest = parser.parse_known_args()
    if rest[:1] == ['--']:
        rest = rest[1:]
    try:
        run(ns.root, ns.project, ns.expected, rest, ns.test)
    except (GuardError, OSError, KeyError, ValueError, subprocess.CalledProcessError) as error:
        print(f'JSD build stopped: {error}', file=sys.stderr)
        sys.exit(2)
