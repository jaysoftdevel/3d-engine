# JSD 0.4.1 performance and display behavior

The sample requests borderless desktop fullscreen and uncapped presentation by default. The previous VSync default and additional 8 ms sleep (a 125 FPS ceiling) have been removed from the sample loop. Existing engine consumers retain their constructor defaults. `--windowed`, `--vsync`, `--no-vsync`, F11 and `--no-fps` provide explicit controls.

The FPS counter averages actual wall-clock frame periods over approximately half a second. Presentation and normal loop overhead are included. The offline `--fps` value advances simulation and controls capture cadence; it is never presented as measured renderer performance. `--no-ui` hides all overlays.

## Rendering changes

- Complete-vertex indexing preserves normals, UV and color seams while reducing duplicate GPU vertex inputs/storage. Fully unique meshes retain the unindexed path when it is smaller. Both GL3.3 triangles and GL4 terrain patches support indexing.
- Per-instance normal matrices replace per-vertex matrix inversion. Static meshes bypass wind trigonometry. Shadow vertices skip lighting normals, previous transforms and motion outputs.
- Streaming/deforming mesh updates reuse existing VAO/VBO storage and skip static deduplication; topology/bounds are refreshed.
- Once-per-frame draw preparation shares model transforms, bounds, material keys, motion lookup and ordering across passes. Frustum planes are normalized once per pass. Opaque batches and transparency retain their original order semantics.
- Cached uniform locations avoid thousands of repeated driver lookups; program replacement retires the cache. Reused instance/light scratch vectors reduce transient allocations. Flat light bins retain the same 128-light tile budget and overflow reporting.
- Unchanged-size output avoids 16-tap cubic reconstruction. Gaussian bloom uses paired bilinear taps. GI rejects distance/hemisphere samples with zero contribution before further fetches. Back-facing direct light avoids unused PCF shadow sampling.
- Optional finite-frame/GL diagnostics are outside the normal release path. Asynchronous GPU timers never wait for an unavailable query; the latest completed value is retained.

These changes retain the scene's geometry, quality settings and enabled effects. They do not introduce automatic quality/resolution reduction or artificial GPU load.

## Frame-time pass (unreleased)

Measured on the SUNDOG showcase at 1920x1080, TAA, ultra shadows and volumetric fog, uncapped, on a Quadro M2000M (OpenGL 4.6 NVIDIA 580). Frame time fell from 95.7/90.3/89.9 ms to 61.9/59.6/60.8 ms at scene times 16/40/55 s (+48..55% FPS). Quality settings, resolution and enabled effects are unchanged; captured frames differ from the previous build in under 0.01% of pixels (PSNR above 59 dB), at sub-pixel tie-break locations.

- **Depth prepass.** Opaque meshes and terrain first write depth only (the lean shadow vertex shader, batches merged by mesh/wind), then the full PBR shader runs with `GL_EQUAL` and no depth writes, so each pixel is shaded once instead of once per overlapping surface. Alpha-masked materials are left to the shading pass; blended materials are unaffected. `invariant gl_Position` guarantees both passes produce identical depth. The prepass is skipped for wireframe and after `reloadMaterialShaders` (custom vertex stages may move geometry). Its draws are not counted in `Statistics`.
- **Hardware PCF.** The 3x3/5x5/7x7 nearest-compare filter is evaluated with compare-enabled bilinear taps placed on texel corners, each summing four exact comparisons. The result equals the previous per-texel loop at about a third of the texture operations. Built-in shaders use the `shadowPcf` uniform on texture unit 13 through a sampler object; the legacy `shadowMap` binding on unit 4 is unchanged for custom material shaders.
- **Analytic depth reconstruction.** SSAO, its blur, screen-space GI/reflections and particle softness reconstruct view-space depth from the projection entries instead of multiplying by the inverse matrix per sample; SSAO uses a precomputed sample table and a rotation recurrence instead of per-sample trigonometry and normalization.
- **Volumetric fog** marches light-space coordinates affinely instead of a matrix multiply per step. Particle fragments test soft-particle fade before noise sampling. Opaque shadow batches merge across materials; per-object motion history uses a hash map.

Per-pass cost before/after (ms, 1080p, scene time 40 s): shadow map 4.1 -> 4.3, depth prepass 0 -> 2.6, scene meshes 32.5 -> 10.4, terrain 12.4 -> 3.5, SSAO 11.6 -> 9.7, lighting (GI + reflections) 16.5 -> 13.4, volumetric fog and post 9.7 -> 8.5. Rejected after measurement (no gain): `textureGather` PCF, a min/max shadow-tile early-out (penumbra covers most surfaces), constant-bound SSAO loops, and back-face culling (about 1.7 ms but changes behavior for open meshes and cameras inside meshes).

## GPU selection and measurement

The launcher respects the host's normal OpenGL driver and existing environment settings. `JSD_GPU=discrete ./run-showcase.sh` requests Mesa's first non-default GPU (`DRI_PRIME=1`) and, when an NVIDIA driver is installed, NVIDIA PRIME offload. GPU selection and support depend on the machine. Primary references: [Mesa DRI_PRIME](https://docs.mesa3d.org/envvars.html#dri-prime) and [NVIDIA PRIME](https://download.nvidia.com/XFree86/Linux-x86_64/550.135/README/primerenderoffload.html).

Startup prints the renderer name. A `llvmpipe` result means software rendering; it is not a hardware GPU benchmark. Uncapping presentation and lowering CPU overhead allow more throughput, but utilization depends on the scene, processor, driver, compositor and GPU. This package cannot promise 100% GPU usage or change driver power policy.

For profiling on your GPU:

```bash
cd jsd-game-engine
./run-showcase.sh --profile frame-times.csv
# Optional synchronized/windowed modes:
./run-showcase.sh --windowed --vsync
```

The CSV contains frame time, CPU submission time, asynchronous GPU time, draws and triangles. CPU submission includes any driver stalls and is not pure application CPU cost. GPU timer zero means not yet available or disabled. Use a repeated representative run after asset/shader/terrain warmup; exclude first-use frames. Captures, --validate and software rendering distort throughput measurements.

## Comparison on the checked host

The bundled report compares 0.4.0 and 0.4.1 on Mesa25 llvmpipe, fixed 960x540 output, LP_NUM_THREADS=4, 36 frames per shot, times 16/40/55, high quality and no UI/readback/validation in the timed loop. Tests run sequentially with the same settings. This is a software-driver comparison, not a hardware FPS forecast. See PERFORMANCE.json for the historical 0.4.1 measured results; 0.4.2 changes source-build/distribution and does not claim a new performance benchmark.

| Scene start | 0.4.0 ms/frame | 0.4.1 ms/frame | Throughput increase |
| --- | ---: | ---: | ---: |
| 16 s | 589.4 | 568.1 | 3.7% |
| 40 s | 558.2 | 496.8 | 12.4% |
| 55 s | 554.4 | 500.7 | 10.7% |

Combined throughput increased 8.7% in this run. Small differences can vary between runs; these are measurements on the checked software driver, not a promise for other machines.

## Presets, autofocus and processing update

This update changes scheduling, resource reuse and driver submission. It does not lower the selected resolution, geometry, texture tier, shadow filtering or effect sample counts. The existing showcase High preset retains its prior appearance. Minimal through Max are explicit quality choices; autofocus changes depth of field only when that effect is enabled.

- Uniform values are retained per linked program and compared bitwise. Unchanged values and inactive shader inputs skip driver updates. Shader replacement retires the cache. Compact scalar/vector records keep frequently used state together; matrix storage is allocated separately only where needed.
- Prepared draw storage persists across frames. Large lists use the existing job pool to compute independent transforms, bounds, normal matrices and material keys; stable ordering and all GL calls stay on the render thread. Small lists run locally to avoid scheduling overhead. Nested work and exceptions are joined safely.
- Automatic workers respect Linux CPU affinity and reserve one available CPU for the calling thread. Other platforms use hardware concurrency. `JSD_WORKERS=N` sets the background-worker budget, limited by available CPUs; explicit `Jobs(N)` instances keep their requested size. Sleeping worker threads do not create artificial CPU load.
- Terrain generation evaluates each shared grid vertex once while retaining the exact emitted triangle order, normals, UVs and skirts. Terrain height callbacks must be deterministic for a world position and safe to call on workers. Distance-priority sorting compares squared grid distances instead of repeated square roots.
- Exposure, color, focus, anisotropy and other uniform-only edits retain framebuffer storage. Target reallocation happens only when dimensions, attachment layouts or enabled target-producing effects change. Texture changes that retain the uploaded dimensions update filtering without rebuilding pixel data/mipmaps; native-size uploads use retained source pixels directly.
- Autofocus remains on the GPU in two 1x1 textures. It performs no normal-frame CPU readback or synchronization. The blur retains the selected sample count and foreground rejection; linear depth is reconstructed from the projection entries. See `GRAPHICS_PRESETS.md`.

The source release includes `PROCESSING_VALIDATION.json` with the final measurements and comparison results. These measurements use Mesa llvmpipe, a software OpenGL driver, and are not a dedicated-GPU speed forecast. Frame time includes actual rendering and presentation; offline simulation cadence is not treated as measured FPS. No driver power, process-affinity or system scheduling settings are changed.

Measured processing work (same settings/output):

| Workload | Before | After | Result |
| --- | ---: | ---: | --- |
| Draw-heavy frame, 1,600 boxes | 18.74 ms | 14.21 ms | 24.2% lower frame time |
| Uniform calls per draw-heavy frame | 4,918 | 200 | 95.9% fewer |
| Exposure/anisotropy edit texture uploads | 12 | 0 | Existing storage reused |
| Exposure/anisotropy edit framebuffer creations | 8 | 0 | Existing storage reused |
| Generation of 60 terrain tiles | 32.46 ms | 10.88 ms | 2.98x faster |
| Terrain height evaluations | 1,704,000 | 498,300 | 70.8% fewer |

The draw-heavy fixture uses 320x180 native output, four warmup frames, then 16 timed frames; values are medians of three process runs. Terrain tests use subdivisions 8/16/32/64 and the engine's terrain-height function; generation timings exclude comparison-file writing. The resulting image and complete terrain vertex buffers are byte-identical.

Full showcase, 640x360, unchanged High settings (software driver):

| Mission start | Before median frame | After median frame | Frame-time reduction |
| --- | ---: | ---: | ---: |
| 16 s | 422.56 ms | 416.94 ms | 1.3% |
| 40 s | 344.42 ms | 327.87 ms | 4.8% |
| 55 s | 408.66 ms | 404.31 ms | 1.1% |

Each mission uses two before/after pairs with order reversed in the second pair. Runs contain 24 frames; first eight warmup frames and the final capture frame are excluded. The table reports the median of the two run medians. All six paired showcase captures are byte-identical. These smaller, software-driver frame-time differences vary with host load; the raw runs are retained in the JSON rather than treated as a hardware FPS promise. Both builds use four llvmpipe raster threads and seven engine background workers for the comparison.

Native UI checks exercise every preset button, both settings pages, focus controls and saved configurations. Regression tests render actual center targets to verify autofocus acquisition, smooth changes, camera cuts, sky focus and manual mode. The retained quality settings and sample counts are tested independently from the new explicit Max preset.

The driver-call/texture/framebuffer benchmark is provided as `tools/benchmark_processing.cpp` and uses only the public engine API. Build it after installing the local SDK:

```bash
c++ -O2 -std=c++20 -Iinclude tools/benchmark_processing.cpp -Llib \
    -Wl,-rpath,"$PWD/lib" -ljsd-game-engine -o build/processing-benchmark
./build/processing-benchmark comparison.ppm
```

It prints actual frame time and intercepted GL call counts, and writes a comparison image. Compile against each matching engine SDK when comparing builds; use the same driver, environment and dimensions. The benchmark does not select lower quality settings dynamically.
