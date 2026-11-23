#!/usr/bin/env bash
set -euo pipefail
project_root=$(cd -- "$(dirname -- "$0")" && pwd -P)
exec python3 -B "$project_root/tools/build_project.py" --root "$project_root" --project jsd.engine --expected 0.5.21 -- "$@"
