#!/usr/bin/env bash
set -euxo pipefail

echo "Running SensingHub build script..."

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Load default build args if not provided
if [ -z "${BUILD_ARGS:-}" ]; then
  source "${SCRIPT_DIR}/build_args.sh"
fi

echo "BUILD_ARGS=${BUILD_ARGS}"

sudo apt-get clean
sudo apt-get update -y

sudo apt-get install -y --no-install-recommends \
  autoconf \
  automake \
  libtool \
  pkg-config \
  make \
  gcc \
  g++ \
  libprotobuf-dev \
  protobuf-compiler \
  libglib2.0-dev


WORKSPACE="${GITHUB_WORKSPACE:-$(pwd)}"
cd "${WORKSPACE}"

export CFLAGS="-g -Og -fno-eliminate-unused-debug-types"
export CXXFLAGS="-g -Og -fno-eliminate-unused-debug-types"
rm -rf build || true
mkdir -p build

autoreconf -fi
./configure ${BUILD_ARGS}
make -j"$(nproc)"
make install

echo "Build completed successfully."
