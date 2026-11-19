# Game integration and compatibility

## Sibling SDK dependency

Keep the engine and each game as siblings. The archive contains only `jsd-game-engine`; all source/build/dependency/test/tooling files are inside it. `examples/showcase` is an optional internal example, disabled by default with `JSD_BUILD_SHOWCASE=OFF`. It links the public engine target; it is not part of the engine library or exported SDK dependencies. Engine tests use independent fixtures. The game owns its assets, gameplay, saves and main loop. The engine owns reusable runtime and graphics code.

```cmake
cmake_minimum_required(VERSION 3.20)
project(MyGame LANGUAGES CXX)
set(JSD_GAME_ENGINE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../jsd-game-engine" CACHE PATH "Engine SDK")
find_package(JSDGameEngine 0.4 CONFIG REQUIRED PATHS "${JSD_GAME_ENGINE_DIR}" NO_DEFAULT_PATH)
add_executable(my-game src/main.cpp)
target_link_libraries(my-game PRIVATE JSD::GameEngine)
set_target_properties(my-game PROPERTIES
    INSTALL_RPATH "$ORIGIN/../../jsd-game-engine/lib")
install(TARGETS my-game RUNTIME DESTINATION bin)
```

The source release contains no installed libraries or executables. From inside `jsd-game-engine`, `./build.sh` compiles bundled SDL/Jolt and the engine, then installs a local SDK into its own `include`, `lib` and `bin` directories. `./run.sh` opens the engine lab and `./test.sh` runs engine-only tests. The optional `./run-showcase.sh` enables its internal example target in the same source build. `JSD::GameEngineStatic` includes the required Jolt archive transitively. Use a separate CMake build directory when switching a consumer between source and SDK dependency modes.

For a source dependency, set `JSD_BUILD_TESTS`, `JSD_BUILD_EXAMPLES` and `JSD_BUILD_SHOWCASE` OFF before `add_subdirectory("${JSD_GAME_ENGINE_DIR}" engine-build)` and link the same target. Link `JSD::GameEngine` or `JSD::GameEngineStatic` in either mode. No engine files need to be copied into your game.

A system SDK install also works:

```bash
cmake -S jsd-game-engine -B engine-build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_LIBDIR=lib
cmake --build engine-build --parallel 4
cmake --install engine-build --prefix /your/sdk
cmake -S my-game -B game-build -DJSD_GAME_ENGINE_DIR=/your/sdk
```

## Main loop and lifetimes

```cpp
#include <jsd/engine.hpp>
jsd::Platform platform;
jsd::gfx::Renderer graphics(platform, jsd::gfx::GraphicsSettings::high());
jsd::PhysicsWorld physics;
jsd::Registry entities;
jsd::FixedStep clock;
// Poll input, advance fixed-step simulation, interpolate visuals, then:
graphics.begin({{0, 3, -8}, rf::normalized(jsd::V3{0, -1, 8}), 70, .08f});
graphics.primitive(jsd::gfx::Primitive::Box, {{0,1,2}, {2,2,2}, 0});
graphics.end();
platform.present();
```

The internal showcase supplies the complete continuous loop, input, interpolation, asset upload, audio and physics. The engine API retains `rf` as well as the `jsd::gfx` alias to avoid breaking source consumers. Creating a `GpuScene` uploads its resources; destroy it before its `Renderer`. Destroy the renderer before its current GL context/platform. Destroy `AudioDevice` before `AudioMixer`. Submit lights after `begin()` every frame. Persistent GPU handles are retired with the corresponding `destroy*` call. Renderer/GPU calls must occur on the context thread.

`Draw.modelMatrix` overrides the legacy `Transform` when present. Use a stable unique nonzero `objectId` to track motion for each rigid object, including across frames with no movement. `GpuScene::draw` requires a disjoint `idBase` per instance. For an existing app/window, construct `Renderer(Context)` with function resolution and drawable-size callbacks; the context must be current, and the application handles event polling/presentation.

An `EnvironmentId` comes from `createEnvironment(width,height,floatRGB)`; set it on `Environment.image`. `Environment.deltaSeconds` drives exposure adaptation. The four-argument `createEnvironment` overload can limit lighting highlights without clipping the sky source; the existing three-argument overload preserves full source radiance. Atmosphere/cloud defaults are disabled, preserving old scene behavior. New material fields are appended with neutral defaults.

Use `<jsd/particles.hpp>` and `<jsd/cinematic.hpp>` for new emitters and camera tracks; `<jsd/engine.hpp>` includes both. See [particle integration](PARTICLES.md). Emission and physics are game-owned; submit particles between `begin()` and `end()`. `Renderer::image` uses the same 1280x720 logical UI canvas as text/rects.

Graphics settings may be changed before/between frames; inspect the returned requested/effective report. `resetHistory()` is available for scene cuts. Match physics body shapes and render transforms in game code; the engine does not infer collisions from render meshes.

## Deploy and relocate

The optional installed showcase resolves `../lib` from the engine's `bin` directory and reads assets from `share/jsd-game-engine/showcase/assets` relative to that prefix. Your own game may use the sibling SDK layout shown above. For another deployment layout, install the engine library next to your executable or in its own `lib` folder and set an appropriate `$ORIGIN`-relative RPATH. Rebuild the game to apply an installed RPATH; build-tree CMake may add absolute paths for development.

`lib/libjsd-game-engine.so.0.4.4` plus `.so.0` and `.so` symlinks are required for shared linking. SDL2 is built from bundled source and installed into the generated engine/lib, then loaded from there; it still needs system window/audio dependencies and a working GL driver. Static engine linking does not make SDL or the driver static. Jolt is already linked into the shared engine; static consumers link `libJolt.a` too through CMake. `lib/pkgconfig/jsd-game-engine.pc` and `frontier-engine.pc` provide relocatable compiler/link flags.

## Frontier 0.2 source compatibility

`find_package(FrontierEngine 0.2 CONFIG REQUIRED)` and `Frontier::Engine` / `Frontier::EngineStatic` remain valid. Includes under `<frontier/...>`, namespace `rf`, original aggregate field order, enum values 0–3 for AA, `frontier_engine_version()` and settings `version=1` remain available. Newly appended structures/settings require recompilation; this release promises source migration compatibility, not old-binary ABI compatibility. The old `libfrontier-engine.so`/`.a` linker names forward to the JSD SDK after installation; no old-version binary SONAME is impersonated.

The actual RED FRONTIER 0.1 source adapter is retained at `compat/red_frontier_0_1`, installed at `include/frontier-compat/red_frontier_0_1`. Put it first in includes, then engine/include, then the old game's src. Replace the old renderer/platform implementation files with the external library:

```bash
JSD_SDK=/absolute/path/jsd-game-engine
g++ -std=c++20 -O2 -pthread \
    -I"$JSD_SDK/compat/red_frontier_0_1" -I"$JSD_SDK/include" -Isrc \
    src/main.cpp src/game/game.cpp src/game/save.cpp src/game/app.cpp tests/regression.cpp \
    -L"$JSD_SDK/lib" -ljsd-game-engine -Wl,-l:libSDL2-2.0.so.0 -ldl \
    -Wl,-rpath,"$JSD_SDK/lib" -o game-with-external-engine
```

The adapter references the consuming game's own `game/game.hpp`; the engine library itself does not. This does not imply compatibility with separate colony/VOIDLINE APIs or assets.

## Display mode and frame timing

`Platform::setFullscreen(bool)` requests borderless desktop fullscreen, `isFullscreen()` reads its SDL flag, and `setVSync(bool)` requests swap interval 0 or 1. They return false on unsupported/offscreen operations. The original constructor remains windowed with VSync for existing consumers; the showcase explicitly requests fullscreen and disables VSync. Poll drawable dimensions on resize as usual; Renderer(Context) does this through its callback each frame.

`jsd::FrameRateCounter::sample(seconds)` accepts completed wall-clock frame periods. `fps()` updates over half-second windows, `milliseconds()` gives the corresponding average period, and `reset()` clears the reading. It does not measure GPU utilization. Avoid feeding fixed simulation/capture deltas into this counter.

The standard mesh shader now consumes normal-matrix columns at locations 13–15, in addition to the retained earlier attributes 0–12. Existing custom shaders can keep calculating their own normals from the model matrix; unused extra attributes are harmless. Reloading a material program invalidates its uniform-location cache. GPU calls and submission remain confined to the context thread.

For the source-only release and automatic prerequisite/build workflow, see [BUILDING.md](BUILDING.md). JSD exports discover the adjacent source-built SDL CMake package; static engine consumers still link that SDL shared runtime. The source archive includes no generated lib/bin/export/cache products.
