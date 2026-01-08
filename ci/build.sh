#!/usr/bin/env bash
set -euxo pipefail

echo "Running SensingHub build script..."

cd "${GITHUB_WORKSPACE}"
rm -rf build || true
mkdir -p build

autoreconf -fi
./configure --prefix=/usr ${BUILD_ARGS:-}
make -j"$(nproc)"
make DESTDIR="${GITHUB_WORKSPACE}/build" install

echo "Build done. Listing build/:"