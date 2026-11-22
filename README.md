# JSD Game Engine 0.5.14 — standalone source project

An independent native C++20 Linux engine. The archive contains one top-level directory: `jsd-game-engine`. The optional autoplay showcase is included under `examples/showcase`, as a target of the engine build. All primary source, public headers, dependencies, build scripts, engine tools, documentation and tests live here in `jsd-game-engine`. The showcase is disabled by default (`JSD_BUILD_SHOWCASE=OFF`). The engine builds, tests and runs its graphics lab independently. The graphics/runtime APIs and 0.4.1 performance improvements remain available.

**No compiled executables, libraries, object files, Gradle wrapper JARs or build caches are distributed.** All engine, SDL2 and Jolt code is built locally; cgltf/stb/SMAA source is compiled as part of the engine. Images, meshes and audio are runtime input data.

Version 0.5.14 adds thread-safe live audio voice updates with smoothed channel
gains, retaining clip position while the camera moves or sound presence changes.
See [Live audio](docs/LIVE-AUDIO-0.5.14.md).

Version 0.5.12 fixes Windows consumer definitions, executable paths and file-clock
cache portability. It retains the aperiodic materials from 0.5.11.
See [Windows changes](docs/WINDOWS-0.5.12.md).
See [SURFACES-0.5.11.md](docs/SURFACES-0.5.11.md) for API details.

Version 0.5.13 adds an optional directional sky sphere with filtered, stable
stars, atmospheric visibility and camera/FOV/aspect reprojection.
See [Sky sphere](docs/SKY-SPHERE-0.5.13.md).

## Editable engine projects

The 160-second SUNDOG tour now flies continuously from the relay into a mountain cave refinery and extraction gallery, then returns without a camera cut. [Expansion details](examples/showcase/docs/DEEP_REFINERY.md).

The complete SUNDOG project is in `projects/sundog/sundog.jsdproject`, with all runtime resources in `sundog.assets/`. Run `./run-project.sh`, or open it in the separate sibling editor. Projects retain geometry, materials, motion, terrain, lighting, optics, effects, audio, HUD, physics proxies and rendering settings. [Project format and workflow](docs/JSDPROJECT.md).

## Use

```bash
# From this jsd-game-engine directory:
./build.sh                         # source build + local shared/static SDK install
./build.sh debug                   # debug build
./run.sh                           # builds engine/dependencies, opens graphics lab
./test.sh                          # engine-only regression/render checks
```

There are no parent-level wrappers or required files. Every engine launch performs an incremental build, including after source edits. `run-demo.sh` is retained as an alias for the engine lab. Engine decoder tests use engine-owned fixtures; none read showcase assets.

Run `./run-showcase.sh` or `./run.sh --showcase` to enable/build and launch the optional showcase. To build it without launching, use `./build.sh release -DJSD_BUILD_SHOWCASE=ON`; its CMake target and executable are `jsd-showcase`. `./test.sh --with-showcase` adds its smoke check. Source, assets, authoring tools and credits are inside `examples/showcase`. Only enabled builds install its executable and assets. The showcase remains fullscreen and uncapped by default with a lower-left measured FPS counter; F11, --windowed and --vsync remain supported.

The default build uses bundled dependency source and does not fetch code. If compiler/build tools or desktop development headers are missing, the scripts run `install_deps.sh`. OS package installation requires network/admin access; normal compilation/install stays inside the workspace. Set `JSD_AUTO_INSTALL_DEPS=0` if your toolchain/headers are already provisioned. The GPU driver, glibc and OS window/audio libraries are system prerequisites.

`JSD_BUILD_JOBS=8` selects parallelism and `CMAKE=/path/to/cmake` selects CMake 3.21+. `JSD_INSTALL_PREFIX=/your/sdk` selects an SDK destination. The engine is compiled once into PIC object files, reused by its shared/static outputs. Both variants use the same source-built SDL shared runtime. [Build details](docs/BUILDING.md).

## External game dependency

After `./build.sh`, headers and generated libraries/configuration are installed into this directory. These generated files are local build products; they are excluded from the source release.

```cmake
find_package(JSDGameEngine 0.4 CONFIG REQUIRED
    PATHS "${CMAKE_CURRENT_SOURCE_DIR}/../jsd-game-engine" NO_DEFAULT_PATH)
add_executable(my-game src/main.cpp)
target_link_libraries(my-game PRIVATE JSD::GameEngine)
```

Use JSD::GameEngineStatic for the static engine archive. It still links the source-built SDL shared runtime. Jolt is linked automatically. The source package has no game dependency; use the engine alone or keep games alongside it. Direct source-mode CMake also works before SDK installation. [Integration](docs/INTEGRATION.md).

## Features retained

- Off/FXAA/SMAA/MSAA/TAA, texture quality, anisotropy, directional shadows, view distance, SSAO, DOF, screen-space GI/SSR, edge fusion and CPU/GPU terrain tessellation.
- HDR PBR/IBL, bloom, exposure, tone mapping, grading, fog/clouds, motion vectors/blur, temporal reconstruction, wind, clearcoat, triplanar mapping and instancing.
- Dust, steam, sparks, sand and colliding mesh gravel; cinematic camera, textured UI and finite framebuffer diagnostics.
- Jolt physics, ECS/hierarchy, fixed-step simulation, input, jobs/task graph, glTF animation/CPU skinning, image/HDR/WAV loaders and stereo positional audio.
- Indexed static geometry, streaming mesh updates, shared per-frame preparation, cached uniforms and optimized post-processing from 0.4.1.

[Feature algorithms and limits](docs/FEATURES.md), [particle API](docs/PARTICLES.md), [performance](docs/PERFORMANCE.md). Hardware ray tracing, Vulkan and vendor ML upscalers remain unimplemented; this source-build change adds no new rendering backend.

## Compatibility and requirements

Frontier/JSD public headers, original settings and Frontier::Engine / JSD::GameEngine shared/static CMake targets remain. Recompile consumers when updating the SDK. The RED FRONTIER 0.1 adapter remains included. Existing Platform constructors retain their old display defaults; the showcase selects fullscreen explicitly.

Validated on Linux x86-64, GCC14.2, CMake3.31.6 and Mesa EGL/OpenGL3.3/4.5. Desktop OpenGL3.3+ is required at runtime; GL4 enables hardware tessellation. Source builds use your own compiler/system ABI; the distribution imposes no prebuilt glibc/libstdc++ version floor. A C/C++20 toolchain and OS development headers are needed; see BUILDING.md. Other architectures/platforms are unvalidated.

Original code/assets use LICENSE.txt; dependencies retain their own licenses. SDL2 source provenance and its omitted precompiled Android helper are recorded in third_party/SDL2/SOURCE.json. See [engine third-party notices](docs/THIRD_PARTY.md); optional showcase asset credits are in [examples/showcase/docs/CREDITS.md](examples/showcase/docs/CREDITS.md). The package audit rejects compiled-code signatures and library/object suffixes before release.

Graphics controls now include Minimal, Low, Medium, High and Max presets in both examples. Press `C` in the showcase for the 30% transparent panel; depth of field uses autofocus when enabled. See [presets and autofocus](docs/GRAPHICS_PRESETS.md) and [processing measurements](docs/PROCESSING_VALIDATION.json).

Rover wheel direction, circumference-based rotation and ground contact are corrected in the [rover physics update](docs/ROVER_PHYSICS.md).

The [SI physics layer](docs/PHYSICS.md) now provides density-based mass/inertia, gravity, force/torque, stable damping/drag, fixed-step scheduling and swept cinematic motion. Rover/crane actors use collision-aware solver poses. [Physical camera and photometry](docs/OPTICS.md) add metre-based lenses, shutter/ISO exposure and lumen/lux lighting without changing legacy defaults.

Minimal now uses a 320 m horizon and a 64 px Lowest world-texture tier, with compact forward rendering and coarse distant terrain. [Settings, diagnostics and trade-offs](docs/MINIMAL_RENDERING.md).
