# Validation — 19 November 2026 / JSD 0.4.0

Environment: Debian 13 Linux x86-64, GCC 14.2, CMake 3.31.6, bundled SDL 2.32.4 and Mesa 25.0.7 llvmpipe (LLVM 19.1.7). Tests execute actual GL contexts, shaders, framebuffers and readback. This is software-driver correctness validation; no discrete-GPU performance or broad hardware certification is claimed.

| Check | Result |
| --- | --- |
| Offline Release engine build | Shared/static SDK, Jolt archive, graphics lab, tests and relocatable CMake/pkg-config exports |
| CPU/runtime/settings suite with sibling assets | 58 checks passed |
| OpenGL 4.5 graphics/runtime suite | 87 checks passed; hardware tessellation shader path |
| Forced OpenGL 3.3 / GLSL 330 | 86 checks passed; CPU terrain fallback |
| Pixel feature matrix | 23 cases pass; allocation checked separately; RENDER_MATRIX.json |
| Particle GPU behavior | Occlusion, visible dust/steam/sparks/sand, one instanced sprite draw, soft intersections and finite buffers |
| Mesh/material safety | Finite unit normals, transformed geometry, invalid normals/mapping rejected, parallel tangent/normal basis stays finite |
| VFX simulation | Bounded rates, deterministic subdivisions/reset, steam rise, spark retirement, gravel contact/bounce |
| Camera/sky/UI | Track interpolation/loop validation; clouds and image overlay affect actual pixels |
| Separate game / shared SDK | Native autoplay scene, all five effects, Jolt cargo, scan assets and photographic HDR sky |
| Static SDK consumer | Sample builds and renders at mission time 40 with finite-buffer validation |
| Source engine dependency | Fresh offline Jolt/engine/sample build and scene render |
| Frontier 0.2 CMake compatibility | Shared/static consumers build and render with SDK 0.4 |
| Actual RED FRONTIER 0.1 game rebuilt | 43 gameplay/persistence checks and native rendering pass through retained adapter |
| Loop reset | Twelve frames crossing time 64 reset mission, physics and particles without GL errors |
| Native SDL path | SDL offscreen video driver and dummy audio callback run the scene |
| Sanitizers | Original engine/wrapper code instrumented with AddressSanitizer and UndefinedBehaviorSanitizer; CPU/graphics suites pass |
| Relocated release archive | Prebuilt scene, fresh game build against relocated SDK, and external consumer verified |

`validation/` contains logs. Rendered previews in `preview/` are actual native-engine readbacks. The MP4 joins four two-second offline captures at 960x540/24 capture frames per second. Still images are 1920x1080. Capture rate does not indicate real-time performance; the host uses software rasterization. No generated/retouched image stands in for engine output.

The graphics checks include all five AA modes, texture reallocation/restoration, MSAA/tessellation fallback, multipass effects, distance culling, full-output temporal reconstruction, local lights, HDR prefilter/BRDF targets, shader reload rejection, visible object motion blur and finite scene/normal/material/motion/lighting buffers. A regression triangle with its smooth normal parallel to its UV tangent reproduces the formerly invalid tangent basis. Fresnel cosine inputs are clamped to their physical range to avoid undefined negative-base powers from float rounding; the full-HD repair capture validates every frame. Geometry tests caught zero rock normals; the asset cooker welds UV position seams before decimation to avoid open cracks.

The pixel matrix verifies effects execute and change images; it is not a visual-quality metric. Screen-space effects retain their documented limits. SUNDOG does not match Steel Nomad's production art quality. No AAA performance claim is made.

Sanitizer scope: original engine and runtime wrapper sources are instrumented; optimized Jolt and system SDL/Mesa are not. UBSan vptr checks are excluded because Jolt is built without RTTI. LeakSanitizer cannot operate under this traced host; successful runs disable leak detection, so leak freedom is not asserted. Sanitizer binaries/build directories are not shipped.

Six sample-file CPU checks are conditional on the sibling sample assets. An SDK installed alone runs the remaining checks. Native hardware audio, Windows, non-x86 platforms, HDR displays, discrete GPUs, production-scale content and long-session stress have not been validated.

## Reproduce

```bash
./build.sh
./test.sh
python3 tests/render_matrix.py
cd ../sample-game
./build.sh
./run.sh --offscreen --frames 16 --time 40 --width 1920 --height 1080 --validate --capture repair.ppm
JSD_GAME_BUILD_DIR=build-static JSD_GAME_STATIC_ENGINE=ON ./build.sh
JSD_GAME_BUILD_DIR=build-source JSD_GAME_USE_ENGINE_SOURCE=ON ./build.sh
```

CPU-only: `./bin/jsd-engine-tests`. With Mesa, `MESA_GL_VERSION_OVERRIDE=3.3 MESA_GLSL_VERSION_OVERRIDE=330` forces fallback. Instrument original engine sources with `-fsanitize=address,undefined -fno-sanitize=vptr -fno-omit-frame-pointer`, link the matching optimized Jolt archive, then run with `ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1`. The Jolt wrapper uses `JPH_OBJECT_LAYER_BITS=16`, `JPH_OBJECT_STREAM` and `NDEBUG` to match that archive.
