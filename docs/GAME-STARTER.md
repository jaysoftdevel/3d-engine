# Start a game with the JSD title menu

Copy the `starter` directory into your new game workspace. Set `gameTitle` in
`main.cpp`, put your asset loading after the renderer is created, and replace
`drawGame` with your game's frame loop.

## Build against an engine source tree

From the copied starter directory on Linux, run:

```bash
cmake -S . -B build -DJSD_ENGINE_SOURCE="/path/to/3d-engine" -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/starter
```

From Windows Command Prompt with Visual Studio 2022 and CMake installed, run:

```bat
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DJSD_ENGINE_SOURCE="C:\3D\3d-engine"
cmake --build build --config Release
build\Release\starter.exe
```

For an installed SDK, copy `share/jsd-game-engine/starter` and give CMake its
install prefix with `-DCMAKE_PREFIX_PATH=...` in place of `JSD_ENGINE_SOURCE`.

## Connect your game

Create a `jsd::shell::Window` once the SDL OpenGL window exists. Call
`showStartup` before constructing your renderer, and pass `rendererStartup()`
to the renderer. The splash updates during shader compilation and binary-cache
restores. Call `showStartup` again between asset-loading phases. Escape and
window close cancel startup via `jsd::shell::StartupCancelled`.

The starter includes **New Game**, **Resume Session**, **Settings**, **Credits**
and **Exit**. It supports the mouse, arrow keys and Enter. Escape takes you
back from a menu page and opens or closes the in-game menu. Fullscreen and
vertical sync are wired up in Settings. Set `TitleOptions::savesSupported` to show the save and load entries and
connect them to your game’s save system.

Each `Page` supplies the title, ordered menu items, and optional context and
status text. The menu returns the selected item index. Reset its selection
when switching pages. Games that already have a UI renderer can implement
`jsd::shell::Canvas` and call `Menu::draw` inside their own frame.
