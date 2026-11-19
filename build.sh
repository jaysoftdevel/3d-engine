#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")"
engine_mode="${1:-release}"
case "$engine_mode" in
    release) engine_type=Release ;;
    debug) engine_type=Debug ;;
    *) printf 'Usage: ./build.sh [release|debug]\n' >&2; exit 2 ;;
esac
engine_cmake="${CMAKE:-cmake}"
engine_build="${JSD_BUILD_DIR:-build/$engine_mode}"
"$engine_cmake" -S . -B "$engine_build" -DCMAKE_BUILD_TYPE="$engine_type" -DCMAKE_INSTALL_LIBDIR=lib
"$engine_cmake" --build "$engine_build" --parallel "${JSD_BUILD_JOBS:-4}"
"$engine_cmake" --install "$engine_build" --prefix "${JSD_INSTALL_PREFIX:-$PWD}"
printf 'Built JSD Game Engine 0.4.0 (%s), SDK, graphics lab and regression suite.\n' "$engine_mode"
