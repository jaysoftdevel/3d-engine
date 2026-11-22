#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")"
with_showcase=OFF
case "${1:-}" in
    "") ;;
    --with-showcase|--with-sample) with_showcase=ON; shift ;;
    --help) printf 'Usage: ./test.sh [--with-showcase]\nEngine-only checks are the default.\n'; exit 0 ;;
    *) printf 'Usage: ./test.sh [--with-showcase]\n' >&2; exit 2 ;;
esac
if [[ $# -gt 0 ]]; then printf 'Unexpected test arguments.\n' >&2; exit 2; fi
bash ./build.sh "${JSD_BUILD_TYPE:-Release}" -DJSD_BUILD_TESTS=ON -DJSD_BUILD_EXAMPLES=ON -DJSD_BUILD_SHOWCASE="$with_showcase"
jsd_sdk="${JSD_INSTALL_PREFIX:-$PWD}"
"$jsd_sdk/bin/jsd-physics-tests"
"$jsd_sdk/bin/jsd-engine-tests" --graphics
MESA_GL_VERSION_OVERRIDE=3.3 MESA_GLSL_VERSION_OVERRIDE=330 "$jsd_sdk/bin/jsd-engine-tests" --graphics
"$jsd_sdk/bin/jsd-graphics-lab" --offscreen --preset high --view-distance 80 --width 640 --height 360 --frames 8 --render-scale .67 --no-ui
if [[ "$with_showcase" == ON ]]; then
    "$jsd_sdk/bin/jsd-showcase-physics-tests"
    "$jsd_sdk/bin/jsd-showcase" --offscreen --frames 8 --width 640 --height 360 --validate
fi
