#!/usr/bin/env python3
"""Fetch the pinned CC0 sources used by the showcase. Not needed to build/run."""
from pathlib import Path
import hashlib
import io
import json
import urllib.request

ROOT = Path(__file__).resolve().parents[1] / 'assets/showcase/scans'
MANIFEST = json.loads((ROOT / 'SOURCES.json').read_text())


def fetch(record):
    target = ROOT / record['path']
    if target.exists() and hashlib.sha256(target.read_bytes()).hexdigest() == record['sha256']:
        return target
    request = urllib.request.Request(record['url'], headers={'User-Agent': 'JSD-Asset-Cooker/0.4'})
    with urllib.request.urlopen(request, timeout=90) as response:
        data = response.read()
    if hashlib.sha256(data).hexdigest() != record['sha256']:
        raise RuntimeError('Source checksum mismatch: ' + record['url'])
    target.parent.mkdir(parents=True, exist_ok=True)
    temporary = target.with_suffix(target.suffix + '.part')
    temporary.write_bytes(data)
    temporary.replace(target)
    print(target)
    return target


for asset in MANIFEST['assets']:
    for source in asset['source_files']:
        raw = fetch(source)
        if 'cooked_path' in source:
            from PIL import Image, ImageFile
            ImageFile.MAXBLOCK = 16 * 1024 * 1024
            target = ROOT / source['cooked_path']
            target.parent.mkdir(parents=True, exist_ok=True)
            with Image.open(io.BytesIO(raw.read_bytes())) as image:
                image = image.convert('RGB').resize((1024, 1024), Image.Resampling.LANCZOS)
                temporary = target.with_suffix('.part')
                image.save(temporary, format='JPEG', quality=96, subsampling=0)
                temporary.replace(target)
print('Sources ready. Run Blender cook_scans.py, then unpack_scan_lods.py.')
