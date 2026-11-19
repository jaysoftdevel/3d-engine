#!/usr/bin/env python3
"""Reject compiled code/caches in a source-only release (assets and scripts are allowed)."""
import argparse
from pathlib import Path
import tarfile

SUFFIXES = {'.a', '.o', '.obj', '.so', '.dll', '.dylib', '.lib', '.exe', '.pdb', '.jar', '.class', '.wasm', '.spv', '.cso', '.dxil', '.metallib', '.bc', '.gch', '.pch', '.pyc', '.pyo', '.node', '.dex'}
MAGICS = (b'\x7fELF', b'!<arch>\n', b'!<thin>\n', b'MZ', b'\x00asm', b'\xca\xfe\xba\xbe',
          b'\xfe\xed\xfa\xce', b'\xce\xfa\xed\xfe', b'\xfe\xed\xfa\xcf', b'\xcf\xfa\xed\xfe')

def check(name, header):
    path = Path(name)
    if path.suffix.lower() in SUFFIXES or '.so.' in path.name:
        return 'compiled-file suffix'
    if path.name == 'CMakeCache.txt' or 'CMakeFiles' in path.parts:
        return 'generated build cache'
    if any(header.startswith(magic) for magic in MAGICS):
        return 'compiled-code signature'
    return None

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('path', type=Path)
    args = parser.parse_args()
    failures = []
    count = 0
    if args.path.is_dir():
        for path in args.path.rglob('*'):
            if path.is_file() and not path.is_symlink():
                with path.open('rb') as stream:
                    reason = check(str(path.relative_to(args.path)), stream.read(16))
                count += 1
                if reason: failures.append((str(path), reason))
    else:
        with tarfile.open(args.path) as archive:
            for member in archive:
                if not member.isfile(): continue
                with archive.extractfile(member) as stream:
                    reason = check(member.name, stream.read(16))
                count += 1
                if reason: failures.append((member.name, reason))
    if failures:
        for name, reason in failures: print(f'REJECT {name}: {reason}')
        raise SystemExit(1)
    print(f'PASS: {count} source/asset/document files; no compiled code, libraries or build caches.')

if __name__ == '__main__': main()
