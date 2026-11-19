# Automatic source build

The release contains source and runtime data. Engine/sample executables, shared/static libraries, object files, compiled Java helpers and CMake caches are excluded. SDL2 2.32.4 and Jolt5.7.0 source are bundled; no binary SDL lookup, FetchContent download or precompiled SDK fallback is used by the default build. Non-Linux SDL renderer folders with embedded compiled shader blobs and both Gradle wrapper JARs are omitted.

## One-command workflow

```bash
./run.sh                    # workspace root: build and launch
./build.sh                  # build only
./build.sh debug            # debug source build
```

The sibling sample's `run.sh` does the same. It first calls the engine's build, which compiles SDL, Jolt, one PIC engine object set, shared/static engine libraries, graphics lab and regression executable; it then builds/installs the sample against that generated SDK. Later launches configure/check the build graph and rebuild changed source incrementally. A failed configure/compile/install prevents launch; scripts do not silently fall back to an old executable.

Generated output locations:

| Path | Content generated locally |
| --- | --- |
| jsd-game-engine/build/release or debug | CMake cache, dependency builds, engine objects/targets |
| jsd-game-engine/lib and bin | Installed local SDK libraries, SDL runtime, lab/tests |
| jsd-game-engine/include/SDL2 and include/Jolt | Installed third-party public headers |
| sample-game/build/release or debug | Independent sample build |
| sample-game/bin/sample-game | Installed sample executable |

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
| JSD_GAME_BUILD_DIR | Sample build directory override |
| JSD_GAME_ENGINE_DIR | Engine source root, or an already source-built SDK |
| JSD_GAME_USE_ENGINE_SOURCE | ON compiles engine as a game CMake subdirectory; default scripts use the generated SDK |
| JSD_GAME_STATIC_ENGINE | ON links static engine plus source-built shared SDL |
| JSD_AUTO_INSTALL_DEPS | 0 disables OS package installation |
| JSD_HEADLESS_ONLY | ON permits missing desktop headers for EGL-only builds |

Example static sample: `JSD_GAME_BUILD_DIR=build/static JSD_GAME_STATIC_ENGINE=ON ./sample-game/build.sh`.

## Direct CMake

```bash
cmake -S jsd-game-engine -B engine-build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_LIBDIR=lib
cmake --build engine-build --parallel 4
cmake --install engine-build --prefix "$PWD/jsd-game-engine"
cmake -S sample-game -B game-build -DJSD_GAME_ENGINE_DIR="$PWD/jsd-game-engine"
cmake --build game-build --parallel 4
```

In a fresh source-only workspace with no installed SDK, a direct sample CMake configure defaults to source dependency mode and builds engine/SDL/Jolt automatically. Explicit `-DJSD_GAME_USE_ENGINE_SOURCE=ON` forces that route; `OFF` uses a previously generated SDK. Use separate directories when switching modes. The sample build script can install that source-mode SDK with JSD_GAME_USE_ENGINE_SOURCE=ON as well.

Building the engine once and installing it elsewhere creates a reusable dependency for multiple games. Its CMake exports resolve the adjacent, source-built SDL package automatically. Shared library RPATHs and pkg-config prefixes are relative so the generated SDK can be relocated with its consumers.

## Audit the source release

```bash
python3 jsd-game-engine/tools/audit_source_package.py /path/to/jsd-engine-workspace-v0.4.2-source.tar.gz
```

The audit detects native executable/object/archive signatures, compiled-library suffixes, Java/WASM binaries and build caches. Text shell/Python scripts remain executable; glTF .bin buffers, images and audio are data, not prebuilt programs. Auditing a tree after building will intentionally report its generated binaries; audit the pristine source tree or release archive.
