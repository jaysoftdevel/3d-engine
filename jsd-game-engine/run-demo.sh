#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")"
if [[ "${1:-}" == --showcase ]]; then
    shift
    exec ../sample-game/run.sh "$@"
elif [[ "${1:-}" == --graphics-lab ]]; then
    shift
elif [[ $# == 0 && -f ../sample-game/CMakeLists.txt ]]; then
    exec ../sample-game/run.sh
fi
./build.sh
exec "${JSD_INSTALL_PREFIX:-$PWD}/bin/jsd-graphics-lab" "$@"
