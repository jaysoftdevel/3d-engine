#!/usr/bin/env bash
set -euo pipefail
engine_root=$(cd -- "$(dirname -- "$0")" && pwd -P)
"$engine_root/build.sh" "${JSD_BUILD_TYPE:-release}" -DJSD_BUILD_PROJECT_PLAYER=ON
engine_prefix=${JSD_INSTALL_PREFIX:-$engine_root}
if [[ $# -eq 0 ]]; then set -- --project "$engine_root/projects/sundog/sundog.jsdproject"; fi
exec "$engine_prefix/bin/jsd-project-player" "$@"
