#!/usr/bin/env bash
set -euo pipefail
jsd_deps_root=$(cd -- "$(dirname -- "$0")" && pwd -P)
python3 -B "$jsd_deps_root/tools/project_guard.py" source --root "$jsd_deps_root" --project jsd.engine --expected 0.5.12
jsd_admin=()
if (( EUID != 0 )); then
    if ! command -v sudo >/dev/null; then
        printf 'Installing OS development packages requires root. Run this script as root, or install the packages listed in docs/BUILDING.md.\n' >&2
        exit 1
    fi
    jsd_admin=(sudo)
fi
# These are OS/toolchain dependencies, not prebuilt engine, Jolt or SDL packages.
if command -v apt-get >/dev/null; then
    "${jsd_admin[@]}" apt-get update
    "${jsd_admin[@]}" apt-get install -y build-essential cmake ninja-build python3 pkg-config \
        libgl-dev libegl-dev libx11-dev libxext-dev libxrandr-dev libxcursor-dev \
        libxfixes-dev libxi-dev libxss-dev libwayland-dev libwayland-bin wayland-protocols \
        libxkbcommon-dev libasound2-dev libpulse-dev libudev-dev
elif command -v dnf >/dev/null; then
    "${jsd_admin[@]}" dnf install -y gcc gcc-c++ make cmake ninja-build python3 pkgconf-pkg-config \
        mesa-libGL-devel mesa-libEGL-devel libX11-devel libXext-devel libXrandr-devel \
        libXcursor-devel libXfixes-devel libXi-devel libXScrnSaver-devel wayland-devel \
        wayland-protocols-devel libxkbcommon-devel alsa-lib-devel pulseaudio-libs-devel systemd-devel
elif command -v pacman >/dev/null; then
    "${jsd_admin[@]}" pacman -S --needed --noconfirm base-devel cmake ninja python pkgconf \
        mesa libglvnd libx11 libxext libxrandr libxcursor libxfixes libxi libxss \
        wayland wayland-protocols libxkbcommon alsa-lib libpulse systemd-libs
else
    printf 'Install a C/C++20 compiler, CMake 3.20+, make/ninja, pkg-config, OpenGL/EGL, X11/Wayland and audio development headers. See docs/BUILDING.md.\n' >&2
    exit 1
fi
