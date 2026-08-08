#!/usr/bin/env bash

set -e

echo "==> Cleaning old build..."
make clean >/dev/null 2>&1 || true

echo "==> Compiling hyper-optimized static nnn..."
make O_STATIC=1 \
     O_NOUG=1 \
     O_NORL=1 \
     O_NOX11=1 \
     CFLAGS="-Os -flto=$(nproc) -ffunction-sections -fdata-sections" \
     LDFLAGS="-static -Wl,--gc-sections -Wl,-s" \
     -j"$(nproc)"

echo "==> Stripping remaining debug symbols..."
strip --strip-all nnn 2>/dev/null || true

if command -v upx >/dev/null 2>&1; then
    echo "==> Compressing with UPX..."
    upx --ultra-brute nnn 2>/dev/null || upx --best nnn
else
    echo "==> UPX not found, skipping compression."
fi

echo ""
echo "--- Final Size ---"
ls -lh nnn
