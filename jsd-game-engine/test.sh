#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")"
./build.sh
jsd_sdk="${JSD_INSTALL_PREFIX:-$PWD}"
"$jsd_sdk/bin/jsd-engine-tests" --graphics
MESA_GL_VERSION_OVERRIDE=3.3 MESA_GLSL_VERSION_OVERRIDE=330 "$jsd_sdk/bin/jsd-engine-tests" --graphics
"$jsd_sdk/bin/jsd-graphics-lab" --offscreen --preset high --view-distance 80 --width 640 --height 360 --frames 8 --render-scale .67 --no-ui
if [[ -f ../sample-game/CMakeLists.txt ]]; then
    JSD_GAME_SKIP_ENGINE_BUILD=ON ../sample-game/build.sh
    ../sample-game/bin/sample-game --offscreen --frames 8 --width 640 --height 360 --assets ../sample-game/assets --validate
fi
