#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")"
./build.sh
if [[ "${JSD_GPU:-auto}" == discrete ]]; then
    export DRI_PRIME="${DRI_PRIME:-1}"
    if [[ -r /proc/driver/nvidia/version ]]; then
        export __NV_PRIME_RENDER_OFFLOAD="${__NV_PRIME_RENDER_OFFLOAD:-1}"
        export __GLX_VENDOR_LIBRARY_NAME="${__GLX_VENDOR_LIBRARY_NAME:-nvidia}"
    fi
fi
exec ./bin/sample-game "$@"
