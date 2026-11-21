# Windows engine and showcase (0.5.4)

Supported native build: Windows 10/11 x64, Visual Studio 2022 with **Desktop development with C++** (MSVC v143 and Windows SDK), CMake 3.21+, and Python 3.10+. Put `cmake` and `python` on PATH. SDL2 and Jolt are compiled from the bundled sources; no vcpkg or prebuilt dependency download is required. Use a desktop GPU driver with OpenGL 3.3 or later.

From the updated engine directory:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Configuration Release
powershell -NoProfile -ExecutionPolicy Bypass -File .\run-showcase.ps1
```

For Debug, pass `-Configuration Debug` to both scripts. `build.ps1 -Test` also runs CTest; `-Install` installs into the engine directory. Failed configure/build/test commands stop immediately. The engine source identity, version, build ownership, installation prefix and operation lock are checked before building. Existing Linux build directories are separate from `build/windows-vs2022`.

Executables and DLLs are placed together in `build/windows-vs2022/bin/Release` (or `Debug`). Run `jsd-showcase.exe` directly or use the launcher, which forwards showcase options. A capture invocation is:

```powershell
.\run-showcase.ps1 -ShowcaseArguments @('--offscreen', '--frames', '2', '--capture', 'capture.ppm')
```

The renderer and showcase use SDL2 + desktop OpenGL on both operating systems. On Windows `--offscreen` uses a hidden SDL OpenGL window and requires a working interactive desktop graphics session; it is not a display-free software renderer. Linux retains its Mesa EGL offscreen path. Asset discovery uses SDL's executable location and then the source asset directory, so launching from another working directory is supported. Installed showcase assets live in `share/jsd-game-engine/showcase/assets`.

The Windows shared SDK is `bin/jsd-game-engine.dll` plus `lib/jsd-game-engine.lib`; the static library is `lib/jsd-game-engine-static.lib`. SDL2.dll is staged beside the executables. All components use the dynamic MSVC runtime consistently. A machine running a deployed Release build needs the Visual C++ 2015–2022 x64 runtime. Debug builds need the development runtime supplied by Visual Studio. Build and run native CTest on Windows to validate driver-specific behavior.

Linux `build.sh` and `run-demo.sh` remain supported. The editor and game are outside this update.
