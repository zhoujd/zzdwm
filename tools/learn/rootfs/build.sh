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
# Using a lightweight Alpine base as a structural example
wget -qO- https://alpinelinux.org | tar -xzf - -C "${ROOTFS_DIR}"

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
