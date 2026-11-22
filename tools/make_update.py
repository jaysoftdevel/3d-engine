#!/usr/bin/env python3
"""Create a guarded update directory from explicit baseline/target source trees."""
import argparse
import json
from pathlib import Path
import shutil
import sys
from project_guard import GuardError, digest, require, source, version
from apply_update import manifest


def source_files(root):
    excluded = {'build', 'bin', 'lib', 'lib64', 'share', 'prebuild', 'prebuilds', 'prebuilt', 'prebuilts'}
    result = {}
    for path in root.rglob('*'):
        rel = path.relative_to(root)
        if rel.parts[0] in excluded or rel.parts[0].startswith('build-') or any(p == '.git' or p.startswith('.jsd-') or p == '__pycache__' for p in rel.parts) or rel.parts[:2] in (('include', 'Jolt'), ('include', 'SDL2')):
            continue
        require(not path.is_symlink(), f'Symlink in source: {path}')
        if path.is_file():
            result[rel.as_posix()] = path
    return result


def component(base, target, pid, destination):
    old, new = source(base, pid), source(target, pid)
    require(version(old['version']) < version(new['version']), 'Target must have a newer version than baseline')
    before, after = source_files(base), source_files(target)
    require(not (set(before) - set(after)), 'Source deletions require a new manifest schema; refusing incomplete update')
    files = []
    for rel, path in sorted(after.items()):
        sha = digest(path)
        previous = digest(before[rel]) if rel in before else None
        mode = 0o755 if path.suffix == ".sh" and not rel.startswith("third_party/") else path.stat().st_mode & 0o777
        previous_mode = before[rel].stat().st_mode & 0o777 if rel in before else None
        if sha == previous and mode == previous_mode:
            continue
        require(path.stat().st_mode & 0o777 in (0o644, 0o755), f'Unsupported file mode: {path}')
        output = destination / 'files' / pid / rel
        output.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, output)
        output.chmod(mode)
        files.append({'path': rel, 'before_sha256': previous, 'sha256': sha, 'mode': mode})
    anchors = ['CMakeLists.txt', 'JSD_PROJECT.json']
    if pid == 'jsd.engine':
        anchors += ['include/jsd/engine.hpp', 'include/frontier/renderer.hpp', 'src/runtime/runtime.cpp']
    return {'project_id': pid, 'from_versions': [old['version']], 'legacy_versions': [], 'target_version': new['version'], 'base_fingerprints': {rel: digest(base / rel) for rel in anchors}, 'files': files}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('engine', 'editor'):
        parser.add_argument('--' + name + '-base', type=Path)
        parser.add_argument('--' + name, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--name', required=True)
    args = parser.parse_args()
    require(not args.output.exists(), 'Output must be a new directory')
    pairs = []
    for name in ('engine', 'editor'):
        base, target = getattr(args, name + '_base'), getattr(args, name)
        require(bool(base) == bool(target), f'Both --{name}-base and --{name} are required')
        if base:
            base, target = base.resolve(), target.resolve()
            require(base != target and base not in target.parents and target not in base.parents, 'Baseline and target must be separate source trees')
            require(args.output.resolve() != target and target not in args.output.resolve().parents and base not in args.output.resolve().parents, 'Output must be outside source trees')
            source(base, 'jsd.' + name);source(target, 'jsd.' + name)
            pairs.append((base, target, 'jsd.' + name))
    require(pairs, 'Choose at least one explicit component pair')
    args.output.mkdir(parents=True)
    try:
        data = {'format': 'jsd-update', 'schema': 2, 'package_id': args.name, 'components': [component(b, t, p, args.output) for b, t, p in pairs]}
        (args.output / 'manifest.json').write_text(json.dumps(data, indent=2) + '\n')
        for name in ('apply_update.py', 'project_guard.py'):
            shutil.copy2(Path(__file__).parent / name, args.output / name)
        launcher = args.output / 'apply.sh'
        launcher.write_text('#!/usr/bin/env bash\nset -euo pipefail\npackage_root=$(cd -- "$(dirname -- "$0")" && pwd -P)\nexec python3 -B "$package_root/apply_update.py" --package "$package_root" "$@"\n')
        launcher.chmod(0o755)
        manifest(args.output)
    except BaseException:
        shutil.rmtree(args.output)
        raise
    print(f'Created and verified {args.output}')

if __name__ == '__main__':
    try:
        main()
    except (GuardError, OSError, KeyError, ValueError) as error:
        print(f'Package creation stopped: {error}', file=sys.stderr)
        sys.exit(2)
