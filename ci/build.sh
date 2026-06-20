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
  nanopb \
  libnanopb-dev

# If the requested --host cross-compiler is not present, fall back to native build
if echo "${BUILD_ARGS}" | grep -q -- '--host='; then
  HOST_TRIPLE=$(echo "${BUILD_ARGS}" | grep -oP '(?<=--host=)\S+')
  if ! command -v "${HOST_TRIPLE}-gcc" &>/dev/null; then
    echo "Cross-compiler for ${HOST_TRIPLE} not found, falling back to native build"
    BUILD_ARGS=$(echo "${BUILD_ARGS}" | sed 's/--host=[^ ]*//g')
  fi
fi

echo "Effective BUILD_ARGS=${BUILD_ARGS}"

WORKSPACE="${GITHUB_WORKSPACE:-$(pwd)}"
cd "${WORKSPACE}"

rm -rf build || true
mkdir -p build

# Remove pre-generated proto files so they are regenerated with the
# current protoc version, avoiding version mismatch errors.
rm -rf apis/proto/proto_gen apis/proto/nanopb_gen

autoreconf -fi
./configure ${BUILD_ARGS} \
  CFLAGS="-I/usr/include/nanopb" \
  CXXFLAGS="-I/usr/include/nanopb" \
  CPPFLAGS="-I/usr/include/nanopb" \
  LDFLAGS="-lprotobuf-nanopb"
make -j"$(nproc)"
make DESTDIR="${WORKSPACE}/build" install

echo "Build completed successfully."
