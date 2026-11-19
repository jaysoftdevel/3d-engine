# Single-folder source validation — JSD 0.4.4

Checked on Debian13 Linux x86-64, GCC14.2, CMake3.31.6 and Mesa25 llvmpipe. Engine, SDL2 and Jolt artifacts were compiled locally from bundled source. The host supplies the compiler, OS development headers and graphics/window/audio runtimes. Results are summarized in SOURCE_BUILD_VALIDATION.json; logs are in validation/.

| Check | Result |
| --- | --- |
| Archive layout | Exactly one top-level directory, jsd-game-engine; no sample-game folder |
| Direct CMake configure without showcase flags | JSD_BUILD_SHOWCASE defaults OFF |
| Clean source archive, showcase disabled | Fresh SDL/Jolt/shared/static SDK build, full tests and graphics lab launch pass |
| Disabled target graph/install | No jsd-showcase target, installed executable or showcase assets |
| CPU/runtime suite | 62 checks passed, including independent engine decoder/animation fixtures |
| Native graphics/runtime suite | 93 checks passed |
| Forced GL3.3 / GLSL330 suite | 92 checks passed |
| Optional showcase enabled | jsd-showcase builds in the engine graph and renders with finite-buffer validation |
| CTest showcase-smoke | Passed; registered when showcase and engine tests are enabled |
| Build-tree showcase | Renders from unrelated working directory using source asset fallback |
| Relocated installed showcase | Renders from unrelated working directory with original source assets unavailable |
| Explicit invalid asset override | Fails clearly rather than falling back to another asset root |
| run.sh --showcase | Enables/builds/installs/launches the internal showcase successfully |
| Showcase asset integrity | All 25 scanned runtime-file hashes match the manifest after moving |
| Script syntax | Engine shell scripts and engine/showcase Python sources parse successfully |
| Final source release audit | No compiled programs, shared/static/object libraries, Gradle JARs, shader bytecode files or build caches |

The final archive is independently audited after generation. Runtime images/audio/glTF buffers and decoder fixtures are input data; scripts are text. Unused non-Linux SDL folders containing precompiled Direct3D/Metal shader arrays and Gradle wrapper JARs remain omitted. SDL source provenance and modifications are recorded in third_party/SDL2/SOURCE.json.

The prerequisite installer was not executed as administrator on this host. Validation uses a provisioned compiler and OS headers. JSD_AUTO_INSTALL_DEPS=0 disables package installation; JSD_HEADLESS_ONLY=ON explicitly permits EGL-only CI builds when desktop development headers are unavailable.

0.4.4 changes packaging and integrates the optional example into the engine's CMake graph. Public engine APIs, source-only dependency builds and rendering behavior are retained. Historical 0.4.2/0.4.3 logs cover incremental rebuilds, build failure propagation, external shared/static/source consumers, SDK/test relocation and paths with spaces. Historical measurements remain documented in RELEASE_NOTES/PERFORMANCE. Hardware GPU performance, physical audio, Fedora/Arch installer execution, other architectures, Windows and long-session stress remain unvalidated.

## Reproduce

```bash
cd jsd-game-engine
./test.sh                              # engine only; showcase OFF
./bin/jsd-engine-tests
./run.sh --offscreen --frames 2 --width 640 --height 360 --no-ui
./test.sh --with-showcase                # enable and check internal showcase too
./run-showcase.sh --offscreen --frames 2 --time 40 --width 640 --height 360 --validate
python3 tools/audit_source_package.py /path/to/jsd-game-engine-v0.4.4-source.tar.gz
```

Audit the pristine tree/archive, not a tree after building its binaries. See BUILDING.md for CMake flags and installation paths.
