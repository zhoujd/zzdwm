#!/bin/sh

set -eu

SCRIPT_DIR=$(dirname "$(readlink -f "$0")")
MNT_DIR=$(git rev-parse --show-toplevel)
WS=$SCRIPT_DIR

usage() {
    app=$(basename $0)
    cat <<EOF
Usage: $app {option}
Option:
build|-b        build ai and ai-tui
release|-r      release with local toolchains
publish|-p      publish Linux static ai and ai-tui
clean|-c        clean build products
EOF
}

build() {
    make -C "$SCRIPT_DIR" ai ai-tui
    echo "Build done"
}

release() {
    make -C "$SCRIPT_DIR" release
    echo "Build release done"
}

publish() {
    if [ -n "${INSIDE_DOCKER:-}" ]; then
        echo "Build release"
        make -C "$SCRIPT_DIR" clean
        make -C "$SCRIPT_DIR" all
    else
        img=${MUSL_IMG:-zhoujd/alpine}
        docker run \
            --name="build-ai-1" \
            --rm \
            -i \
            -u "$(id -u):$(id -g)" \
            -e INSIDE_DOCKER=1 \
            -v "$MNT_DIR:$MNT_DIR" \
            -w "$WS" \
            "$img" \
            sh -c "
            cat /etc/os-release &&
            make clean &&
            make all
            "
    fi
    echo "Build publish done"
}

clean() {
    make -C "$SCRIPT_DIR" clean
    echo "Clean done"
}

case ${1:-""} in
    build|-b )
        build
        ;;
    release|-r )
        release
        ;;
    publish|-p )
        publish
        ;;
    clean|-c )
        clean
        ;;
    * )
        usage
        ;;
esac
