#!/usr/bin/env bash
set -euxo pipefail

echo "Running SensingHub build script..."

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Load default build args if not provided.
if [ -z "${BUILD_ARGS:-}" ]; then
  source "${SCRIPT_DIR}/build_args.sh"
fi

echo "BUILD_ARGS=${BUILD_ARGS}"

WORKSPACE="${GITHUB_WORKSPACE:-$(pwd)}"
cd "${WORKSPACE}"

rm -rf build || true
mkdir -p build

autoreconf -fi
./configure ${BUILD_ARGS}
make -j"$(nproc)"
make DESTDIR="${WORKSPACE}/build" install

echo "Build completed successfully."
