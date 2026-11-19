#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")"
./bin/jsd-engine-tests --graphics
MESA_GL_VERSION_OVERRIDE=3.3 MESA_GLSL_VERSION_OVERRIDE=330 ./bin/jsd-engine-tests --graphics
./bin/jsd-graphics-lab --offscreen --preset high --view-distance 80 --width 640 --height 360 --frames 8 --render-scale .67 --no-ui
if [[ -x ../sample-game/bin/sample-game ]]; then
    ../sample-game/bin/sample-game --offscreen --frames 8 --width 640 --height 360 --assets ../sample-game/assets
fi
