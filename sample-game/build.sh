#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")"
jsd_engine_root="${JSD_GAME_ENGINE_DIR:-$PWD/../jsd-game-engine}"
jsd_engine_root="$(cd -- "$jsd_engine_root" && pwd)"
if [[ "${1:-}" == --help ]]; then
    printf 'Usage: ./build.sh [release|debug] [CMake -D options...]\n'; exit 0
fi
case "${1:-${JSD_BUILD_TYPE:-Release}}" in
    release|Release) jsd_mode=release; jsd_type=Release ;;
    debug|Debug) jsd_mode=debug; jsd_type=Debug ;;
    *) printf 'Build mode must be release or debug.\n' >&2; exit 2 ;;
esac
if [[ $# -gt 0 ]]; then shift; fi
source_mode="${JSD_GAME_USE_ENGINE_SOURCE:-OFF}"
sdk_dir="${JSD_INSTALL_PREFIX:-$jsd_engine_root}"
if [[ "$source_mode" == ON ]]; then
    source "$jsd_engine_root/cmake/build-common.sh"
    jsd_require_tools
elif [[ "${JSD_GAME_SKIP_ENGINE_BUILD:-OFF}" != ON && -f "$jsd_engine_root/CMakeLists.txt" ]]; then
    "$jsd_engine_root/build.sh" "$jsd_mode"
fi
game_cmake="${CMAKE:-cmake}"
game_build="${JSD_GAME_BUILD_DIR:-build/$jsd_mode}"
engine_location="$sdk_dir"
source_flags=()
if [[ "$source_mode" == ON ]]; then
    engine_location="$jsd_engine_root"
    source_flags=(-DJSD_HEADLESS_ONLY="${JSD_HEADLESS_ONLY:-OFF}" -DCMAKE_INSTALL_LIBDIR=lib)
fi
"$game_cmake" -S . -B "$game_build" -DCMAKE_BUILD_TYPE="$jsd_type" \
    -DJSD_GAME_ENGINE_DIR="$engine_location" -DJSD_GAME_RUNTIME_ENGINE_DIR="$sdk_dir" \
    -DJSD_GAME_USE_ENGINE_SOURCE="$source_mode" -DJSD_GAME_STATIC_ENGINE="${JSD_GAME_STATIC_ENGINE:-OFF}" \
    "${source_flags[@]}" "$@"
"$game_cmake" --build "$game_build" --parallel "${JSD_BUILD_JOBS:-4}"
if [[ "$source_mode" == ON ]]; then
    "$game_cmake" --build "$game_build" --target jsd-game-engine-static --parallel "${JSD_BUILD_JOBS:-4}"
    "$game_cmake" --install "$game_build/engine-build" --prefix "$sdk_dir"
fi
"$game_cmake" --install "$game_build" --prefix "$PWD"
