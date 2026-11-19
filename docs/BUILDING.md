# Automatic source build

The release contains source and runtime data. Engine/showcase executables, shared/static libraries, object files, compiled Java helpers and CMake caches are excluded. SDL2 2.32.4 and Jolt5.7.0 source are bundled; no binary SDL lookup, FetchContent download or precompiled SDK fallback is used by the default build. Non-Linux SDL renderer folders with embedded compiled shader blobs and both Gradle wrapper JARs are omitted.

## Engine workflow

The archive has exactly one top-level directory: `jsd-game-engine`. Its optional showcase lives in `examples/showcase`. No parent-level build files or scripts are needed. Start in the engine directory:

```bash
cd jsd-game-engine
./run.sh                    # build engine/dependencies and launch engine graphics lab
./build.sh                  # build only
./build.sh debug            # debug source build
./test.sh                   # complete engine-only regression/render checks
```

The engine compiles SDL, Jolt, one PIC engine object set, shared/static engine libraries, its graphics lab and regression executable. Tests use fixtures in `tests/fixtures`, also installed into `share/jsd-game-engine/test-fixtures`. With `JSD_BUILD_SHOWCASE=OFF` (the default for a new build), the showcase target is not configured, built or installed. Engine tests use independent fixtures. Later launches configure/check the build graph and rebuild changed source incrementally. A failed configure/compile/install prevents launch; scripts do not silently fall back to an old executable.

## Optional showcase target

From the engine directory:

```bash
./build.sh release -DJSD_BUILD_SHOWCASE=ON  # engine + jsd-showcase + asset install
./run-showcase.sh                          # enables/builds it, then launches
./run.sh --showcase                        # equivalent launch shortcut
./test.sh --with-showcase                  # regression suite + showcase smoke check
```

The CMake target is `jsd-showcase`. It links the existing engine target within the same build graph; no second engine build or SDK lookup is needed. `JSD_BUILD_SHOWCASE` is independent of `JSD_BUILD_EXAMPLES` (the graphics lab). CMake options persist in a build directory; pass `-DJSD_BUILD_SHOWCASE=OFF` to disable it again. Disabling excludes the target and future installation; it does not delete previously installed files. `test.sh` explicitly disables the showcase unless requested; its older `--with-sample` switch is retained as an alias.

Enabled installs place the executable in `bin/jsd-showcase` and assets/notices in `share/jsd-game-engine/showcase`. The executable locates installed assets relative to its prefix, falling back to the source assets for build-tree runs; `--assets PATH` overrides this. Launching from another working directory and relocating a complete installed prefix both work.

Generated output locations:

| Path | Content generated locally |
| --- | --- |
| jsd-game-engine/build/release or debug | CMake cache, dependency builds, engine objects/targets |
| jsd-game-engine/lib and bin | Installed local SDK libraries, SDL runtime, lab/tests |
| jsd-game-engine/include/SDL2 and include/Jolt | Installed third-party public headers |
| jsd-game-engine/share/jsd-game-engine/test-fixtures | Installed engine test input data |
| jsd-game-engine/bin/jsd-showcase | Optional showcase executable |
| jsd-game-engine/share/jsd-game-engine/showcase | Optional showcase assets and notices |

These products are not in the release archive. Remove build directories and generated SDK outputs for a fully clean rebuild; keep the source include/frontier and include/jsd directories.

## OS prerequisites

The scripts detect build tools and desktop headers, then invoke install_deps.sh when needed. This installs compiler/build tools and OS development packages, not a prebuilt engine/SDL/Jolt. Administrative privileges and network access are needed only for missing OS packages. CMake builds use bundled source without network access after prerequisites are installed.

Debian/Ubuntu packages: build-essential, cmake, ninja-build, python3, pkg-config, libgl-dev, libegl-dev, libx11-dev, libxext-dev, libxrandr-dev, libxcursor-dev, libxfixes-dev, libxi-dev, libxss-dev, libwayland-dev, libwayland-bin, wayland-protocols, libxkbcommon-dev, libasound2-dev, libpulse-dev and libudev-dev. Fedora/dnf and Arch/pacman equivalents are in install_deps.sh. The system graphics driver, glibc, X11/Wayland/audio libraries and build tools remain OS components; this project does not rebuild the OS/toolchain.

For pre-provisioned, offline or non-root environments:

```bash
JSD_AUTO_INSTALL_DEPS=0 ./build.sh
```

Do not set that override on a machine missing desktop headers unless supplying them yourself. Native SDL configuration fails clearly if OpenGL and an X11/Wayland backend cannot be built. For CI/EGL-only builds without desktop headers, use a separate build directory and `JSD_HEADLESS_ONLY=ON JSD_AUTO_INSTALL_DEPS=0`; interactive SDL windows are not supported in such a minimal configuration. EGL/OpenGL runtime drivers are still required for graphics checks.

## Controls

| Variable | Purpose |
| --- | --- |
| CMAKE | CMake executable, 3.20+ |
| CC / CXX | C/C++ compiler selection |
| JSD_BUILD_TYPE | Release or Debug (run.sh uses this mode) |
| JSD_BUILD_JOBS | Parallel compile jobs; default 4 |
| JSD_BUILD_DIR | Engine build directory override |
| JSD_INSTALL_PREFIX | Generated engine SDK destination; default engine directory |
| JSD_AUTO_INSTALL_DEPS | 0 disables OS package installation |
| JSD_HEADLESS_ONLY | ON permits missing desktop headers for EGL-only builds |

## Direct CMake

Run from the parent containing `jsd-game-engine`:

```bash
cmake -S jsd-game-engine -B engine-build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_LIBDIR=lib
cmake --build engine-build --parallel 4
cmake --install engine-build --prefix "$PWD/jsd-game-engine"
```

To opt into the showcase in that build:

```bash
cmake -S jsd-game-engine -B engine-build -DJSD_BUILD_SHOWCASE=ON
cmake --build engine-build --target jsd-showcase --parallel 4
ctest --test-dir engine-build -R showcase-smoke --output-on-failure
```

The smoke test is registered only when both `JSD_BUILD_SHOWCASE` and `JSD_BUILD_TESTS` are enabled. Building all default targets before installing generates the engine's shared and static SDK outputs as well. A minimal SDK-only source build can set `JSD_BUILD_TESTS=OFF`, `JSD_BUILD_EXAMPLES=OFF` and `JSD_BUILD_SHOWCASE=OFF`.

Building the engine once and installing it elsewhere creates a reusable dependency for multiple games. Its CMake exports resolve the adjacent, source-built SDL package automatically. Shared library RPATHs and pkg-config prefixes are relative so the generated SDK can be relocated with its consumers.

## Audit the source release

```bash
# From jsd-game-engine:
python3 tools/audit_source_package.py /path/to/jsd-game-engine-v0.4.4-source.tar.gz
```

The audit detects native executable/object/archive signatures, compiled-library suffixes, Java/WASM binaries and build caches. Text shell/Python scripts remain executable; glTF .bin buffers, images and audio are data, not prebuilt programs. Auditing a tree after building will intentionally report its generated binaries; audit the pristine source tree or release archive.
