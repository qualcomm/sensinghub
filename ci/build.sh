#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause-Clear
#
# Copyright (c) 2025 Qualcomm Innovation Center, Inc. All rights reserved.

set -euxo pipefail

# utils/inc/qshPb.h wraps its nanopb includes in an outer extern "C" block.
# nanopb's own pb.h already guards its C-linkage declarations internally
# and deliberately declares a C++-only "nanopb::MessageDescriptor" template
# after that internal guard closes; qshPb.h's outer extern "C" re-wraps that
# template too, giving it illegal C linkage. This is a source bug that
# should be fixed in qshPb.h (drop the outer extern "C"); until then, patch
# the installed header in place so C++ TUs compile.
patch_nanopb_header_for_cxx_linkage() {
  local pb_h="/usr/include/pb.h"
  if grep -q 'extern "C++"' "${pb_h}"; then
    return 0
  fi
  sudo sed -i \
    -e '/^namespace nanopb {$/i extern "C++" {' \
    -e '/^}  \/\/ namespace nanopb$/a }' \
    "${pb_h}"
}

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
  libbsd-dev \
  nanopb \
  libnanopb-dev

patch_nanopb_header_for_cxx_linkage

# Build and install QMI Framework (provides qmi_cci.h, qmi_idl_lib.h,
# qmi_idl_lib_internal.h, common_v01.h, libqmi_common/libqencdec/libqcci/libqcsi
# and qmi-framework.pc for pkg-config)
git clone --depth 1 https://github.com/qualcomm/qmi-framework.git /tmp/qmi-framework && \
    cd /tmp/qmi-framework && \
    autoreconf --install && \
    ./configure --prefix=/usr && \
    make -j"$(nproc)" && \
    make install && \
    ldconfig && \
	  sed -i 's|^Cflags: .*|Cflags: -I${includedir} -I${includedir}/qmi_framework|' /usr/lib/pkgconfig/qmi-framework.pc && \
    cd / && rm -rf /tmp/qmi-framework

# Fetch FastRPC public headers (provides remote.h, AEEStdErr.h, etc. required
# by services/sensorsdaemon)
git clone --depth 1 https://github.com/qualcomm/fastrpc.git /tmp/fastrpc && \
    mkdir -p /usr/include/fastrpc && \
    cp /tmp/fastrpc/inc/*.h /usr/include/fastrpc/ && \
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
# utils/src/qshJsonParser.cpp calls strlcpy(), a BSD extension not provided
# by glibc; force-include <bsd/string.h> (from libbsd-dev) for the
# declaration and link against libbsd for the definition. This is a source
# bug that should be fixed by adding the include directly in
# qshJsonParser.cpp and declaring the libbsd dependency in
# utils/Makefile.am/configure.ac.
#
# core/sessionImpl statically initializes std::atomic<T> class members
# (e.g. "std::atomic<bool> glinkSession::_is_thread_created = false;"),
# which needs C++17's guaranteed copy elision to compile; std::atomic's
# copy constructor is deleted, so pre-C++17 modes reject it. This should be
# expressed in configure.ac (e.g. AX_CXX_COMPILE_STDCXX([17])) rather than
# passed as a raw flag here.
./configure ${BUILD_ARGS} \
  --with-fastrpc-includes=/usr/include/fastrpc \
  CPPFLAGS="-I/usr/include/nanopb -I/usr/include/qmi_framework" \
  CFLAGS="-include bsd/string.h -I/usr/include/nanopb -I/usr/include/qmi_framework" \
  CXXFLAGS="-std=c++17 -include bsd/string.h -I/usr/include/nanopb -I/usr/include/qmi_framework" \
  LDFLAGS="-lprotobuf-nanopb -lbsd"
make -j"$(nproc)"
make DESTDIR="${WORKSPACE}/build" install

echo "Build completed successfully."
