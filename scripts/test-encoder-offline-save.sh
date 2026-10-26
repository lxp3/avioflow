#!/bin/bash

set -euo pipefail

CONTAINER_ID="${1:-ea43a638fa81}"
WORKDIR="/data/user/lxp/docker/avioflow"
BUILD_DIR="${BUILD_DIR:-build}"

docker exec "${CONTAINER_ID}" bash -lc "
set -euo pipefail
cd '${WORKDIR}'
cmake -S . -B '${BUILD_DIR}' -DAVIOFLOW_ENABLE_BINARY=ON -DAVIOFLOW_ENABLE_PYTHON=OFF -DAVIOFLOW_ENABLE_NODE_JS=OFF -DBUILD_SHARED_LIBS=OFF
cmake --build '${BUILD_DIR}' --target ffmpeg-encoder-offline-save-test -j\$(nproc)
'./${BUILD_DIR}/avioflow/bin/ffmpeg-encoder-offline-save-test'
"
