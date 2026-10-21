#!/bin/sh

set -eu

SCRIPT_DIR=$(dirname "$(readlink -f "$0")")

case "${1:-""}" in
    release|-r )
        make -C "$SCRIPT_DIR" release
        ;;
    build|-b )
        make -C "$SCRIPT_DIR" ai ai-tui
        ;;
    clean|-c )
        make -C "$SCRIPT_DIR" clean
        ;;
    * )
        echo "usage: $0 {release|-r|build|-b|clean|-c}" >&2
        exit 1
        ;;
esac
