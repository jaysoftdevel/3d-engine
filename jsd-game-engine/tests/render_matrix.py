#!/usr/bin/env python3
"""Validate that feature switches alter real rendered pixels; Python stdlib only."""
import argparse
import concurrent.futures
import json
import os
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--demo', type=Path, default=Path(__file__).resolve().parents[1] / 'bin/jsd-graphics-lab')
parser.add_argument('--output', type=Path, default=Path('/tmp/jsd-render-matrix'))
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=True)
cases = {
    'off': [],
    'taa': ['--aa', 'taa'],
    'upscale': ['--render-scale', '.67'],
    'bloom': ['--bloom', 'high'],
    'exposure': ['--auto-exposure', '1'],
    'tone-map': ['--tone-mapper', 'reinhard'],
    'grade': ['--contrast', '1.3', '--saturation', '.4', '--temperature', '.4'],
    'vignette': ['--vignette', '.7'],
    'volumetric-fog': ['--volumetric-fog', 'high'],
    'fxaa': ['--aa', 'fxaa'],
    'smaa': ['--aa', 'smaa'],
    'msaa': ['--aa', 'msaa', '--msaa', '4'],
    'shadow': ['--shadows', 'high'],
    'ao': ['--ao', 'high'],
    'dof': ['--dof', 'high', '--focus', '8'],
    'probe': ['--gi', 'ambient-probe'],
    'ssgi': ['--gi', 'screen-space', '--gi-quality', 'high'],
    'ssr': ['--ssr', 'high'],
    'edge-fusion': ['--edge-fusion', 'high'],
    'view-distance': ['--view-distance', '160'],
    'terrain-mesh-high': ['--tessellation', 'high', '--terrain-backend', 'mesh'],
    'texture-low': ['--textures', 'low'],
    'anisotropy-off': ['--anisotropy', '1'],
}

def render(case):
    name, extra = case
    path = args.output / (name + '.ppm')
    command = [str(args.demo.resolve()), '--offscreen', '--preset', 'legacy', '--view-distance', '80',
               '--width', '640', '--height', '360', '--frames', '1', '--no-ui', '--capture', str(path), *extra]
    environment = dict(os.environ, LP_NUM_THREADS='2')
    run = subprocess.run(command, capture_output=True, text=True, env=environment)
    if run.returncode:
        raise RuntimeError(name + ': ' + run.stdout + run.stderr)
    with path.open('rb') as stream:
        if stream.readline().strip() != b'P6':
            raise RuntimeError('Expected PPM capture')
        width, height = map(int, stream.readline().split())
        if stream.readline().strip() != b'255':
            raise RuntimeError('Expected 8-bit capture')
        pixels = stream.read()
    if len(pixels) != width * height * 3:
        raise RuntimeError('Incomplete framebuffer capture')
    return name, pixels, run.stdout.strip().splitlines()[-1]

with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
    rendered = {name: (pixels, stats) for name, pixels, stats in pool.map(render, cases.items())}

report = {}
for name, (pixels, stats) in rendered.items():
    reference = 'probe' if name == 'ssgi' else 'off'
    baseline = rendered[reference][0]
    differences = [abs(a-b) for a, b in zip(pixels, baseline)]
    changed = sum(any(differences[p:p+3]) for p in range(0, len(differences), 3))
    mean = sum(differences) / len(differences)
    report[name] = {'reference': reference, 'changed_pixels': changed,
                    'mean_absolute_channel_difference': round(mean, 6), 'statistics': stats}
    print(f'{name}: {changed} changed pixels, mean difference {mean:.6f}')
    if name not in ('off', 'texture-low', 'anisotropy-off') and changed == 0:
        raise RuntimeError(f'{name} produced no visible change')

(args.output / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
print('Render matrix passed. Texture allocation is checked separately by the GPU regression suite.')
