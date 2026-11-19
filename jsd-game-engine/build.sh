#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")"
jsd_engine_root="$PWD"
source "$jsd_engine_root/cmake/build-common.sh"
if [[ "${1:-}" == --help ]]; then
    printf 'Usage: ./build.sh [release|debug] [CMake -D options...]\n'; exit 0
fi
jsd_select_mode "${1:-${JSD_BUILD_TYPE:-Release}}"
if [[ $# -gt 0 ]]; then shift; fi
jsd_require_tools
engine_cmake="${CMAKE:-cmake}"
engine_build="${JSD_BUILD_DIR:-build/$jsd_mode}"
engine_prefix="${JSD_INSTALL_PREFIX:-$PWD}"
printf 'Configuring JSD 0.4.2: engine, Jolt and SDL2 from source (%s).\n' "$jsd_type"
"$engine_cmake" -S . -B "$engine_build" -DCMAKE_BUILD_TYPE="$jsd_type" -DCMAKE_INSTALL_LIBDIR=lib \
    -DCMAKE_INSTALL_PREFIX="$engine_prefix" -DJSD_HEADLESS_ONLY="${JSD_HEADLESS_ONLY:-OFF}" "$@"
"$engine_cmake" --build "$engine_build" --parallel "${JSD_BUILD_JOBS:-4}"
"$engine_cmake" --install "$engine_build" --prefix "$engine_prefix"
printf 'Built JSD 0.4.2 and bundled dependencies from source; SDK: %s\n' "$engine_prefix"
