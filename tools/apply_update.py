#!/usr/bin/env python3
"""Strict manifest v2 updater; explicit project targets, complete preflight, rollback."""
import argparse
import contextlib
import json
import os
from pathlib import Path
import re
import shutil
import sys
import tempfile
import uuid
from project_guard import (GuardError, PROJECTS, cmake_identity, compatible, digest,
    operation_lock, read_json, require, safe_file, source, version)


def manifest(package):
    data = read_json(package / 'manifest.json')
    require(data.get('format') == 'jsd-update' and data.get('schema') == 2, 'Unsupported update manifest')
    require(re.fullmatch(r'[A-Za-z0-9._-]+', data.get('package_id', '')), 'Invalid package identifier')
    seen = set()
    for component in data['components']:
        pid = component['project_id']
        require(pid in PROJECTS and pid not in seen, 'Duplicate or unknown component')
        seen.add(pid)
        version(component['target_version'])
        require(component.get('from_versions'), 'Missing permitted starting versions')
        for ver in component['from_versions']:
            require(version(ver) < version(component['target_version']), 'Update must advance the component version')
        files = component['files']
        require(files and 'JSD_PROJECT.json' in {f['path'] for f in files}, 'Update must include a project version identity')
        paths = set()
        for entry in files:
            rel = entry['path']
            require(rel not in paths and rel not in ('manifest.json', '.jsd-operation.lock', '.jsd-update-journal.json'), f'Duplicate/reserved path: {rel}')
            paths.add(rel)
            require(re.fullmatch('[a-f0-9]{64}', entry['sha256']), 'Invalid payload hash')
            require('before_sha256' in entry and (entry['before_sha256'] is None or re.fullmatch('[a-f0-9]{64}', entry['before_sha256'])), 'Missing/invalid base fingerprint')
            require(entry['mode'] in (0o644, 0o755), 'Invalid payload file mode')
            payload = safe_file(package, 'files/' + pid + '/' + rel)
            require(payload.is_file() and digest(payload) == entry['sha256'], f'Corrupt payload: {pid}/{rel}')
        identity = read_json(safe_file(package, 'files/' + pid + '/JSD_PROJECT.json'))
        require(identity.get('project_id') == pid and identity.get('version') == component['target_version'] and identity.get('cmake_project') == PROJECTS[pid], 'Payload project identity does not match manifest')
        source(package / 'files' / pid, pid, component['target_version'])
        component['_identity'] = identity
    return data


def preflight(package, data, targets):
    plans = []
    for pid, root in targets.items():
        require(root.is_dir(), f'Target is not a project directory: {root}')
        from project_guard import no_symlinks
        no_symlinks(root / '.jsd-update-backups')
        no_symlinks(root / '.jsd-operation.lock')
        require(not (root / '.jsd-update-journal.json').exists(), f'Incomplete earlier update: {root}; restore its backup first')
        component = next((c for c in data['components'] if c['project_id'] == pid), None)
        require(component is not None, f'Package has no {pid} update')
        name, current = cmake_identity(root)
        require(name == PROJECTS[pid], f'Wrong project at {root}; expected {pid}')
        require(current in component['from_versions'] or current == component['target_version'], f"Unsupported {pid} starting version {current}; expected {component['from_versions']}")
        marker = root / 'JSD_PROJECT.json'
        if marker.exists():
            source(root, pid, current)
        else:
            require(current in component.get('legacy_versions', []), f'Project identity is missing at {root}')
            require(component.get('base_fingerprints'), 'Legacy bootstrap has no identity fingerprints')
        applied = current == component['target_version']
        if not applied:
            for rel, sha in component['base_fingerprints'].items():
                path = safe_file(root, rel)
                require(path.is_file() and digest(path) == sha, f'Wrong base revision or local change: {path}')
        changed = []
        for entry in component['files']:
            path = safe_file(root, entry['path'])
            require(not path.exists() or path.is_file(), f'Target is not a regular file: {path}')
            expected = entry['sha256'] if applied else entry['before_sha256']
            actual = digest(path) if path.exists() else None
            require(actual == expected, f'Partial update, unexpected revision or local change: {path}')
            if applied and os.name != 'nt':
                require(path.stat().st_mode & 0o777 == entry['mode'], f'Unexpected installed mode: {path}')
            else:
                changed.append((entry, path))
        plans.append((component, root, changed))
    roots = list(targets.values())
    require(len(set(roots)) == len(roots), 'Engine and editor cannot share a target directory')
    require(all(a not in b.parents for a in roots for b in roots if a != b), 'Project update targets must be separate directories')
    # When both are explicitly selected, verify the resulting versions together.
    if set(targets) == set(PROJECTS):
        identities = {c['project_id']: c['_identity'] for c, _, _ in plans}
        compatible(identities['jsd.engine'], identities['jsd.editor'])
    return plans


def apply(package, targets):
    package = Path(package).resolve()
    # A target must be explicit and cannot contain the updater payload itself.
    targets = {pid: Path(os.path.abspath(path)) for pid, path in targets.items()}
    for root in targets.values():
        from project_guard import no_symlinks
        no_symlinks(root)
        require(root != package and root not in package.parents and package not in root.parents, 'Extract the update outside the project target')
    data = manifest(package)
    preflight(package, data, targets)  # No target writes before all selected projects pass.
    with contextlib.ExitStack() as stack:
        for root in sorted(targets.values()):
            stack.enter_context(operation_lock(root))
        plans = preflight(package, data, targets)
        active = [(c, r, changes) for c, r, changes in plans if changes]
        if not active:
            print('Already applied; all payload files and versions verified')
            return
        token = uuid.uuid4().hex
        backups = {}
        staged = []
        committed = []
        created_dirs = []
        try:
            # Stage ALL files and backups on each target filesystem before committing.
            for component, root, changes in active:
                backup = root / '.jsd-update-backups' / (data['package_id'] + '-' + token)
                backup.mkdir(parents=True)
                backups[root] = backup
                for entry, dest in changes:
                    if dest.exists():
                        old = backup / entry['path'];old.parent.mkdir(parents=True, exist_ok=True)
                        shutil.copy2(dest, old)
                    missing = []
                    parent = dest.parent
                    while not parent.exists():
                        missing.append(parent);parent = parent.parent
                    for parent in reversed(missing):
                        parent.mkdir();created_dirs.append(parent)
                    fd, temp_name = tempfile.mkstemp(prefix='.jsd-stage-', dir=dest.parent)
                    os.close(fd)
                    temp = Path(temp_name)
                    staged.append((entry, root, dest, temp))
                    shutil.copyfile(package / 'files' / component['project_id'] / entry['path'], temp)
                    temp.chmod(entry['mode'])
                    require(digest(temp) == entry['sha256'], f'Staged payload hash mismatch: {dest}')
                journal = {'package_id': data['package_id'], 'backup': str(backup), 'files': [entry for entry, _ in changes]}
                (root / '.jsd-update-journal.json').write_text(json.dumps(journal, indent=2) + '\n')
            # Publish identity markers last, after all other files in all components.
            staged.sort(key=lambda item: item[0]['path'] == 'JSD_PROJECT.json')
            for entry, root, dest, temp in staged:
                os.replace(temp, dest)
                committed.append((entry, root, dest))
            for _, root, _ in active:
                (root / '.jsd-update-journal.json').unlink()
        except BaseException:
            failed = []
            for entry, root, dest in reversed(committed):
                try:
                    old = backups[root] / entry['path']
                    if old.exists():
                        shutil.copy2(old, dest)
                    else:
                        dest.unlink(missing_ok=True)
                except OSError:
                    failed.append(str(dest))
            for _, _, _, temp in staged:
                temp.unlink(missing_ok=True)
            if not failed:
                for _, root, _ in active:
                    (root / '.jsd-update-journal.json').unlink(missing_ok=True)
                for parent in reversed(created_dirs):
                    if parent.exists() and not any(parent.iterdir()):
                        parent.rmdir()
            else:
                print('Rollback incomplete; restore backups listed in update journals: ' + ', '.join(failed), file=sys.stderr)
            raise
        for component, root, _ in active:
            print(f"Updated {component['project_id']} to {component['target_version']}: {root}\nBackup: {backups[root]}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--package', type=Path, default=Path(__file__).resolve().parent)
    parser.add_argument('--engine', type=Path)
    parser.add_argument('--editor', type=Path)
    parser.add_argument('--check', action='store_true', help='Validate only; write nothing')
    args = parser.parse_args()
    targets = {pid: path for pid, path in [('jsd.engine', args.engine), ('jsd.editor', args.editor)] if path is not None}
    require(targets, 'Select --engine /path/to/engine and/or --editor /path/to/editor; no directories are guessed')
    if args.check:
        package = args.package.resolve()
        from project_guard import no_symlinks
        for root in targets.values():
            no_symlinks(root)
        preflight(package, manifest(package), {pid: path.resolve() for pid, path in targets.items()})
        print('Preflight passed; no files changed')
    else:
        apply(args.package, targets)

if __name__ == '__main__':
    try:
        main()
    except (GuardError, OSError, KeyError, ValueError) as error:
        print(f'Update stopped: {error}', file=sys.stderr)
        sys.exit(2)
