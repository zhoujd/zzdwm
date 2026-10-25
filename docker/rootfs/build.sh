#!/usr/bin/env bash
# build.sh
set -euo pipefail

ROOTFS_DIR="build/rootfs"
OUTPUT_DIR="output"
OUTPUT_TAR="${OUTPUT_DIR}/rootfs.tar.gz"

echo "=== Cleaning old environments ==="
sudo rm -rf "${ROOTFS_DIR}" "${OUTPUT_DIR}"
mkdir -p "${ROOTFS_DIR}" "${OUTPUT_DIR}"

echo "=== Pulling Alpine Linux Base Archetype ==="
DL_DIR="build"
FS_VER="v3.24"
FS_PKG="alpine-minirootfs-3.24.3-x86_64.tar.gz"
FS_URL="https://dl-cdn.alpinelinux.org/alpine/${FS_VER}/releases/x86_64/${FS_PKG}"
if [ ! -f "${DL_DIR}/${FS_PKG}" ]; then
    wget -P ${DL_DIR} ${FS_URL}
fi
tar -xzf ${DL_DIR}/${FS_PKG} -C "${ROOTFS_DIR}"

echo "=== Injecting Native Resolution Patches (Chroot) ==="
# Mount host DNS definitions so the chroot can access the internet to fetch packages
sudo cp /etc/resolv.conf "${ROOTFS_DIR}/etc/resolv.conf"
# Step into the rootfs and install the terminal definitions required by the TUI
sudo chroot "${ROOTFS_DIR}" apk update
sudo chroot "${ROOTFS_DIR}" apk add ncurses-terminfo-base

echo "=== Layering Custom Git Configurations and Binaries ==="
# Safely copy files tracked inside your repository over the rootfs tree
if [ -d "overlay" ]; then
    sudo cp -rp overlay/* "${ROOTFS_DIR}/"
fi

if [ -d "config" ]; then
    sudo cp -p config/hosts "${ROOTFS_DIR}/etc/hosts"
fi

echo "=== Compressing into final deployment bundle ==="
cd "${ROOTFS_DIR}"
sudo tar --numeric-owner --absolute-names -czpf "../../${OUTPUT_TAR}" .

echo "=== Complete! Target created at: ${OUTPUT_TAR} ==="
