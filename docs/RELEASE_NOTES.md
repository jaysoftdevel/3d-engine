# JSD Game Engine 0.4.0

The earlier demo lacked a detailed environment, reusable soft-particle effects and an autonomous scene. This release adds those as engine APIs plus a separate SUNDOG sample consumer.

- VfxEmitter factories for dust, steam, sparks, sand and gravel; fixed-step deterministic simulation, rates/bursts, wind/drag/turbulence, lifetime evolution and surface bounce/friction.
- GPU-instanced soft HDR billboards, stretched sparks, copied-depth intersection fading, temporal reactive masks and lit/shadowed mesh gravel.
- Hermite cinematic camera tracks; full-resolution textured UI.
- Ray-marched procedural clouds, wind displacement, triplanar maps, macro variation, clearcoat and geometry helpers.
- Sharp source-resolution HDR sky, source-mip-aware lighting prefilter, consistent environment rotation and optional separate lighting highlight limit.
- Correct tangent handedness, safe tangent basis fallback, clamped Fresnel cosines, unit-normal validation and finite framebuffer diagnostics. Zero rock normals and scan seam cracks were corrected.
- More selective glTF texture upload and constructor cleanup on partial GPU resource failure.
- SUNDOG's 64-second loop: rover approach, cargo handling/Jolt drop, drone repair, hopper particles, steam vents and relay activation. CC0 scanned rocks/sand/sky have source credits and hashes. Raw retrieval/cooking is optional; all runtime assets are bundled.

Source compatibility with Frontier 0.2 and JSD 0.3 is retained; rebuild C++ consumers because appended structures change ABI. Existing ParticleEmitter and old graphics/settings/CMake APIs remain. Capability flags continue to report unimplemented backends accurately.

Validation: 58 CPU, 87 native GL, 86 forced GL3.3 checks, 23 pixel cases, sanitizers, original-game adapter, shared/static/source SDK consumers and relocated-archive build/run. No discrete-GPU or production-quality certification is claimed. Current visual quality remains below Steel Nomad; Vulkan/RT, vendor ML upscalers, production character tooling and other documented future systems are not implemented.
