#!/usr/bin/env bash

set -e

dep() {
    echo "==> Checking build dependencies..."
    local missing=()

    for cmd in make gcc strip upx; do
        if ! command -v "$cmd" >/dev/null 2>&1; then
            missing+=("$cmd")
        fi
    done

    if [ ${#missing[@]} -ne 0 ]; then
        echo "Missing tools: ${missing[*]}"
        echo "Attempting to install build tools & headers..."

        if command -v apt-get >/dev/null 2>&1; then
            sudo apt-get update && sudo apt-get install -y \
                    build-essential libncurses-dev upx
        elif command -v dnf >/dev/null 2>&1; then
            sudo dnf install -y \
                    gcc make ncurses-devel upx
        elif command -v pacman >/dev/null 2>&1; then
            sudo pacman -S --needed \
                    base-devel ncurses upx
        elif command -v apk >/dev/null 2>&1; then
            sudo apk add \
                    build-base ncurses-dev upx
        else
            echo "Package manager not detected."
            echo "Please install gcc, make, strip, upx, and ncurses headers manually."
            exit 1
        fi
    else
        echo "All build dependencies are installed!"
    fi
}

build() {
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
}

help() {
    echo "Usage: $0 {dep|build|help}"
    echo ""
    echo "Commands:"
    echo "  dep    Check and install required build tools (gcc, make, upx, ncurses)"
    echo "  build  Clean, statically compile, strip, and UPX compress nnn"
    echo "  help   Show this help message"
}

case "${1:-build}" in
    dep)
        dep
        ;;
    build)
        build
        ;;
    help|-h|--help)
        help
        ;;
    *)
        echo "Error: Unknown command '$1'"
        echo ""
        help
        exit 1
        ;;
esac
