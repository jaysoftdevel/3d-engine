#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")"
if [[ "${1:-}" == --help && $# == 1 ]]; then echo 'Usage: ./test.sh'; exit 0; fi
[[ $# == 0 ]] || { echo 'Unexpected arguments' >&2; exit 2; }
mode="${JSD_BUILD_TYPE:-Release}"
exec python3 -B tools/build_project.py --root "$PWD" --project jsd.engine --expected 0.5.29 --test -- "$mode"
