#!/usr/bin/env bash
set -euo pipefail
if command -v apt-get >/dev/null; then
    sudo apt-get update
    sudo apt-get install -y build-essential cmake python3 pkg-config libsdl2-2.0-0 libgl1 libegl1
elif command -v dnf >/dev/null; then
    sudo dnf install -y gcc-c++ cmake python3 pkgconf-pkg-config SDL2 mesa-libGL mesa-libEGL
elif command -v pacman >/dev/null; then
    sudo pacman -S --needed gcc make cmake python3 pkgconf sdl2 mesa
else
    printf 'Install a C++20 compiler, SDL2 runtime and OpenGL 3.3 driver. CMake 3.20+, Python 3 and EGL for offscreen checks.\n'
    exit 1
fi
