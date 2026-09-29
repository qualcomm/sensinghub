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

# Build and install QMI Framework (provides qmi_cci.h, qmi_idl_lib.h,
# qmi_idl_lib_internal.h, common_v01.h, libqmi_common/libqencdec/libqcci/libqcsi
# and qmi-framework.pc for pkg-config)
git clone --depth 1 https://github.com/qualcomm/qmi-framework.git /tmp/qmi-framework && \
    cd /tmp/qmi-framework && \
    autoreconf --install && \
    ./configure --prefix=/usr && \
    make -j"$(nproc)" && \
    sudo make install && \
    sudo ldconfig && \
	  sudo sed -i 's|^Cflags: .*|Cflags: -I${includedir} -I${includedir}/qmi_framework|' /usr/lib/pkgconfig/qmi-framework.pc && \
    cd / && rm -rf /tmp/qmi-framework

# Fetch FastRPC public headers (provides remote.h, AEEStdErr.h, etc. required
# by services/sensorsdaemon)
git clone --depth 1 https://github.com/qualcomm/fastrpc.git /tmp/fastrpc && \
    sudo mkdir -p /usr/include/fastrpc && \
    sudo cp /tmp/fastrpc/inc/*.h /usr/include/fastrpc/ && \
    rm -rf /tmp/fastrpc

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
  --with-fastrpc-includes=/usr/include/fastrpc \
  CPPFLAGS="-I/usr/include/nanopb -I/usr/include/qmi_framework" \
  CFLAGS="-I/usr/include/nanopb -I/usr/include/qmi_framework" \
  CXXFLAGS="-I/usr/include/nanopb -I/usr/include/qmi_framework" \
  LDFLAGS="-lprotobuf-nanopb"
make -j"$(nproc)"
make DESTDIR="${WORKSPACE}/build" install

echo "Build completed successfully."
