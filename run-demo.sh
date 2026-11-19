#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")"
if [[ "${1:-}" == --showcase ]]; then
    shift
    exec ../sample-game/run.sh "$@"
elif [[ "${1:-}" == --graphics-lab ]]; then
    shift
    exec ./bin/jsd-graphics-lab "$@"
elif [[ $# == 0 && -x ../sample-game/bin/sample-game ]]; then
    exec ../sample-game/run.sh
else
    exec ./bin/jsd-graphics-lab "$@"
fi
