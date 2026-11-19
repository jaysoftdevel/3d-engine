# SMAA reference sources

Upstream: https://github.com/iryoku/smaa

Retrieved 19 November 2026. File blob IDs:

- `SMAA.hlsl`: `2c8d4979edb78d7ca9a8c251e01ae6518f48789f`
- `Textures/SearchTex.h`: `9a65bba90d378d3d4bc43a746d142935c8f28fb0`
- `LICENSE.txt`: `dbce0e6bc8574fd9c8e3eb0500b04243bc1a42d6`
- `Textures/AreaTexDX10.dds`: `7a3b1f8d1c9b2b59fa71211faa3622d3b9dcc314`

Original shader, search data, DDS area data and license are retained. `tools/embed_smaa.py` derives embedded C++ shader text and RG area bytes. It accounts for the source DDS BGR channel order. The engine uses the reference GLSL 3 SMAA 1x HIGH preset with color edge detection, weight search and neighborhood blending.
