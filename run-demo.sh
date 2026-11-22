#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")"
if [[ "${1:-}" == --showcase ]]; then
    shift
    exec bash ./run-showcase.sh "$@"
elif [[ "${1:-}" == --graphics-lab ]]; then
    shift
fi
bash ./build.sh "${JSD_BUILD_TYPE:-Release}" -DJSD_BUILD_EXAMPLES=ON
exec "${JSD_INSTALL_PREFIX:-$PWD}/bin/jsd-graphics-lab" "$@"
