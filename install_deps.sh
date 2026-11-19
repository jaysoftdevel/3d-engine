#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")"
exec ./jsd-game-engine/install_deps.sh "$@"
