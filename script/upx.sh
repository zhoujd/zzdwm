#!/bin/sh

## Compress the binary
## $ upx --ultra-brute nnn || upx --best nnn
## Revert it back to the original 1.3MB binary at any time
## $ upx -d nnn

# 1. Download the official static release
VER=5.2.0
curl -LO https://github.com/upx/upx/releases/download/v${VER}/upx-${VER}-amd64_linux.tar.xz

# 2. Extract the archive
tar -xf upx-${VER}-amd64_linux.tar.xz

# 3. Move the binary into your PATH
sudo mv upx-${VER}-amd64_linux/upx /usr/local/bin/

# 4. Clean up downloaded files
rm -rf upx-${VER}-amd64_linux*
