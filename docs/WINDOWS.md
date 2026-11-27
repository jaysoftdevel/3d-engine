# Windows engine — 0.5.22

Use the **batch scripts from Command Prompt**. PowerShell is not required. Python 3.10+ and a Visual Studio C++ toolchain remain required; bundled SDL2 and Jolt compile from source.

```bat
cd /d D:\jsd_win\3d-project\3d-engine
install.bat --check
install.bat
```

`install.bat` configures, builds and installs the engine, bundled runtime DLLs into the engine directory. It accepts:

```bat
install.bat Debug --test --jobs 8
install.bat --configuration Release --test
build.bat --configuration Release
```

`build.bat` configures/builds without installation unless `--install` is supplied. `--test` runs CTest before installation; a failed configure/build/test immediately stops later stages. `--check` performs preflight only, without creating build directories or logs. The SDK contains libraries and headers; launch the game from its own workspace. Tests run from the build directory and are not installed.

## Tool detection

The batch entry points try `py -3`, `python`, then `python3` and verify Python 3.10+ instead of trusting an executable name. The exact running interpreter is passed to CMake's project/version guards. Bytecode generation is disabled.

The driver uses Microsoft's `vswhere` to discover Visual Studio/Build Tools installations with x64 C++ tools. It supports the Visual Studio 2022 and 2026 generators when the selected CMake version advertises them. It searches PATH, the standard CMake installation, and Visual Studio's bundled CMake. Install the Desktop development with C++ workload, Windows SDK, and either standalone CMake or the Visual Studio CMake component. There are no automatic downloads or administrator prompts.

Explicit overrides are available:

```bat
install.bat --cmake "C:\Program Files\CMake\bin\cmake.exe" --generator "Visual Studio 17 2022"
```

The driver keeps `build/windows-vs2022` and `build/windows-vs2026` separate. Existing build caches must match the source project, architecture, generator and Visual Studio instance. Incompatible caches are refused rather than deleted or reused. Source identities, versions, installation-prefix guards and operation locks remain enforced.

## Failure logs

The build prints its selected toolchain and each stage's log path. Logs are under the selected Windows build directory:

- `configure.log`
- `build-release.log` or `build-debug.log`
- `test-release.log` or `test-debug.log`
- `install-release.log` or `install-debug.log`

On failure the relevant log's last diagnostics are printed and the failing command's exit code is propagated to the batch caller. Paths containing spaces are passed as separate arguments.

## MSVC shader fix

The previous build hit **C2026** because SMAA was embedded in a single oversized raw string literal. The embedded source now uses independent chunks of at most 8192 UTF-8 bytes, joined once at startup. It is not one adjacent concatenated compiler literal. The generator uses the same representation so regeneration cannot reintroduce the error. All 54,447 normalized shader bytes and the original license are preserved; SMAA and all quality presets remain enabled. `shader-data` verifies the reconstructed shader against the bundled source.

The reported C4244 math conversions now use explicit float casts, preserving the existing float output precision.

## Runtime

The renderer uses SDL2 and OpenGL on both platforms. A current desktop GPU driver with OpenGL 3.3+ is needed. Windows offscreen tests use hidden SDL windows and need a working desktop graphics session. Release deployments require the matching Visual C++ x64 runtime. `install.bat --test` performs native validation; native Windows/MSVC execution cannot be run from the Linux validation environment.

References: [MSVC C2026](https://learn.microsoft.com/en-us/cpp/error-messages/compiler-errors-1/compiler-error-c2026?view=msvc-170), [Microsoft vswhere](https://github.com/microsoft/vswhere/wiki/Find-VC), [CMake Visual Studio generators](https://cmake.org/cmake/help/latest/manual/cmake-generators.7.html).
