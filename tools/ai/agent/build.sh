#!/bin/sh

set -eu

SCRIPT_DIR=$(dirname "$(readlink -f "$0")")
MNT_DIR=$(git rev-parse --show-toplevel)
WS=$SCRIPT_DIR

usage() {
    app=$(basename $0)
    cat <<EOF
Usage: $app {option}
Options:
build|-b        build ai tools (debug)
release|-r      release with local toolchains
publish|-p      publish Linux static ai tools
install|-i      install ai tools (default: /usr/local/bin)
test|-t         run the ai regression tests (needs cc)
clean|-c        clean build products
EOF
}

build() {
    make -C "$SCRIPT_DIR" DEBUG=yes ai
    echo "Build debug done"
}

release() {
    make -C "$SCRIPT_DIR" release
    echo "Build release done"
}

install() {
    sudo make -C "$SCRIPT_DIR" install
    echo "Install release done"
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

test_ai() {
    make -C "$SCRIPT_DIR" test-ai
    echo "Test done"
}

case ${1:-""} in
    build|-b )
        build
        ;;
    release|-r )
        release
        ;;
    install|-i )
        install
        ;;
    publish|-p )
        publish
        ;;
    test|-t )
        test_ai
        ;;
    clean|-c )
        clean
        ;;
    * )
        usage
        ;;
esac
