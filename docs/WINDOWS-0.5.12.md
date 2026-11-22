# JSD 0.5.12 — Windows consumer compatibility

Public engine targets propagate NOMINMAX, WIN32_LEAN_AND_MEAN and SDL_MAIN_HANDLED
to consuming targets, in addition to the existing /utf-8 and /Zc:__cplusplus
options for MSVC. Static physics and source-built SDL keep the same configuration
and C runtime as the consumer. SDL executable paths use UTF-8 conversion.

Project mesh/clip caches now use typed filesystem path/size/time keys. No file
clock representation is narrowed or passed to an ambiguous to_string overload.
Full timestamp precision is retained; project file formats are unchanged.

The engine archive and DLL cross-built for Windows x64 in Release and Debug
alongside Lunar Colony 0.61.1, using Clang 22.1.8/MinGW (Zig 0.17.1). Engine portable
tooling tests pass (46/46). A native MSVC build and Windows runtime/driver test
were not available; the supplied native build and test entry points are retained.
See WINDOWS.md for tool detection and native build commands.
