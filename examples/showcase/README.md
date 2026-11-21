# SUNDOG / Recovery at Kestrel Relay

An optional internal engine example in `jsd-game-engine/examples/showcase`. It owns the mission, art, camera track, scene assembly and physics setup, and links the public engine target. It is disabled by default with `JSD_BUILD_SHOWCASE=OFF`.

From the engine root:

```bash
./run-showcase.sh                           # enables/builds and launches
./build.sh release -DJSD_BUILD_SHOWCASE=ON   # build only
./test.sh --with-showcase                   # engine checks and showcase smoke check
```

The CMake target/executable is `jsd-showcase`; enabling it adds source to the existing engine build graph. All engine, SDL and Jolt code is built from source; no separate game project or prebuilt SDK is required. Installed assets resolve relative to the executable's prefix, with a source-tree fallback for development. See [source-build details](../../docs/BUILDING.md).

The sample starts in borderless fullscreen at the desktop drawable resolution, with an actual wall-clock FPS counter at lower left. VSync and the earlier 125 FPS sleep cap are disabled by default. The 160-second mission loops automatically. The rover approaches through dust, the crane unloads cargo, a physics-driven crate falls, a maintenance drone welds with sparks, a hopper pours sand and bouncing gravel, vents release steam and the relay powers up. Camera tracks tour the outpost, accelerate over the mountains into an illuminated cave refinery, inspect the extraction gallery and return continuously to the opening view. There is no player objective or finished game package.

| Control | Action |
| --- | --- |
| Space | Pause/resume simulation |
| C | Show/hide graphics settings; click values to cycle, click the header for page two |
| Tab | Toggle free camera and mouse capture |
| WASD + mouse | Move/look in free camera |
| R | Restart mission, physics, particles and temporal history |
| F1 | Toggle mission HUD; FPS remains visible |
| F11 | Toggle fullscreen/windowed |
| Escape | Exit |

## Capture and quality

```bash
./run-showcase.sh --quality ultra
./run-showcase.sh --windowed --width 1920 --height 1080
./run-showcase.sh --vsync
./run-showcase.sh --windowed --profile frame-times.csv
./run-showcase.sh --offscreen --frames 24 --time 38 --width 1280 --height 720 --no-ui --validate --capture shot.ppm
./run-showcase.sh --offscreen --time 14 --fps 24 --frames 48 --width 960 --height 540 --sequence frames
```

`--time SECONDS` pre-rolls simulation to a mission time. `--fps` controls fixed offline capture cadence; use 24, 30 or 60. `--sequence` writes PPM frames for external encoding. `--capture` saves the final frame. These offline captures are not real-time FPS measurements. `run-showcase.sh` changes to the engine root, so relative output paths resolve there.

`--quality minimal|low|medium|high|max` selects rendering quality (`ultra` remains an alias for Max). The panel provides five direct preset buttons. `--settings FILE.ini` loads all engine graphics settings after preset selection. Press `C` to edit the same two-page settings panel used by the graphics lab. Settings take effect immediately. The panel background is 30% transparent (70% opacity), with opaque text for readability. Opening it releases the mouse and suspends free-camera movement; closing it restores capture if free camera is active and the window has focus. The mission continues unless paused with Space. The panel is independent of the F1 mission HUD toggle. Use the engine's `./run.sh --write-settings FILE.ini` to save a configuration. `--assets PATH` selects an asset root; `--no-audio`, `--no-ui`, `--free-camera` and `--no-particles` support inspection. `--validate` checks GL errors and finite HDR/render buffers each frame. `--debug-dir DIRECTORY` writes diagnostic buffers. `--help` lists options.

Interactive fullscreen uses the desktop resolution; `--width`/`--height` control windowed/offscreen dimensions (default 1920x1080). `--windowed` restores a resizable window. `--no-vsync` requests uncapped presentation (the default); `--vsync` requests synchronized presentation. The driver/compositor can override a swap-interval request. `--no-fps` hides only the FPS counter; `--no-ui` hides both HUD and FPS. During initial timing warmup the counter displays `-- FPS`. Its number is measured throughput, including presentation, and is independent of `--fps`, which controls simulation/capture cadence.

`--profile FILE.csv` records per-frame wall time, CPU submission time, asynchronous GPU timer results, draw calls and triangles. This opt-in diagnostic writes buffered CSV; zero GPU time indicates a query has not completed or profiling was disabled in settings. See [performance](../../docs/PERFORMANCE.md).

The launcher uses the driver-selected GPU by default and never forces software rendering. On hybrid systems, `JSD_GPU=discrete ./run-showcase.sh` requests the first non-default Mesa GPU or NVIDIA PRIME offload when its driver is installed. An explicit existing `DRI_PRIME` or NVIDIA offload environment setting is respected. The renderer name is printed at startup. No software switch guarantees 100% GPU usage.

A native GPU is recommended; software GL is useful for correctness testing. The scene is an engine showcase and remains below Steel Nomad's production art quality. Included previews are actual native-renderer readbacks, with no image retouching.

## Build and install

`JSD_BUILD_SHOWCASE=ON` adds `jsd-showcase` alongside the existing engine targets. It does not change the public shared/static engine exports. `JSD_BUILD_EXAMPLES` separately controls the graphics lab. Disabled builds do not configure or install the showcase. Options persist in their CMake build directory.

Enabled installs include `bin/jsd-showcase`, assets under `share/jsd-game-engine/showcase/assets` and the matching docs/licenses. Run the installed executable from any working directory, or use `--assets PATH` for another asset root. `src/basic_demo.cpp` retains the earlier compact example as reference source and is not a build target.

## Assets

Asset credits and font notices are in [docs/CREDITS.md](docs/CREDITS.md) and `licenses/`. These tools/assets belong to the optional showcase and are not required by the engine library.

All runtime assets are included. Original industrial models are assembled from reusable engine geometry. Original surface maps and HUD/signage are generated by `tools/generate_showcase_assets.py` (Python, NumPy, Pillow, SciPy and DejaVu fonts); older basic assets by `tools/generate_sample_assets.py` (standard library). Poly Haven's CC0 rock scans, gravelly sand and 4K sky include creator credits, links and SHA-256 hashes in `assets/showcase/scans/SOURCES.json`.

Run the following authoring commands from `examples/showcase`. To recreate scanned LODs: run `python3 tools/fetch_scan_sources.py`, then `blender --background --python tools/cook_scans.py`, then `python3 tools/unpack_scan_lods.py`. The fetcher needs network access; the cooker needs Blender 4.3+ and Pillow. Runtime/builds do not need these tools. The cooker welds position seams before decimation and leaves scan undersides open to preserve UVs; place scan bases below terrain.

Preset mappings, autofocus controls and worker tuning are documented in [graphics presets](../../docs/GRAPHICS_PRESETS.md). Depth of field now autofocuses on the view center when enabled; its response and manual fallback can be edited on page one. `--write-settings FILE.ini` saves the current showcase configuration on exit.

The rover uses mesh-calibrated, distance-driven wheel rotation and six terrain contact locations. Wheel/rim state updates during simulation; stopping and mission resets cannot add phantom spin. See [rover physics](../../docs/ROVER_PHYSICS.md) for equations and validation.

The current [physics layer](../../docs/PHYSICS.md) gives the rover a swept collision proxy and uses accepted motion for wheel odometry. Compound structural primitives and the visible slab have static collision proxies. Crane cargo now stays one 25 kg physical body from pickup through dynamic release and settlement; hook/cargo rendering follows the accepted solver pose. `./test.sh --with-showcase` includes obstacle, pickup/release continuity and settling checks.

The Deep Refinery expansion adds editable plant machinery, moving ore, local lights and steam. See [the continuous tour](docs/DEEP_REFINERY.md) for chapter times and regeneration instructions.
