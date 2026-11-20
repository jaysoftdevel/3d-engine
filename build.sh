#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")"
jsd_engine_root=$(pwd -P)
source "$jsd_engine_root/cmake/build-common.sh"
jsd_build_usage() {
    printf 'Usage: ./build.sh [release|debug] [CMake -D options...]\n       ./build.sh clean\n'
}
case "${1:-}" in
    --help|-h) jsd_build_usage; exit 0 ;;
    clean|--clean)
        if [[ $# -ne 1 ]]; then jsd_build_usage >&2; exit 2; fi
        # Clean needs no compiler, CMake, dependency installation or build mode.
        # Resolve paths before deleting anything, including environment overrides.
        engine_prefix=$(realpath -m -- "${JSD_INSTALL_PREFIX:-$jsd_engine_root}")
        engine_clean_dirs=("$jsd_engine_root/build" "$jsd_engine_root/prebuild"
            "$jsd_engine_root/prebuilds" "$jsd_engine_root/prebuilt" "$jsd_engine_root/prebuilts")
        if [[ -n "${JSD_BUILD_DIR:-}" ]]; then
            engine_clean_dirs+=("$JSD_BUILD_DIR")
        fi
        for engine_dir in "${engine_clean_dirs[@]}"; do
            engine_resolved=$(realpath -m -- "$engine_dir")
            # Never allow a build override to delete the project or its sources.
            case "$engine_resolved" in
                /) printf 'Refusing to clean filesystem root.\n' >&2; exit 2 ;;
            esac
            case "$jsd_engine_root/" in
                "$engine_resolved/"*)
                    printf 'Refusing to clean project/ancestor directory: %s\n' "$engine_dir" >&2
                    exit 2 ;;
            esac
        done
        if [[ "$engine_prefix" != "$jsd_engine_root" ]]; then
            if [[ "$engine_prefix" == / ]]; then
                printf 'Refusing to clean filesystem root as SDK prefix.\n' >&2; exit 2
            fi
            case "$jsd_engine_root/" in
                "$engine_prefix/"*)
                    printf 'Refusing to clean project ancestor as SDK prefix: %s\n' "$engine_prefix" >&2
                    exit 2 ;;
            esac
        fi
        for engine_dir in "${engine_clean_dirs[@]}" "$engine_prefix"; do
            engine_resolved=$(realpath -m -- "$engine_dir")
            if [[ -e "$engine_dir" && ! -d "$engine_dir" ]]; then
                printf 'Clean directory is not a directory: %s\n' "$engine_dir" >&2; exit 2
            fi
            for engine_source in src include third_party compat examples cmake configs docs licenses tools tests; do
                case "$engine_resolved/" in
                    "$jsd_engine_root/$engine_source/"*)
                        printf 'Refusing to clean source directory: %s\n' "$engine_dir" >&2
                        exit 2 ;;
                esac
            done
        done
        # Source headers under include/jsd, frontier and frontier-compat stay put.
        engine_clean_dirs+=("$jsd_engine_root/bin" "$jsd_engine_root/lib"
            "$jsd_engine_root/lib64" "$jsd_engine_root/share"
            "$jsd_engine_root/include/SDL2" "$jsd_engine_root/include/Jolt")
        engine_clean_dirs+=("$engine_prefix/include/SDL2" "$engine_prefix/include/Jolt")
        if [[ "$engine_prefix" != "$jsd_engine_root" ]]; then
            # An external SDK prefix may also contain unrelated software.
            engine_clean_dirs+=("$engine_prefix/include/jsd" "$engine_prefix/include/frontier"
                "$engine_prefix/include/frontier-compat"
                "$engine_prefix/bin/jsd-graphics-lab" "$engine_prefix/bin/jsd-engine-tests"
                "$engine_prefix/bin/jsd-showcase" "$engine_prefix/bin/sdl2-config"
                "$engine_prefix/share/jsd-game-engine" "$engine_prefix/share/licenses/SDL2"
                "$engine_prefix/share/aclocal/sdl2.m4")
            for engine_libdir in "$engine_prefix/lib" "$engine_prefix/lib64"; do
                engine_clean_dirs+=("$engine_libdir/cmake/JSDGameEngine"
                    "$engine_libdir/cmake/FrontierEngine" "$engine_libdir/cmake/SDL2"
                    "$engine_libdir/cmake/Jolt" "$engine_libdir/pkgconfig/jsd-game-engine.pc"
                    "$engine_libdir/pkgconfig/frontier-engine.pc" "$engine_libdir/pkgconfig/sdl2.pc")
                # Globs include versioned runtimes and their symlinks.
                engine_clean_dirs+=("$engine_libdir"/libjsd-game-engine.*
                    "$engine_libdir"/libfrontier-engine.* "$engine_libdir"/libJolt.*
                    "$engine_libdir"/libSDL2*.so* "$engine_libdir"/libSDL2*.a
                    "$engine_libdir"/libSDL2*.la "$engine_libdir"/libSDL2*.dylib)
            done
        fi
        rm -rf -- "${engine_clean_dirs[@]}"
        printf 'Removed build trees, prebuilds and generated engine/SDL/Jolt SDK outputs.\n'
        exit 0 ;;
esac
jsd_select_mode "${1:-${JSD_BUILD_TYPE:-Release}}"
if [[ $# -gt 0 ]]; then shift; fi
jsd_require_tools
engine_cmake="${CMAKE:-cmake}"
engine_build="${JSD_BUILD_DIR:-build/$jsd_mode}"
engine_prefix="${JSD_INSTALL_PREFIX:-$PWD}"
printf 'Configuring JSD 0.4.4: engine, Jolt and SDL2 from source (%s).\n' "$jsd_type"
"$engine_cmake" -S . -B "$engine_build" -DCMAKE_BUILD_TYPE="$jsd_type" -DCMAKE_INSTALL_LIBDIR=lib \
    -DCMAKE_INSTALL_PREFIX="$engine_prefix" -DJSD_HEADLESS_ONLY="${JSD_HEADLESS_ONLY:-OFF}" "$@"
"$engine_cmake" --build "$engine_build" --parallel "${JSD_BUILD_JOBS:-4}"
"$engine_cmake" --install "$engine_build" --prefix "$engine_prefix"
printf 'Built JSD 0.4.4 and bundled dependencies from source; SDK: %s\n' "$engine_prefix"
