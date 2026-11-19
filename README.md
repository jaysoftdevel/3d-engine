# JSD engine source workspace 0.4.2

Source-only Linux C++20 engine and autoplay sample. No prebuilt executables, object files, shared/static libraries or build caches are included. SDL2 and Jolt source are bundled; cgltf, stb and SMAA are compiled into the engine. Runtime meshes, images and audio are data assets.

```text
jsd-game-engine/     # independent engine and dependency source, public API, build tools
sample-game/         # separate game-owned showcase source and runtime assets
```

## Build automatically and run

```bash
./run.sh
# Or the existing entry point:
cd sample-game
./run.sh
```

First launch configures and compiles SDL2, Jolt, the engine SDK and sample, then starts fullscreen with measured FPS. Later launches run an incremental build, so modified code is rebuilt automatically. No source/dependency downloads are needed once OS prerequisites are present.

Missing compiler/build tools and Linux development headers are installed by `install_deps.sh` through the available package manager; that step needs network access and administrative privileges. It does not install a prebuilt SDL or engine package. The OS graphics driver and system libraries come from your Linux installation.

Build without launching: `./build.sh`. Debug: `./build.sh debug`. Engine alone: `cd jsd-game-engine && ./build.sh`. Set `JSD_BUILD_JOBS=8` for parallelism, or `JSD_AUTO_INSTALL_DEPS=0` to manage system prerequisites yourself.

Space pauses, Tab switches camera, WASD/mouse fly, R restarts, F1 hides the mission HUD, F11 toggles fullscreen and Escape exits. `--windowed`, `--vsync`, `--no-fps` and `--profile FILE.csv` are retained.

[Build details](jsd-game-engine/docs/BUILDING.md) · [Engine API/integration](jsd-game-engine/docs/INTEGRATION.md) · [Sample controls](sample-game/README.md) · [Validation](jsd-game-engine/docs/VALIDATION.md).
