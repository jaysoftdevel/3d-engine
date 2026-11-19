# Source-only validation — JSD 0.4.2

Checked on Debian13 Linux x86-64, GCC14.2, CMake3.31.6 and Mesa25 llvmpipe. All engine, SDL2 and Jolt artifacts used for these checks were compiled locally from the source-only tree. System compiler, development headers and graphics/window/audio runtimes were supplied by the host environment. Source-build results are summarized in SOURCE_BUILD_VALIDATION.json; logs are in validation/.

| Check | Result |
| --- | --- |
| Pristine source-tree audit | No compiled executable/library/object/JAR or CMake cache before first build |
| Root run.sh from clean tree | Builds SDL C sources, Jolt, one PIC engine object set, both SDK libraries, lab/tests and sample, then renders |
| Source SDL at runtime | Dynamic linkage resolves the newly built adjacent libSDL2, not the old SDK/system SDL |
| Unchanged-source launch | Incremental build; no C/C++ compilation commands |
| Source timestamp update | runtime.cpp recompiles once; SDK/sample relink and run |
| Failed-build propagation | Intentional CMake exit42 stops launch; an existing executable is not used |
| Native graphics/runtime suite | 93 checks passed |
| Forced GL3.3 / GLSL330 | 92 checks passed |
| Fullscreen default | SDL offscreen-window driver reports fullscreen at native1024x768 and renders |
| Static engine sample | Builds against source-built SDK and renders with finite-buffer validation |
| Engine-as-source game mode | Fresh SDL/Jolt/engine/game CMake subdirectory build, generated SDK install and render pass |
| Relocated generated SDK | Shared/static Frontier consumers render from a different SDK path; pkg-config resolves that prefix |
| Fresh direct CMake | Source mode selected automatically before an SDK exists |
| Relocated source archive | Clean build/run from extracted source, including a path containing spaces |
| Final release audit | No compiled code, native/object/static/shared library, Gradle JAR, shader bytecode file or build cache |

The release archive is audited independently after generation. Runtime images/audio/glTF buffers are input data; scripts are text. Unused non-Linux SDL Direct3D/Metal folders with embedded precompiled shader arrays and Gradle wrapper JARs are omitted. SDL source provenance and changes are recorded in third_party/SDL2/SOURCE.json.

The installer branches are syntax-reviewed against SDL's Linux dependency guide, but package-manager installation was not executed with administrator privileges on this host. Validation uses a pre-provisioned compiler and OS development headers. The automatic dependency check can be disabled with JSD_AUTO_INSTALL_DEPS=0. CMake fails rather than silently producing a desktop-less SDL build when native prerequisites are missing; JSD_HEADLESS_ONLY=ON explicitly permits EGL-only CI builds.

0.4.2 changes build/distribution only. The rendering/physics/features and 0.4.1 optimizations are retained. Prior 0.4.1 sanitizer/pixel/performance results remain historical evidence in RELEASE_NOTES/PERFORMANCE, not new hardware measurements for this release. Discrete GPU performance, physical audio, Fedora/Arch package installation, other architectures, Windows and long-session stress remain unvalidated.

## Reproduce

```bash
./run.sh --offscreen --frames 4 --time 40 --width 640 --height 360 --validate
cd jsd-game-engine
./test.sh
python3 tools/audit_source_package.py /path/to/jsd-engine-workspace-v0.4.2-source.tar.gz
```

Run the package audit on the pristine tree/archive, not on a tree after it has generated its own binaries. Source/build/SDK/runtime details are in BUILDING.md. Compilation is incremental, but shader/asset/terrain warmup still occurs at runtime.
