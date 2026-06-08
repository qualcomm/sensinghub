#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause-Clear
#
# Copyright (c) 2025 Qualcomm Innovation Center, Inc. All rights reserved.

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
  libglib2.0-dev \
  python3 \
  python3-pip

# Install nanopb generator
python3 -m pip install --user nanopb

# Ensure protoc-gen-nanopb is in PATH
export PATH="$HOME/.local/bin:$PATH"

# Debug
which protoc
which protoc-gen-nanopb

WORKSPACE="${GITHUB_WORKSPACE:-$(pwd)}"
cd "${WORKSPACE}"

rm -rf build || true
mkdir -p build

autoreconf -fi
./configure ${BUILD_ARGS}
make -j"$(nproc)"
make DESTDIR="${WORKSPACE}/build" install

echo "Build completed successfully."
