#!/usr/bin/env bash
# Sourced by the engine build scripts.
jsd_require_tools() {
    local jsd_missing=0 jsd_desktop_missing=0
    for jsd_tool in "${CMAKE:-cmake}" "${CC:-cc}" "${CXX:-c++}" pkg-config; do
        command -v "$jsd_tool" >/dev/null 2>&1 || jsd_missing=1
    done
    if ! command -v make >/dev/null 2>&1 && ! command -v ninja >/dev/null 2>&1; then jsd_missing=1; fi
    if [[ "${JSD_HEADLESS_ONLY:-OFF}" != ON ]] && command -v pkg-config >/dev/null 2>&1; then
        pkg-config --exists gl egl x11 wayland-client xkbcommon alsa libpulse || jsd_desktop_missing=1
        command -v wayland-scanner >/dev/null 2>&1 || jsd_desktop_missing=1
    fi
    if (( jsd_missing || jsd_desktop_missing )) && [[ "${JSD_AUTO_INSTALL_DEPS:-1}" != 0 ]]; then
        printf 'Installing missing compiler/build tools and Linux development headers. SDL2 itself is built from bundled source.\n'
        bash "$jsd_engine_root/install_deps.sh"
    fi
    for jsd_tool in "${CMAKE:-cmake}" "${CC:-cc}" "${CXX:-c++}"; do
        if ! command -v "$jsd_tool" >/dev/null 2>&1; then
            printf 'Missing build tool: %s. Run %s/install_deps.sh.\n' "$jsd_tool" "$jsd_engine_root" >&2
            return 1
        fi
    done
}
jsd_select_mode() {
    case "${1:-${JSD_BUILD_TYPE:-Release}}" in
        release|Release) jsd_mode=release; jsd_type=Release ;;
        debug|Debug) jsd_mode=debug; jsd_type=Debug ;;
        *) printf 'Build mode must be release or debug.\n' >&2; return 2 ;;
    esac
}
