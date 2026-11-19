#!/usr/bin/env python3
"""Rebuild embedded shader/area data from the bundled permissively licensed SMAA sources."""
from pathlib import Path
import struct
root = Path(__file__).resolve().parents[1]
source = (root / 'third_party/smaa/SMAA.hlsl').read_text()
(root / 'src/smaa_shader.hpp').write_text('#pragma once\nstatic const char* smaaReference=R"SMAA(' + source + ')SMAA";\n')
dds = (root / 'third_party/smaa/Textures/AreaTexDX10.dds').read_bytes()
assert dds[:4] == b'DDS '
height, width = struct.unpack_from('<II', dds, 12)
bits = struct.unpack_from('<I', dds, 88)[0]
assert (width, height, bits) == (160, 560, 24)
assert struct.unpack_from('<III', dds, 92) == (0xff0000, 0xff00, 0xff)
# DDS is BGR24; SMAA GLSL reads the original red/green area channels.
area = bytes(c for i in range(width * height) for c in (dds[128+i*3+2], dds[128+i*3+1]))
(root / 'src/smaa_area.hpp').write_text('#pragma once\nstatic const unsigned char smaaAreaBytes[]={\n' + ','.join(map(str, area)) + '\n};\n')
