# JSD Game Engine 0.4.2 — source-only build automation

- Removed every distributed executable, shared/static/object library, generated SDK/export/cache, precompiled Gradle helper and unused non-Linux SDL shader blobs.
- Bundled SDL2 2.32.4 source; SDL and Jolt build directly in CMake. No system/binary SDL lookup or code download fallback.
- Engine PIC objects compile once and feed both shared/static engine variants.
- Root/sample launchers always build incrementally before launch; independent engine lab and test launchers build as well.
- Automatic OS/toolchain prerequisite setup, source-built SDL CMake dependency exports and relocatable pkg-config metadata.
- Fresh direct sample CMake builds auto-select source dependency mode before an SDK exists.
- Fullscreen/FPS/performance features from 0.4.1 retained. Render/runtime APIs remain source compatible.

# JSD Game Engine 0.4.1

- Measured FPS at lower left; F1 hides the mission HUD while preserving FPS, --no-fps hides only FPS, --no-ui hides both.
- Borderless desktop fullscreen by default; F11 and --windowed/--fullscreen select modes. Offscreen capture dimensions are unchanged.
- Uncapped sample presentation by default: no 8 ms sleep, VSync disabled on request; --vsync restores synchronization. Optional hybrid-GPU launcher selection and CSV profiling.
- Static vertex indexing, per-instance normal matrices, static wind bypass, depth-only shadow vertices, cached uniforms, shared frame preparation and flat light bins.
- Streaming mesh updates reuse VAO/VBO instead of creating GPU objects or running static deduplication each frame.
- Full-size copy path, combined Gaussian taps, zero-contribution GI early exits and unused back-face PCF rejection.
- Actual software-driver throughput comparison, mode/resize checks, bounded 200-light test, dynamic mesh pixel restoration, GL3.3 fallback and sanitizer coverage. No scene-quality or resolution reduction.

Public Frontier/JSD APIs and settings are retained. Existing Platform construction remains windowed/VSync; the sample explicitly requests the new defaults. Driver/compositor policy can override presentation requests, and 100% GPU utilization is not guaranteed. See PERFORMANCE.md for reproducible measurements and limits.

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
