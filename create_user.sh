#!/bin/bash

# If not provided, defaults to "ubuntu" with UID and GID of 1000
USER=${USER:-"ubuntu"}
USER_ID=${USER_ID:-1000}
GROUP_ID=${GROUP_ID:-1000}

# Create user
groupadd -g "$GROUP_ID" "$USER"
useradd -m -s /bin/bash -u "$USER_ID" -g "$GROUP_ID" "$USER"

# Add the user to sudo group
apt-get update
apt-get -qq install sudo
usermod -aG sudo "$USER"

# Add user to sudoers without password
echo "${USER} ALL=(root) NOPASSWD:ALL" > /etc/sudoers.d/"$USER"
chmod 0440 /etc/sudoers.d/"$USER"