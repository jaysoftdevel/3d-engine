# JSD Game Engine 0.4.2 — source only

An independent native C++20 Linux engine, with a separate sibling autoplay sample. This release replaces the prebuilt SDK distribution with automatic source builds. The graphics/runtime APIs and 0.4.1 performance improvements remain available.

**No compiled executables, libraries, object files, Gradle wrapper JARs or build caches are distributed.** All engine, SDL2 and Jolt code is built locally; cgltf/stb/SMAA source is compiled as part of the engine. Images, meshes and audio are runtime input data.

## Use

```bash
./run-showcase.sh                  # builds engine/dependencies/sample, then runs
./run-demo.sh --graphics-lab        # builds engine/dependencies, then opens settings
./build.sh                         # source build + local shared/static SDK install
./build.sh debug                   # debug build
./test.sh                          # builds, then runs regression/render checks
```

The root workspace `./run.sh` and sibling `../sample-game/run.sh` also build automatically before launching. Every launch performs an incremental build, including after source edits. The sample remains fullscreen and uncapped by default with a lower-left measured FPS counter; F11, --windowed and --vsync remain supported.

The default build uses bundled dependency source and does not fetch code. If compiler/build tools or desktop development headers are missing, the scripts run `install_deps.sh`. OS package installation requires network/admin access; normal compilation/install stays inside the workspace. Set `JSD_AUTO_INSTALL_DEPS=0` if your toolchain/headers are already provisioned. The GPU driver, glibc and OS window/audio libraries are system prerequisites.

`JSD_BUILD_JOBS=8` selects parallelism and `CMAKE=/path/to/cmake` selects CMake 3.20+. `JSD_INSTALL_PREFIX=/your/sdk` selects an SDK destination. The engine is compiled once into PIC object files, reused by its shared/static outputs. Both variants use the same source-built SDL shared runtime. [Build details](docs/BUILDING.md).

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

Frontier/JSD public headers, original settings and Frontier::Engine / JSD::GameEngine shared/static CMake targets remain. Recompile consumers when updating the SDK. The RED FRONTIER 0.1 adapter remains included. Existing Platform constructors retain their old display defaults; the sample selects fullscreen explicitly.

Validated on Linux x86-64, GCC14.2, CMake3.31.6 and Mesa EGL/OpenGL3.3/4.5. Desktop OpenGL3.3+ is required at runtime; GL4 enables hardware tessellation. Source builds use your own compiler/system ABI; the distribution imposes no prebuilt glibc/libstdc++ version floor. A C/C++20 toolchain and OS development headers are needed; see BUILDING.md. Other architectures/platforms are unvalidated.

Original code/assets use LICENSE.txt; dependencies retain their own licenses. SDL2 source provenance and its omitted precompiled Android helper are recorded in third_party/SDL2/SOURCE.json. Poly Haven asset credits remain beside the sample scans and in [third-party notices](docs/THIRD_PARTY.md). The package audit rejects compiled-code signatures and library/object suffixes before release.
