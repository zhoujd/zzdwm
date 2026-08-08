#!/bin/sh

set -e  # Exit immediately if any command fails

echo "==> Cleaning old build artifacts..."
make clean

echo "==> Building static, size-optimized nnn binary..."
make CFLAGS="-Os -flto -ffunction-sections -fdata-sections" \
     LDFLAGS="-static -Wl,--gc-sections -Wl,-s" \
     O_NOUG=1 \
     O_NORL=1 \
     O_NOX11=1

echo "==> Stripping all symbols..."
strip --strip-all nnn

if command -v upx >/dev/null 2>&1; then
    echo "==> UPX found! Compressing binary..."
    upx --ultra-brute nnn 2>/dev/null || upx --best nnn
else
    echo "==> UPX not found (skipping executable compression)."
fi

echo "==> Build complete!"
echo ""
echo "--- Binary details ---"
file nnn
echo ""
echo "--- Final Size ---"
ls -lh nnn
