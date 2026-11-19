# SUNDOG / Recovery at Kestrel Relay

An autonomous gameplay scene using the external `../jsd-game-engine` SDK. The game owns the mission, art, camera track, scene assembly, physics setup and executable. Reusable graphics, particles and runtime live in the engine.

```bash
./run.sh
./build.sh                         # builds engine/SDL/Jolt SDK and sample
```

Every run.sh invocation builds incrementally before launch. First launch compiles bundled SDL/Jolt and the engine SDK; no executables or libraries are prebuilt in this release. Missing OS build prerequisites are installed through install_deps.sh when authorized by the local user via sudo. Set JSD_AUTO_INSTALL_DEPS=0 for a provisioned toolchain. [Source-build details](../jsd-game-engine/docs/BUILDING.md).

The sample starts in borderless fullscreen at the desktop drawable resolution, with an actual wall-clock FPS counter at lower left. VSync and the earlier 125 FPS sleep cap are disabled by default. The 64-second mission loops automatically. The rover approaches through dust, the crane unloads cargo, a physics-driven crate falls, a maintenance drone welds with sparks, a hopper pours sand and bouncing gravel, vents release steam and the relay powers up. Camera tracks tour the outpost. There is no player objective or finished game package.

| Control | Action |
| --- | --- |
| Space | Pause/resume simulation |
| Tab | Toggle free camera and mouse capture |
| WASD + mouse | Move/look in free camera |
| R | Restart mission, physics, particles and temporal history |
| F1 | Toggle mission HUD; FPS remains visible |
| F11 | Toggle fullscreen/windowed |
| Escape | Exit |

## Capture and quality

```bash
./run.sh --quality ultra
./run.sh --windowed --width 1920 --height 1080
./run.sh --vsync
./run.sh --windowed --profile frame-times.csv
./run.sh --offscreen --frames 24 --time 38 --width 1280 --height 720 --no-ui --validate --capture shot.ppm
./run.sh --offscreen --time 14 --fps 24 --frames 48 --width 960 --height 540 --sequence frames
```

`--time SECONDS` pre-rolls simulation to a mission time. `--fps` controls fixed offline capture cadence; use 24, 30 or 60. `--sequence` writes PPM frames for external encoding. `--capture` saves the final frame. These offline captures are not real-time FPS measurements. `run.sh` changes to this directory, so relative output paths resolve here.

`--quality low|high|ultra` selects rendering quality. `--settings FILE.ini` loads all engine graphics settings after preset selection. Use the engine's `../jsd-game-engine/run-demo.sh --graphics-lab` to edit/save them. `--assets PATH` selects an asset root; `--no-audio`, `--no-ui`, `--free-camera` and `--no-particles` support inspection. `--validate` checks GL errors and finite HDR/render buffers each frame. `--debug-dir DIRECTORY` writes diagnostic buffers. `--help` lists options.

Interactive fullscreen uses the desktop resolution; `--width`/`--height` control windowed/offscreen dimensions (default 1920x1080). `--windowed` restores a resizable window. `--no-vsync` requests uncapped presentation (the default); `--vsync` requests synchronized presentation. The driver/compositor can override a swap-interval request. `--no-fps` hides only the FPS counter; `--no-ui` hides both HUD and FPS. During initial timing warmup the counter displays `-- FPS`. Its number is measured throughput, including presentation, and is independent of `--fps`, which controls simulation/capture cadence.

`--profile FILE.csv` records per-frame wall time, CPU submission time, asynchronous GPU timer results, draw calls and triangles. This opt-in diagnostic writes buffered CSV; zero GPU time indicates a query has not completed or profiling was disabled in settings. See [performance](../jsd-game-engine/docs/PERFORMANCE.md).

The launcher uses the driver-selected GPU by default and never forces software rendering. On hybrid systems, `JSD_GPU=discrete ./run.sh` requests the first non-default Mesa GPU or NVIDIA PRIME offload when its driver is installed. An explicit existing `DRI_PRIME` or NVIDIA offload environment setting is respected. The renderer name is printed at startup. No software switch guarantees 100% GPU usage.

A native GPU is recommended; software GL is useful for correctness testing. The scene is an engine showcase and remains below Steel Nomad's production art quality. Included previews are actual native-renderer readbacks, with no image retouching.

## Build modes

- SDK/shared (default scripts): `./build.sh` builds the sibling engine/dependencies first, then links its generated SDK. An explicit `JSD_GAME_ENGINE_DIR=/absolute/sdk` can reuse an SDK previously built from source. Direct CMake defaults to source mode when no SDK exists.
- SDK/static: `JSD_GAME_BUILD_DIR=build-static JSD_GAME_STATIC_ENGINE=ON ./build.sh`.
- Source: `JSD_GAME_BUILD_DIR=build-source JSD_GAME_USE_ENGINE_SOURCE=ON ./build.sh`.

Use a fresh build directory when switching modes. CMake links only public `JSD::GameEngine` or `JSD::GameEngineStatic`. Copy this project as the starting point for another sibling game. `src/basic_demo.cpp` preserves the earlier compact physics/animation example; it is not the default target.

## Assets

All runtime assets are included. Original industrial models are assembled from reusable engine geometry. Original surface maps and HUD/signage are generated by `../jsd-game-engine/tools/generate_showcase_assets.py` (Python, NumPy, Pillow, SciPy and DejaVu fonts); older basic assets by `generate_sample_assets.py` (standard library). Poly Haven's CC0 rock scans, gravelly sand and 4K sky include creator credits, links and SHA-256 hashes in `assets/showcase/scans/SOURCES.json`.

To recreate scanned LODs: run `fetch_scan_sources.py`, then Blender with `--background --python ../jsd-game-engine/tools/cook_scans.py`, then `unpack_scan_lods.py`. The fetcher needs network access; the cooker needs Blender 4.3+ and Pillow. Runtime/builds do not need these tools. The cooker welds position seams before decimation and leaves scan undersides open to preserve UVs; place scan bases below terrain.
