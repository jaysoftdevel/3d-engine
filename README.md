# JSD Game Engine 0.4.0

A native C++20 Linux graphics and game runtime with an independently linkable shared/static SDK. Version 0.4 adds five particle effects, atmospheric clouds, improved HDR environment processing, richer materials, reusable geometry tools and an autoplay showcase. Existing Frontier/JSD source integration and all graphics controls remain available.

```text
jsd-game-engine/     # reusable engine source, public headers, SDK and graphics lab
sample-game/         # separate consuming project: SUNDOG / Recovery at Kestrel Relay
```

## Run

```bash
./run-showcase.sh                  # 64-second looping autonomous mission
./run-demo.sh --graphics-lab        # graphics/settings laboratory
./run-demo.sh --graphics-lab --write-settings my-graphics.ini
```

With the sibling sample present, `./run-demo.sh` without arguments starts the showcase. Existing laboratory flags still work; `--graphics-lab` explicitly selects it. The lab exposes original and advanced settings on two pages. Click to edit, WASD/arrows to navigate, Tab to capture/release the mouse. Games can load its saved settings with `--settings FILE.ini`.

The showcase follows a six-wheeled rover into a relay outpost, unloads cargo, releases a Jolt rigid body, welds a junction with sparks, pours sand and gravel, vents steam and brings the relay online. It includes a free camera. See [sample controls and capture options](../sample-game/README.md).

## Build and integrate

Install a C++20 compiler, CMake 3.20+, make, SDL2 runtime and an OpenGL driver. `./install_deps.sh` supports common Linux package managers. C++ dependency sources are bundled; builds do not download them.

```bash
./build.sh                         # shared/static SDK, lab and tests; installs here
cd ../sample-game
./build.sh                         # game builds against the installed sibling SDK
./run.sh
```

`JSD_BUILD_JOBS=8` selects parallelism; `CMAKE=/path/to/cmake` selects CMake. `./build.sh debug` enables debug symbols. Source dependency mode from the sample: `JSD_GAME_BUILD_DIR=build-source JSD_GAME_USE_ENGINE_SOURCE=ON ./build.sh`. Use a separate build directory for each linking mode.

```cmake
find_package(JSDGameEngine 0.4 CONFIG REQUIRED
    PATHS "${CMAKE_CURRENT_SOURCE_DIR}/../jsd-game-engine" NO_DEFAULT_PATH)
add_executable(my-game src/main.cpp)
target_link_libraries(my-game PRIVATE JSD::GameEngine)
```

Use `JSD::GameEngineStatic` for static engine linkage. SDL2 and the graphics driver remain runtime requirements. The engine has no dependency on the sample's assets, headers or main loop. [Integration guide](docs/INTEGRATION.md).

## Rendering and runtime

- Off/FXAA/SMAA/MSAA/TAA; texture quality and anisotropy; shadows, view distance, SSAO, DOF, screen-space GI/SSR, edge fusion and CPU/GPU terrain tessellation.
- Motion vectors, reactive temporal reconstruction, render scaling, sharpening, HDR bloom, exposure, tone mapping, grading, motion blur and volumetric height fog.
- GGX PBR and HDR image-based lighting, full-resolution environment backgrounds, triplanar maps, macro variation, clearcoat, instancing and tiled point/spot lights.
- Ray-marched procedural clouds, wind-deformed vegetation, beveled boxes, spheres, toruses, rocks, curved tubes and correct transformed normals.
- Dust, steam, velocity-stretched HDR sparks, sand and mesh gravel. Fixed-step emission, lifetime evolution, wind/drag/turbulence, surface collision, depth-faded instanced billboards and temporal reactive masks. [Particle API](docs/PARTICLES.md).
- Jolt rigid bodies, contacts/CCD/ray casts, ECS, hierarchy, fixed-step simulation, input, jobs/task graph, LOD selection, glTF animation/CPU skinning, PNG/JPEG/HDR assets and spatial stereo audio.
- Cinematic camera tracks, textured full-resolution UI, framebuffer diagnostics and finite-value validation.

[Feature details](docs/FEATURES.md) describe the actual algorithms and limits. Vulkan, hardware ray tracing, vendor ML upscalers, frame generation, virtualized geometry and production character/content tools remain unimplemented. Capability flags accurately report unavailable backends. Output is SDR, with HDR internal lighting. The showcase is a substantial refinement, not Steel Nomad visual parity or a finished AAA engine.

## Compatibility and requirements

Frontier 0.2 public headers, the `rf` namespace, original enum values, settings format and `Frontier::Engine` / `Frontier::EngineStatic` targets remain. `find_package(FrontierEngine 0.2)` works. Recompile consumers: appended public structures change the C++ ABI. The RED FRONTIER 0.1 source adapter is retained; unrelated VOIDLINE/colony APIs are not assumed compatible.

Prebuilt binaries: Linux x86-64, glibc 2.38+, GLIBCXX_3.4.32+, desktop OpenGL 3.3+. OpenGL 4.0+ enables hardware terrain tessellation. SDL2 runtime is bundled; system window/audio libraries and the graphics driver come from the host. Build from source on older distributions. Jolt uses baseline SSE2, without AVX requirements.

## Validation and licenses

```bash
./test.sh
python3 tests/render_matrix.py
```

EGL headless checks work with Mesa software rendering. [Validation results](docs/VALIDATION.md) include 58 CPU checks, 87 native graphics checks, 86 forced-GL3.3 checks, 23 pixel cases and legacy/source/static/relocated SDK checks. These are functional tests, not discrete-GPU performance certification.

Original code and generated artwork use [LICENSE.txt](LICENSE.txt). CC0 scans, surface maps and the sky come from Poly Haven; author credits and provenance are in [third-party notices](docs/THIRD_PARTY.md) and the sample's `assets/showcase/scans/SOURCES.json`.
