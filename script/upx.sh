#!/bin/sh

# 1. Download the official static release
#curl -LO https://github.com/upx/upx/releases/download/v4.2.2/upx-4.2.2-amd64_linux.tar.xz
wget https://github.com/upx/upx/releases/download/v5.2.0/upx-5.2.0-amd64_linux.tar.xz

# 2. Extract the archive
tar -xf upx-4.2.2-amd64_linux.tar.xz

# 3. Move the binary into your PATH
sudo mv upx-4.2.2-amd64_linux/upx /usr/local/bin/

# 4. Clean up downloaded files
rm -rf upx-4.2.2-amd64_linux*
