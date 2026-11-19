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

## GPU selection and measurement

The launcher respects the host's normal OpenGL driver and existing environment settings. `JSD_GPU=discrete ./run.sh` requests Mesa's first non-default GPU (`DRI_PRIME=1`) and, when an NVIDIA driver is installed, NVIDIA PRIME offload. GPU selection and support depend on the machine. Primary references: [Mesa DRI_PRIME](https://docs.mesa3d.org/envvars.html#dri-prime) and [NVIDIA PRIME](https://download.nvidia.com/XFree86/Linux-x86_64/550.135/README/primerenderoffload.html).

Startup prints the renderer name. A `llvmpipe` result means software rendering; it is not a hardware GPU benchmark. Uncapping presentation and lowering CPU overhead allow more throughput, but utilization depends on the scene, processor, driver, compositor and GPU. This package cannot promise 100% GPU usage or change driver power policy.

For profiling on your GPU:

```bash
cd sample-game
./run.sh --profile frame-times.csv
# Optional synchronized/windowed modes:
./run.sh --windowed --vsync
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
