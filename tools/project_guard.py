#!/usr/bin/env python3
"""Project/version, output ownership and installed SDK checks (Python standard library)."""
import argparse
import contextlib
import hashlib
import json
import os
if os.name == "nt":
    import msvcrt
else:
    import fcntl
from pathlib import Path
import re
import sys

PROJECTS = {'jsd.engine': 'JSDGameEngine', 'jsd.editor': 'JSDStudio'}
class GuardError(RuntimeError):
    pass

def require(condition, message):
    if not condition:
        raise GuardError(message)

def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def read_json(path):
    try:
        return json.loads(Path(path).read_text())
    except (OSError, ValueError) as error:
        raise GuardError(f'Invalid or missing metadata {path}: {error}') from error

def version(value):
    require(isinstance(value, str) and re.fullmatch(r'\d+\.\d+\.\d+', value), f'Invalid version: {value}')
    return tuple(map(int, value.split('.')))

def cmake_identity(root):
    text = (Path(root) / 'CMakeLists.txt').read_text()
    match = re.search(r'project\(\s*(\w+)\s+VERSION\s+(\d+\.\d+\.\d+)\b', text)
    require(match, f'Missing CMake project/version in {root}')
    return match.groups()

def source(root, project_id, expected=None):
    root = Path(root).resolve()
    meta = read_json(root / 'JSD_PROJECT.json')
    require(meta.get('format') == 'jsd-project' and meta.get('schema') == 1, f'Unsupported project identity: {root}')
    require(meta.get('project_id') == project_id and meta.get('kind') == 'source', f'Wrong project at {root}; expected {project_id}')
    require(meta.get('cmake_project') == PROJECTS[project_id], f'Wrong CMake project identity at {root}')
    name, ver = cmake_identity(root)
    version(ver)
    require(name == PROJECTS[project_id] and ver == meta.get('version'), f'Mixed project versions at {root}')
    if expected:
        require(ver == expected, f'Expected {project_id} {expected}, found {ver}')
    if project_id == 'jsd.engine':
        for rel in ('include/jsd/engine.hpp', 'include/frontier/renderer.hpp'):
            match = re.search(r'\bVersion\s*=\s*"([^"]+)"', (root / rel).read_text())
            require(match and match[1] == ver, f'Mixed public version: {rel}')
        runtime = (root / 'src/runtime/runtime.cpp').read_text()
        require(re.search(r'jsd_game_engine_version\s*\([^)]*\)\s*\{\s*return\s*"' + re.escape(ver) + r'"', runtime), 'Mixed runtime engine version')
    require(meta.get('api_version') == 1, f'Unsupported project API at {root}')
    require(not (root / '.jsd-update-journal.json').exists(), f'Incomplete update at {root}; restore the backup named in .jsd-update-journal.json first')
    return meta

def compatible(engine, editor):
    req = editor['requires']['engine']
    require(engine.get('project_id') == 'jsd.engine', 'Editor dependency is not a JSD engine')
    require(engine.get('api_version') == req['api_version'], 'Incompatible engine API')
    require(version(req['min_version']) <= version(engine['version']) < version(req['max_version_exclusive']),
            f"Editor requires engine >= {req['min_version']} and < {req['max_version_exclusive']}; found {engine['version']}")

def cache_values(build):
    path = Path(build) / 'CMakeCache.txt'
    result = {}
    if path.is_file():
        for line in path.read_text(errors='replace').splitlines():
            match = re.match(r'([^/#][^:=]*):[^=]*=(.*)', line)
            if match:
                result[match[1]] = match[2]
    return result

def no_symlinks(path):
    path = Path(os.path.abspath(path))
    for part in (path, *path.parents):
        require(not part.is_symlink() and not (hasattr(part, 'is_junction') and part.is_junction()), f'Symlink output/target is not allowed: {part}')
    return path

def output_path(root, path):
    root, path = Path(root).resolve(), no_symlinks(path).resolve()
    require(path != root and path not in root.parents, f'Output would overwrite source/ancestor: {path}')
    # Only conventional output trees may live within a source project.
    if root in path.parents:
        first = path.relative_to(root).parts[0]
        require(first in ('build', 'prebuild', 'prebuilds', 'prebuilt', 'prebuilts') or first.startswith('build-'),
                f'Output overlaps source files: {path}')
    for parent in (path, *path.parents):
        if parent == root:
            break
        require(not (parent / 'JSD_PROJECT.json').exists(), f'Output belongs to another source project: {parent}')
    require(not (path / 'CMakeLists.txt').exists(), f'Output contains source files: {path}')
    return path

def build_path(root, build, meta, allow_container=False, configuring=False):
    build = output_path(root, build)
    cache = cache_values(build)
    if cache:
        require(Path(cache.get('CMAKE_HOME_DIRECTORY', '')).resolve() == Path(root).resolve(), f'Build cache belongs to another source tree: {build}')
        require(cache.get('CMAKE_PROJECT_NAME') == meta['cmake_project'], f'Build cache belongs to another project: {build}')
        old = cache.get('CMAKE_PROJECT_VERSION', cache.get(meta['cmake_project'] + '_VERSION'))
        if old:
            require(version(old) <= version(meta['version']), f'Build cache is newer than source: {old}')
    owner = build / 'JSD_BUILD.json'
    if owner.exists():
        data = read_json(owner)
        require(data.get('project_id') == meta['project_id'] and data.get('source') == str(Path(root).resolve()), f'Wrong build owner: {build}')
        require(version(data['version']) <= version(meta['version']), f'Build owner is newer than source: {build}')
    elif build.exists() and any(build.iterdir()) and not cache:
        internal = Path(root).resolve() in build.parents
        require((allow_container and internal) or (configuring and {p.name for p in build.iterdir()} <= {'CMakeFiles'}), f'Nonempty output has no matching build owner: {build}')
    if allow_container and build.exists():
        for marker in build.rglob('JSD_PROJECT.json'):
            raise GuardError(f'Clean target contains a source project: {marker.parent}')
        for cached in build.rglob('CMakeCache.txt'):
            data = cache_values(cached.parent)
            require(Path(data.get('CMAKE_HOME_DIRECTORY', '')).resolve() == Path(root).resolve(), f'Clean target contains a foreign build: {cached.parent}')
    return build

def install_prefix(root, prefix, meta):
    root, prefix = Path(root).resolve(), no_symlinks(prefix).resolve()
    local = prefix == root
    receipt = prefix / 'share' / ('jsd-game-engine' if meta['project_id'] == 'jsd.engine' else 'jsd-editor') / 'INSTALL.json'
    if receipt.exists():
        installed = read_json(receipt)
        require(installed.get('project_id') == meta['project_id'], f'Wrong install prefix identity: {prefix}')
        require(version(installed['version']) <= version(meta['version']), f'Cannot overwrite newer installed version: {prefix}')
    if local:
        for name in ('bin', 'lib', 'lib64', 'share', 'include'):
            directory = prefix / name
            no_symlinks(directory)
            if directory.exists():
                for path in directory.rglob('*'):
                    if path.is_symlink():
                        require(path.is_file() and prefix in path.resolve().parents, f'Install path escapes prefix: {path}')
        return prefix
    require(prefix not in root.parents and root not in prefix.parents, f'Install prefix overlaps source: {prefix}')
    for parent in (prefix, *prefix.parents):
        require(not (parent / 'JSD_PROJECT.json').exists(), f'Install prefix belongs to a source project: {parent}')
    require(not (prefix / 'CMakeCache.txt').exists() and not (prefix / 'CMakeLists.txt').exists(), f'Install prefix is a source/build tree: {prefix}')
    receipt = prefix / 'share' / ('jsd-game-engine' if meta['project_id'] == 'jsd.engine' else 'jsd-editor') / 'INSTALL.json'
    if prefix.exists() and any(prefix.iterdir()):
        data = read_json(receipt)
        require(data.get('format') == 'jsd-install' and data.get('project_id') == meta['project_id'], f'Wrong install prefix owner: {prefix}')
        require(version(data['version']) <= version(meta['version']), f'Cannot overwrite newer installed version: {prefix}')
    # Reject nested output symlinks before CMake installs through them.
    for name in ('bin', 'lib', 'lib64', 'share', 'include'):
        directory = prefix / name
        no_symlinks(directory)
        if directory.exists():
            for p in directory.rglob('*'):
                if p.is_symlink():
                    require(p.is_file() and prefix in p.resolve().parents, f'Install path escapes prefix: {p}')
    return prefix

def sdk(config_dir):
    config_dir = Path(config_dir).resolve()
    require(config_dir.name == 'JSDGameEngine' and config_dir.parent.name == 'cmake', f'Expected JSDGameEngine package directory: {config_dir}')
    prefix = config_dir.parents[2]
    meta = read_json(prefix / 'share/jsd-game-engine/INSTALL.json')
    require(meta.get('format') == 'jsd-install' and meta.get('project_id') == 'jsd.engine' and meta.get('kind') == 'sdk', f'Wrong SDK identity: {prefix}')
    require(meta.get('files'), 'SDK receipt has no fingerprints')
    essential = {'include/jsd/engine.hpp', 'include/frontier/renderer.hpp', 'lib/cmake/JSDGameEngine/JSDGameEngineConfig.cmake', 'lib/cmake/JSDGameEngine/JSDGameEngineConfigVersion.cmake', 'lib/libjsd-game-engine.so.' + meta['version']}
    windows_sdk = 'bin/jsd-game-engine.dll' in meta['files']
    if windows_sdk:
        essential.discard('lib/libjsd-game-engine.so.' + meta['version'])
        essential.update({'bin/jsd-game-engine.dll', 'bin/SDL2.dll', 'lib/jsd-game-engine.lib'})
    require(essential <= set(meta['files']), 'Incomplete SDK receipt: missing required engine files')
    for rel, sha in meta['files'].items():
        path = safe_file(prefix, rel)
        require(path.is_file() and digest(path) == sha, f'Incomplete or mixed SDK: {path}')
    links = meta.get('symlinks', {})
    require(windows_sdk or {'lib/libjsd-game-engine.so', 'lib/libjsd-game-engine.so.0'} <= set(links), 'Missing SDK runtime library link fingerprints')
    for rel, target in links.items():
        path = prefix / rel
        safe_file(prefix, str(Path(rel).parent / '.guard-path-check'))
        require(path.is_symlink() and os.readlink(path) == target and prefix in path.resolve().parents and path.is_file(), f'Mixed SDK library link: {path}')
    return meta

def safe_file(root, rel):
    require(isinstance(rel, str) and rel and '\\' not in rel and not Path(rel).is_absolute() and all(p not in ('', '.', '..') for p in rel.split('/')), f'Invalid relative file path: {rel}')
    path = no_symlinks(Path(root) / rel)
    require(Path(root).resolve() in path.resolve().parents, f'File escapes target: {rel}')
    return path

@contextlib.contextmanager
def operation_lock(root):
    # A persistent lock file avoids unlink/recreate races between concurrent operations.
    path = Path(root) / '.jsd-operation.lock'
    no_symlinks(path)
    with path.open('a+b') as handle:
        try:
            if os.name == 'nt':
                handle.seek(0, 2)
                if handle.tell() == 0:
                    handle.write(b'\0'); handle.flush()
                handle.seek(0)
                msvcrt.locking(handle.fileno(), msvcrt.LK_NBLCK, 1)
            else:
                fcntl.flock(handle, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except OSError as error:
            raise GuardError(f'Another build/update is using {root}') from error
        try:
            yield
        finally:
            if os.name == 'nt':
                handle.seek(0)
                msvcrt.locking(handle.fileno(), msvcrt.LK_UNLCK, 1)
            else:
                fcntl.flock(handle, fcntl.LOCK_UN)

def record_build(root, build, meta):
    Path(build).mkdir(parents=True, exist_ok=True)
    data = {'project_id': meta['project_id'], 'source': str(Path(root).resolve()), 'version': meta['version']}
    path = Path(build) / 'JSD_BUILD.json'
    path.write_text(json.dumps(data, indent=2) + '\n')

def receipt(root, prefix, meta):
    prefix = Path(prefix)
    files = {}
    symlinks = {}
    patterns = ('lib/libjsd-game-engine*', 'lib/libSDL2*', 'lib/libJolt*', 'lib/cmake/JSDGameEngine/*', 'lib/cmake/SDL2/*', 'include/jsd/**/*', 'include/frontier/**/*', 'include/frontier-compat/**/*', 'include/Jolt/**/*', 'include/SDL2/**/*') if meta['project_id'] == 'jsd.engine' else ('bin/jsd-editor', 'lib/libjsd-game-engine*', 'lib/libSDL2*')
    if meta['project_id'] == 'jsd.engine':
        patterns += ('bin/jsd-*.dll', 'bin/SDL2*.dll', 'lib/jsd-*.lib', 'lib/SDL2*.lib', 'lib/Jolt*.lib')
    for pattern in patterns:
        for path in prefix.glob(pattern):
            if path.is_symlink():
                require(path.is_file() and prefix.resolve() in path.resolve().parents, f'Installed link escapes prefix: {path}')
                symlinks[path.relative_to(prefix).as_posix()] = os.readlink(path)
            elif path.is_file():
                files[path.relative_to(prefix).as_posix()] = digest(path)
    require(files, 'No installed files to record')
    data = dict(meta, format='jsd-install', kind='sdk' if meta['project_id'] == 'jsd.engine' else 'runtime', files=files, symlinks=symlinks)
    out = prefix / 'share' / ('jsd-game-engine' if meta['project_id'] == 'jsd.engine' else 'jsd-editor') / 'INSTALL.json'
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(data, indent=2, sort_keys=True) + '\n')

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=('source', 'cmake', 'install', 'receipt', 'dependency', 'sdk'))
    parser.add_argument('--root', required=True, type=Path)
    parser.add_argument('--project', required=True, choices=PROJECTS)
    parser.add_argument('--expected', required=True)
    parser.add_argument('--build', type=Path)
    parser.add_argument('--prefix', type=Path)
    parser.add_argument('--engine', type=Path)
    args = parser.parse_args()
    meta = source(args.root, args.project, args.expected)
    if args.action in ('cmake', 'install'):
        build_path(args.root, args.build, meta, configuring=args.action == 'cmake')
        install_prefix(args.root, args.prefix, meta)
        if args.action == 'cmake':
            record_build(args.root, args.build, meta)
    elif args.action == 'receipt':
        receipt(args.root, args.prefix, meta)
    elif args.action in ('dependency', 'sdk'):
        require(args.engine.resolve() != args.root.resolve() and args.engine.resolve() not in args.root.resolve().parents and args.root.resolve() not in args.engine.resolve().parents, 'Engine and editor must be separate projects')
        engine = sdk(args.engine) if args.action == 'sdk' else source(args.engine, 'jsd.engine')
        compatible(engine, meta)

if __name__ == '__main__':
    try:
        main()
    except (GuardError, OSError, KeyError, ValueError) as error:
        print(f'JSD guard: {error}', file=sys.stderr)
        sys.exit(2)
