#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")"
if [[ "${1:-}" == --help && $# == 1 ]]; then echo 'Usage: ./test.sh'; exit 0; fi
[[ $# == 0 ]] || { echo 'Unexpected arguments' >&2; exit 2; }
mode="${JSD_BUILD_TYPE:-Release}"
bash ./build.sh "$mode" -DJSD_BUILD_TESTS=ON
ctest --test-dir "${JSD_BUILD_DIR:-$PWD/build/${mode,,}}" --output-on-failure
